# Ping Chat
 
A terminal chat app that hides encrypted messages inside ICMP Echo Request packets so the traffic looks like ordinary `ping` activity to a network observer. Each packet carries a small custom `pc_header` (magic `0xC0DE`, message type, sequence, fragment info, payload length) immediately after the standard ICMP header. The payload is encrypted with AES-256-ECB before transmission. The receiver ignores all non-echo and non-Ping-Chat packets so normal pings pass through silently.
 
```
[ IP header (kernel) ][ ICMP header (8 B) ][ pc_header (11 B) ][ AES-256 ciphertext ]
```
 
## Features
 
- **Covert messaging** — messages ride inside ICMP echo requests, indistinguishable from normal pings to a casual observer
- **AES-256-ECB encryption** — payloads are encrypted via OpenSSL's EVP API using a pre-shared 32-byte key; captured packets show only ciphertext in Wireshark
- **Full-duplex** — send and receive simultaneously using pthreads; no need to run separate programs
- **Readline integration** — arrow-key history, line editing, and a clean `>>` prompt via libreadline
- **Colored output** — your messages in green, incoming messages in cyan
## Dependencies
 
- GCC
- OpenSSL (`libssl-dev`)
- Readline (`libreadline-dev`)
```bash
sudo apt install libssl-dev libreadline-dev
```
 
## Build
 
```bash
make        # builds ./chat
make clean  # remove binary
```
 
## Setup
 
Generate a 32-byte AES key (do this once, on one machine):
 
```bash
openssl rand -out chat.key 32
```
 
Copy the same key file to the other machine:
 
```bash
scp chat.key user@192.168.1.50:~/ping_chat/
```
 
Both sides must have the identical `chat.key` or decryption will fail.
 
## Usage
 
Requires **root** (raw sockets need `CAP_NET_RAW`).
 
### Interactive mode
 
```bash
sudo ./chat -i 192.168.1.50
```
 
Type a message and press Enter. Incoming messages appear in cyan, your messages in green. Use arrow keys to recall previous messages. Type `QUIT` or press Ctrl-D to exit.
 
### Single-message mode
 
```bash
sudo ./chat -i 192.168.1.50 -m "Hello from Ping Chat!"
```
 
### Localhost testing
 
```bash
sudo ./chat -i 127.0.0.1
```
 
## Options
 
| Flag | Default | Description |
|------|---------|-------------|
| `-i` | `127.0.0.1` | Destination IP address |
| `-m` | *(none)* | Send this message and exit; omit for interactive mode |
 
## Verify with tcpdump
 
```bash
sudo tcpdump -i lo icmp -X
```
 
The `0xc0 0xde` magic bytes will appear right after the 8-byte ICMP header in the hex dump. The payload following the `pc_header` will be encrypted ciphertext rather than readable ASCII.
 
## Architecture
 
- **chat.c** — main entry point; loads the encryption key, spawns the receiver thread, runs the sender loop
- **sender.c** — builds ICMP packets with the custom `pc_header`, encrypts the payload with AES-256-ECB, computes the RFC 1071 checksum, and sends via raw socket
- **receiver.c** — listens on a raw ICMP socket, filters for Ping Chat packets by magic number, decrypts the payload, and displays with timestamps
- **crypto.c / crypto.h** — AES-256-ECB encrypt/decrypt wrappers around OpenSSL's EVP API, plus key file loading
- **protocol.h** — defines the ICMP header, `pc_header` struct, magic number, message types, and the checksum function
## Notes
 
- The ICMP checksum (RFC 1071) covers the entire ICMP portion — header, `pc_header`, and ciphertext — with the checksum field zeroed before computation. Incorrect checksums are silently dropped by the kernel on real networks.
- Fragmentation fields (`frag_count`, `frag_index`) are present in the header but fixed at 1/0 for now; long-message splitting is a future milestone.
- AES-256-ECB is used for simplicity. Identical plaintext messages produce identical ciphertext, which is a known weakness. For stronger security, AES-256-CBC with a random IV per message would be the next step.
- Max message size is limited by `MAX_PAYLOAD` (1400 bytes) minus AES block padding overhead.