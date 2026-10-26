# Build and validation

Initialize the pinned libuvc dependency, then use a PS5 payload SDK with its FreeBSD USB ABI headers:

```sh
git submodule update --init
export PS5_PAYLOAD_SDK=/absolute/path/to/ps5-payload-sdk
make src/web_page.c
make ps5 EXTERNAL_BT=1
make camera
```

Edit web/index.html; src/web_page.c is generated. Do not bundle private .deps caches. WSL inherits the working directory, and executable shell scripts require LF line endings.

## Targeted host tests

```sh
make build/test_four_links build/test_players build/test_move_bridge build/test_nav_bridge
./build/test_four_links
./build/test_players
./build/test_move_bridge
./build/test_nav_bridge
make build/test_studio_web build/test_eye_detector build/test_eye_preview build/test_uvc_camera build/test_mjpeg_stream
./build/test_studio_web
./build/test_eye_detector
./build/test_eye_preview
./build/test_uvc_camera
./build/test_mjpeg_stream
make test_move_config build/test_vpad
./build/test_vpad
make build/test_aim_setup build/test_hci_external build/test_external_registration
./build/test_aim_setup
./build/test_hci_external
./build/test_external_registration
```

The historically named test_four_links now checks eight admissions, ninth rejection and slot reuse. UVC uses API stand-ins. Eye uses synthetic Bayer images. MJPEG tests check raw/chunked framing and URL restrictions. The broad upstream make test encounters Linux's missing FreeBSD sys/sysctl.h in the lock test; distinguish that environment issue from an input regression.

## Deployment

1. Read the live version; preserve only diagnostics needed for the failure.
2. Deliberate full replacement uses POST /api/stop, which also stops the website and signals the camera.
3. Wait for anypad.lock to disappear and camera completion. Never deploy duplicate main/camera instances.
4. Deploy the exact versioned main ELF through the configured payload loader. Verify /api/state.
5. Deploy the camera ELF once; verify /api/camera and preview freshness.
6. Reconnect controllers and separately test native controller access. Do not call the build finalized while native reconnect is broken.

Use SDK prospero-deploy with the user's actual console address/loader port; no private address is required by this package. User-facing Disconnect All and Restart Bluetooth retain the website; full stop does not.

## Camera test evidence

A finite server on the PS5 served generated JPEGs over chunked multipart HTTP. The receiver logged an active phone source and decoded frames. Clearing its URL restored Eye streaming. This validates receiver plumbing, not the actual Android app's camera permissions, capture, Wi-Fi latency or stability. No personal frame was needed.
