# Adapter selection development

2026-10-25: source now tries eight external HCI USB identities, preferring the tested ASUS. Realtek/TP-Link and CSR candidates are untested on this PS5 and may need firmware/quirks. No firmware uploader is included. Generic opt-in accepts an explicit hexadecimal VID:PID in /data/move-studio/external-adapter-id; Sony/Marvell/Intel IDs are rejected. The actual opened device must match the requested identity and expose complete Bluetooth HCI endpoints before claim. No internal-node fallback is allowed. This is source-level transport coverage, not eight tested adapter models. Current deployed beta6.5 remains the earlier ASUS build.

ID research reference: https://github.com/torvalds/linux/blob/v6.12/drivers/bluetooth/btusb.c . Table/transport tests pass; broader hardware and firmware initialization are outstanding.

Remaining work includes in-process external USB registration, hotplug/retry selection, per-adapter identity storage, UI selection/status and lifecycle acceptance. Do not switch radios and silently reuse another adapter's pairing keys or host address. Existing pairings remain bound to their adapter.

Native recovery remains separate: no proven system Bluetooth restart has been established. Existing HID client Init/Term exports do not prove a system-service restart ABI. Preserve the successful external close/reconnect path while investigating the older internal-radio regression.
