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


#define RECV_BUF \
    (60 + (int)sizeof(struct icmp_header) + (int)sizeof(struct pc_header) + MAX_PAYLOAD)

int main(void)
{
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        fprintf(stderr, "socket: %s  (are you running as root?)\n",
                strerror(errno));
        return 1;
    }

    printf("Ping-Chat receiver listening... (Ctrl-C to stop)\n\n");

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
            fprintf(stderr, "recvfrom: %s\n", strerror(errno));
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

        //checking for our magic number
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
            fprintf(stderr, "Warning: truncated packet from %s — skipping\n",
                    inet_ntoa(src.sin_addr));
            continue;
        }

        if (payload_len > MAX_PAYLOAD)
            payload_len = MAX_PAYLOAD;   

        const uint8_t *raw_payload =
            buf + ip_hlen + sizeof(*icmp) + sizeof(*pch);

        char message[MAX_PAYLOAD + 1];
        memcpy(message, raw_payload, payload_len);
        message[payload_len] = '\0';

        time_t now = time(NULL);
        char ts[9];  
        strftime(ts, sizeof(ts), "%H:%M:%S", localtime(&now));

        printf("[%s] %s (seq %u): %s\n",
               ts,
               inet_ntoa(src.sin_addr),
               ntohs(pch->seq_num),
               message);
        fflush(stdout);
    }

    close(sock);
    return 0;
}
