# Calibration menu

Beta6.11 adds center-and-corner screen aiming setup: center, top-left,
bottom-right, then live pointer testing. This is separate from stationary gyro
zeroing. The shared fusion and pointer math are GPL-3.0-or-later; see CREDITS.md.

## Screen aiming

1. Select an assigned primary Move and open Calibration on the TV being used.
   Aiming at a PC/phone page instead measures that display, not the TV. If the
   page cannot be displayed on the TV, use matching TV locations with targets
   inset 10% from its edges. Sit at the intended playing position.
2. Select Set up aiming. Point at center, hold the physical Move button still
   for half a second, and release. X is not the confirmation button.
3. Point at the top-left target and press/release Move. Repeat at bottom-right.
4. Test the live pointer, then select Save screen calibration. Invalid screen
   spans or stale input are rejected; no default profile is silently saved.

Screen geometry is saved atomically per player as
`/data/move-studio/aim-player-N.ini`, with `MVA1 half_x half_y yaw_offset
pitch_offset` (radians). Target geometry uses 0.8 normalized corner locations and validated span/aspect checks. Factory calibration is required.

This change is confined to Move Management. The wizard tests its own pointer and
saves its own per-player screen geometry. Select recenters the live test pointer.
Other applications and native games do not automatically consume this profile.
This is not a new quaternion or 6DoF wire protocol.

Host tests simulate the center/corner sequence, valid geometry, profile parsing,
save, input loss and disconnect. Native pointer feel, TV targets still require hardware testing. Navigation controllers are not optical
pointing devices and do not participate in these targets.

Beta6.10 adds a Calibration page for each connected ZCM1 Move. Place the
controller on a stable surface, release all buttons, select its player/device,
then select Start calibration. The page shows progress and completion/error.

The sampler requires three seconds and at least 100 input reports, rejects
rotation above 0.15 rad/s or acceleration changing by over 0.08 g on any axis,
and fails on input loss or a five-second deadline. Movement rejection preserves
the previous bias. This assumes the user follows the stationary instructions;
slow constant rotation cannot reliably be distinguished from gyro bias.

The measured raw gyro mean is applied at the existing conversion step to both
emulator packets and combined virtual output. Reset removes only that runtime
correction. Reconnecting resets it too; factory calibration, pairing and layouts
are untouched. No neutral game input is synthesized during sampling.

The page links aiming settings and camera setup and explains Select recentering
inside the emulator. It does not claim to calibrate screen geometry, orientation,
optical pose or Nav stick hardware. Current factory scaling remains required;
connect by USB if it is unavailable. Multi-device selection uses controller
identity, and disconnected/incorrect device requests are rejected.

Host tests cover zero-rate correction, motion rejection preserving old bias,
silent-device timeout, reset, disconnection and API request validation. Native
build and live UI/API checks are separate from hardware stationary calibration
and aiming verification, which require a user's controller test.
