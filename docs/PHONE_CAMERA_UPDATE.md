# Phone camera update: beta6.4, 2026-10-25

The user confirmed IP Webcam connects. Real-room tracking quality and latency are still being evaluated.

- A bare private-LAN HTTP IPv4 address with explicit port defaults to /video. A trailing slash also defaults to /video; custom stream paths remain supported. API saves the canonical full URL.
- Phone/UVC detection normalizes brightness and boosts chroma while preserving hue, rejects dark/near-neutral pixels, and retains white cores only when connected to colored support. It does not recolor the preview. PS3 Eye thresholds remain unchanged. Tests cover dim red/green/blue/magenta, neutral lights, dark color noise and unchanged input pixels.
- Automatic USB/Eye fallback remains enabled, as requested by the user. Phone retry eligibility is thirty seconds; frame-idle timeout is increased to eight seconds. Connection timeouts and decode time can add delay.
- JPEG payloads now use block copies bounded by JPEG and HTTP chunk lengths rather than byte-by-byte processing. Headers retain the strict bounded parser. A test streams two synthetic 1 MiB frames across socket-sized fragments.
- Live phone sampling observed 1920x1080 frames around 1.35 MB each. Recommend 640x480 with moderate JPEG quality for a 320x240 detector. No personal frames were saved.
- URL/parser/API and detector host tests pass; full SDK builds pass. Source pinning is a native manager change requiring user confirmation during real phone drop/reconnect. No optical 3D pose or complete gyro-drift correction is claimed.

Native DualSense reconnect and external Bluetooth transport limitations remain unchanged. One observed drop logged a format rejection; its exact invalid field is still unconfirmed.
