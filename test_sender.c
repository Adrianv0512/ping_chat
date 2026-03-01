#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

struct icmp_header {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
};

int main(int argc, char *argv[]) {

  int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
  printf("%d\n", sock);
  printf("%s\n", strerror(errno));

  char buffer[1024];
  struct icmp_header *icmp = (struct icmp_header *)buffer;
  icmp->type = 8;
  icmp->code = 0;
  icmp->id = htons(getpid());
  icmp->seq = htons(1);
  icmp->checksum = 0;

  char *payload = "Hello from Ping Chat!";
  memcpy(buffer + sizeof(struct icmp_header), payload, strlen(payload));


  struct sockaddr_in dest = { .sin_family = AF_INET };
  inet_pton(AF_INET, "127.0.0.1", &dest.sin_addr);
  sendto(sock, buffer, sizeof(struct icmp_header) + strlen(payload), 0,
       (struct sockaddr *)&dest, sizeof(dest));


  return 0;
}




