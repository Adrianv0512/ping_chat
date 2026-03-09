#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <errno.h>
#include <string.h>
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

  char buffer[1024];

  struct sockaddr_in src = { .sin_family = AF_INET };

  socklen_t src_len = sizeof(src);

  // loop forever, checking for messages
  // TODO: Could handle SIGINT explicitly
  while (1) {
    int bytes_rec = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&src, &src_len);

    struct iphdr *ip_header = (struct iphdr*)buffer;
    unsigned int ip_header_len = ip_header->ihl * 4;

    struct icmp_header *icmp_head = (struct icmp_header*)(buffer + ip_header_len);

    char *payload = buffer + ip_header_len + sizeof(struct icmp_header);
    unsigned int payload_len = bytes_rec - ip_header_len - sizeof(struct icmp_header);
    printf("Received from %s: ", inet_ntoa(src.sin_addr));
    printf("%.*s \n", payload_len, payload);
  }
  return 0;	
}
