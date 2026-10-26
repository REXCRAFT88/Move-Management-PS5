# Current status and evidence

Beta6.11 implements Porpoise-style center/top-left/bottom-right aiming targets,
live pointer testing and per-player saved screen profiles. Its fusion/geometry
and lifecycle pass synthetic tests. This feature is confined to Move Management;
other applications and native PS5 games do not consume these profiles.
See CALIBRATION.md. Actual target calibration and gameplay are not yet user-verified.

Beta6.10 adds a connected-Move Calibration page with stationary session gyro
zeroing, movement/timeout rejection, reset, and aiming/camera setup links.
See CALIBRATION.md for behavior and validation boundaries. Hardware gyro-zeroing
and resulting aiming still need user verification.

2026-10-26 virtual output: SDK-linked UserService/Pad imports passed native
client initialization and credential restoration. A finite neutral virtual
device was identified, bound, fed and removed successfully. Move + Nav gameplay
and home-screen input remain unverified. Explicit external-only beta6.9 build
exposes the mode selector; normal artifacts stay guarded. See VIRTUAL_OUTPUT_BETA.md.

The dashboard now has a persistent top notice: connect desired DualSense controllers before starting Move Management with internal Bluetooth; use rest mode/wake to recover reconnect, or restart if recovery fails. This is a workaround, not a native coexistence fix. The tested external ASUS setup has not reproduced the failure.

Snapshot: beta6.2, 2026-10-24. Experimental PS5 homebrew, tested on firmware 13.60; no general firmware compatibility guarantee.

Newer evidence (2026-10-25): normal camera build beta6.4 and external-only beta6.5-asus-test are documented in PHONE_CAMERA_UPDATE.md and EXTERNAL_BLUETOOTH.md. ASUS supports one tested wireless Move + Nav pair; user confirmed native DualSense reconnect while Move was active and after clean external shutdown. Move reconnected after bridge restart. Nav required a full controller power cycle, after which both HID channels and input profile became ready. Eight physical links, broad multiplayer/gameplay, long idle and camera concurrency are not certified. The internal-radio failure below remains unresolved; external coexistence avoids that access rather than repairing it.

| Feature | Evidence | Remaining work |
| --- | --- | --- |
| ZCM1 Move buttons and analog T | User gameplay confirmed T as Wii B; packet/profile tests | More games/controllers |
| Move gyro aim without holding T | User confirmed expected tilt axes | Camera-based drift correction |
| Select recenter | Implemented and user tested | Position consistency and long sessions |
| Nav USB/wireless | Guided hardware reports and gameplay; protocol tests | LED still blinks; repeated reconnect and multiplayer |
| Eight physical slots, four players | Eight-link admission, ninth rejection, highest-slot routing and assignment tests | Eight simultaneous internal-radio links not proven |
| Native DualSense in Porpoise | User confirmed menu inputs | Reconnect fails while bridge runs |
| PS3 Eye colored spheres | User reported strong tracking; PS5 preview verified | False wall-light detections; eight-color hardware tests |
| Live preview | PS5 image/bmp endpoint verified | Browser/touch UI QA |
| UVC backend | Builds; required USB exports verified; Eye fallback works | Real UVC device stream not hardware-tested |
| Phone HTTP MJPEG | Parser tests; PS5 decoded a synthetic TCP stream | Actual S23 Ultra/app permissions, capture and latency |
| Ignored wall-light areas | Explicit masks and detector/API tests | User draws regions; clear when camera moves |
| Virtual Move+Nav DualSense | Layout/motion and virtual-pad mock tests | Disabled: native service loading previously stopped payload |
| Standalone PS5 app tile | Launcher source exists | Installer/service loading failed; no working tile claimed |
| Vibration | Output-report support and older guided diagnostic evidence; consumer sends MVR1 | Current standalone producer lacks MVR1 receive/timeout path |
| RPCS3 | Protocol and integration plan | Not implemented or installed |

## Bluetooth regression record

Beta6.0 establishes Move/Nav connections, but a native DualSense may fail to reconnect after being turned off. Stopping the payload did not recover it in reported tests; rest mode or a restart did. Beta6.1 skipped shared configuration writes and reduced queued reads to 2/2. It did not fix DualSense and prevented Move links. Beta6.2 retains the operational beta6.0 transport. It is not a coexistence fix.

Finite open-only/read-only isolation payloads are prepared. They require an observer attempting native reconnect. They are not proven fixes. See root NATIVE-COEXISTENCE-AUDIT.md. Do not run duplicate instances or erase pairing keys to hide failures.

## Product boundaries

Camera observations are 2D pixels and apparent blob size, not validated 3D position, optical orientation or complete 6DoF pose. The producer's quaternion is identity. Camera-based pointer correction is unfinished. Four player outputs do not prove eight physical radio connections.

On 2026-10-25 the finite open-only USB probe also coincided with DualSense reconnect rejection ("device unsupported"). No reads were queued and no HCI commands sent. Clean-baseline recovery and isolation of open/init/endpoint allocation remain required; see NATIVE-COEXISTENCE-AUDIT.md.

2026-10-25 follow-up: after rest-mode recovery and a confirmed working reconnect baseline, an O_RDWR-only open of the internal Bluetooth USB node (no USB ioctls or reads) also produced the reported unsupported-device reconnect failure. Native sharing remains unresolved; main bridge is stopped for recovery.
