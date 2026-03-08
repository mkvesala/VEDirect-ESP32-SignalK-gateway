# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [v1.0.0] - 2026-03-08

First versioned release. Complete rewrite from a single-file sketch into a
class-based architecture following the ESP32 Gateway Design Pattern.

### Added
- `VEDSensor` — encapsulates UART2 / HardwareSerial(2) reading into a dedicated
  class with a FreeRTOS task pinned to Core 0 and thread-safe `portMUX_TYPE`
  cache. Provides `getSnapshot()` for atomic reads from the main loop
- `VEDProcessor` — converts raw VE.Direct integer values to SI units
  (V, A, W, SoC as 0–1 ratio) and populates `ESPNow::BatteryDelta` for brokers
- `SignalKBroker` — WebSocket connection to SignalK server; all five values
  (house voltage, current, power, SoC, start voltage) are sent in a single
  JSON delta every 1 s. MAC-based unique `$source` name via `esp_efuse_mac_get_default()`
- `ESPNowBroker` — new ESP-NOW broadcast of `ESPNowPacket<BatteryDelta>` every 1 s
  using the shared `espnow_protocol.h` packet format
- `DisplayManager` — LCD 16x2 I2C management; scans for address 0x27 or 0x3F
  using two stack-allocated `LiquidCrystal_I2C` members and a raw pointer selector
- `VEDApplication` — orchestrator that owns all subsystems as stack-allocated
  members, wires dependencies via constructor initializer list, runs a
  `WifiState` state machine and timed loop handlers
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
- All `String` usage replaced with `snprintf` + `char[]` throughout
- All magic numbers promoted to `static constexpr` with `UPPER_SNAKE_CASE` names
- Private class members named `_snake_case`, static members `s_snake_case`

### Removed
- `std::unique_ptr` / `std::make_unique` removed from LCD initialisation
- I2C bus recovery (SCL clock-stretching loop) removed — was a debug workaround
- Global variable spaghetti replaced by encapsulated class state
- `String`-based SK URL, hostname and RSSI label construction removed
- Single-file `.ino` monolith replaced by 11 separate `.h`/`.cpp` modules
