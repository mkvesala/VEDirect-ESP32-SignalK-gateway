# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.2.3] - 2026-11-07

Shared-protocol release. This gateway's own firmware logic is untouched — no class, timing
constant or SignalK path changed, and the `BatteryDelta` payload this device broadcasts is
unchanged.

### Added
- `ESPNowMsgType::HALMET_OIL_DELTA = 10` and the `HALMETOilDelta` payload struct in
  `espnow_protocol.h` (engine oil pressure, `propulsion.0.oilPressure` [Pa], sent by
  HALMET-ESP32-SignalK-gateway), synced byte-identically from the fleet-wide copy. Additive
  only: no existing enum value or payload struct changed, so every existing receiver stays
  compatible. This gateway neither sends nor reads the new type

[v1.2.3]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.2.2...v1.2.3

## [v1.2.2] - 2026-10-12

Documentation-only release. No firmware logic, wire format or SignalK path changed.

### Added
- README: Disclaimer section below the badges, matching the other gateway projects

### Removed
- README: paragraph describing `espnow_protocol.h` as a fleet-wide hand-copied file

[v1.2.2]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.2.1...v1.2.2

## [v1.2.1] - 2026-07-29

Shared-protocol release. `espnow_protocol.h` is hand-copied into every ESP32 project on
the boat; the copies had drifted into four versions and were merged into a single
fleet-wide superset, which every project now holds byte-identically. This gateway's own
firmware logic is untouched — no class, timing constant or SignalK path changed, and the
`BatteryDelta` payload this device broadcasts is unchanged, so every existing receiver
stays compatible.

### Added
- `ESPNowMsgType::HALMET_WATER_DELTA = 7` and `ESPNowMsgType::DEPTH_DELTA = 8` in
  `espnow_protocol.h` — the fleet-wide `msg_type` number space now covers all eight
  senders on the boat
- `ESPNow::HALMETWaterDelta` payload struct — fresh water tank level
  (`tanks.freshWater.0.currentLevel`, 0.0–1.0), sent by `HALMET-ESP32-SignalK-gateway`
- `ESPNow::DepthDelta` payload struct (16 bytes) — depth below surface / transducer /
  keel in metres plus an `age_ms` freshness field, sent by `SignalK-ESP-NOW-gateway`.
  The values are relayed from the SignalK server rather than measured locally, so they
  carry no freshness of their own: an individual path is `NAN` when stale or never
  received, and `age_ms` (`UINT32_MAX` = never) describes the depth feed as a whole
- `ESPNow::GnssData::cog_valid`, `lat_deg` and `lon_deg` members plus a `hasCog()`
  accessor — receiver-side course validity is now tracked separately from position
  validity, and the internal struct carries the position needed by a receiver's
  position display
- File-level documentation block in `espnow_protocol.h` — states that no single project
  owns the file, that a change is done only once every copy is identical, and lists the
  fleet-wide `msg_type` allocation together with the senders and receivers, so a new
  value is never allocated by looking at only two projects
- Commented-out `GENERIC_SK_DELTA = 20` reservation documenting a rejected
  runtime-configurable path-relaying message (`char path[48]` per packet) — recorded so
  the number is not reused and the trade-off is not re-litigated

### Fixed
- Position and SOG suppressed at anchor on ESP-NOW receivers —
  `ESPNow::convertGnssDeltaToData()` folded a NaN COG into `fix_ok`, so a stationary
  boat, which has a valid position fix but an undefined course, appeared to have no fix
  at all and receivers hid position and SOG entirely. `fix_ok` now reflects position fix
  only; COG validity lives in the separate `cog_valid` flag, and SOG and position are
  gated on the fix rather than on the course. Receiver-side conversion only — the data
  this gateway transmits is unaffected

### Changed
- `ESPNow::GnssData` field comments clarify that `cog_true_x10` is meaningful only when
  `cog_valid` is set and that `sog_knots_x10` is zero without a fix; the "CROWPANEL
  INTERNAL DATA" section is documented as receiver-side only — sending projects such as
  this one compile the structs and inline converters but emit no code for them

[v1.2.1]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.2.0...v1.2.1

