# Ping Chat

A terminal chat app that hides plaintext messages inside ICMP Echo Request packets so the traffic looks like ordinary `ping` activity to a network observer. Each packet carries a small custom `pc_header` (magic `0xC0DE`, message type, sequence, fragment info, payload length) immediately after the standard ICMP header. The receiver ignores all non-echo and non-Ping-Chat packets so normal pings pass through silently.

```
[ IP header (kernel) ][ ICMP header (8 B) ][ pc_header (11 B) ][ plaintext payload ]
```

## Build

```bash
make        # builds ./sender and ./receiver
make clean  # remove binaries
```

Requires GCC and the standard C library — no external dependencies.

## Usage

Both programs require **root** (raw sockets need `CAP_NET_RAW`).

### Interactive mode

Run the receiver in one terminal, then the sender in another. Type a line and
press Enter; it arrives on the other side immediately.

```bash
# Terminal 1
sudo ./receiver

# Terminal 2
sudo ./sender -i 127.0.0.1
Hello from Ping Chat!
This is a second message.
^D
```

### Single-message mode

```bash
sudo ./sender -i 127.0.0.1 -m "Hello from Ping Chat!"
```

### Remote host

```bash
# On the remote machine
sudo ./receiver

# Locally
sudo ./sender -i 192.168.1.42 -m "Hi from across the network"
```

## Sender options

| Flag | Default | Description |
|------|---------|-------------|
| `-i` | `127.0.0.1` | Destination IP address |
| `-m` | *(none)* | Send this message and exit; omit for stdin loop |

## Verify with tcpdump

```bash
sudo tcpdump -i lo icmp -X
```

The `0xc0 0xde` magic bytes will appear right after the 8-byte ICMP header in
the hex dump, followed by the message text in ASCII.

## Notes

- The ICMP checksum (RFC 1071) covers the entire ICMP portion — header,
  `pc_header`, and payload — with the checksum field zeroed before computation.
  Incorrect checksums are silently dropped by the kernel on real networks.
- Fragmentation fields (`frag_count`, `frag_index`) are present in the header
  but fixed at 1/0 for now; long-message splitting is a future milestone.
- Encryption (AES-256-CBC via OpenSSL) is a planned next step.
- Sender and receiver are separate executables today; full-duplex via
  `pthreads` comes later.
