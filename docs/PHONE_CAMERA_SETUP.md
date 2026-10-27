# Using your phone as the camera

This release supports a local HTTP MJPEG camera stream, including Android
IP Webcam. It does not bundle a phone app or require Android USB Webcam mode.

1. Put the phone and PS5 on the same local network.
2. In IP Webcam, start its camera server and allow camera access.
3. Start the main Move Management ELF and the separate `MoveManagement-Camera.elf`
   through your PS5 payload loader. Run only one instance of each.
4. Open `http://<PS5-IP>:8095/`, then **Camera tracking**.
5. Enter the phone address displayed by the app, including the port, for example
   `http://192.168.1.50:8080`. Select **Use phone camera**.
6. Keep the phone server running. Allow up to about 30 seconds for a retry,
   then check the preview and camera status.

The `/video` suffix is added automatically for a bare address or trailing slash;
a custom MJPEG path is also accepted. Use a private-LAN IPv4 address and an
explicit port. HTTPS, hostnames, credentials and redirects are not supported.
Start with 640x480 and moderate JPEG quality to reduce frame size and delay.

Phone input takes priority while configured. USB webcam/PS3 Eye fallback remains
available if its stream stops. Select **Use USB camera** to clear the phone URL.
No video is recorded by Move Management. Position the camera so the glowing Move
spheres remain in view; use ignored regions for stationary colored wall lights.

A real phone connection has been user-confirmed. Frame stability and tracking
quality depend on the phone, exposure, lighting and network; long-session and
recovery validation are still in progress. PS4 Camera and ZCM2 Move support are
not included; see [PS4 hardware research](PS4_HARDWARE.md).
