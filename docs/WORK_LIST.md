# Active work list

Updated 2026-10-25. Keep this list aligned with actual implementation and hardware evidence.

## Bluetooth

- [x] Initial external-only ASUS build test: user confirmed DualSense reconnects and navigates normally before Move/Nav migration. Long-session and simultaneous controller tests remain pending.
- [ ] Investigate external adapter becoming silent during idle; restarting its external session restored ready state in the first live bridge test.
- [ ] Fix Nav automatic reconnect after external service restart: initial wireless connection worked, but restart produced HID channel 0x11 setup timeout. Fully powering Nav off and back on recovered it without re-pairing; both channels/profile then opened. Move reconnect and DualSense coexistence passed their initial checks.
- [x] Migrate one deliberately selected ZCM1 Move to ASUS through USB, back up and verify its host, and confirm wireless dashboard connection. Nav migration remains pending; do not repoint every attached controller automatically.
- [ ] Validate input, output, reconnect, unplug/reinsert, multiplayer and native DualSense coexistence with the external backend.
- [x] Initial external shutdown check: Move/Nav disconnected cleanly, external scan setting restored, lock released, and user confirmed DualSense reconnects/navigates without rest mode or reboot. Reattachment, long sessions and adapter removal remain pending.
- [ ] Find a proven native Bluetooth recovery mechanism after internal raw-device access. A client HID Term/Init is not proven to restart the system service; rest mode has not recovered every observed failure.

## Additional controller inputs and outputs

- [ ] Validate beta6.9 Move + Nav virtual DualSense home-screen and game input, analog triggers, motion, disconnect cleanup and native DualSense reconnect. Native client startup and neutral virtual-device lifecycle passed on 2026-10-26; see VIRTUAL_OUTPUT_BETA.md.
- [ ] Implement and hardware-test DualShock 3 input using full report layout and USB enable/registration; preserve analog triggers rather than OmniPad's digital-only DS3 trigger interpretation.
- [ ] Add USB HID transport/descriptor-based PS2 converter support after collecting adapter identities and reports. No universal converter layout assumed.
- [ ] Research native virtual Move and Access controller report/capability APIs. PoorDS4 application forwarding alone does not establish these output types.

## Controller profile artwork

- [ ] In the **Wii Remote + Nunchuk** profile preview, show a **Navigation controller beside the Move controller**. Include both in the combined preview rather than showing Move alone. Keep the Wii Remote-only preview as Move alone; verify layout at phone and PS5 browser sizes. Requested by the user on 2026-10-25; not implemented yet.

## Camera

- [ ] Validate beta6.4 phone tracking and frame stability with moderate-resolution IP Webcam settings; keep automatic fallback.
- [ ] Identify the exact field behind the earlier MJPEG format rejection rather than attributing every drop to Wi-Fi or resolution.
