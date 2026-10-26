# Troubleshooting

## DualSense reconnect

Known unresolved bridge regression, reproducible with the camera off. Stop the bridge; reported recovery required rest mode/restart when stopping alone was insufficient. Re-establish native input before another diagnostic. Do not delete pairing keys or repeatedly reset every controller as a default response.

The next isolation tests open endpoints without reads versus with reads, sending no HCI commands. Their observer must attempt native reconnect during the finite window. Reducing queues and skipping setup writes was already insufficient. The source audit does not prove that consuming a foreign event can hand it back to Sony.

## S23 Ultra / phone camera

USB detection is not UVC support. The tested S23 Ultra's USB menu had no Webcam entry; descriptors exposed networking/debug interfaces rather than video. Samsung Camera Share is a separate Galaxy ecosystem feature.

Suggested app: [CamRelay](https://github.com/FAMJAM1/CamRelay). Source inspection confirms chunked multipart MJPEG at /video, port 8080. No APK is bundled or installed. When present, the user must install from upstream, grant camera permission and start video. For LAN use, disable the app's localhost-only/USB-only binding, put phone and PS5 on the same reachable LAN and enter its displayed URL in Motion Detection. Microphone access is unnecessary for tracking.

Start with rear camera, fixed mount, modest resolution such as 640x480, and check rotation/mirroring. The app can bake these into the feed. Real-phone performance is unverified. Direct USB/ADB camera transport for this phone is not implemented. Guest Wi-Fi isolation or a blocked port can prevent connection; do not change firewall/router security without authorization.

## Wall-light false detections

Use Mark ignored lights and draw over a stationary wall light. Controllers inside that region will also be ignored. Clear regions after moving the camera or switching sources. Automatically discarding every stationary blob is wrong because a player can hold a Move still.

## T or motion incorrect

Confirm ZCM1 VID 054c/PID 03d5, stored calibration, live role and packet freshness. T uses raw report samples 5/6 and digital bit 4:7, not Circle. Check pitch/yaw/roll independently. Avoid double bias subtraction. Nav PID 042f has no IMU.

## Website unavailable

Rest mode and full stop end the web server. A closed site cannot relaunch its own ELF. Use the payload loader or a separately validated native launcher. The media shortcut installer is unfinished.

## Virtual output

Keep the compatibility marker absent. Stage logs bracket credentials and each library load; logs do not make a faulting library safe. Native reconnect remains priority over enabling virtual mode. Mock success is not hardware proof.
