#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>

#include "sender.h"
#include "receiver.h"
#include "crypto.h"
#include "ui.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-i dest_ip] [-m message]\n"
            "  -i  destination IP  (default: 127.0.0.1)\n"
            "  -m  message to send (if omitted, opens TUI)\n"
            "Requires root (raw socket).\n",
            prog);
}

static void *receive(void *arg) {
    (void)arg;
    /* don't let SIGINT kill the receiver — let main thread handle it */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    read_messages();
    return NULL;
}

uint8_t g_key[32];

int main(int argc, char* argv[]) {
    const char *dest_ip  = "127.0.0.1";
    const char *one_shot = NULL;

    int opt;
    while ((opt = getopt(argc, argv, "i:m:h")) != -1) {
        switch (opt) {
        case 'i': dest_ip  = optarg; break;
        case 'm': one_shot = optarg; break;
        case 'h':
        default:
            usage(argv[0]);
            return opt == 'h' ? 0 : 1;
        }
    }

    if (load_key("chat.key", g_key) != 0) {
        fprintf(stderr, "Failed to load key from chat.key\n");
        return 1;
    }

    if (one_shot) {
        /* one-shot path: skip the TUI entirely, behave like the old CLI */
        ui_init(dest_ip, g_key);
        int rc = send_messages(dest_ip, one_shot);
        ui_shutdown();
        return rc;
    }

    ui_init(dest_ip, g_key);

    pthread_t tid;
    if (pthread_create(&tid, NULL, receive, NULL) != 0) {
        ui_shutdown();
        fprintf(stderr, "pthread_create failed\n");
        return 1;
    }

    send_messages(dest_ip, NULL);

    pthread_cancel(tid);
    pthread_join(tid, NULL);

    ui_shutdown();
    return 0;
}
