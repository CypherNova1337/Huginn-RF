# Legal & responsible-use notes

Huginn-RF is a tool for **authorized** wireless security research, education,
and penetration testing. Read this before you transmit.

## The rules this firmware is built around

1. **Only touch what's yours or explicitly authorized.** Test on your own
   devices, your own lab, or a target for which you hold **written** permission
   (a signed engagement scope / rules of engagement). "It was reachable" is not
   authorization.
2. **Radio is regulated.** Transmitting on 915 MHz / 2.4 GHz and jamming or
   deauthenticating devices may require a license and is illegal in many
   jurisdictions when done to equipment you don't control. Know your local rules
   (FCC Part 15 / 97, ETSI, your national regulator).
3. **Interception is regulated too.** Capturing handshakes, beacons, or
   telemetry can fall under wiretap / communications-privacy law even when
   passive. Wardriving location logging is generally lawful; decrypting or
   replaying other people's traffic generally is not.
4. **No denial of service against infrastructure you don't own.** This is why
   the LoRaWAN join fuzzer is locked to a network server you declare as your
   own test instance, and why it is rate-limited — it's a backend-robustness
   test harness, not a flooding tool.

## What the firmware does to keep you honest

- Offensive modules require a one-time on-device **"authorized use"
  acknowledgement** (`config.yaml: core.authorized_ack: true` or an on-screen
  prompt) before they'll transmit.
- The join fuzzer refuses to run unless `lorawan_fuzzer.target_is_mine: true`
  and a non-default server address are set, and it caps request rate.
- All transmit actions are written to the on-SD log with timestamp + GPS, so
  an engagement has an audit trail.

These are guardrails, not a legal shield. If you don't have authorization,
don't transmit.
