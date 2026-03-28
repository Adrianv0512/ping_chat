#define _POSIX_C_SOURCE 200809L

#include "protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int build_and_send(int sock,
                          const struct sockaddr_in *dest,
                          const char *message,
                          uint16_t seq)
{

    uint8_t buf[sizeof(struct icmp_header) + sizeof(struct pc_header)
                + MAX_PAYLOAD];
    memset(buf, 0, sizeof(buf));

    size_t msg_len = strlen(message);
    if (msg_len > MAX_PAYLOAD) {
        fprintf(stderr, "Message too long — truncating to %d bytes\n",
                MAX_PAYLOAD);
        msg_len = MAX_PAYLOAD;
    }

    struct icmp_header *icmp = (struct icmp_header *)buf;
    struct pc_header   *pch  = (struct pc_header  *)(buf + sizeof(*icmp));
    uint8_t            *payload = buf + sizeof(*icmp) + sizeof(*pch);

    icmp->type     = 8;
    icmp->code     = 0;
    icmp->id       = htons((uint16_t)getpid());
    icmp->seq      = htons(seq);
    icmp->checksum = 0;

    pch->magic       = htons(PC_MAGIC);   
    pch->msg_type    = PC_MSG_TEXT;
    pch->seq_num     = htons(seq);
    pch->frag_count  = htons(1);          
    pch->frag_index  = htons(0);
    pch->payload_len = htons((uint16_t)msg_len);

    memcpy(payload, message, msg_len);

    int icmp_total = (int)(sizeof(*icmp) + sizeof(*pch) + msg_len);
    icmp->checksum = compute_checksum(buf, icmp_total);


    ssize_t sent = sendto(sock, buf, (size_t)icmp_total, 0,
                          (const struct sockaddr *)dest, sizeof(*dest));
    if (sent < 0) {
        fprintf(stderr, "sendto: %s\n", strerror(errno));
        return -1;
    }

    printf("Sent %zu bytes to %s\n", msg_len, inet_ntoa(dest->sin_addr));
    return 0;
}

//AI GENERATED USAGE PRINT STATEMENT
static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-i dest_ip] [-m message]\n"
            "  -i  destination IP  (default: 127.0.0.1)\n"
            "  -m  message to send (if omitted, reads from stdin)\n"
            "Requires root (raw socket).\n",
            prog);
}

int main(int argc, char *argv[])
{
    const char *dest_ip  = "127.0.0.1";
    const char *one_shot = NULL;        

    int opt;
    while ((opt = getopt(argc, argv, "i:m:")) != -1) {
        switch (opt) {
        case 'i': dest_ip  = optarg; break;
        case 'm': one_shot = optarg; break;
        default:
            usage(argv[0]);
            return 1;
        }
    }

    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        fprintf(stderr, "socket: %s  (are you running as root?)\n",
                strerror(errno));
        return 1;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, dest_ip, &dest.sin_addr) != 1) {
        fprintf(stderr, "Invalid destination IP: %s\n", dest_ip);
        close(sock);
        return 1;
    }

    uint16_t seq = 1;

    if (one_shot) {
        int rc = build_and_send(sock, &dest, one_shot, seq);
        close(sock);
        return rc == 0 ? 0 : 1;
    }

    printf("Ping-Chat sender ready. Type a message and press Enter.\n");
    printf("Sending to %s  (Ctrl-D to quit)\n\n", dest_ip);

    char line[MAX_PAYLOAD + 2];  
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[--len] = '\0';

        if (len == 0)
            continue;  

        build_and_send(sock, &dest, line, seq++);
    }

    printf("Goodbye.\n");
    close(sock);
    return 0;
}
