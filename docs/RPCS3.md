# RPCS3 / PS3 emulator integration plan

Status: proposed. No RPCS3 source patch, binary replacement or installed PS3-emulator update is included. The previously inspected PS3 emulator is outside this change set.

## Goal and boundary

Add an opt-in Move Management host input provider for real Move buttons, analog T, calibrated motion and optical observations. Support two Moves per PS3 player without converting each hand into a separate DualSense.

Audit the specific PS5 RPCS3 fork's host input APIs and its Move/CellGem implementation against [upstream cellGem.cpp](https://github.com/RPCS3/rpcs3/blob/master/rpcs3/Emu/Cell/Modules/cellGem.cpp). Do not assume that current desktop APIs or structure layouts exist unchanged in that fork.

Recommended design:

- Receive and validate [the input protocol](PROTOCOL.md) in the emulator host layer. Keep Bluetooth ownership outside the emulator.
- Explicitly map four bridge players and primary/secondary hands to the target's Move slots. Keep Nav in a navigation/pad role.
- Translate T (L2 plus analog) and Move (wire bit 0x20000) without merging Move with Cross.
- Feed calibrated sensors into one existing or new fusion path. The transmitted quaternion is identity.
- Route optical loss to tracking validity without disconnecting buttons/gyro.
- Use emulator-native typed and endian-aware HLE structures. Never cast a network buffer into guest memory.
- Treat full guest camera emulation as another backend. Input packets contain observations, not full image frames; the low-rate dashboard BMP is not a PS3 Eye emulation stream.

## Prerequisites not provided

This bridge does not supply validated 3D pose, depth, magnetometer calibration, CellGem calibration semantics or full-rate guest camera frames. Determine game requirements before claiming compatibility. Two glowing spheres do not by themselves establish two calibrated Move poses.

## Proposed increments

1. Receive packets and show per-hand diagnostics, with no guest changes.
2. Map every button and analog T independently.
3. Add IMU/fusion and test pitch/yaw/roll separately.
4. Add 2D tracking-valid state with explicit camera/display calibration.
5. Implement full camera frames/position estimation where the game requires them.
6. Validate dual-handed single-player use, multiplayer, assignment changes, occlusion, shutdown and native input coexistence.

Use synthetic fixtures in CI. Do not bundle games, firmware, pairing keys or personal camera recordings.
