# Loopback protocol

All multibyte values are explicitly little-endian; floats are IEEE-754 binary32. Never cast packets to a compiler-native struct. No authentication is provided: bind/send loopback only. One application owns each receive port; multi-consumer fanout is not implemented.

## Ports

Player indices are zero-based 0..3; UI numbers are 1..4.

| Stream | UDP port |
| --- | --- |
| Primary Move | 9976 + player |
| Nav | 9980 + player |
| Second Move | 10000 + player |
| Internal camera observations | 9990 |
| Internal preview BMP | 9991 |

Eight physical slots are distinct from four player outputs. Two Moves can be two hands for one player.

## Input v2: exactly 184 bytes

| Absolute offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | 0x3142564d, bytes MVB1 |
| 4 | u16 | version 2 |
| 6 | u16 | byte length 184 |
| 8 | u64 | process/stream sequence |
| 16 | u64 | producer PS5 CLOCK_MONOTONIC microseconds |
| 24 | u32 | valid input 0/1 |
| 28 | u32 | reserved zero |
| 32 | 120 bytes | pad payload below |
| 152,156 | f32 each | optical x/y, unmirrored 0..319 / 0..239; Y down |
| 160,164 | f32 each | apparent sphere width/height in pixels |
| 168 | u64 | optical monotonic time |
| 176 | u32 | optical valid 0/1 |
| 180 | u32 | reserved zero |

The reference decoder also accepts old v1 input packets of exactly 152 bytes (same header and pad payload, no optics). This producer sends v2.

### Pad payload offsets relative to byte 32

| Relative offset | Type | Meaning |
| --- | --- | --- |
| 0 | u32 | buttons |
| 4..7 | four u8 | LX/LY/RX/RY, center 128, Y down |
| 8,9 | u8 each | L2/R2 analog 0..255 |
| 10..11 | bytes | zero padding |
| 12..27 | four f32 | quaternion x/y/z/w: currently identity, not measured pose |
| 28..39 | three f32 | acceleration in g, including gravity |
| 40..51 | three f32 | angular velocity rad/s |
| 52..75 | bytes | touch fields, currently zero |
| 76..79 | u32 | connected 0/1 |
| 80..87 | u64 | sample timestamp, monotonic microseconds |
| 88..119 | bytes | extension/reserved fields, currently zero |

Calibrated axes: Pad X = Move X; Pad Y = Move Z; Pad Z = -Move Y, applied to both acceleration and gyro. Camera-image coordinates are a separate frame. Do not subtract factory gyro zero twice or swap roll/pitch again based on artwork.

### Button bits

| Value | Meaning |
| --- | --- |
| 0x1,0x2,0x4,0x8 | Select/Create, L3, R3, Start/Options |
| 0x10,0x20,0x40,0x80 | Up, Right, Down, Left |
| 0x100,0x200 | L2, R2 |
| 0x400,0x800 | L1, R1 |
| 0x1000,0x2000,0x4000,0x8000 | Triangle, Circle, Cross, Square |
| 0x10000 | PS |
| 0x20000 | Move button on the wire |

Internal pad_state uses PAD_TOUCHPAD (0x00100000) for Move and translates it during publication. T is L2 plus its analog byte. Nav L2 is also L2: stream identity determines meaning. Wii B or virtual R2 mapping belongs to the consumer/output profile.

### Validation and lifetime

Reject wrong magic/version/size, nonzero reserved fields, invalid flags, non-finite floats, out-of-range optics and future timestamps. On the same PS5, input older than 500 ms and optics older than 200 ms are stale. Release controls on timeout/invalidation. Optical loss is not a Bluetooth disconnect; hide stale boxes without disabling valid gyro/buttons.

Reject duplicate/out-of-order sequences within an active stream. A process restart may reset sequence; the reference client allows a new generation after timeout. Do not compare monotonic clocks across machines: a remote relay needs receipt-age handling or destination-domain retimestamping.

## Internal camera observations

Magic EYE1 (0x31455945). Header: u16 version at 4; u16 size at 6; u64 sequence at 8; u64 time at 16; u32 width/height at 24/28. Entries at 32 are 24 bytes: u32 valid; u32 area; f32 x/y/width/height. v1 has six entries (176 bytes); v2 has eight (224 bytes). Camera sends v2; receiver accepts both.

Palette order: blue, red, green, yellow, magenta, cyan, orange, violet. Primary players use blue/red/green/magenta; second Moves use unused colors. Orange/violet have synthetic tests, not eight-sphere hardware validation. Dashboard mirrors x; wire x remains unmirrored.

Preview is a 57,654-byte top-down 160x120 BGR BMP. HTTP cache expires after 1.5 seconds. It is for setup, not a full guest-camera emulation stream. Frames are not recorded to disk.

## Vibration feedback: consumer contract, producer gap

The reference consumer sends 32-byte MVR1 feedback to the input sender's loopback IP/ephemeral port: u16 version 1 at 4, u16 size 32 at 6, u64 input sequence at 8, u64 time at 16, u8 strong/weak at 24/25, u16 duration-ms at 26 (currently 250), zeros at 28..31.

The current standalone producer lacks the receive/timeout path. Older diagnostics and Move output reports are not proof of current end-to-end vibration. Implement peer/port validation, freshness, sequence, bounded duration and expiry-to-zero while preserving sphere color before claiming support.
