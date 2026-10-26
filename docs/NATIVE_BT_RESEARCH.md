# Native Bluetooth coexistence/recovery research

Follow-up: user recovered native DualSense by rest mode/wake. A native-paired
Move navigated the home screen and remained responsive through a sixty-second
Bluetooth HID callback subscription. Init/register/unregister succeeded, but
zero callbacks arrived despite buttons, T and tilt. This is not a usable Move
report path yet and does not justify reading the shared HCI endpoint again.

The external adapter is a separate transport, not evidence that internal access is safe. Raw O_RDWR open of the internal ugen node reproduced unsupported DualSense reconnect; later rest-mode recovery was unreliable. No proven Sony service restart has been found.

Next prepared comparison: tools/native-usbd-open-only.c opens the internal radio through Sony's USB service for 45 seconds, with no raw kernel file, claims, reads, HCI commands or pairing changes. This must be tested only after confirming a working native reconnect baseline, with the main bridge stopped and an observer. The helper closes its own handle/runtime. It is an experiment, not a recovery tool, and is not yet hardware-tested.

If open-only succeeds, separately test claims and traffic in finite stages; service-handle success does not prove safe event delivery. If it fails, stop pursuing raw ownership adjustments without an independent recovery mechanism. Existing libSceBluetoothHid client Init/Term exports do not establish a system service restart ABI. Do not call guessed Term/Reset functions or kill system processes to manufacture recovery.

Pairing/layout/backup files remain untouched. Any test result must distinguish native reconnect during the window, immediately after close, and after rest/reboot. Current USB-first external behavior should remain available as the working fallback.

Hardware result, 2026-10-25: with user-confirmed working DualSense reconnect and external bridge stopped, the Sony USB-service open-only probe opened internal 1286:2059 successfully. The user turned DualSense off/reconnected/navigated normally during its 45-second window. Log confirmed handle close, USB client exit and completion. This contrasts with the earlier failed raw O_RDWR open, but does not prove claims, reads or HCI traffic can coexist. A separate CLAIM_ONLY build then claimed interface 0 successfully and started a finite no-reads/no-commands window; user result pending.

Claim-only result: user confirmed DualSense reconnect/navigation worked during the window. Log confirmed interface release returned 0 and the service handle/client closed. The next finite probe (native-usbd-event-read.c) discovers interface 0's interrupt endpoint from descriptors and reads events only for 45 seconds. Claim succeeded at endpoint 0x82; no HCI commands or ACL traffic are sent. Event-read result remains pending. A silent endpoint would not prove full active traffic can coexist.

Event-read result: user reported unsupported-device/cannot reconnect. Log showed one 6-byte event, first code 0x0f (HCI Command Status), then release=0 and completion. No probe HCI command was sent, so this read observed another owner's command traffic. This supports event-delivery interference as a concern, but does not prove the complete kernel mechanism or sole cause. Closing the USB client has been verified; native reconnect recovery after close is awaiting user confirmation. Do not repeat reads or reset the internal radio while recovery remains unknown.

The probe's nominal 45-second loop made only one synchronous read before finishing. Its per-call timeout must not be assumed to enforce a hard wall-clock bound on this native API; future read probes need validated asynchronous cancellation or another independently verified timeout mechanism. Prefer native input-service access that leaves Sony as HCI owner, or an explicitly designed event broker; the direct-reader path is not a coexistence solution.

Post-close result: user confirmed DualSense still reports unsupported/cannot reconnect without rest mode. Stop all further direct internal reads until recovery. No automatic native restoration is established. Offline inspection of the previously collected sceBluetoothHidInit code shows client state checks, synchronization and a descriptor-open path; unresolved calls prevent a complete ABI/behavior claim, and this is not evidence of a system-radio restart command. A blind client Term/Init cycle is not justified as a recovery method.
