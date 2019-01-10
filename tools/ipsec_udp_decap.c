#define _GNU_SOURCE
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

// htons:
#include <arpa/inet.h>
// IPPROTO_UDP etc:
#include <linux/in.h>
// struct sadb_x_policy:
#include <linux/pfkeyv2.h>
// IPSEC_*:
#include <linux/ipsec.h>
// UDP_*
#include <linux/udp.h>

void error_neg_fun(int s, char *msg, char *file, int line) {
    if (s < 0)
        error_at_line(1, errno, file, line, "%s failed", msg);
}

#define error_neg(s, msg) error_neg_fun((s), (msg), __FILE__, __LINE__)


int main(int argc, char *argv[]) {
    int port;
    if (argc != 2 && argc != 4) {
        fprintf(stderr, "usage: %s <port> [<keepalive-ip> <keepalive-port>]\n", program_invocation_short_name);
        exit(1);
    }
    port = atoi(argv[1]);

    int sock;
    error_neg(sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP),
              "socket");
    error_neg(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int) { 1 }, sizeof(int)),
              "setsockopt");

    /*
      These two setsockopts make it so, that the ESP-over-UDP
      encapsulation socket never gets handled by IPsec, even if some
      of the policies would apply to it.

      Our use case is creating a tunnel over the internet, where both
      endpoints are behind NAT and in this case, we can be sure, that
      the policies will never match the socket, therefore we don't
      need them.  They require root access anyway, so let's just skip
      them.
    */

    /* struct sadb_x_policy policy; */
    /* memset(&policy, 0, sizeof(policy)); */
    /* policy.sadb_x_policy_len = sizeof(policy) / sizeof(uint64_t); */
    /* policy.sadb_x_policy_exttype = SADB_X_EXT_POLICY; */
    /* policy.sadb_x_policy_type = IPSEC_POLICY_BYPASS; */

    /* policy.sadb_x_policy_dir = IPSEC_DIR_OUTBOUND; */
    /* error_neg(setsockopt(sock, IPPROTO_IP, IP_IPSEC_POLICY, &policy, sizeof(policy)), */
    /*           "setsockopt policy out"); */

    /* policy.sadb_x_policy_dir = IPSEC_DIR_INBOUND; */
    /* error_neg(setsockopt(sock, IPPROTO_IP, IP_IPSEC_POLICY, &policy, sizeof(policy)), */
    /*           "setsockopt policy in"); */

    error_neg(setsockopt(sock, IPPROTO_UDP, UDP_ENCAP, &(int) { UDP_ENCAP_ESPINUDP },
                         sizeof(int)),
              "setsockopt UDP_ENCAP");

    // Note: we rely on INADDR_ANY being all 0s, so no need to hton it.
    error_neg(
              bind(sock, &(struct sockaddr_in) { AF_INET, htons(port), { INADDR_ANY }, { 0 } },
                   sizeof(struct sockaddr_in)),
              "bind");

    // In the 3 parameter mode, we are sending RFC3948 NAT-Keepalive packets
    if (argc == 4) {
        int target_port = atoi(argv[3]);
        struct in_addr target_address;
        error_neg(
                  inet_pton(AF_INET, argv[2], &target_address) ? 0 : -1,
                  "inet_pton");

        while (1) {
            error_neg(
                      sendto(sock, &(char) { 0xFF }, 1, 0,
                             &(const struct sockaddr_in) { AF_INET, htons(target_port), target_address, { 0 } },
                             sizeof(struct sockaddr_in)),
                      "sendto");
            sleep(5);
        }
    } else {
        pause();
    }

    return 0;
}
