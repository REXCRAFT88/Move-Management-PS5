# Integrating Move Management into PS5 software

Move Management is a standalone input producer. A PS5 game, emulator, homebrew application, engine, or diagnostic tool can consume its documented local input streams without embedding a Bluetooth host or camera driver.

## Pick an integration boundary

- **Input streams:** read button, stick, trigger and motion snapshots over loopback UDP. This is the main application-independent integration path.
- **Dashboard HTTP API:** use documented configuration/status endpoints when building a setup or management interface. Do not poll HTTP for per-frame gameplay input.
- **Screen geometry:** explicitly load saved per-player geometry if your application wants it. The profile does not automatically change aiming in another process.
- **Virtual DualSense:** this is a separate experimental system-output backend, not a requirement for consuming the input protocol.

Read [PROTOCOL.md](PROTOCOL.md) for exact bytes, units, validity flags and ports. The reference implementation is [move_management_client.hpp](../integrations/move_management_client.hpp), exposed as `move_management::Client`. Its GPL-3.0-or-later license and [source credits](CREDITS.md) apply; the wire specification can also guide a decoder implemented in the language of your application.

## Minimal reference-client workflow

```cpp
#include "move_management_client.hpp"

move_management::Client move;
// Open once during input initialization; P1 primary Move uses port 9976.
if (!move.open(9976)) {
    // Keep normal input available and report that the stream is unavailable.
}

// On each input tick, now_us is CLOCK_MONOTONIC time on this PS5.
move.poll(now_us);
if (move.fresh(now_us)) {
    const auto& packet = move.packet();
    // Translate packet.pad into the input state used by your application.
    // Check move.optical_fresh(now_us) separately before using camera fields.
} else {
    // Release this source's buttons/triggers and invalidate its motion/aim.
}

// At shutdown, release the input source and close its socket.
move.close();
```

[examples/udp_consumer.cpp](../examples/udp_consumer.cpp) is a complete five-second diagnostic reader. On a Linux development machine, compile it with `c++ -std=c++17 -pthread examples/udp_consumer.cpp -o build/udp_consumer`. It receives only if a compatible local producer is running; it does not control Bluetooth or inject inputs. Compile the same socket client with the PS5 SDK for an on-console consumer. This is a loopback protocol, not a PC-to-console streaming service.

## Input ownership and mapping

1. Keep native controller input available and make this source opt-in.
2. Open nonblocking loopback sockets once per player/hand stream you need.
3. Drain with a bounded per-frame budget and validate before exposing fields.
4. Publish snapshots through your normal input layer. Keep network work out of UI callbacks and avoid unsynchronized writes into engine state.
5. Translate button layouts once. In the wire format, Move is its own bit (`0x20000`); it is distinct from physical X. T is analog L2 and must not gate gyro aiming. A combined-output layout may remap these controls without changing the raw input contract.
6. On timeout, invalidation or disconnect, release this source's held inputs. Reset stateful orientation/filter data on a new connection generation.
7. Treat camera validity independently: losing a sphere must not discard still-valid buttons or gyro.
8. When Move owns aiming, suppress only the conflicting native gyro contribution. Preserve native buttons, sticks, triggers and menus according to the input policy of your application.

Use separate sources or explicit arbitration when merging controllers; do not accidentally add two aiming systems together. Emulator authors must separately implement their own guest-device semantics. A game or engine can map the same validated state directly to its action system.

## Players and hands

There are four logical players and eight physical software slots. A player may have one Move, Move + Nav, or two Moves. Nav and a second Move cannot occupy the same second-hand role. A second Move is not automatically a new player.

Identity, physical slot, logical player and application input port are different concepts. Slots can be reused after disconnect; do not derive ownership from connection order. Nav supplies buttons and a stick, not a Move IMU or glowing sphere. Hardware capacity and eight simultaneous wireless links remain unverified.

The protocol provides primary Move, Nav and secondary Move streams separately. Open only the streams required by your software. One socket consumer per port is supported; multi-application fanout is not implemented.

## Calibration and aiming

The producer applies factory scale and axis conversion, plus optional session gyro zeroing. Acceleration is in g including gravity; angular velocity is rad/s; time is monotonic microseconds. The published quaternion is currently identity, not measured orientation. Use your own orientation/fusion path rather than accepting it as a tracked pose. Do not reapply the producer's factory calibration or axis rotation.

Your application owns pointer geometry, recentering, sensitivity and any additional stationary-bias estimation. See [CALIBRATION.md](CALIBRATION.md) for the optional `MVA1` screen profile and [aim_screen_profile.hpp](../integrations/aim_screen_profile.hpp) for a validated parser. Loading the profile is an explicit choice; existing applications are not modified by saving it in Move Management.

Camera coordinates and apparent sphere size are image-space observations, not validated world position, depth or full 6DoF pose. Optical drift correction needs alignment and an explicit fusion policy. Never fill a 3D position field directly from a bounding box.

## Feedback and lifecycle

The documented feedback message and client helper do not establish a working producer feedback receiver: current standalone rumble receiving remains unimplemented. Do not advertise game rumble until both ends and a physical controller are validated.

The producer owns Bluetooth, USB registration and camera processes. Do not open a second raw reader for the PS5 internal radio. Prefer the tested external adapter path; native DualSense reconnect interference remains unresolved on the internal path. Consumer startup/shutdown should not restart the producer or disconnect unrelated controllers.

## Validation checklist

Test unavailable service, disabled integration, malformed/truncated/NaN packets, future/stale/duplicate timestamps, explicit invalidation, producer restart, slot reuse, reassignment, optical loss/reacquisition, native input arbitration, independent Move/X/T, and all four output channels. Test reconnect, sleep/resume, real hardware capacity and feedback separately; mocks do not certify those behaviors.

Run `make build/test_bridge_consumer && ./build/test_bridge_consumer` for the producer-to-client regression using synthetic calibration data. This does not require another application checkout.
