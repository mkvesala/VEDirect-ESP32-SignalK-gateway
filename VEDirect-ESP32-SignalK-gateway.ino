#include <WiFi.h>
#include <ArduinoJson.h>
#include <ArduinoWebsockets.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoOTA.h>
#include "secrets.h"

using namespace websockets;

// Wifi ja OTA
const String SK_URL = String("ws://") + SK_HOST + ":" + String(SK_PORT) + "/signalk/v1/stream" + ((strlen(SK_TOKEN) > 0) ? "?token=" + String(SK_TOKEN) : "");
String RSSIc = "NA";
String SK_SOURCE = "esp32.vedirect";
String HOSTNAME = SK_SOURCE; //OTA host name
bool LCD_ONLY = false; // ilman wifi-yhteyttä vain LCD-näyttö käytössä = true
const uint32_t WIFI_TIMEOUT_MS = 90000; // Yritetään luoda wifi-yhteys 1.5 minuutin ajan

// VE.Direct UART
static const int RX_PIN = 16;     // RX2
static const int TX_PIN = -1;
static const uint32_t VE_BAUD = 19200;

// I2C LCD-näyttö
std::unique_ptr<LiquidCrystal_I2C> lcd;
bool lcd_present = false;
static char prev_top[17] = "";
static char prev_bot[17] = "";

