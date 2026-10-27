# Attribution and licensing

Retain actual upstream license files, copyright notices and required corresponding source. A reference link is not a license or endorsement. This project is not affiliated with Sony, Nintendo, Samsung or the projects below.

## Code heritage and dependencies

| Project | Relationship |
| --- | --- |
| [AnyPad PS5, sinfiltros / elmonomalvad0](https://github.com/sinfiltros/AnyPad-PS5) | Original host, HCI/L2CAP/HID, virtual-pad and payload foundations; GPL-3.0-or-later heritage. Original URL was unavailable during latest audit. |
| [S-Mohammad-A / AnyPad-PS5-English](https://github.com/S-Mohammad-A/AnyPad-PS5-English) | English source baseline; retain original authorship/license. Fresh source audit: 9475e9f. |
| [elripalda / Porpoise](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5) | Source of incorporated GPL-3.0-or-later aiming fusion, pointer geometry, target workflow and the adapted reference client. Retain original authorship and license notices. No dependency on that application is required. |
| [libuvc contributors](https://github.com/libuvc/libuvc) | BSD-licensed UVC dependency. Pinned submodule d07de4fa23905ee8cac06f5d24b2a03d42a3b363; see its LICENSE.txt. |
| [Sean Barrett and stb contributors](https://github.com/nothings/stb) | stb_image JPEG decoder; retain the header's dual MIT/public-domain notice. |
| [inspirit / PS3EYEDriver](https://github.com/inspirit/PS3EYEDriver) and [Linux gspca/OV534 contributors](https://github.com/torvalds/linux/tree/master/drivers/media/usb/gspca) | Eye registers/init references. Camera header declares GPL-3.0-or-later and notes GPL-2.0-or-later register-table provenance. Audit individual copied tables and preserve notices before redistribution. |
| [PS5 payload SDK contributors](https://github.com/ps5-payload-dev/sdk) | Toolchain/runtime/kernel helpers and USB ABI headers. Preserve notices for any redistributed SDK portions; do not bundle private caches. |

## Porpoise aiming code incorporated in beta6.11

`third_party/porpoise_aim/porpoise_aim.cpp` and `.hpp` are copied from Project
Porpoise, copyright 2026 Ruben, GPL-3.0-or-later, with original notices retained.
`src/aim_setup.cpp` adapts the center/corner sequence, 0.8 target positions and
screen-span checks from Porpoise's `src/ui_app_controls.cpp`. Source reference:
[Porpoise](https://github.com/elripalda/Porpoise-Dolphin-Emulator-for-PS5), as
present in this project's `feature/move-nav-integration` handoff checkout at
2c295d4. No changes to the Porpoise application are included here. These are actual included code,
not only research references. Preserve source and license in distributions.

## Other research references

- [bigboss-ps3dev/PS4EYECam](https://github.com/bigboss-ps3dev/PS4EYECam), [ps4eye/ps4eye](https://github.com/ps4eye/ps4eye), and [psmoveapi camera documentation](https://psmoveapi.readthedocs.io/en/latest/camera.html): PS4 camera initialization, USB 3/UVC and format research. No driver port or firmware is included; see PS4_HARDWARE.md. The existing psmoveapi credit also covers the ZCM2 report/calibration research.

- [johnhenry/ps-access](https://github.com/johnhenry/ps-access): real Access controller USB identity, profiles and physical input documentation; credits Jacek Fedorynski's earlier editor. Research only, no Access output implemented.
- [FGGstore/FGG-XSense](https://github.com/FGGstore/FGG-XSense) and [srbraboo/Ghostpad](https://github.com/srbraboo/Ghostpad): alternative virtual-device registration/report and assignment research. No code copied or shell patches installed; revisions and limits in MOVE_ACCESS_OUTPUT_RESEARCH.md.

- [thp/psmoveapi](https://github.com/thp/psmoveapi): Move reports and factory calibration interpretation. Its full 6DoF tracking/fusion is not included.
- [nitsch/moveonpc input report](https://github.com/nitsch/moveonpc/wiki/Input-report): ZCM1 buttons, T and sensor layout.
- [Linux hid-sony](https://github.com/torvalds/linux/blob/master/drivers/hid/hid-sony.c): Move RGB/rumble and Nav reports. Preserve notices if copying implementation, not merely interpreting a layout.
- [ItsBlurf/PoorDS4](https://github.com/ItsBlurf/PoorDS4): native Sony input ownership, application bridge and recovery. Reviewed, not merged wholesale.
- [X-F1REBALL-X/HearBridge-PS5](https://github.com/X-F1REBALL-X/HearBridge-PS5): coexistence/retry research. Issue 29 is comparative evidence, not proof of this project's cause.
- [diegobarbosaa/OmniPad-PS5](https://github.com/diegobarbosaa/OmniPad-PS5): virtual initialization comparison, not merged implementation.
- [FAMJAM1/CamRelay](https://github.com/FAMJAM1/CamRelay): Android HTTP framing/source inspection and proposed companion app. No APK bundled or silently installed.
- [diveshpatil9104/owlmic](https://github.com/diveshpatil9104/owlmic), [Genymobile/scrcpy](https://github.com/Genymobile/scrcpy): phone/USB alternatives researched, not implemented transports.
- [nekohaku/REMovePS4](https://github.com/nekohaku/REMovePS4), [nefarius/DsHidMini](https://github.com/nefarius/DsHidMini), [opentrack Eye instructions](https://github.com/opentrack/opentrack/wiki/PS3-Eye-open-driver-instructions): user-supplied earlier leads; no complete ports claimed.

## Release checklist

Include LICENSE and required third-party notices. Preserve authorship in modified files. Verify license obligations at the exact dependency revisions and provide corresponding source/build instructions where required. Exclude games, firmware/BIOS, credentials, pairing keys, identities, logs, personal screenshots and camera images. Update this document for each new copied/adapted source, identifying its revision, actual files, and whether it is code, dependency, adapted logic or research only.
