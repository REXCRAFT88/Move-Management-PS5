# Move Management for PS5

Move Management (formerly Move Studio) is an experimental homebrew payload and local web dashboard for PS Move and Navigation controllers on a jailbroken PS5. It manages controller connections, player assignments, motion input, calibration, and camera observations for compatible software. It is a standalone input service for PS5 software.

**Current version: beta6.12. Hardware evidence comes from firmware 13.60. This is a development beta, not a finalized compatibility release.**

## Features and status

| Feature | Current evidence and limits |
| --- | --- |
| Move buttons and analog T | Hardware reports and earlier compatible-application gameplay verified. |
| Wireless Navigation controller | Buttons, stick and L2 verified; reconnection sometimes requires a full controller power cycle or USB setup. |
| External USB Bluetooth | ASUS adapter tested with Move + Nav while native DualSense reconnect continued working. Other adapters are candidates, not certified. |
| Automatic USB setup | Plug in Move/Nav, wait for the ready notification, unplug USB, then press PS. Original host backups are retained. |
| Four players, eight physical software slots | Assignments and routing tested synthetically. Eight simultaneous physical connections are not verified. |
| Player management | P1–P4 buttons, automatic primary/secondary roles, per-controller disconnect and color selection. |
| Gyro calibration | Stationary zeroing, progress, movement/timeout rejection and reset; session-only correction. Synthetic tests pass; hardware retest needed. |
| Screen aiming setup | center-and-corner center, top-left and bottom-right targets, physical Move-button confirmation, live pointer test and saved per-player geometry. Synthetic tests pass; TV/controller test needed. |
| PS3 Eye tracking | Live preview, colored-sphere detection and ignored wall-light regions. False positives remain possible. |
| Phone camera | Local HTTP MJPEG/IP Webcam input, automatic /video path, preview and USB fallback. User has connected a real phone; long-session stability remains under test. |
| USB UVC camera | libuvc backend builds and mock tests pass. Real generic UVC streaming is not certified. |
| Move + Nav virtual DualSense | Explicit experimental build; creation/binding/neutral input/removal verified. Real button, game and multiplayer validation is pending. |
| Native Move / Access controller output | Research only. Export availability is verified, registration/report semantics are not. |

Screen calibration currently affects the Move Management test pointer and saves its own geometry. It does not alter other applications or native games. An application must explicitly integrate the bridge and any desired calibration profile.

## Getting started

1. Start your PS5 jailbreak and ELF payload loader.
2. Prefer a tested external Bluetooth adapter. Start the external-only payload.
3. Open `http://<PS5-IP>:8095/` from a device on your local network.
4. Connect a Move or Nav by USB and turn it on. Wait for the ready-to-unplug notification, unplug it, then press PS.
5. Assign controllers to P1–P4. Use **Calibration**, **Button layouts**, and **Camera tracking** as needed.

After adapter removal or a new external session, repeat USB registration. Seamless wireless recovery is not claimed. The service can remain running when the web page is closed. Full service stop also closes the website; use your payload loader to start it again.

## Bluetooth limitation

Reading the PS5 internal Bluetooth event endpoint has broken native DualSense reconnection in isolation tests. Closing the reader did not immediately restore it. Rest mode/wake recovered some tests; a restart was needed in others. No reliable live Bluetooth reset is implemented.

When using internal Bluetooth experimentally, connect desired DualSense controllers before starting. If reconnection fails, stop the bridge and use rest mode/wake, then restart if necessary. External-only builds do not fall back to the internal radio. The tested ASUS setup preserved native reconnect during use and after shutdown.

## Build

Clone with the pinned libuvc submodule and use a PS5 payload SDK:

```sh
git submodule update --init --recursive
export PS5_PAYLOAD_SDK=/absolute/path/to/ps5-payload-sdk
make src/web_page.c
make ps5 EXTERNAL_BT=1
```

This builds `dist/MoveManagement-External-beta6.12.elf`. The explicitly experimental combined-output build is:

