#include "tcp_ss.h"

#include <iostream>
#include <format>

#include <arpa/inet.h>
#include <unistd.h>
#include <linux/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sock_diag.h>

namespace tcp_ss {

constexpr size_t BUF_SIZE = 1 << 16;
constexpr size_t TCP_ESTABLISHED = 1;

// adapted from ss.c
enum {
	SS_UNKNOWN,
	SS_ESTABLISHED,
	SS_SYN_SENT,
	SS_SYN_RECV,
	SS_FIN_WAIT1,
	SS_FIN_WAIT2,
	SS_TIME_WAIT,
	SS_CLOSE,
	SS_CLOSE_WAIT,
	SS_LAST_ACK,
	SS_LISTEN,
	SS_CLOSING,
	SS_NEW_SYN_RECV,
	SS_BOUND_INACTIVE,
	SS_MAX
};

// adapted from ss.c
std::string socket_state_to_string(__u8 state) {
	static const char * const sstate_name[] = {
		[SS_UNKNOWN] = "UNKNOWN",
		[SS_ESTABLISHED] = "ESTAB",
		[SS_SYN_SENT] = "SYN-SENT",
		[SS_SYN_RECV] = "SYN-RECV",
		[SS_FIN_WAIT1] = "FIN-WAIT-1",
		[SS_FIN_WAIT2] = "FIN-WAIT-2",
		[SS_TIME_WAIT] = "TIME-WAIT",
		[SS_CLOSE] = "UNCONN",
		[SS_CLOSE_WAIT] = "CLOSE-WAIT",
		[SS_LAST_ACK] = "LAST-ACK",
		[SS_LISTEN] =	"LISTEN",
		[SS_CLOSING] = "CLOSING",
		[SS_NEW_SYN_RECV] = "UNDEF",
		[SS_BOUND_INACTIVE] = "UNDEF",
	};
    return sstate_name[state];
}

std::string ip_to_string(__be32 const * ip, int family) {
    size_t len = family == AF_INET ? INET_ADDRSTRLEN : INET6_ADDRSTRLEN;
    std::vector<char> result(len+1);
    char const * ret = inet_ntop(family, ip, result.data(), len);
    if (ret == nullptr) {
        std::cerr << "failed parsing IP errno: " << errno << std::endl; 
    }
    return std::string(result.data());
}

SocketStats::SocketStats(inet_diag_msg * diag_msg, tcp_info info_):
    family{diag_msg->idiag_family},
    rqueue{diag_msg->idiag_rqueue},
    wqueue{diag_msg->idiag_wqueue},
    retrans{diag_msg->idiag_retrans},
    sport{ntohs(diag_msg->id.idiag_sport)},
    dport{ntohs(diag_msg->id.idiag_dport)},
    if_index{diag_msg->id.idiag_if},
    source{ip_to_string(diag_msg->id.idiag_src, family)},
    dest{ip_to_string(diag_msg->id.idiag_dst, family)},
    state{socket_state_to_string(diag_msg->idiag_state)},
    info{info_},
    rtt{info.tcpi_rtt/1000.0},
    rttvar{info.tcpi_rttvar/1000.0}
{

}

std::string SocketStats::connection_key() const {
    return std::format("{}{}_{}{}", source, sport, dest, dport);
}

void SocketStats::update(SocketStats const & prev_stats) {
    prev_info.emplace(prev_stats.info);
}

void send_query(int fd) {
    sockaddr_nl nladdr = {
        .nl_family = AF_NETLINK,
        .nl_pad = 0,
        .nl_pid = 0,
        .nl_groups = 0
    };
    __u8 idiag_ext = (1 << (INET_DIAG_INFO - 1));
    idiag_ext |= (1 << (INET_DIAG_VEGASINFO - 1));
    idiag_ext |= (1 << (INET_DIAG_CONG - 1));

    struct
    {
        struct nlmsghdr nlh;
        struct inet_diag_req_v2 idr;
    } req = {
        .nlh = {
            .nlmsg_len = sizeof(req),
            .nlmsg_type = SOCK_DIAG_BY_FAMILY,
            .nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP,
            .nlmsg_seq = 123456,
            .nlmsg_pid = 0
        },
        .idr = {
            .sdiag_family = AF_INET,
            .sdiag_protocol = IPPROTO_TCP,
            .idiag_ext = idiag_ext,
            .pad = 0,
            .idiag_states = 1 << TCP_ESTABLISHED,
            .id = {},
        }
    };

    iovec iov = {
        .iov_base = &req,
        .iov_len = sizeof(req)
    };
    msghdr msg = {
        .msg_name = &nladdr,
        .msg_namelen = sizeof(nladdr),
        .msg_iov = &iov,
        .msg_iovlen = 1
    };
    for (;;) {
        if (sendmsg(fd, &msg, 0) < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw SocketError(std::format("failed to send netlink message: errno={}", errno));
        }
        return;
    }
}

// adapted from ss.c
void parse_rtattr_flags(struct rtattr *tb[], int max, struct rtattr *rta,
                       int len, unsigned short flags)
{
    unsigned short type;

    std::memset(tb, 0, sizeof(struct rtattr *) * (max + 1));
    while (RTA_OK(rta, len)) {
        type = rta->rta_type & ~flags;
        if ((type <= max) && (!tb[type])) {
            tb[type] = rta;
        }
        rta = RTA_NEXT(rta, len);
    }
    if (len) {
        std::cerr << "len mismatch" << "len:" << len << "rta_len: " << rta->rta_len << std::endl;
    }
}

std::vector<SocketStats> receive_responses(int fd) {
    std::vector<char> buffer(BUF_SIZE);
    int len = 0;
    std::vector<SocketStats> stats;
    while ((len = recv(fd, buffer.data(), buffer.size(), 0)) > 0) {
        struct nlmsghdr *nlh = (struct nlmsghdr *)buffer.data();

        for (; NLMSG_OK(nlh, len); nlh = NLMSG_NEXT(nlh, len)) {
            // Check for END message
            if (nlh->nlmsg_type == NLMSG_DONE) {
                return stats;
            }
            // Check for ERROR message
            if (nlh->nlmsg_type == NLMSG_ERROR) {
                const struct nlmsgerr *err = reinterpret_cast<nlmsgerr*>(NLMSG_DATA(nlh));

                if (nlh->nlmsg_len < NLMSG_LENGTH(sizeof(*err))) {
                    std::cerr << "NLMSG_ERROR\n";
                } else {
                    errno = -err->error;
                    std::cerr << "errno: " << errno << std::endl;
                    std::cerr << "NLMSG_ERROR with errno\n";
                }
                throw SocketError(std::format("failed to receive netlink message: errno={}", errno));
            }

            struct inet_diag_msg *msg = reinterpret_cast<inet_diag_msg*>(NLMSG_DATA(nlh));

            struct rtattr *tb[INET_DIAG_MAX+1];
            parse_rtattr_flags(tb, INET_DIAG_MAX, (struct rtattr *)(msg+1), nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*msg)), NLA_F_NESTED);
            if (!tb[INET_DIAG_INFO]) {
                continue;
            }
            int payload_len = RTA_PAYLOAD(tb[INET_DIAG_INFO]);
            tcp_info info;
            std::memcpy(&info, RTA_DATA(tb[INET_DIAG_INFO]), payload_len);
            stats.push_back(SocketStats(msg, info));
        }
    }
    return stats;
}

std::vector<SocketStats> get_tcp_socket_stats() {
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_SOCK_DIAG);
    if (fd < 0) {
        throw SocketError("Failed to open socket");
    }

    std::vector<SocketStats> current_stats;
    try {
        send_query(fd);
        current_stats = receive_responses(fd);
        close(fd);
    } catch (SocketError & e) {
        close(fd);
        throw e;
    }
    return current_stats;
}
}
