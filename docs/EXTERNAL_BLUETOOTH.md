# External Bluetooth transport development

2026-10-25: live descriptor inventory through sceUsbdGetDeviceList found ASUS 0b05:190e with Bluetooth class e0/01/01, plus Logitech 046d:c52b with HID interfaces. The latter is a Unifying receiver, not a general Bluetooth HCI adapter.

The finite tools/external-bt-probe.c targets only ASUS 0b05:190e. It uses Sony USB APIs, claims interface 0, sends HCI Read Local Version Information (0x1001), receives command completion on interrupt endpoint 0x81 and releases/closes the adapter. No internal-radio device is opened, no HCI reset issued, no firmware uploaded and no pairing changed.

Hardware result: init=0, open succeeded, claim=0, command sent=3 bytes, event 0e0c020110000a0b000a5d006187, release=0. Status is success, HCI version 0x0a, manufacturer 0x005d. This proves basic external command/event traffic, not Move/Nav pairing, ACL reliability, native DualSense coexistence or long sessions. Normal beta6.2 bridge/camera remained running during this probe and still reported active afterward.

## Next implementation

### Implementation progress, 2026-10-25

The new hci_external backend uses bounded synchronous command/event/ACL transfers through Sony's USB API. It validates ASUS 0b05:190e and HCI endpoints, reassembles events/ACL, handles removal, and releases/closes resources without pending callbacks. Mock tests cover identity/endpoint rejection, commands, fragmented traffic, removal, repeated close and reopen. The SDK build succeeds.

A finite hardware host initialization check succeeded: 6 ACL buffers of 1021 bytes, 8 LE buffers of 251 bytes, local address available, Simple Pairing/page scan enabled. It polled for ten seconds, restored page scan to its prior value, and closed successfully. It did not pair controllers or establish native DualSense coexistence.

The separate `make ps5 EXTERNAL_BT=1` build is beta6.5-asus-test. The internal hci_usb object is omitted entirely. It uses pads-asus.db and a separate asus-address.bin, and disables the automatic USB registration worker so existing controller host addresses are not silently changed. Calibration/assignment records remain shared. This experimental full bridge is built but not deployed: a clean native reconnect baseline is pending. Successful external close does not repair any damage left by an earlier internal-radio session.

The items below include remaining acceptance work; basic transport/host initialization is now implemented, not complete controller support.

- Add a distinct hci_ops backend using the external USB device handle. Explicit opt-in, no fallback to the internal node.
- Validate VID/PID and Bluetooth interface/endpoints before claim; handle removal/reinsert without assuming stable bus addresses.
- Maintain concurrent interrupt-event and bulk-ACL reads, bounded command/output transfers, reassembly, cancellation and complete cleanup.
- Test endpoint discovery, event/ACL fragmentation, timeouts, removal and errors using host mocks before deploying a bridge build.
- Test standard HCI initialization on the external adapter. ASUS USB-BT500 uses Realtek 8761BU; firmware/setup may need attention, despite Read Local Version succeeding. Do not blindly upload firmware.
- Keep adapter host addresses, controller pairing and saved registrations separate. Existing controllers are paired to the internal radio; do not silently overwrite their host addresses. Any user-selected move to the external adapter needs a backup and a clear USB pairing flow.
- Validate physical Move/Nav input and native DualSense reconnect separately, then multiplayer/camera together. One adapter can serve multiple links subject to hardware/stack capacity; two adapters do not automatically double capacity.

## References

### Initial external bridge hardware test

The external-only beta6.5-asus-test bridge was deployed after the user's reported rest/restart cycle. Before migrating Move/Nav, the user confirmed DualSense turns off, reconnects and navigates normally while this build runs. This is a first coexistence success, not long-session or concurrent-controller certification.

With exactly one ZCM1 Move attached by USB, the explicit pairing tool passed a read-only preflight, preserved its original host report locally, wrote the ASUS address using feature report 5 and verified it through report 4. A restore build is available. The user then confirmed that the Move appears connected wirelessly in Move Management; the live API identifies it as PS Move ZCM1 (054c:03d5), with the external bridge ready. Independent button/trigger/motion validation and native DualSense reconnect while this Move is active remain pending. No identities or backup contents belong in public packages.