```sh
make ps5 EXTERNAL_BT=1 VIRTUAL_BETA=1
```

It produces `dist/MoveManagement-Virtual-External-beta6.12.elf`. Combined output starts off; enable it in the dashboard after assigning one Move + one Nav to a player. Default mapping: Move button → X, physical X → L1, T → R2, Nav stick → left stick, Nav L2 → L2. Move motion can supply right-stick input and/or gyro.

`make ps5` builds the internal-radio experiment; it is not the recommended coexistence path. `make camera` builds the optional camera payload. Deploy one exact ELF with the SDK's `prospero-deploy -h <PS5-IP> -p <loader-port> <ELF>`; never run duplicate bridge instances.

See [build and validation](docs/BUILD_AND_TEST.md) for targeted tests, dependencies and deployment steps. This source repository does not bundle a private SDK, games, firmware, camera images, runtime controller identities, pairing files or old debug binaries.

## Developer and AI documentation

Start with the [documentation index](docs/README.md), then:

- [Current feature status](docs/STATUS.md) and [machine-readable capabilities](docs/capabilities.json)
- [AI handoff and invariants](docs/AI_HANDOFF.md)
- [Architecture](docs/ARCHITECTURE.md), [input protocol](docs/PROTOCOL.md), [HTTP API](docs/HTTP_API.md)
- [Calibration](docs/CALIBRATION.md), [automatic USB setup](docs/AUTOMATIC_USB_SETUP.md), [session policy](docs/USB_SESSION_POLICY.md)
- [Adapter compatibility](docs/ADAPTER_COMPATIBILITY.md), [native Bluetooth findings](docs/NATIVE_BT_RESEARCH.md)
- [Integration guidance](docs/INTEGRATION.md), [Move/Access research](docs/MOVE_ACCESS_OUTPUT_RESEARCH.md)

Historical documents mention earlier Move Studio/AnyPad builds; they are not current certification. Existing `/data/move-studio` paths, legacy internal identifiers and `X-AnyPad` header are retained for compatibility with existing installations.

## Sources and credits

This project builds on community work; [full credits and licensing notes](docs/CREDITS.md) distinguish included code from research references.

- [sinfiltros/AnyPad-PS5](https://github.com/sinfiltros/AnyPad-PS5) and [S-Mohammad-A/AnyPad-PS5-English](https://github.com/S-Mohammad-A/AnyPad-PS5-English): the original Bluetooth, virtual-controller and dashboard foundation.
- [Project Porpoise](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5), copyright Ruben: included GPL-3.0-or-later aiming fusion and pointer geometry, and the adapted center/corner workflow.
- [psmoveapi](https://github.com/thp/psmoveapi), [moveonpc](https://github.com/nitsch/moveonpc), [DsHidMini](https://github.com/nefarius/DsHidMini), and [Linux hid-sony](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c): controller-report and calibration references.
- [libuvc](https://github.com/libuvc/libuvc), [PS3EYEDriver](https://github.com/inspirit/PS3EYEDriver), [Linux OV534/gspca](https://github.com/torvalds/linux/tree/master/drivers/media/usb/gspca), and [stb](https://github.com/nothings/stb): camera/decoder dependencies and references. Original notices are retained.
- [PS5 payload SDK](https://github.com/ps5-payload-dev/sdk): toolchain, runtime and platform helpers.
- [PoorDS4](https://github.com/ItsBlurf/PoorDS4), [OmniPad](https://github.com/diegobarbosaa/OmniPad-PS5), [REMovePS4](https://github.com/nekohaku/REMovePS4), [ps-access](https://github.com/johnhenry/ps-access), [FGG-XSense](https://github.com/FGGstore/FGG-XSense), and [Ghostpad](https://github.com/srbraboo/Ghostpad): compatibility and output research; no complete ports are claimed.

GPL-3.0; see [LICENSE](LICENSE). Preserve individual third-party notices. This project is not affiliated with Sony or PlayStation.
