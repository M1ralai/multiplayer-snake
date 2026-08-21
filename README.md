# Multiplayer Snake

A terminal-based peer-to-peer Snake prototype written in C++20. It explores a small binary UDP protocol, room-code encoding, network discovery helpers, concurrent input/network loops, and a tick-based game simulation.

## What this project demonstrates

- A POSIX UDP socket wrapper with non-blocking receive behavior
- Packed packets for join, accept/refuse, start, input, ping/pong, and disconnect messages
- An eight-character room code that encodes an IPv4 address and UDP port
- STUN binding-response parsing based on RFC 5389
- UPnP discovery and SOAP requests for optional port forwarding
- Separate input, network, and simulation threads with shared-state synchronization
- Automated checks for room-code, connection, STUN, and UPnP components

## Architecture

The host binds a UDP socket, advertises an address as a room code, and handles a versioned join handshake. After the host accepts a peer, both processes exchange direction packets while their local simulations advance on a fixed tick.

```text
keyboard input -> input thread ----\
                                    -> shared directions -> tick loop -> render
UDP peer ------> network thread ---/
```

Both peers receive a common random seed in the join flow so apple placement can follow the same sequence. Input packets include a tick number, but the current implementation applies received directions directly; it does not yet buffer, acknowledge, replay, or resynchronize inputs as a complete lockstep protocol would.

### Room codes and NAT helpers

`RoomCode` converts a four-byte IPv4 address plus a two-byte port into an eight-character URL-safe representation and decodes it back to a peer address. The STUN client parses mapped-address attributes to discover a public endpoint. The UPnP helper uses SSDP and SOAP to request a UDP mapping when the router supports the expected interface.

## Build and run

Requirements: a C++20 compiler, Make, and a POSIX environment. The Makefile defaults to `clang++`.

```bash
make
make run
```

Run the test executable with:

```bash
make test
```

The program presents host/join choices in the terminal. Hosts share the generated room code; joining peers decode it and initiate the UDP handshake.

## Controls

| Key | Action |
| --- | --- |
| `W` or Up | Move up |
| `S` or Down | Move down |
| `A` or Left | Move left |
| `D` or Right | Move right |
| `Q` | Quit |

## Limitations

- UDP delivery is unreliable; the protocol has no retransmission, sequencing window, or state recovery.
- Tick values are transmitted but a full deterministic lockstep/input-delay implementation is not present.
- Raw packed C++ structs are sent on the wire, so ABI, endianness, and cross-platform compatibility are not defined.
- STUN discovers a public endpoint but the project does not implement general NAT hole punching or a relay fallback.
- UPnP support depends on router behavior and assumes a limited control-path shape.
- Networking is IPv4-only, and compiler/platform portability is still incomplete.
- STUN and UPnP tests depend on the local network and may be skipped when those services are unavailable.
