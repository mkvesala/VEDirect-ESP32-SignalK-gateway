#include "DisplayManager.h"

// === P U B L I C ===

// Constructor
DisplayManager::DisplayManager(VEDProcessor &processorRef)
    : _processor(processorRef)
{}

// Initialize
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

// Status message
void DisplayManager::showMessage(const char* l1, const char* l2) {
    printLines(l1, l2);
}

// Diagnostics
void DisplayManager::showDiagData(uint32_t heap, uint32_t main, uint32_t reader) {
    if (!_lcd_present || !_lcd) return;

    char top[17];
    char bot[17];

    snprintf(top, sizeof(top), "HEAP %luB", (unsigned long)heap);

    snprintf(bot, sizeof(bot), "WM M:%lu R:%lu",
             (unsigned long)main, (unsigned long)reader);

    printLines(top, bot);
}

// Data message
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

// === P R I V A T E ===

// Helper to copy 16 characters
void DisplayManager::copy16(char* dst, const char* src) {
    strncpy(dst, src, 16);
    dst[16] = '\0';
}

// Basic print to two lines
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

// I2C scan
bool DisplayManager::i2cPresent(uint8_t addr) {
    Wire.beginTransmission(addr);
    return (Wire.endTransmission() == 0);
}
