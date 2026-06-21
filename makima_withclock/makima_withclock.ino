#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <SPI.h>
#include <LittleFS.h>
#include "hardware/rtc.h"  // RP2040内蔵RTC用
#include "hardware/watchdog.h"
#include "pico/util/datetime.h"

#include "my_u8g2_font.h" // 
extern const uint8_t u8g2_font_myfont[] U8G2_FONT_SECTION("u8g2_font_myfont");

#define COLOR_NEON_BLUE      0x07FF  // ネオンブルー (RGB565)
#define COLOR_BLOODY_ORANGE  0xF200  // ブラッディオレンジ (RGB565)

// --- ピン配置 ---
#define TFT_CS        17
#define TFT_DC        16
#define TFT_RST       15
#define TFT_MOSI      19
#define TFT_SCLK      18

#include "pico/util/datetime.h"

//ウオッチドッグタイムのリフレッシュインターバル時間
#define WDT_reflesh_time 50 //ms

// --- 初期時刻設定 ---
#define INIT_YEAR  2025
#define INIT_MONTH 12
#define INIT_DAY   31
#define INIT_DOTW  3    // 0=日, 1=月, 2=火, 3=水, 4=木, 5=金, 6=土
#define INIT_HOUR  23
#define INIT_MIN   59
#define INIT_SEC   40

//各背景画面の表示時間
#define DispMAKIMA 13000 //ms
#define ChainsoMan 3000 //ms

// --- レイアウト設定 (320x240画面用) ---
// 1行目: 日付 (センタリング)
#define ROW_DATE    40 
// 2行目: 曜日 (センタリング)
#define ROW_DAY     85 
// 最下段: 時刻 (固定位置)
#define ROW_TIME    230 

// 時刻表示用の各X座標
#define COL_AMPM    5
#define COL_HOUR    70
#define COL_SEP1    132
#define COL_MIN     165
#define COL_SEP2    225
#define COL_SEC     257

#define SemiCollonY_offset -5
#define DateX_offset -55
#define WeekdayX_Offset - 70
#define FontWidthOffset 22

Adafruit_ST7789 tft = Adafruit_ST7789(&SPI, TFT_CS, TFT_DC, TFT_RST);
U8G2_FOR_ADAFRUIT_GFX u8g2_gfx;

const int screenWidth = 320;
const int screenHeight = 240;
uint16_t lineBuffer[screenWidth];
const char* currentBgFile = "";

// 背景を復元してから文字を描画する関数
void drawTextWithBackground(int16_t x, int16_t y, const char* text, const char* bgFile) {
    // --- DotoMatrix-25に合わせて最適化した数値 ---
    const int16_t h = 58;      // 消去する全高（60から55に微減し、下の行への干渉を防ぐ）
    const int16_t off_y = 46;  // 上方向へ50px遡る（これで上端のダブりを消す）
    
    // 消去幅の計算（文字の右端・左端のダブり防止）
    int16_t margin = 16; 
    int16_t textWidth = (strlen(text) * 22) + margin; 
    
    int16_t drawX = x - (margin / 2);
    if (drawX < 0) drawX = 0;
    if (drawX + textWidth > screenWidth) textWidth = screenWidth - drawX;

    File f = LittleFS.open(bgFile, "r");
    if (!f) return;

    tft.startWrite();
    for (int16_t i = 0; i < h; i++) {
        int16_t currentRow = y - off_y + i;
        if (currentRow < 0 || currentRow >= screenHeight) continue;

        uint32_t pos = (uint32_t)currentRow * 320 * 2;
        if (f.seek(pos)) {
            f.read((uint8_t*)lineBuffer, 640);
            tft.setAddrWindow(drawX, currentRow, textWidth, 1);
            tft.writePixels(&lineBuffer[drawX], textWidth);
        }
    }
    tft.endWrite();
    f.close();

    u8g2_gfx.setCursor(x, y);
    u8g2_gfx.print(text);
}

void displayBinaryImage(const char* filename) {
    currentBgFile = filename;
    File f = LittleFS.open(filename, "r");
    if (!f) return;
    tft.startWrite();
    tft.setAddrWindow(0, 0, screenWidth, screenHeight);
    for (int y = 0; y < screenHeight; y++) {
        f.read((uint8_t*)lineBuffer, screenWidth * 2);
        tft.writePixels(lineBuffer, screenWidth);
        if (y % 40 == 0) watchdog_update();
    }
    tft.endWrite();
    f.close();
}