## [v1.2.0] - 2026-07-17

`SignalKBroker` now owns its `WebsocketsClient` through a `std::unique_ptr` and builds a
brand-new client on every reconnect. This fixes a rare permanent WebSocket reconnect
failure where the gateway lost its SignalK connection after a long run (~12–48 h) and
never recovered without a reboot — even though Wi-Fi stayed up, heap/stack were healthy,
the main loop kept running, and stale detection plus exponential backoff fired correctly.

### Fixed
- Permanent WebSocket reconnect failure after long uptime — the root cause was
  `SignalKBroker` keeping a single lifetime `WebsocketsClient` (`_ws` by value) and
  `closeWebsocket()` only calling `_ws.close()`. The same object — and thus the same
  underlying `WiFiClient` / lwIP socket — was reused on every reconnect; once that socket
  stuck in an unrecoverable state (or leaked its fd), every subsequent `connect()`
  inherited the corrupted transport and failed permanently. `connectWebsocket()` now
  builds a fresh client with `std::make_unique<WebsocketsClient>()` and destroys it on a
  failed connect; `closeWebsocket()` destroys it with `_ws.reset()`. Destroying the
  client runs the `WiFiClient` destructor, freeing the lwIP socket fd, so every reconnect
  starts from a clean TCP/WebSocket state

### Changed
- `SignalKBroker` owns the `WebsocketsClient` via `std::unique_ptr<websockets::WebsocketsClient>`
  (`#include <memory>`) instead of a by-value member — the client is created per
  `connectWebsocket()` and destroyed during teardown rather than persisted for the
  application lifetime. Reconnect, exponential backoff, ping/pong liveness and SignalK
  delta logic are unchanged — only the client lifecycle changed
- `connectWebsocket()` now registers the `onMessage` / `onEvent` callbacks on the freshly
  created client **before** calling `connect()` (previously registered only after a
  successful connect) — a synchronously-fired `ConnectionOpened` event can no longer be
  missed; the lambdas capture `this` (the broker), so recreating the client does not
  invalidate them
- Every `_ws` dereference is now null-guarded (`if (_ws_open && _ws) _ws->poll()`, `ping()`,
  `pong()`, `send()`); `isOpen()` still reads the `_ws_open` bool, so the public API and
  callers (`DisplayManager`, `WebUIManager`) are unchanged
- `sendDelta()` send-failure teardown now routes through `closeWebsocket()` instead of an
  inline `_ws.close()` — all client destruction funnels through one place, so `_last_pong_ms`
  is consistently reset and the backoff loop reconnects with a fresh client

[v1.2.0]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.1.0...v1.2.0

## [v1.1.0] - 2026-07-04

WiFi AP interface hardened with three-layer security and intrusion detection.
`espnow_protocol.h` extended with GNSS payload structs for the shared fleet protocol.
Active WebSocket ping/pong liveness detects a silently-dead SignalK server and heals
it with a graceful, transport-only reconnect (no reboot).

### Added
- `SignalKBroker::ping()` — sends a client-initiated WebSocket ping frame; the `GotPong`
  event refreshes `_last_pong_ms`. `handleWebsocket()` pings every `WS_PING_MS` (~10 s)
  while the socket is open
- `SignalKBroker::isStale()` — reports a half-open connection where `isOpen()` still
  returns `true` but no pong has arrived within `PONG_TIMEOUT_MS` (~30 s); `handleWebsocket()`
  then calls `closeWebsocket()` and lets the existing exponential backoff reconnect,
  recovering from a silently-dead SignalK server (e.g. macOS host power-saving) without
  dropping WiFi or rebooting
- `WS_PING_MS` (`VEDApplication`, 9 973 ms) and `PONG_TIMEOUT_MS` (`SignalKBroker`,
  29 989 ms) liveness timing constants, plus `_last_ping_ms` / `_last_pong_ms` timestamp
  members — the ping cadence stays below the pong timeout so ~2–3 lost pongs are required
  before the link is judged dead
- `WiFi.softAP()` called immediately after `WiFi.mode(WIFI_AP_STA)` — secures the AP
  interface before any client can connect (hidden SSID, WPA2 password, max 1 connection)
