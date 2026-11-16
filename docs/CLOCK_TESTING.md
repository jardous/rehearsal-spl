# BLE CTS clock acceptance

The clock/calendar and display can be tested on the host, but the BLE radio,
iOS discovery, pairing, reconnection and simultaneous audio acquisition need
the assembled MatrixPortal S3 and an actual iPhone. Compilation is not proof
of successful iPhone synchronization.

1. Flash `mock` first. Select `VIEW CLOCK`, without connecting a phone: verify
   dashes and `PAIR PHONE`, no bogus 1970 date. Check UP/DOWN taps cycle all six
   screens in both directions. Check UP-then-DOWN brightens and DOWN-then-UP dims,
   with repeat while held and no screen/mode/MAX action when either is released.
2. Initially connect **Rehearsal SPL** with nRF Connect / LightBlue and accept
   pairing. After the first bond it may appear in normal iPhone Bluetooth
   settings for subsequent manual connection. Check `STATUS` moves through pairing
   / discovery to `ble=READY clock_valid=1`, with the same date/time as the phone.
   Allow several seconds for iOS to expose services. The displayed colon should
   blink each second; no digits/date should clip or wrap on the 64×32 panel.
3. Lock the iPhone, background/close the connecting app, and confirm time still
   updates. Watch `sync_age_s` reset at least every minute while connected.
   Automatic reconnection and background behavior are iOS-dependent.
4. Turn the phone's Bluetooth off / take it out of range. The clock must keep
   ticking with an amber dot, including midnight. Reconnect and verify a fresh
   sync and green dot. Rapidly repeat disconnect/reconnect while reading CTS
   and while pairing; there must be no reset, frozen UI or steadily shrinking heap.
5. Temporarily change the phone's local time/timezone (disable automatic time
   only as necessary), including a backwards change and midnight/month rollover.
   Verify notifications or the next 60-second read update the matrix correctly,
   with no duplicate UTC/DST offset. Restore the phone's original settings.
   `CLOCK SYNC` should request a read without interrupting graphics/buttons.
6. Reboot the matrix: the view returns to SPL, time is initially unset, and the
   saved bond permits resync without pairing again after connecting from iPhone
   settings. Separately test automatic reconnect with the scanner app fully
   closed and no manual phone connection: it failed within a 60-second window
   on the tested iPhone. Do not confuse a saved bond with verified auto-reconnect.
   If stale keys prevent pairing, use `CLOCK FORGET` and iPhone **Forget This
   Device**, then re-pair. Verify SPL settings/calibration are unchanged.
7. Try a BLE central without CTS: `NO CTS`, or retained last-good time with an
   amber dot, never invented time. Reject pairing and retry with the iPhone.
   A GATT simulator can test malformed/unknown dates, read failures and Service
   Changed indications. Verify the latter reconnects and discovers fresh handles.
8. Flash `meter` and repeat with real microphone input. Remove/fault the mic:
   the clock must remain usable, but SPL/audio views and serial must still report
   the fault. Correct CTS updates must not clear SPL maximum/history.
9. Soak at least 30 minutes with real audio, panel refresh, serial on/off, BLE
   advertising, repeated pairing/bond writes, reconnects and each display view.
   Check no increasing audio errors/overruns/deadline misses, no heap/stack
   exhaustion, stable panel refresh and no acquisition regressions. Re-pairing
   writes flash; do not dismiss any detected continuity loss as harmless.

Keep the last known-working UF2 for recovery. These are acceptance instructions,
not a claim that the checks have already been performed.
