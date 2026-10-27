# PS4 Camera and newer Move controllers

Status: investigated, not implemented or hardware-verified in this release.

## PS4 Camera

The PS4 Camera has a proprietary AUX connector carrying USB 3.0. A suitable
AUX-to-USB 3 adapter, such as the Sony CFI-ZAA1, and a SuperSpeed USB connection
are needed. A PS3 Eye USB 2 capture path is not interchangeable with this device.

[PS4EYECam](https://github.com/bigboss-ps3dev/PS4EYECam),
[ps4eye](https://github.com/ps4eye/ps4eye), and
[psmoveapi camera documentation](https://psmoveapi.readthedocs.io/en/latest/camera.html)
provide driver, initialization and camera-format references. The documented
desktop path uploads firmware before a UVC driver captures frames. On PS5,
first determine whether the console has initialized the camera already; do
not reset or upload firmware to a device owned by a native service blindly.

Remaining implementation work:

- Identify adapter and camera before and after initialization on the PS5.
- Establish native-service ownership and whether a separate initialization
  path is necessary; do not interfere with the console input services.
- Validate SuperSpeed endpoints and native USB asynchronous/isochronous
  capabilities in the existing transport adapter.
- Support the actual camera resolutions, formats and stereo image layout;
  our current generic UVC size list alone is not sufficient.
- Feed one decoded view into the existing RGB preview and sphere detector;
  calibrate stereo geometry separately before claiming depth or 3D tracking.
- Test plug/unplug, fallback, frame timing and concurrent controller input.

No camera firmware is bundled or downloaded by Move Management. Any eventual
firmware initialization must use an appropriately supplied image and preserve
the applicable distribution terms. These references do not establish PS5 support.

## PS4-era Move / CECH-ZCM2

[psmoveapi](https://github.com/thp/psmoveapi) supports the newer model and
documents model-specific report and calibration handling:
[input implementation](https://github.com/thp/psmoveapi/blob/master/src/psmove.c),
[calibration implementation](https://github.com/thp/psmoveapi/blob/master/src/psmove_calibration.c),
and [model API](https://github.com/thp/psmoveapi/blob/master/include/psmove.h).

The tested Move Management path is ZCM1. ZCM2 needs separate validation of USB
identity/host registration, wireless input lengths and sensor encoding, factory
calibration interpretation, light/rumble output, and restart/reconnect behavior.
Do not add its device ID to the ZCM1 profile and assume the bytes match. The
normalized button/trigger/motion/optical interface can be reused once a correct
model-specific backend is implemented.

Hardware captures and user tests are required before either device is listed
as supported. References above are research only; no new driver code is copied
or new hardware support claimed by beta6.12.
