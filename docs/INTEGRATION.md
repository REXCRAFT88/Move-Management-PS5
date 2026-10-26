# Integrating into an emulator or application

Use loopback UDP as the boundary. Do not start another raw Bluetooth host inside an emulator. Preserve native Sony input and keep bridge input opt-in until coexistence is fixed.

## Consumer workflow

1. Keep native input as the default; add an explicit build flag and runtime switch.
2. Open nonblocking loopback sockets once per required player/hand stream. See [PROTOCOL.md](PROTOCOL.md).
3. Drain with a bounded per-frame budget; validate before exposing fields.
4. Publish snapshots through the emulator's input layer, not UI calls from a network thread.
5. Translate buttons once. T must remain independent and must not gate gyro aiming.
6. On invalidation/timeout, release buttons and stop feedback. Reset bias/fusion state on a connection-generation change.
7. Treat optical validity separately. Retain valid gyro/buttons when the sphere leaves the image.
8. Preserve DualSense menu buttons, sticks and triggers. Exclude only its gyro when Move owns the aiming source.

The reference consumer in integrations/porpoise_move_bridge.hpp is GPL-3.0-or-later and copied from the Porpoise handoff. Check licensing before incorporating its implementation into a different project. examples/udp_consumer.cpp demonstrates the API. Neither example replaces the full target input adapter.

## Players and hands

A player may have one Move, Move+Nav, or two Moves. Nav and second Move cannot occupy the same second-hand role. A second Move is not automatically another player. Move+Nav is the intended pair for future virtual DualSense mode; dual Moves are an emulator configuration.

Persistent address identity, physical slot, UI player number and emulator port are different concepts. Do not infer player from connection order. Nav has no IMU or sphere and must not overwrite Move motion.

## Calibration ownership

The producer applies factory scale and axis rotation. The consumer estimates stationary runtime bias, fuses orientation, maps aim and recenters. Acceleration is in g (including gravity), gyro in rad/s, time in microseconds. The quaternion currently sent is identity. Use the target's fusion path rather than treating it as measured orientation.

Optical pixels/apparent size are not world position. Drift correction needs camera/display alignment, scale and a fusion policy. Full pose/depth cannot be inferred by copying a bounding box into a guest-position field.

## Acceptance criteria

Test absent service, disabled flag/marker, truncated/corrupt/NaN data, future/stale/duplicate packets, explicit invalidation, producer restart, reassignment, optical loss/reacquisition, native menu input, T-independent aiming, and all four output channels. Hardware tests separately cover radio capacity, native reconnect, sleep/resume and recovery. A mocked packet test does not establish these behaviors on a console.
