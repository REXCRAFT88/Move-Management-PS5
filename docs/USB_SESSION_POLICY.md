# Fresh USB registration policy — beta6.8

Requested by the user on 2026-10-25: automatic wireless recovery is no longer promised after dongle disconnection. Users reconnect controllers by USB first.

On external transport loss, the USB worker stops, connections close and active player roles are released. The external saved-pairing database and cached adapter address are cleared. On every new external session, the saved database is cleared before host initialization; failure to clear it prevents startup. Historical calibration, player identities, button layouts and original-host backups are preserved.

The external build no longer allowlists controllers from old calibration files, Nav identity files or saved player records. Only successful current-session USB setup permits their wireless connection. The worker resets its notification queue and device-seen state on restart, so a fresh USB scan can register controllers still plugged in. After the ready notification, unplug and press PS. This policy also applies after app restart or manual Bluetooth restart, not only physical removal.

No controller factory reset or Sony-native pairing deletion is performed. A controller can still try its stored Bluetooth host before USB setup; the application does not accept it from historical registrations. The internal-radio build retains its prior behavior. Hardware unplug/reinsert and fresh-USB acceptance need validation on this version.