- `WiFi.onEvent(ARDUINO_EVENT_WIFI_AP_STACONNECTED)` registered in `begin()` before
  `WiFi.begin()` — callback calls `esp_wifi_deauth_sta()` immediately in the FreeRTOS
  `arduino_events` task and sets `volatile bool _ap_intruder` flag
- `handleAPIntruder()` in `VEDApplication` — processes the intruder flag in `loop()`
  context; logs MAC address to Serial and shows `AP: INTRUDER!` on LCD
- `volatile bool _ap_intruder` and `uint8_t _ap_intruder_mac[6]` private members in
  `VEDApplication` for thread-safe event-to-loop handoff (MAC copied before flag is set)
- `#include <esp_wifi.h>` in `VEDApplication.h` for `esp_wifi_deauth_sta()`
- `AP_SSID` and `AP_PASS` added to `secrets.example.h`
- `ESPNow::GnssDelta` payload struct in `espnow_protocol.h` — GNSS position, speed,
  course (lat/lon °, SOG m/s, COG true rad, magnetic variation rad, SIV, fix type, fix OK)
- `ESPNow::GnssData` internal struct in `espnow_protocol.h` — integer×10 representation
  (COG 0–3599, SOG knots×10, fix_ok) with `getCogDeg()`, `getSogKnots()`, `hasFix()` helpers
- `ESPNow::convertGnssDeltaToData()` inline converter from wire float format to internal
  integer format
- `ESPNowMsgType::GNSS_DELTA = 4` added to the shared protocol enum
- `applyStaticIP()` in `VEDApplication` — configures a fixed IP via `WiFi.config()`
  from `WIFI_STATIC_IP` / `WIFI_GATEWAY` / `WIFI_SUBNET` (parsed with
  `IPAddress::fromString()`); applied before every `WiFi.begin()` — both the initial
  connect in `begin()` and the WiFi-drop "hard reset" in `handleWifi()` — so the
  device address no longer depends on the router's DHCP lease table
- `WIFI_STATIC_IP`, `WIFI_GATEWAY`, `WIFI_SUBNET` added to `secrets.example.h`

### Fixed
- `initWifiServices()` now guarded by `_wifi_services_initialized` flag — prevents
  `ArduinoOTA.begin()` and `WebServer` route registration from running more than once
  per power cycle. Previously each WiFi reconnect re-registered mDNS and leaked a UDP
  socket and duplicate route handlers, causing the heap to shrink steadily until crash
- WiFi-drop recovery in `handleWifi()` now closes the SignalK websocket
  (`_signalk.closeWebsocket()`) before reconnecting, and performs a harder reset
  (`WiFi.disconnect(true)` + brief `delay()` + `WiFi.setSleep(false)`) — addresses
  network freezes (e.g. caused by macOS power-saving on the SignalK server side)
  that previously left the connection stuck until a manual reboot
- `_next_ws_try_ms` now resets to the current time on a successful WiFi reconnect —
  previously a stale exponential-backoff timestamp from before the WiFi drop could
  delay SignalK websocket reconnection by up to ~2 minutes after WiFi recovered

### Changed
- Liveness recovery is transport-only — a silently-dead connection is now healed by a
  graceful WebSocket reconnect instead of `ESP.restart()`; this supersedes the reverted
  `isOpen()`-keyed reboot watchdog (`a2f6566`), which could not detect a half-open TCP
  connection and would reboot a healthy device during transient server outages
- `WIFI_TIMEOUT_MS` increased from 90 001 ms (90 s) to 179 999 ms (3 min) — allows more
  time for the STA interface to associate before falling back to ESP-NOW-only mode
- `TX_INTERVAL_MS` reduced from 997 ms to 499 ms — sends SignalK delta updates to the
  server twice as often

[v1.1.0]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/compare/v1.0.0...v1.1.0

## [v1.0.0] - 2026-04-10

First versioned release. Complete rewrite from a single-file sketch into a
class-based architecture following the ESP32 Gateway Design Pattern.