The bridge later logged no controller replies after three idle probes; explicitly restarting its external session restored ready state. The cause is unresolved, including any interaction with separate USB helper lifecycle. Do not call long-idle stability solved. The prepared finite host check had only covered ten seconds. More diagnosis and lifecycle testing are needed.

After the Move connected through ASUS, the user repeated the DualSense off/reconnect/home-screen test and confirmed: DualSense reconnects and navigates, and Move stays connected. This establishes coexistence in that tested active-controller scenario. External shutdown/restart, Nav, full multiplayer, camera concurrency and long idle sessions remain separate acceptance items; native service restoration after old internal-radio access is still unproven.

The user subsequently confirmed Move and Nav both appear wirelessly connected. The dashboard showed both assigned to Player 1 (primary Move + Nav). Logs showed continuing input samples from both and raw Move motion; every physical button/axis and gameplay behavior still need explicit validation. A deliberate full stop then disconnected both links (reason 0x16), restored external page scan to 0, logged stopped and removed the instance lock. Native DualSense reconnection after that shutdown is awaiting user confirmation; do not label the complete stop/restart lifecycle passed yet.

Follow-up: after the full external-service stop, the user confirmed DualSense turns off, reconnects and navigates normally without rest mode or reboot. This shutdown coexistence check passed. The external bridge has restarted and reports ready; Move/Nav reattachment after that restart is the next check. This demonstrates leaving native Bluetooth usable in the tested external-only cycle, not resetting/recovering Sony's service after internal raw access.

Restart result: the Move reconnected, but the Nav did not reach ready state. Logs showed a recognized Nav connection request (exact_nav=1), successful HCI accept/connection, then HID control channel 0x11 setup timeouts and a setup-failed disconnect. A subsequent attempt was accepted but timed out before connection completion. The host address/allowlist are therefore not the first suspected cause; full Nav power-cycle and channel-state diagnostics are next. Do not mark Nav restart/reconnect support passed based on its initial successful connection.

Nav recovery: the user fully powered the unplugged Nav off (PS held until its light went out), then pressed PS. It reconnected alongside the Move without rewriting pairing. Live state showed primary Move + Nav on Player 1. Logs confirmed both Nav HID channels 0x11/0x13 open, profile ready, enable feature sent and successful HID control replies. This is a verified workaround, not an automatic reconnect fix. Do not send virtual-cable-unplug or rewrite host addresses just to hide stale connection state.

- Pair/restore source: tools/pair-external-move.c. It requires exactly one Move and no USB Nav, verifies identity/target/backup, and attempts rollback on failed verification. DRY_RUN=1 suppresses writes; RESTORE_ORIGINAL=1 restores the saved host. Requires existing calibrated ZCM1 registration for the full motion test.
- Nav migration source: tools/nav_usb_pair.c with NAV_EXTERNAL. One USB Nav passed NAV_DRY_RUN preflight; its original host was backed up per identity, its ASUS host was written/read back, and its seed identity saved. The Move remained connected through this operation. Wireless Nav connection/input is awaiting confirmation. NAV_EXTERNAL + NAV_RESTORE selects that Nav's original host backup; no generic system-radio reset is involved.
- Protocol reference: https://github.com/thp/psmoveapi/blob/master/src/psmove.c (report 5 host-write layout, report 4 host-read layout).

- ASUS support: https://www.asus.com/us/supportonly/usb-bt500/helpdesk_download/
- Linux kernel-team identification of 0b05:190e as Realtek 8761BU ASUS BT500: https://lists.ubuntu.com/archives/kernel-team/2022-June/131058.html
- Logitech receiver identities: https://pwr-solaar.github.io/Solaar/devices/

## Native Bluetooth recovery research

Existing local probes expose sceBluetoothHidInit/Term/Open/Close and callback APIs. A client Term/Init is not proven to restart Sony's system Bluetooth service or restore its radio ownership. Do not infer an ABI from a symbol name or kill system processes blindly. No reliable service restart has been established. The later file-only test did not recover after rest mode according to the user; full restart was requested and subsequently reported. Keep the earlier rest-mode successes and later failure distinct.