void setup() {
    Serial.begin(115200);
    SPI.setTX(TFT_MOSI);
    SPI.setSCK(TFT_SCLK);

    tft.init(240, 320);
    tft.setRotation(1);
    
    if (!LittleFS.begin()) while(1) watchdog_update();

    u8g2_gfx.begin(tft);
    //u8g2_gfx.setFont(u8g2_font_helvB24_tr); 
    u8g2_gfx.setFont(u8g2_font_myfont);
    u8g2_gfx.setFontMode(1);
    u8g2_gfx.setForegroundColor(COLOR_NEON_BLUE);

    // 起動時の時刻をセット (2026年5月14日 07:05:00 木曜日)
    // dotw: 0=日, 1=月, 2=火, 3=水, 4=木, 5=金, 6=土
    rtc_init();
    
    // 定義した値を使用してセット
    datetime_t t = {
        .year  = INIT_YEAR,
        .month = INIT_MONTH,
        .day   = INIT_DAY,
        .dotw  = INIT_DOTW,
        .hour  = INIT_HOUR,
        .min   = INIT_MIN,
        .sec   = INIT_SEC
    };
    rtc_set_datetime(&t);
}

void loop() {
    static datetime_t last_t = {0};
    static bool firstRun = true;
    static unsigned long lastSwitch = 0;
    static bool isMakima = false;

    // --- 背景切り替え ---
    if (millis() - lastSwitch > (isMakima ? DispMAKIMA : ChainsoMan)) {
        isMakima = !isMakima;
        displayBinaryImage(isMakima ? "/makima.bin" : "/chanesouman.bin");
        
        // --- ここで背景に応じたフォント色をセット ---
        if (isMakima) {
            u8g2_gfx.setForegroundColor(COLOR_NEON_BLUE);
        } else {
            u8g2_gfx.setForegroundColor(COLOR_BLOODY_ORANGE);
        }
        // ------------------------------------------

        lastSwitch = millis();
        firstRun = true; // 背景色が変わったので、全ての文字を再描画させる
    }
    
    datetime_t t;
    rtc_get_datetime(&t);
    char buf[32];

    // --- 1. 日付の更新 (2026/05/14 形式) ---
    if (firstRun || t.day != last_t.day) {
        sprintf(buf, "%04d/%02d/%02d", t.year, t.month, t.day);
        // センタリング計算: (320 - (文字数 * 1文字幅18)) / 2
        int16_t x_date = ((screenWidth - (strlen(buf) * FontWidthOffset)) / 2) DateX_offset;
        drawTextWithBackground(x_date, ROW_DATE, buf, currentBgFile);
    }

    // --- 2. 曜日の更新 ---
    if (firstRun || t.day != last_t.day) {
        const char* days[] = {"Sun", "Mon", "Tues", "Wednes", "Thurs", "Fri", "Satur"};
        const char* dayStr = days[t.dotw];
        int16_t x_day = ((screenWidth - (strlen(dayStr) * FontWidthOffset)) / 2) WeekdayX_Offset;
        drawTextWithBackground(x_day, ROW_DAY, dayStr, currentBgFile);
    }

    // --- 3. AM/PM の更新 ---
    bool isPM = (t.hour >= 12);
    bool lastPM = (last_t.hour >= 12);
    if (firstRun || isPM != lastPM) {
        drawTextWithBackground(COL_AMPM, ROW_TIME, isPM ? "PM" : "AM", currentBgFile);
    }

    // --- 4. 時(Hour)の更新 ---
    if (firstRun || t.hour != last_t.hour) {
        int h12 = t.hour % 12;
        if (h12 == 0) h12 = 12;
        sprintf(buf, "%02d", h12);
        drawTextWithBackground(COL_HOUR, ROW_TIME, buf, currentBgFile);
    }

    // --- 5. 分(Minute)の更新 ---
    if (firstRun || t.min != last_t.min) {
        sprintf(buf, "%02d", t.min);
        drawTextWithBackground(COL_MIN, ROW_TIME, buf, currentBgFile);
    }
    
    // --- 6. 秒(Second)の更新 ---
    if (firstRun || t.sec != last_t.sec) {
        sprintf(buf, "%02d", t.sec);
        drawTextWithBackground(COL_SEC, ROW_TIME, buf, currentBgFile);
    }

    // --- コロンの0.5秒点滅処理 ---
    // millis()を500で割った値が偶数か奇数かで0.5秒周期を作る
    static bool lastState = false;
    bool currentState = (millis() / 500) % 2 == 0;

    if (firstRun || currentState != lastState) {
        if (currentState) {
            // 点灯
            drawTextWithBackground(COL_SEP1, ROW_TIME SemiCollonY_offset, ":", currentBgFile);
            drawTextWithBackground(COL_SEP2, ROW_TIME SemiCollonY_offset, ":", currentBgFile);
        } else {
            // 消灯（スペースを描画することで背景を復元）
            drawTextWithBackground(COL_SEP1, ROW_TIME SemiCollonY_offset, " ", currentBgFile);
            drawTextWithBackground(COL_SEP2, ROW_TIME SemiCollonY_offset, " ", currentBgFile);
        }
        lastState = currentState;
    }
    last_t = t;
    firstRun = false;
    watchdog_update();
    delay(WDT_reflesh_time);
}
