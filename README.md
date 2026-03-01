# Ping Chat

A terminal-based chat application that uses ICMP echo request/reply packets as its transport layer. Messages are embedded in the data section of ICMP packets, making traffic appear as ordinary ping activity to network observers.

## Current Status — Week 1 Complete

Established proof of concept: we can embed custom payloads in ICMP echo request packets and extract them on the receiving end.

### What was built

- **`test_sender.c`** — Opens a raw ICMP socket, constructs an Echo Request packet with a custom payload string in the data section, computes the ICMP checksum, and sends it to a target IP address. The ICMP header fields (type, code, id, sequence number) are set manually and converted to network byte order using `htons()`.

- **`test_receiver.c`** — Opens a raw ICMP socket and listens for incoming packets with `recvfrom()`. Parses the IP header (using the `ihl` field to determine its variable length), locates the ICMP header and payload, and prints the extracted message. Payload length is determined arithmetically from the total bytes received rather than relying on null termination, which will be important once encryption is added in Week 4.

### Key takeaways from Week 1

- Raw sockets (`SOCK_RAW`) require root privileges to open.
- `recvfrom()` on a raw ICMP socket returns the full packet starting from the IP header, not the ICMP header. The IP header must be skipped using `ihl * 4` to reach the ICMP data.
- Multi-byte header fields must be converted between host and network byte order (`htons()`/`ntohs()`) to ensure correct interpretation across machines.
- The ICMP checksum must be computed with the checksum field set to zero, over the entire ICMP message (header + payload).
- Payload was verified using `tcpdump -i any icmp -X`, which showed the custom string in the ASCII dump of captured packets.

## Next Steps — Week 2

Design and implement the sender module with a custom protocol header prepended to each message payload. This header will include:
- A magic number to distinguish Ping Chat packets from normal pings
- A message type field
- A sequence number
- A fragment count (for future use in Week 5)

The sender should accept user input from the terminal rather than using a hardcoded string, and the target IP should be configurable via command line arguments.

## Build & Run

```bash
# Compile
clang test_sender.c -o test_sender
clang test_receiver.c -o test_receiver

# Run (requires root)
sudo ./test_sender
sudo ./test_receiver

# Verify with tcpdump on the receiving machine
sudo tcpdump -i any icmp -X
```

