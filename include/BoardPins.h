#pragma once

// Zhongjingyuan 2.66 inch SSD1680 e-paper, 152x296.
// Match the ESP-IDF port wiring.
static constexpr int EPAPER_PIN_MOSI = 4;   // SDA / DIN / MOSI
static constexpr int EPAPER_PIN_SCLK = 5;   // SCL / CLK / SCK
static constexpr int EPAPER_PIN_CS = 6;
static constexpr int EPAPER_PIN_DC = 7;     // 0 = command, 1 = data/parameter
static constexpr int EPAPER_PIN_RST = 15;   // RES / RST
static constexpr int EPAPER_PIN_BUSY = 16;  // idle low on this panel

static constexpr int EPAPER_WIDTH = 152;
static constexpr int EPAPER_HEIGHT = 296;
static constexpr int EPAPER_BUSY_IDLE_LEVEL = 0;
