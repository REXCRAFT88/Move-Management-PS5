# Current status and evidence

Move Management beta6.12, 2026-10-26. Previously named Move Studio. Hardware observations are from a jailbroken PS5 on firmware 13.60; other firmware is unverified.

## Hardware-tested paths

- ZCM1 Move buttons, analog T and calibrated tilt; earlier compatible-emulator gameplay confirmed T as B and aiming without holding T.
- Navigation stick/buttons/L2 over USB and external-adapter wireless input.
- ASUS external Bluetooth with Move + Nav, native DualSense reconnect during use, and reconnect after clean service stop. Wireless recovery is not universally reliable; USB registration is the accepted recovery path.
- Automatic USB registration with a ready-to-unplug notification; original host backup and write/readback verification.
- Multiple individual Moves and four visible camera spheres were tested. This does not certify eight simultaneous Bluetooth links.
- PS3 Eye sphere tracking/live preview and real phone HTTP MJPEG connection. False wall-light detections and long-session camera stability remain work items.

## Implemented with synthetic/mock or limited native validation

- Four player outputs, eight physical software slots, identity-based assignments and automatic hand/color selection. Eight-link admission/routing tests are not physical certification.
- Gyro zeroing and reset: stationary sampling, movement rejection, timeout and disconnection tests pass. User hardware retest pending.
- Center/top-left/bottom-right aiming wizard: shared fusion and pointer geometry; synthetic targeting, profile save and stale/disconnect tests pass. TV calibration/pointer feel pending. This is confined to Move Management and does not update other apps.
- Move + Nav virtual DualSense: native client startup and a neutral virtual-device create/bind/feed/remove lifecycle passed. Real controller input, game recognition and multiplayer output remain unverified. Normal builds stay guarded; the explicitly experimental external build exposes the selector with output initially off.
- UVC/libuvc adapter: native compile and API mock tests pass; real generic UVC stream pending.

## Known failures and unsupported features

- Reading internal PS5 Bluetooth events breaks native DualSense reconnect in tested probes. Closing the client does not reliably recover it. Rest/wake or restart may be needed. No reliable live reset is implemented. Prefer the tested external path; there is no internal fallback in external-only builds.
- Seamless external dongle hotplug/wireless recovery is not supported. Reconnect controllers by USB, wait for ready, then unplug and press PS. Nav can require full power-off/on.
- Only the ASUS external adapter has hardware evidence. Other vendor IDs and generic HCI detection are candidates; adapters requiring firmware upload are unsupported.
- Native Move and Access/Leonardo emulation are research only. Export presence and code inspection do not establish registration/report behavior.
- DualShock 3 and PS2 USB converter inputs are research only. Inherited DS4/Xbox/Switch profiles are not hardware-certified in this project.
- Standalone PS5 app-tile installation is not working. Use an ELF loader and the local website.
- Current standalone vibration feedback receiving is missing; earlier output-report/diagnostic acceptance does not establish current game rumble.
- Applications need their own input adapter. Camera observations are 2D, not validated 6DoF pose; published input quaternion remains identity.

See [Calibration](CALIBRATION.md), [virtual output beta](VIRTUAL_OUTPUT_BETA.md), [native Bluetooth evidence](NATIVE_BT_RESEARCH.md), [compatibility](ADAPTER_COMPATIBILITY.md), and [historical status](HISTORICAL_STATUS.md). Historical versions and upstream compatibility notes do not override this document.
