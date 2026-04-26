#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "sender.h"
#include "receiver.h"
#include "crypto.h"

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

void* receive(void*) {
    read_messages();
    return NULL;
}

uint8_t g_key[32];

int main(int argc, char* argv[]) {
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

    if (load_key("chat.key", g_key) != 0) {
        fprintf(stderr, "Failed to load key \n");
        return 1;
    }

    pthread_t tid;
    pthread_create(&tid, NULL, receive, NULL);

    send_messages(dest_ip, one_shot);
    pthread_cancel(tid);
    pthread_join(tid, NULL);
    return 0;
}
