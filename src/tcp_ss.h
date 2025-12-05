#pragma  once

#include <linux/tcp.h>
#include <linux/inet_diag.h>
#include <stdexcept>
#include <vector>
#include <optional>
#include <cstring>

namespace tcp_ss {

class SocketError : public std::runtime_error
{
public:
    SocketError(std::string const & message) throw(): std::runtime_error(message) {};
    virtual char const* what() const throw() { return exception::what(); };
};

struct SocketStats {
    int family;
    unsigned rqueue, wqueue, retrans;
    unsigned sport, dport, if_index;
    std::string source, dest, state;
    tcp_info info;
    std::optional<tcp_info> prev_info;

    double rtt, rttvar;

    SocketStats() {};
    SocketStats(inet_diag_msg * diag_msg, tcp_info info_);

    std::string connection_key() const;
    void update(SocketStats const & prev_stats);
};

std::vector<SocketStats> get_tcp_socket_stats();
}