### Added
- `VEDSensor` — encapsulates UART2 / HardwareSerial(2) reading into a dedicated
  class with a FreeRTOS task pinned to Core 0 and thread-safe `portMUX_TYPE`
  spinlock cache. Provides `getSnapshot()` for atomic reads from the main loop
- `VEDSensor::getReaderStackWatermark()` — returns the FreeRTOS reader task stack
  high water mark in bytes (ESP-IDF FreeRTOS returns bytes, not words)
- `VEDSensor`: task handle saved from `xTaskCreatePinnedToCore()` to support
  stack watermark queries
- `VEDProcessor` — converts raw VE.Direct integer values to SI units
  (V, A, W, SoC as 0–1 ratio) and populates `ESPNow::BatteryDelta` for brokers
- `SignalKBroker` — WebSocket connection to SignalK server; all five values
  (house voltage, current, power, SoC, start voltage) are sent in a single
  JSON delta every 1 s. MAC-based unique `$source` name via `esp_efuse_mac_get_default()`
- `ESPNowBroker` — ESP-NOW broadcast of `ESPNowPacket<BatteryDelta>` every 1 s
  using the shared `espnow_protocol.h` packet format
- `DisplayManager` — LCD 16x2 I2C management; scans for address 0x27 or 0x3F
  using two stack-allocated `LiquidCrystal_I2C` members and a raw pointer selector
- `DisplayManager::showDiagData()` — LCD screen showing free heap (bytes) and
  task stack high water marks for both the main loop task and the reader task
- `VEDApplication` — orchestrator that owns all subsystems as stack-allocated
  members, wires dependencies via constructor initializer list, runs a
  `WifiState` state machine and timed loop handlers with a three-way LCD display
  rotation: WiFi/WS net status, diagnostics, and battery data on a 30-tick cycle
- `VEDPreferences` — skeleton class for future NVS configuration storage
- `WebUIManager` — skeleton class for future HTTP configuration UI
- `WifiState.h` — `enum class WifiState` (INIT / CONNECTING / CONNECTED /
  FAILED / DISCONNECTED / OFF) with `wifiStateToString()` helper
- `version.h` — `SW_VERSION` compile-time constant
- `secrets.h` / `secrets.example.h` — converted from `#define` macros to
  `inline constexpr` form

### Changed
- WiFi mode changed from `WIFI_STA` to `WIFI_AP_STA` (required for concurrent
  ESP-NOW and WiFi operation)
- SignalK send strategy changed from a 5-state round-robin (one value per
  second, 5 s full cycle) to a single delta with all five values every 1 s
- WebSocket reconnection uses exponential backoff (2 s → 120 s max) instead
  of a fixed 2 s retry interval
- WiFi connection timeout is now non-blocking; the main loop keeps running
  with ESP-NOW active while WiFi is still connecting
- `VEDSensor`: `portMUX_TYPE _mux` renamed to `_spinlock` — the variable is a
  spinlock (busy-wait, disables interrupts), not a mutex (blocking, scheduler-aware)
- `DisplayManager`: `SignalKBroker` dependency removed; constructor signature
  changed from `DisplayManager(VEDProcessor&, SignalKBroker&)` to
  `DisplayManager(VEDProcessor&)` — net status display is handled entirely by
  `VEDApplication`
- All `String` usage replaced with `snprintf` + `char[]` throughout
- All magic numbers promoted to `static constexpr` with `UPPER_SNAKE_CASE` names
- Private class members named `_snake_case`, static file-scope variables `s_snake_case`
- `ESP32_gateway_pattern.md`: coding style rules documented; all code-block
  comments translated to English

### Fixed
- Dead `SignalKBroker &_signalk` member in `DisplayManager` removed — it was
  stored in the constructor initializer list but never accessed by any method

### Removed
- `std::unique_ptr` / `std::make_unique` removed from LCD initialisation
- I2C bus recovery (SCL clock-stretching loop) removed — was a debug workaround
- Global variable spaghetti replaced by encapsulated class state
- `String`-based SK URL, hostname and RSSI label construction removed
- Single-file `.ino` monolith replaced by 11 separate `.h`/`.cpp` modules

[v1.0.0]: https://github.com/mkvesala/VEDirect-ESP32-SignalK-gateway/releases/tag/v1.0.0