// I2C-osoitteen skannaus
bool i2c_device_present(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

// Vapauta jumissa oleva I2C-väylä: kellota SCL:ää max 9 kertaa
void i2c_bus_recover(int scl_pin, int sda_pin) {
  pinMode(scl_pin, OUTPUT_OPEN_DRAIN);
  pinMode(sda_pin, INPUT_PULLUP);
  for (int i = 0; i < 9; i++) {
    if (digitalRead(sda_pin) == HIGH) break;
    digitalWrite(scl_pin, LOW);
    delayMicroseconds(5);
    digitalWrite(scl_pin, HIGH);
    delayMicroseconds(5);
  }
  // palauta I2C-väylä käyttöön
  Wire.begin(21,22);
  delay(10);
}

// LCD-näytön alustus
void lcd_init_safe() {
  Wire.begin(21, 22);
  delay(10);
  // Wire.setClock(100000);
  // Wire.setTimeOut(50);
  if (digitalRead(21) == LOW) {
    i2c_bus_recover(22, 21);
  }

  uint8_t addr = 0;
  if (i2c_device_present(0x27)) addr = 0x27;
  else if (i2c_device_present(0x3F)) addr = 0x3F;

  if (addr) {
    lcd = std::make_unique<LiquidCrystal_I2C>(addr, 16, 2);
    lcd->init();
    lcd->backlight();
    lcd_present = true;
  } else {
    lcd_present = false;
  }
}

// Alusta VE.Direct portin lukija
HardwareSerial VESerial(2);

// Luo websocket
WebsocketsClient ws;
volatile bool ws_open = false;
unsigned long next_ws_try_ms = 0;
const unsigned long WS_RETRY_MS = 2000;
const unsigned long WS_RETRY_MAX = 20000;

unsigned long last_tick_ms = 0;
const unsigned long TICK_MS = 1000;     // 1 s / arvo → 5 arvoa ~5 s kierros

// Välimuisti VE.Directistä luetuille arvoille + aikaleimat
struct VedCache {
  volatile int32_t mv  = INT32_MIN;
  volatile int32_t ma  = INT32_MIN;
  volatile int32_t w   = INT32_MIN;
  volatile int32_t soc = INT32_MIN;
  volatile int32_t vs  = INT32_MIN;
  volatile uint32_t ts_mv=0, ts_ma=0, ts_w=0, ts_soc=0, ts_vs=0;
  volatile float v_house, i_house, p_house, socf, v_start;
};

VedCache g_cache;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;

static inline void cache_set(volatile int32_t& slot, volatile uint32_t& ts, int32_t v) {
  portENTER_CRITICAL(&g_mux);
  slot = v; ts = millis();
  portEXIT_CRITICAL(&g_mux);
}

// Luettujen arvojen muunnokset
static inline float mv_to_v(int32_t mv) { return mv / 1000.0f; }
static inline float ma_to_a(int32_t ma) { return ma / 1000.0f; } 
static inline float soc01(int32_t raw_tenths_pct)  { // Victronin SoC tulee esim muodossa 955 = 95.5 %
  float x = raw_tenths_pct / 1000.0f;
  if (x < 0) x = 0;
  if (x > 1) x = 1;
  return x;
  }

inline bool validf(float x) { return !isnan(x) && isfinite(x); }

// Uniikki source SignalK palvelimelle ja OTA:an
void make_source_from_mac() {
  uint8_t m[6]; WiFi.macAddress(m);
  char tail[7]; snprintf(tail, sizeof(tail), "%02x%02x%02x", m[3], m[4], m[5]);
  SK_SOURCE = String("esp32.vedirect-") + tail;
  HOSTNAME = SK_SOURCE;
}

// Websockets
void setup_ws_callbacks() {
  ws.onEvent([](WebsocketsEvent e, String){
    if (e == WebsocketsEvent::ConnectionOpened)  { ws_open = true; }
    if (e == WebsocketsEvent::ConnectionClosed)  { ws_open = false; }
    if (e == WebsocketsEvent::GotPing)           { ws.pong(); }
  });
}

// SignalK deltan lähetys (rakennettu createNested*-metodeilla)
void send_single_delta(const char* path, float value) {
  if (LCD_ONLY) return;                      // ei lähetetä ilman WiFiä
  if (!ws_open || !validf(value)) return;    // websocket kiinni tai arvo ei kelpaa

  StaticJsonDocument<320> doc;
  doc["context"] = "vessels.self";

  auto updates = doc.createNestedArray("updates");
  auto up      = updates.createNestedObject();
  up["$source"] = SK_SOURCE;

  auto values  = up.createNestedArray("values");
  auto o       = values.createNestedObject();
  o["path"]    = path;
  o["value"]   = value;

  char buf[320];
  size_t n = serializeJson(doc, buf, sizeof(buf));
  ws.send(buf, n);
}

//LCD-tulostuksen apuri
static inline void copy16(char* dst, const char* src) {
  strncpy(dst, src, 16);   // kopioi max 16 merkkiä
  dst[16] = '\0';          // varmista loppunolla
}

// LCD-perustulostus kahdelle riville
void lcd_print_lines(const char* l1, const char* l2) {
  if (!lcd_present) return;

  // Katkaistut versiot näytölle tulostusta ja prev_* tallennusta varten
  char t[17], b[17];
  copy16(t, l1);
  copy16(b, l2);

  lcd->setCursor(0, 0);
  lcd->print(t);
  for (int i = (int)strlen(t); i < 16; i++) lcd->print(' ');

  lcd->setCursor(0, 1);
  lcd->print(b);
  for (int i = (int)strlen(b); i < 16; i++) lcd->print(' ');

  // Tallenna täsmälleen se mitä näytettiin (katkaistu + nollaterminoitu)
  copy16(prev_top, t);
  copy16(prev_bot, b);
}

// LCD: 16x2 näyttö kahdessa 8-merkkisessä lohkossa / rivissä
// Näyttää V, VS, A ja SoC arvojen perusteella
void lcd_show_compact(float v_house, float v_start, float i_house, float soc) {
  
  if (!lcd_present) return;

  char top[17];
  char bot[17];

  // Tasaus: %6.2f = 6 merkin leveys, 2 desimaalia; väli 1 merkki lohkojen välissä
  snprintf(top, sizeof(top), "%6.2fV %6.2fV", v_house, v_start);
  snprintf(bot, sizeof(bot), "%6.2fA %5.1f%%", i_house, soc * 100.0f);

  // Päivitä vain jos sisältö on muuttunut (ei vilkkumista)
  if (strcmp(top, prev_top) != 0) {
    lcd->setCursor(0, 0);
    lcd->print(top);
    for (int i = (int)strlen(top); i < 16; i++) lcd->print(' ');
    copy16(prev_top, top);
  }
  if (strcmp(bot, prev_bot) != 0) {
    lcd->setCursor(0, 1);
    lcd->print(bot);
    for (int i = (int)strlen(bot); i < 16; i++) lcd->print(' ');
    copy16(prev_bot, bot);
  }
}

// ========= Taustaluku (Core 0) =========
// Luetaan jatkuva VE.Direct-tekstivirta, parsitaan "LABEL\tVALUE" ja päivitetään välimuisti.
void ved_reader_task(void*) {
  const size_t L = 64;   // VE_DIRECT_LINE_SIZE
  char line[L];
  size_t idx = 0;

  for (;;) {
    if (VESerial.available()) {
      int b = VESerial.read();
      if (b < 0) { taskYIELD(); continue; }
      if (b == '\r') continue;

      if (b == '\n') {
        line[idx] = '\0'; idx = 0;

        char* save = nullptr;
        char* label = strtok_r(line, "\t", &save);
        if (!label || !*label) { vTaskDelay(1); continue; }
        char* val = strtok_r(nullptr, "\t", &save);
        if (!val || !*val)     { vTaskDelay(1); continue; }

        long v;
        if (val[0]=='O') v = (val[1]=='N') ? 1 : 0;
        else             v = strtol(val, nullptr, 10);

        if      (strcmp(label,"V")   == 0) cache_set(g_cache.mv,  g_cache.ts_mv,  (int32_t)v);
        else if (strcmp(label,"I")   == 0) cache_set(g_cache.ma,  g_cache.ts_ma,  (int32_t)v);
        else if (strcmp(label,"P")   == 0) cache_set(g_cache.w,   g_cache.ts_w,   (int32_t)v);
        else if (strcmp(label,"SOC") == 0) cache_set(g_cache.soc, g_cache.ts_soc, (int32_t)v);
        else if (strcmp(label,"VS")  == 0) cache_set(g_cache.vs,  g_cache.ts_vs,  (int32_t)v);
      } else {
        if (idx + 1 < L) {
          line[idx++] = (char)b;
        } else {
          // overflow: flushaa rivin loppu
          while (VESerial.available()) {
            int bb = VESerial.read();
            if (bb == '\n' || bb < 0) break;
          }
          idx = 0;
        }
      }
    } else {
      vTaskDelay(1); // anna muille vuoro
    }
  }
}

// ========= Tilakone: lähetä & näytä 1 arvo / 1 s =========
enum ReadState { RS_IP, RS_V, RS_I, RS_P, RS_SOC, RS_VS };
ReadState state = RS_IP;

static bool age_ok(uint32_t ts, uint32_t max_ms) {
  return ts && (millis() - ts <= max_ms);
}

const uint32_t STALE_MS = 30000; // jos arvo ei päivity 30 s, älä lähetä

// ===== S E T U P =====
void setup() {

  Serial.begin(115200);
  delay(100);

  // I2C & LCD
  lcd_init_safe();
  lcd_print_lines("STARTING...", "INIT WIFI...");
  delay(2000);

  // Wifi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lcd_print_lines("WIFI", "CONNECT...");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_TIMEOUT_MS) { delay(250); } // Yritetään wifi-yhteyttä 1.5 minuutin ajan
  if (WiFi.status() == WL_CONNECTED) {
    make_source_from_mac();
    lcd_print_lines("WIFI OK", WiFi.localIP().toString().c_str());
    delay(2000);

    int RSSIi = WiFi.RSSI();

    if (RSSIi > -55) {
      RSSIc = "EXCELLENT";
    }
    else if (RSSIi < -80) {
      RSSIc = "POOR";
    }
    else {
      RSSIc = "OK";
    }

    lcd_print_lines("SIGNAL LEVEL:", RSSIc.c_str());
    delay(2000);

    // OTA
    ArduinoOTA.setHostname(HOSTNAME.c_str());
    ArduinoOTA.setPassword(WIFI_PASS);
    ArduinoOTA.onStart([](){
      Serial.println("OTA upload starting...");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("OTA upload complete!");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total){
      Serial.printf("Progress: %u%%\r", (unsigned)(progress / (total /100)));
    });
    ArduinoOTA.onError([] (ota_error_t error) {
      Serial.printf("Error:[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Authentication failed.");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin failed.");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect failed.");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive failed.");
      else if (error == OTA_END_ERROR) Serial.println("End failed.");
      else Serial.println("Unknown error.");
    });
    ArduinoOTA.begin();
    Serial.println("OTA ready!");

    // Websocket
    setup_ws_callbacks();
  } else { // Jos ei wifi-yhteyttä, niin käytetään vain LCD-näyttöä
    LCD_ONLY = true;
    lcd_print_lines("LCD ONLY MODE", "NO WIFI");
    delay(2000);
  }

  // Käynnistä VE.Direct lukija
  VESerial.setRxBufferSize(4096);
  VESerial.begin(VE_BAUD, SERIAL_8N1, RX_PIN, TX_PIN);
  // käynnistä lukija Core 0:lle
  xTaskCreatePinnedToCore(ved_reader_task, "vedread", 4096, nullptr, 2, nullptr, 0);
  if (lcd_present) {
    lcd->clear();
  }
  delay(500);
}

