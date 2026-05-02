#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "receiver.h"
#include "crypto.h"
#include "ui.h"

extern uint8_t g_key[32];

#define RECV_BUF \
    (60 + (int)sizeof(struct icmp_header) + (int)sizeof(struct pc_header) + MAX_PAYLOAD)

int read_messages() {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        char err[160];
        snprintf(err, sizeof(err),
                 "recv socket: %s (run as root)", strerror(errno));
        ui_push_message(UI_MSG_ERR, NULL, err);
        return 1;
    }

    ui_push_message(UI_MSG_SYS, NULL, "listening for ICMP messages");

    uint8_t buf[RECV_BUF];
    struct sockaddr_in src;
    socklen_t src_len;

    while (1) {
        memset(buf, 0, sizeof(buf));
        src_len = sizeof(src);

        ssize_t bytes = recvfrom(sock, buf, sizeof(buf), 0,
                                 (struct sockaddr *)&src, &src_len);
        if (bytes < 0) {
            if (errno == EINTR)
                continue;
            char err[128];
            snprintf(err, sizeof(err), "recvfrom: %s", strerror(errno));
            ui_push_message(UI_MSG_ERR, NULL, err);
            continue;
        }

        if (bytes < (ssize_t)sizeof(struct iphdr))
            continue;

        const struct iphdr *ip = (const struct iphdr *)buf;
        unsigned int ip_hlen = (unsigned int)ip->ihl * 4;

        if (bytes < (ssize_t)(ip_hlen + sizeof(struct icmp_header)))
            continue;

        const struct icmp_header *icmp =
            (const struct icmp_header *)(buf + ip_hlen);

        if (icmp->type != 8)
            continue;

        if (bytes < (ssize_t)(ip_hlen + sizeof(*icmp) + sizeof(struct pc_header)))
            continue;

        const struct pc_header *pch =
            (const struct pc_header *)(buf + ip_hlen + sizeof(*icmp));

        if (ntohs(pch->magic) != PC_MAGIC)
            continue;

        uint16_t payload_len = ntohs(pch->payload_len);

        ssize_t needed = (ssize_t)(ip_hlen
                                   + sizeof(*icmp)
                                   + sizeof(*pch)
                                   + payload_len);
        if (bytes < needed) {
            char err[128];
            snprintf(err, sizeof(err),
                     "truncated packet from %s — skipping",
                     inet_ntoa(src.sin_addr));
            ui_push_message(UI_MSG_ERR, NULL, err);
            continue;
        }

        if (payload_len > MAX_PAYLOAD)
            payload_len = MAX_PAYLOAD;

        const uint8_t *raw_payload =
            buf + ip_hlen + sizeof(*icmp) + sizeof(*pch);

        uint8_t decrypted[MAX_PAYLOAD + 1];
        int pt_len = my_decrypt(raw_payload, payload_len, g_key, decrypted);
        if (pt_len <= 0) {
            char err[96];
            snprintf(err, sizeof(err),
                     "decryption failed (ct=%d, pt=%d)",
                     payload_len, pt_len);
            ui_push_message(UI_MSG_ERR, NULL, err);
            continue;
        }
        decrypted[pt_len] = '\0';

        ui_push_message(UI_MSG_PEER,
                        inet_ntoa(src.sin_addr),
                        (const char *)decrypted);
        ui_stat_recv((size_t)bytes);
    }

    close(sock);
    return 0;
}
