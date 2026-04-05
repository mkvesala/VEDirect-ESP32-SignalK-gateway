#include "DisplayManager.h"
#include "SignalKBroker.h"  // tarvitaan koko definaatio (isOpen())

DisplayManager::DisplayManager(VEDProcessor& processorRef, SignalKBroker& signalkRef)
    : _processor(processorRef)
    , _signalk(signalkRef)
{}

// ── I2C-skannaus ──────────────────────────────────────────────
bool DisplayManager::i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}

// ── Alustus ───────────────────────────────────────────────────
void DisplayManager::begin() {
    if (i2cPresent(ADDR_PRI)) {
        _lcd = &_lcd_27;
    } else if (i2cPresent(ADDR_ALT)) {
        _lcd = &_lcd_3f;
    }

    if (_lcd) {
        _lcd->init();
        _lcd->backlight();
        _lcd_present = true;
    }
}

// ── Apuri: kopioi max 16 merkkiä, varmista nollaterminus ─────
void DisplayManager::copy16(char* dst, const char* src) {
    strncpy(dst, src, 16);
    dst[16] = '\0';
}

// ── Perustulostus kahdelle riville (päivittää prev-puskurit) ──
void DisplayManager::printLines(const char* l1, const char* l2) {
    if (!_lcd_present || !_lcd) return;

    char t[17], b[17];
    copy16(t, l1);
    copy16(b, l2);

    _lcd->setCursor(0, 0);
    _lcd->print(t);
    for (int i = static_cast<int>(strlen(t)); i < LCD_COLS; i++) _lcd->print(' ');

    _lcd->setCursor(0, 1);
    _lcd->print(b);
    for (int i = static_cast<int>(strlen(b)); i < LCD_COLS; i++) _lcd->print(' ');

    copy16(_prev_top, t);
    copy16(_prev_bot, b);
}

// ── Statusviesti (molemmat rivit, ei prev-vertailua) ──────────
void DisplayManager::showMessage(const char* l1, const char* l2) {
    printLines(l1, l2);
}

// ── Diagnostiikka: heap + task stack watermarkit ──────────────
void DisplayManager::showDiagData(uint32_t freeHeap, uint32_t mainStackWm, uint32_t readerStackWm) {
    if (!_lcd_present || !_lcd) return;

    char top[17];
    char bot[17];

    // freeHeap enintään 7 merkkiä (ESP32: max ~300000 B)
    snprintf(top, sizeof(top), "HEAP %luB", (unsigned long)freeHeap);

    // watermarkit tavuina (ESP-IDF FreeRTOS palauttaa tavuja, ei sanoja)
    snprintf(bot, sizeof(bot), "WM M:%lu R:%lu",
             (unsigned long)mainStackWm, (unsigned long)readerStackWm);

    printLines(top, bot);
}

// ── Batteriadata — päivitä vain jos sisältö muuttunut ─────────
void DisplayManager::showBatteryData() {
    if (!_lcd_present || !_lcd) return;

    char top[17];
    char bot[17];

    snprintf(top, sizeof(top), "%6.2fV %6.2fV",
             _processor.getHouseVoltage(),
             _processor.getStartVoltage());
    snprintf(bot, sizeof(bot), "%6.2fA %5.1f%%",
             _processor.getHouseCurrent(),
             _processor.getHouseSoc());

    if (strcmp(top, _prev_top) != 0) {
        _lcd->setCursor(0, 0);
        _lcd->print(top);
        for (int i = static_cast<int>(strlen(top)); i < LCD_COLS; i++) _lcd->print(' ');
        copy16(_prev_top, top);
    }
    if (strcmp(bot, _prev_bot) != 0) {
        _lcd->setCursor(0, 1);
        _lcd->print(bot);
        for (int i = static_cast<int>(strlen(bot)); i < LCD_COLS; i++) _lcd->print(' ');
        copy16(_prev_bot, bot);
    }
}
