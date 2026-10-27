# Move Management developer handoff

This is the integration entry point for emulator, game, application and AI-assistant developers. Source, tests and this directory are the living handoff: update them together. Current publication: beta6.12, 2026-10-26. Earlier snapshots are historical evidence.

Read in this order:

For phone-camera users, start with [Phone camera setup](PHONE_CAMERA_SETUP.md).
For proposed additional hardware, see [PS4 Camera and ZCM2 research](PS4_HARDWARE.md).

1. [Current status and evidence](STATUS.md).
2. [Architecture](ARCHITECTURE.md).
3. [Wire protocol](PROTOCOL.md).
4. [PS5 software integration](INTEGRATION.md) and the [generic example](../examples/udp_consumer.cpp).
5. [Build and validation](BUILD_AND_TEST.md), [HTTP API](HTTP_API.md), [Troubleshooting](TROUBLESHOOTING.md).
6. [Attribution and licensing](CREDITS.md), [AI handoff](AI_HANDOFF.md).

This repository provides a standalone producer and a reference client for PS5 games, emulators, homebrew applications, and input tools. Keep camera and Bluetooth ownership in the producer and connect validated snapshots to the input layer of your software.

Never turn a successful compile, returned API status or synthetic packet test into a hardware-support claim. The native DualSense reconnect failure remains unresolved.

New in the standalone beta: [calibration](CALIBRATION.md), [automatic USB setup](AUTOMATIC_USB_SETUP.md), [external session policy](USB_SESSION_POLICY.md), [adapter compatibility](ADAPTER_COMPATIBILITY.md), and [Move/Access research](MOVE_ACCESS_OUTPUT_RESEARCH.md).
