#include "protocol.h"
#include <stdlib.h>
#include <netinet/in.h>

int build_and_send(int sock,
                          const struct sockaddr_in *dest,
                          const char *message,
                          uint16_t seq);

int send_messages(const char* dest_ip, const char* one_shot);