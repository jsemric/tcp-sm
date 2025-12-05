#include <algorithm>
#include <atomic>
#include <gtest/gtest.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#include <thread>

#include "tcp_ss.h"

namespace tcp_ss_test {

constexpr unsigned PORT = 12345;
constexpr unsigned SOCK_RETRIES = 10;

class OneshotTcpServer {

   int listen_sock = -1;
   int client_sock = -1;
   std::atomic<bool> done = false;


public:
   OneshotTcpServer() {}

   ~OneshotTcpServer() {
      close_sockets();
   }

   void stop() {
      done = true;
   }

   void close_sockets() {
      if (client_sock != -1) {
         close(client_sock);
         client_sock = -1;
      }
      if (listen_sock != -1) {
         close(listen_sock);
         listen_sock = -1;
      }
   }

   void start(unsigned port) {
      ASSERT_EQ(listen_sock, -1);
      ASSERT_EQ(client_sock, -1);

      struct sockaddr_in server_addr, client_addr;
      socklen_t addrlen = sizeof(client_addr);
      struct pollfd fds[1]; // Only need to monitor the listening socket

      /* 1. Setup Listening Socket */
      if ((listen_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
         ASSERT_FALSE("ERROR opening socket");
      }

      // Optional: Allow reuse of address/port
      int optval = 1;
      if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
         ASSERT_FALSE("setsockopt failed");
      }

      std::memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_addr.s_addr = INADDR_ANY;
      server_addr.sin_port = htons(port);

      if (bind(listen_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
         ASSERT_FALSE("ERROR on binding");
      }

      if (listen(listen_sock, 1) < 0) {
         ASSERT_FALSE("ERROR on listen");
      }

      fds[0].fd = listen_sock;
      fds[0].events = POLLIN;

      while (client_sock == -1 && !done) {
         int poll_count = poll(fds, 1, 100); 

         if (poll_count < 0) {
               if (errno == EINTR) continue; // Interrupted by signal, retry
         }
         
         if (poll_count == 0) {
               continue;
         }

         if (fds[0].revents & POLLIN) {
               client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &addrlen);
               
               if (client_sock < 0) {
                  if (errno == EAGAIN || errno == EWOULDBLOCK) {
                     client_sock = -1; // Keep the loop running
                     continue;
                  }
                  ASSERT_FALSE("failed accepting the connection");
               }
               break; 
         }
      }
   }
};

int connect_tcp(unsigned port) {
   int sock = 0;
   struct sockaddr_in serv_addr;

   if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      return -1;
   }

   std::memset(&serv_addr, 0, sizeof(serv_addr));
   serv_addr.sin_family = AF_INET;
   serv_addr.sin_port = htons(port);

   if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
       return -1;
   }

   if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       return -1;
   }
   return sock;
}

class TcpStatsTest : public testing::Test {
protected:
   OneshotTcpServer server;
   std::thread server_thread;

   void SetUp() override {
      server_thread = std::thread([this](){ server.start(PORT);});
   }

   void TearDown() override {
      server.stop();
      server.close_sockets();
      server_thread.join();
   }
};

TEST_F(TcpStatsTest, CanFindTcpSocketStats) {
   int socket = -1;
   // repeat couple of times to give server thread time to open the connection
   for (size_t i =0; i < SOCK_RETRIES && socket <= 0; i++) {
      socket = connect_tcp(PORT);
   }
   ASSERT_GT(socket, 0);
   auto stats = tcp_ss::get_tcp_socket_stats();
   close(socket);
   ASSERT_GT(stats.size(), 0);
   auto result = std::find_if(stats.begin(), stats.end(), [](tcp_ss::SocketStats const & stats) {
      return stats.sport == PORT;
   });
   ASSERT_NE(result, stats.end());
}

TEST_F(TcpStatsTest, CannotFindTcpSocketStats) {
   auto stats = tcp_ss::get_tcp_socket_stats();
   auto result = std::find_if(stats.begin(), stats.end(), [](tcp_ss::SocketStats const & stats) {
      return stats.sport == PORT;
   });
   ASSERT_EQ(result, stats.end());
}

}  // namespace

int main(int argc, char **argv) {
   testing::InitGoogleTest(&argc, argv);
   return RUN_ALL_TESTS();
}