#include "tcp_ss.h"

#include <getopt.h>
#include <linux/netlink.h>
#include <sys/socket.h>

#include <chrono>
#include <thread>
#include <iostream>
#include <vector>
#include <map>
#include <optional>
#include <cstring>
#include <format>

constexpr const char * USAGE = "Usage: tcp-stat [OPTIONS]\n"
"Dump real-time statistics about TCP socket\n"
"  -h, --help              show help and exit\n"
"  -s, --sport=PORT        source port to filter\n"
"  -d, --dport=PORT        destination port to filter\n"
"  -H, --host=HOST         host to filter\n"
"  -i, --interval=SEC      how frequently to show statistics (default 2s)\n"
;

struct cmd_options {

    std::optional<unsigned> dport, sport;
    std::optional<std::string> host;
    unsigned interval{2};

    cmd_options() {};

};

static const struct option long_opts[] = {
	{ "help", 0, 0, 'h' },
	{ "interval",1, 0, 'i' },
	{ "sport", 1, 0, 's' },
	{ "dport", 1, 0, 'd' },
	{ "host", 1, 0, 'H' },
};

void fail(std::string const & message, int exit_code = 1) {
    std::cerr << message << std::endl;
    std::exit(exit_code);
}

cmd_options parse_options(int argc, char *const * argv) {
    cmd_options args;
    int opt;
    
    while((opt = getopt_long(argc, argv, "s:d:i:H:h", long_opts, nullptr)) != -1) 
    { 
        switch(opt) 
        { 
            case 'h': 
                std::cout << USAGE << std::endl;
                std::exit(0);
            case 's': 
                args.sport.emplace(std::stoul(optarg));
                break; 
            case 'H': 
                args.host.emplace(optarg);
                break; 
            case 'i': 
                args.interval = std::max((unsigned)std::stoul(optarg), 1U);
                break; 
            case 'd': 
                args.dport.emplace(std::stoul(optarg));
                break; 
            case ':': 
                fail("option needs value");
                break; 
            case '?': 
                fail("unknown option");
                break; 
        } 
    } 
    return args;
}

std::string format_bytes(double bytes) {
    constexpr size_t N = 5;
    static const char * capacities[N] = {
        "B",
        "K",
        "M",
        "G",
        "T",
    };

    size_t index = 0;
    double curr = bytes;
    for (;index < N;index++) {
        if (curr < 1024) {
            break;
        }
        curr /= 1024;
    }
    return std::format("{:.2f}{}", curr, capacities[index]);
}

std::string format_stats(tcp_ss::SocketStats const & stats, unsigned interval) {
    double send_rate{0.}, recv_rate{0.};
    if (stats.prev_info.has_value()) {
        send_rate = (stats.info.tcpi_bytes_sent - stats.prev_info->tcpi_bytes_sent) * 1.0 / interval;
        recv_rate = (stats.info.tcpi_bytes_received - stats.prev_info->tcpi_bytes_received) * 1.0 / interval;
    }
    
    return std::format(
        "{} {:>16}:{:<6} -> {:>16}:{:<6} R-Q:{} W-Q:{} pmtu:{} mss:{} cwnd:{} rtt:{:.2f}/{:.2f}"
        " app_limited:{}"
        " received:{} sent:{} retrans:{} delivery_rate:{} send_rate:{}/s recv_rate:{}/s",
        stats.state, stats.source, stats.sport, stats.dest, stats.dport, stats.rqueue, stats.wqueue, stats.info.tcpi_pmtu,
        stats.info.tcpi_snd_mss, stats.info.tcpi_snd_cwnd, stats.rtt, stats.rttvar, stats.info.tcpi_delivery_rate_app_limited,
        format_bytes(stats.info.tcpi_bytes_received), format_bytes(stats.info.tcpi_bytes_sent), format_bytes(stats.info.tcpi_bytes_retrans),
        format_bytes(stats.info.tcpi_delivery_rate), format_bytes(send_rate), format_bytes(recv_rate));
}

int main(int argc, char * const * argv) {
    using namespace tcp_ss;
    cmd_options args;
    try {
        args = parse_options(argc, argv);
    } catch (std::invalid_argument &) {
        fail("invalid argument");
    }

    std::map<std::string,SocketStats> all_stats;
    for (;;) {
        std::vector<SocketStats> current_stats;
        try {
            current_stats = get_tcp_socket_stats();
        } catch (SocketError & e) {
            fail("received error: " + std::string(e.what()));
        }
        for (auto & s: current_stats) {
            // apply filters
            if (args.host.has_value() && args.host != s.source && args.host != s.dest) {
                continue;
            }

            if (args.sport.has_value() && s.sport != args.sport.value()) {
                continue;
            } 

            if (args.dport.has_value() && s.dport != args.dport.value()) {
                continue;
            } 

            auto key = s.connection_key();
            // update connection speeds
            if (all_stats.contains(key)) {
                s.update(all_stats.at(key));
            }
            all_stats.emplace(key, s);
            std::cout << format_stats(s, args.interval) << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(args.interval * 1000));
    }
    return 0;
}
