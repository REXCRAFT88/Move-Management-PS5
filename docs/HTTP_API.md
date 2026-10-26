# Dashboard API

HTTP port 8095. Existing local-peer and literal-Host checks apply. Mutations require `X-AnyPad: 1`; cross-origin requests receive no CORS grant. This service is not intended for internet exposure.

| Method/path | Behavior |
| --- | --- |
| GET /api/state | Version, Bluetooth status, pads, assignments, observations |
| GET /api/config | Four player layouts and guarded virtual status |
| POST /api/config?player=1&key=trigger&value=2 | Example setting; supported keys are validated in src/ps5_main.c |
| POST /api/assign-auto?addr=AA:BB:CC:DD:EE:FF&player=1 | Assign connected identity; player 1..4; infer hand role |
| POST /api/disconnect?addr=AA:BB:CC:DD:EE:FF | Disconnect one identity, retain registration |
| POST /api/disconnect-all | Disconnect bridge pads, keep web service |
| POST /api/restart-bluetooth | Restart raw host, keep web service; known coexistence failure remains |
| POST /api/stop | Full shutdown and camera stop marker; website disappears |
| GET /api/usb | Registration diagnostic text |
| GET /api/aim?addr=<controller address> | Screen-target stage, freshness, live x/y and half-angle ranges |
| POST /api/aim?addr=<controller address>&action=start | Start center/corner aiming wizard for an assigned primary Move |
| POST /api/aim?addr=<controller address>&action=cancel | Cancel the current wizard without deleting a saved profile |
| POST /api/aim?addr=<controller address>&action=save | Atomically save a completed, fresh screen profile for its player |
| GET /api/calibration?addr=<controller address> | Gyro calibration status (-1 unavailable, 0 ready, 1 sampling, 2 complete, 3 rejected) and progress percent |
| POST /api/calibration?addr=<controller address>&action=start | Start stationary session gyro zeroing on a connected Move |
| POST /api/calibration?addr=<controller address>&action=reset | Remove session gyro bias; retain factory calibration |
| GET /api/log | Bounded log tail |
| GET /api/camera | Source, message and UVC adapter capability |
| GET /api/camera-preview | image/bmp, no-store; 503 when stale/unavailable |
| GET /api/phone-camera | Configured phone URL, empty when disabled |
| POST /api/phone-camera?url=<encoded URL> | Configure local MJPEG stream; empty value disables |
| GET /api/camera-exclusions | Ignored unmirrored 320x240 rectangles |
| POST /api/camera-exclude?x=10&y=20&w=30&h=40 | Add region, maximum eight |
| POST /api/camera-exclude?clear=1 | Clear ignored regions |

Phone URLs require HTTP, a literal RFC1918 IPv4 address, explicit port 1024..65535 and a path. No hostname resolution, URL credentials, redirects, HTTPS, public/loopback targets or control characters. The stream is not contacted until configured.

For a rectangle drawn at mirrored display x with width w, raw x = 320-x-w. Exclusions suppress detections without blanking the preview. Clear them when moving the camera or changing sources.

Saved registration is not a connected controller. Bluetooth=ok means initialization succeeded, not that native controller coexistence works. Inspect src/web.c before depending on legacy assignment routes.
