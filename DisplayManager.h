#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "VEDProcessor.h"

// === C L A S S  D I S P L A Y M A N A G E R ===
//
// - Class DisplayManager - responsible for managing LCD or other display
// - Init: _display.begin();
// - Provides public API to:
//   - Show battery data
//   - Show diagnostics
//   - Show any info message
//   - Get presence
// - Owns: LiquidCrystal_I2C
// - Uses: VEDProcessor
// - Owned by: VEDApplication

class DisplayManager {

public:

    explicit DisplayManager(VEDProcessor &processorRef);

    void begin();
    void showBatteryData();
    void showDiagData(uint32_t heap, uint32_t main, uint32_t reader);
    void showMessage(const char* l1, const char* l2);
    bool isPresent() const { return _lcd_present; }

private:

    static constexpr uint8_t ADDR_PRI = 0x27;
    static constexpr uint8_t ADDR_ALT = 0x3F;
    static constexpr uint8_t LCD_COLS = 16;
    static constexpr uint8_t LCD_ROWS = 2;

    VEDProcessor &_processor;

    LiquidCrystal_I2C _lcd_27 {ADDR_PRI, LCD_COLS, LCD_ROWS};
    LiquidCrystal_I2C _lcd_3f {ADDR_ALT, LCD_COLS, LCD_ROWS};
    LiquidCrystal_I2C* _lcd = nullptr;
    bool _lcd_present = false;

    char _prev_top[17] {};
    char _prev_bot[17] {};

    void printLines(const char* l1, const char* l2);
    static bool i2cPresent(uint8_t addr);
    static void copy16(char* dst, const char* src);

};
