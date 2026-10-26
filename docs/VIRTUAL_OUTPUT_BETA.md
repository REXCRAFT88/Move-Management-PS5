# Combined DualSense output beta

2026-10-26: Move + Nav is the first system output target. It requires both
assigned to the same player, with no second Move assigned. Existing mapping
uses Move T as R2, Move as Cross/X, physical Cross/X as L1,
Nav stick as left stick, Nav L2 as L2,
and calibrated Move motion as right stick and/or gyro, according to layout.

## Startup evidence

The module-only probe successfully loaded UserService, Mbus and Pad and
resolved exports. Calling UserServiceInitialize through the libdl-resolved
pointer did not complete. Rebuilding the same probe with SDK linked imports
returned UserServiceInitialize=0 and PadInit=0. The complete vpad client then
initialized and restored its original credentials successfully.

The finite neutral-device probe returned AddDevice=0x803b0006, observed one
matching virtual-device announcement, bound it successfully, fed neutral
frames without reported refusal, removed it, and restored credentials.
This establishes a device lifecycle, not verified user input or game support.
No shell injection, broad virtual-pad purge or internal-radio reads were used.

## Explicit test build

Build with `EXTERNAL_BT=1 VIRTUAL_BETA=1`. Artifact:
`dist/MoveStudio-Virtual-External-beta6.9.elf`. Normal builds retain their guard.
The beta exposes the mode selector, but does not turn combined output on by
default or create the experimental marker. USB-register Move and Nav against
the external adapter, assign both to P1, and select One DualSense.

Validate home-screen navigation, analog T/R2 and Nav L2, gyro/right stick,
game recognition, pair removal, service stop and native DualSense reconnect.
Multiple virtual pads currently bind to the foreground/initial signed-in user;
four distinct PS5 user assignments are not implemented by this beta.

## Related requests and references

- [OmniPad](https://github.com/diegobarbosaa/OmniPad-PS5), inspected at
  b7682efce18ee0a8d7c80bbc90afbce72151ef96: linked Pad/UserService imports,
  assignment-pending handling, USB DS3 handshake/parser, shell injection.
  Research reference only; no shell patching or generic fixed-layout parser copied.
- [AnyPad English](https://github.com/S-Mohammad-A/AnyPad-PS5-English): existing
  virtual DualSense foundation; its README also documents native Bluetooth
  interference and separate-user limitations. It supplies no established
  internal-radio restoration fix.
- [PoorDS4](https://github.com/ItsBlurf/PoorDS4) forwards native DS4 input into
  applications. This does not establish native virtual Move or Access output.
- DualShock 3 shares much of the working Nav report/USB enable protocol, so
  it is a useful next input profile. It still needs full buttons/sticks/analog
  triggers, registration, transport and hardware tests; not supported yet.
- PS2 USB converters vary by HID descriptor and VID/PID. Use descriptor-based
  parsing or individually verified profiles; do not assume a universal packet.
- Native virtual Move and Nav-as-Access remain research. Do not relabel a
  DualSense as an Access controller or advertise the associated OS features
  without verified device type, capability and report semantics.
