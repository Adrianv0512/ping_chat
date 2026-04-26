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
#include <arpa/inet.h>

#include "sender.h"
#include "crypto.h"

#include <readline/readline.h>
#include <readline/history.h>

extern uint8_t g_key[32];

int build_and_send(int sock,
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



    uint8_t ciphertext[MAX_PAYLOAD];
    int ct_len = my_encrypt((uint8_t *)message, (int)msg_len, g_key, ciphertext);
    memcpy(payload, ciphertext, ct_len);
    pch->payload_len = htons((uint16_t)ct_len); 
    int icmp_total = (int)(sizeof(*icmp) + sizeof(*pch) + ct_len);


    ssize_t sent = sendto(sock, buf, (size_t)icmp_total, 0,
                          (const struct sockaddr *)dest, sizeof(*dest));
    if (sent < 0) {
        fprintf(stderr, "sendto: %s\n", strerror(errno));
        return -1;
    }

    time_t now = time(NULL);
    char ts[12];
    strftime(ts, sizeof(ts), "%I:%M:%S %p", localtime(&now));
    printf("\033[32m[%s] You: %s\033[0m\n", ts, message); 
    return 0;
}

int send_messages(const char* dest_ip, const char* one_shot) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        fprintf(stderr, "socket: %s  (are you running as root?)\n",
                strerror(errno));
        exit(1);
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, dest_ip, &dest.sin_addr) != 1) {
        fprintf(stderr, "Invalid destination IP: %s\n", dest_ip);
        close(sock);
        exit(1);
    }

    uint16_t seq = 1;

    if (one_shot) {
        int rc = build_and_send(sock, &dest, one_shot, seq);
        close(sock);
        exit(rc);
    }

    printf("Ping-Chat sender ready. Type a message and press Enter.\n");
    printf("Sending to %s  (Ctrl-D or type QUIT to quit)\n\n", dest_ip);


    char *line;
    while ((line = readline(">> ")) != NULL) {
        if (strcmp(line, "QUIT") == 0) {
            free(line);
            break;
        }
        if (strlen(line) == 0) {
            free(line);
            continue;
        }
        add_history(line);
        build_and_send(sock, &dest, line, seq++);
        free(line);
    }

    printf("Goodbye.\n");
    close(sock);
    return 0;
}