// ===== L O O P =====
void loop() {

  if (!LCD_ONLY) { // suoritetaan vain, jos on wifi-yhteys

    // OTA
    ArduinoOTA.handle();
    
    // Haamu websocketin tappo
    if (WiFi.status() != WL_CONNECTED && ws_open) {
      ws.close();
      ws_open = false;
    }

    ws.poll();

    // Websocketin avaus ja exponentiaalinen yhteysyritys
    static unsigned long expn_retry_ms = WS_RETRY_MS;
    if (!ws_open && (long)(millis() - next_ws_try_ms) >= 0) {
      if (!ws.connect(SK_URL)) ws_open = false;
      next_ws_try_ms = millis() + expn_retry_ms;
      expn_retry_ms = min(expn_retry_ms * 2, WS_RETRY_MAX); // väli kasvaa eksponentiaalisesti kunnes MAX 
    }
    if (ws_open) expn_retry_ms = WS_RETRY_MS; // palautus vakioarvoon
  }

  if (millis() - last_tick_ms < TICK_MS) return;
  last_tick_ms = millis();

  // Laskuri, vain joka kymmenes kerta näytetään wifi ja websocket tila RS_IP
  static uint8_t ip_every = 10;
  static uint8_t ip_ctr = 0;

  if (state == RS_IP) {
    if (++ip_ctr >= ip_every) {
      ip_ctr = 0;
      state = RS_IP;
    } else {
      state = RS_V;
    }
  }

  // Tilakoneen lukuvaiheet
  switch (state) {

    case RS_IP: {
      if (LCD_ONLY) {
        lcd_print_lines("LCD ONLY MODE", "NO WIFI");
      } else {
        const char* wstext = ws_open ? "OPEN" : "CLOSED";
        char label1[17];
        char label2[17];
        snprintf (label1, sizeof(label1), "WIFI %s", RSSIc.c_str());
        snprintf (label2, sizeof(label2), "WS %s", wstext);
        lcd_print_lines(label1, label2);
      }
      state = RS_V;
    } break;

    case RS_V: {
      int32_t mv = age_ok(g_cache.ts_mv, STALE_MS) ? g_cache.mv : INT32_MIN;
      float v = (mv != INT32_MIN) ? mv_to_v(mv) : NAN;
      send_single_delta("electrical.batteries.house.voltage", v);
      g_cache.v_house = v;
      state = RS_I;
    } break;

    case RS_I: {
      int32_t ma = age_ok(g_cache.ts_ma, STALE_MS) ? g_cache.ma : INT32_MIN;
      float a = (ma != INT32_MIN) ? ma_to_a(ma) : NAN;
      send_single_delta("electrical.batteries.house.current", a);
      g_cache.i_house = a;
      state = RS_P;
    } break;

    case RS_P: {
      int32_t w = age_ok(g_cache.ts_w, STALE_MS) ? g_cache.w : INT32_MIN;
      float p = (w != INT32_MIN) ? (float)w : NAN;
      send_single_delta("electrical.batteries.house.power", p);
      g_cache.p_house = p;
      state = RS_SOC;
    } break;

    case RS_SOC: {
      int32_t socp = age_ok(g_cache.ts_soc, STALE_MS) ? g_cache.soc : INT32_MIN;
      float s = (socp != INT32_MIN) ? soc01(socp) : NAN;
      send_single_delta("electrical.batteries.house.capacity.stateOfCharge", s);
      g_cache.socf = s;
      state = RS_VS;
    } break;

    case RS_VS: {
      int32_t vs = age_ok(g_cache.ts_vs, STALE_MS) ? g_cache.vs : INT32_MIN;
      float vstart = (vs != INT32_MIN) ? mv_to_v(vs) : NAN;
      send_single_delta("electrical.batteries.start.voltage", vstart);
      g_cache.v_start = vstart;
      lcd_show_compact(g_cache.v_house, g_cache.v_start, g_cache.i_house, g_cache.socf);
      state = RS_IP;
    } break;
  }
}
