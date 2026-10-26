# Camera dependencies

libuvc: https://github.com/libuvc/libuvc, revision d07de4fa23905ee8cac06f5d24b2a03d42a3b363 (BSD license; see vendor/libuvc/LICENSE.txt). Upstream source is unchanged and pinned as a Git submodule; camera/config/libuvc/libuvc_config.h is the local build configuration. Initialize dependencies with git submodule update --init. Built with eight asynchronous transfers and eight isochronous packets per transfer.

stb_image: https://github.com/nothings/stb (vendor/stb_image.h contains its dual public-domain/MIT license). Used only for bounded MJPEG decoding.

The PS5 adapter uses the SDK's FreeBSD libusb structures and libSceUsbd exports. It does not detach native USB drivers. UVC support requires compatible standard webcam descriptors and supported YUYV, MJPEG or RGB modes. ADB phone apps are not supported.
