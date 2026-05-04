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
#include "ui.h"

extern uint8_t g_key[32];

int build_and_send(int sock,
                          const struct sockaddr_in *dest,
                          const char *message,
                          uint16_t seq)
{

    uint8_t buf[sizeof(struct icmp_header) + sizeof(struct pc_header)
                + MAX_PAYLOAD + 16];
    memset(buf, 0, sizeof(buf));

    size_t msg_len = strlen(message);
    if (msg_len > MAX_PAYLOAD) {
        char err[64];
        snprintf(err, sizeof(err),
                 "message too long, truncating to %d bytes", MAX_PAYLOAD);
        ui_push_message(UI_MSG_ERR, NULL, err);
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

    uint8_t ciphertext[MAX_PAYLOAD + 16];
    int ct_len = my_encrypt((uint8_t *)message, (int)msg_len, g_key, ciphertext);
    memcpy(payload, ciphertext, ct_len);
    pch->payload_len = htons((uint16_t)ct_len);
    int icmp_total = (int)(sizeof(*icmp) + sizeof(*pch) + ct_len);

    ssize_t sent = sendto(sock, buf, (size_t)icmp_total, 0,
                          (const struct sockaddr *)dest, sizeof(*dest));
    if (sent < 0) {
        char err[128];
        snprintf(err, sizeof(err), "sendto: %s", strerror(errno));
        ui_push_message(UI_MSG_ERR, NULL, err);
        return -1;
    }

    ui_push_message(UI_MSG_SELF, "you", message);
    ui_stat_sent((size_t)sent);
    return 0;
}

int send_messages(const char* dest_ip, const char* one_shot) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sock < 0) {
        char err[160];
        snprintf(err, sizeof(err),
                 "socket: %s (run as root for raw ICMP)", strerror(errno));
        ui_push_message(UI_MSG_ERR, NULL, err);
        return 1;
    }

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    if (inet_pton(AF_INET, dest_ip, &dest.sin_addr) != 1) {
        char err[128];
        snprintf(err, sizeof(err), "invalid destination IP: %s", dest_ip);
        ui_push_message(UI_MSG_ERR, NULL, err);
        close(sock);
        return 1;
    }

    uint16_t seq = 1;

    if (one_shot) {
        int rc = build_and_send(sock, &dest, one_shot, seq);
        close(sock);
        return rc;
    }

    char banner[160];
    snprintf(banner, sizeof(banner), "sending to %s — type a message, enter to send", dest_ip);
    ui_push_message(UI_MSG_SYS, NULL, banner);
    build_and_send(sock, &dest, "Joined", seq++);
    char *line;
    while ((line = ui_read_line()) != NULL) {
        if (line[0] != '\0') {
            build_and_send(sock, &dest, line, seq++);
        }
        free(line);
    }
    build_and_send(sock, &dest, "Left", seq++);
    ui_push_message(UI_MSG_SYS, NULL, "goodbye.");
    close(sock);
    return 0;
}
