#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <FS.h>
#include <LittleFS.h>
#include <hardware/watchdog.h>

// グラフィック・フォント制御用のライブラリ群
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <U8g2_for_Adafruit_GFX.h>

// センサー・外付け時計（RTC）用のライブラリ群
#include <RTClib.h>
#include <Adafruit_BMP280.h>

// 自作の日本語フォントデータを読み込み
#include "my_u8g2_font.h" 
extern const uint8_t u8g2_font_myfont[] U8G2_FONT_SECTION("u8g2_font_myfont");

// =========================================================================
// [デバッグ設定] 
// 1 = PCシリアルモニタへの日英ログ出力を有効化
// 0 = 完成モード（すべてのログ処理を消去し、スタンドアロンで動かす状態）
// =========================================================================
#define DEBUG 1

#if DEBUG
  #define DEBUG_INIT()     Serial.begin(115200)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_INIT()     
  #define DEBUG_PRINTLN(x) 
  #define DEBUG_PRINTF(...) 
#endif

// =========================================================================
// ピン配置・定数定義
// =========================================================================
#define COLOR_NEON_BLUE      0x07FF  
#define COLOR_BLOODY_ORANGE  0xF200  
#define COLOR_HOLIDAY_RED    0xF800  

#define TFT_RST   15
#define TFT_DC    16
#define TFT_CS    17
#define TFT_SCLK  18
#define TFT_MOSI  19

#define I2C0_SDA   5
#define I2C0_SCL   4
#define I2C1_SDA   6
#define I2C1_SCL   7

#define PIN_BTN_ADJUST 22 
#define PIN_BTN_SELECT 20 
#define PIN_BTN_UP     21 

#define WDT_REFRESH_TIME 50 
#define DispMAKIMA   13000 
#define ChainsoMan   3000  

const int screenWidth = 320;
const int screenHeight = 240;

#define ROW_DATE     40   
#define ROW_DAY      85   
#define ROW_TIME     230  
#define COL_AMPM     5    
#define COL_HOUR     70   
#define COL_SEP1     132  
#define COL_MIN      165  
#define COL_SEP2     225  
#define COL_SEC      257  

#define SemiCollonY_offset -5
#define DateX_offset       -55
#define WeekdayX_Offset    -70
#define FontWidthOffset    22

struct HolidayData {
  uint16_t year;       
  uint8_t month;       
  uint8_t day;         
  uint16_t disp_color; 
};

// =========================================================================
// インスタンスとグローバル変数
// =========================================================================
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST); 
U8G2_FOR_ADAFRUIT_GFX u8g2_gfx;                                 
RTC_DS3231 rtc;                                                 
Adafruit_BMP280 bmp(&Wire1);                                    

uint16_t lineBuffer[screenWidth]; 
const char* currentBgFile = "/makima.bin"; 

uint8_t displayMode = 0;      
bool isEditMode = false;      
uint8_t editTarget = 0;       
DateTime editDateTime;        

DateTime last_t;              
bool firstRun = true;         
unsigned long lastSwitch = 0; 
bool isMakimaBg = true;       
uint16_t currentTextColor = COLOR_NEON_BLUE; 

float lastTemp = -999.0;      
float lastPres = -999.0;      

unsigned long adjustPressStartTime = 0; 
bool adjustPressed = false;              

void displayBinaryImage(const char* filename);
void drawTextWithBackground(int16_t x, int16_t y, const char* text, const char* bgFile);
uint16_t checkHolidayColor(DateTime dt);
void handleButtons();
void handleSerialDebug();
void updateClockDisplay(DateTime t);
void drawEditScreen();
void createDefaultHolidaysFile(); 

// =========================================================================
// 初期化 (Setup)
// =========================================================================
void setup() {
  DEBUG_INIT(); 
  
  #if DEBUG
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 2000)) { watchdog_update(); }
  #endif

  DEBUG_PRINTLN("\n====================================");
  DEBUG_PRINTLN("=== SYSTEM STARTUP / システム起動 ===");
  DEBUG_PRINTLN("====================================");

  //watchdog_enable(5000, 1);  ← ★一時的にコメントアウト
  //DEBUG_PRINTLN("[OK] Watchdog timer enabled (5000ms) / ウォッチドッグタイマ有効化 (5000ms)");

  pinMode(PIN_BTN_ADJUST, INPUT_PULLUP);
  pinMode(PIN_BTN_SELECT, INPUT_PULLUP);
  pinMode(PIN_BTN_UP,     INPUT_PULLUP);

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("[ERROR] LittleFS Mount Failed. / LittleFSのマウントに失敗しました。");
    while (1) watchdog_update(); 
  }
  DEBUG_PRINTLN("[OK] LittleFS mounted successfully / LittleFS マウント完了");

  createDefaultHolidaysFile();

  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();

  Wire1.setSDA(I2C1_SDA);
  Wire1.setSCL(I2C1_SCL);
  Wire1.begin();

  if (!rtc.begin()) {
    DEBUG_PRINTLN("[ERROR] RTC not found. / RTCチップが見つかりません。");
    while (1) watchdog_update();
  }
  DEBUG_PRINTLN("[OK] RTC module initialized / RTCモジュール初期化完了");
  
  if (rtc.lostPower()) {
    DEBUG_PRINTLN("[WARN] RTC lost power! Resetting to 2026/01/01. / RTCの電源が喪失していました。2026/01/01に初期化します。");
    rtc.adjust(DateTime(2026, 1, 1, 0, 0, 0));
  }

  if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
    DEBUG_PRINTLN("[WARN] BMP280 sensor not found. / BMP280センサーが検出されませんでした。");
  } else {
    DEBUG_PRINTLN("[OK] BMP280 sensor initialized / BMP280センサー初期化完了");
  }

  tft.init(screenHeight, screenWidth); 
  tft.setRotation(1); 
  
  u8g2_gfx.begin(tft);
  u8g2_gfx.setFont(u8g2_font_myfont);
  u8g2_gfx.setFontMode(1); 
  u8g2_gfx.setForegroundColor(COLOR_NEON_BLUE);

  displayBinaryImage("/makima.bin");
  last_t = rtc.now(); 
  currentTextColor = checkHolidayColor(last_t); 

  DEBUG_PRINTF("[OK] Setup completed. Current Time: %04d/%02d/%02d %02d:%02d:%02d / 初期化完了。現在時刻: %04d/%02d/%02d %02d:%02d:%02d\n", 
               last_t.year(), last_t.month(), last_t.day(), last_t.hour(), last_t.minute(), last_t.second(),
               last_t.year(), last_t.month(), last_t.day(), last_t.hour(), last_t.minute(), last_t.second());

  watchdog_update(); 
}

// =========================================================================
// メインループ (Loop)
// =========================================================================
void loop() {
  watchdog_update(); 

  handleButtons();     
  handleSerialDebug(); 

  if (isEditMode) {
    drawEditScreen();  
  } else {
    DateTime t = rtc.now(); 

    if (displayMode == 0 || displayMode == 1) {
      unsigned long elapsed = millis() - lastSwitch;
      unsigned long targetInterval = (isMakimaBg ? DispMAKIMA : ChainsoMan);
      
      if (elapsed > targetInterval) {
        isMakimaBg = !isMakimaBg; 
        displayMode = isMakimaBg ? 0 : 1;
        
        DEBUG_PRINTF("[INFO] Switching background: %s / 背景切り替え: %s\n", 
                     isMakimaBg ? "/makima.bin" : "/chanesouman.bin", isMakimaBg ? "/makima.bin" : "/chanesouman.bin");
                     
        displayBinaryImage(isMakimaBg ? "/makima.bin" : "/chanesouman.bin"); 
        
        currentTextColor = checkHolidayColor(t); 
        lastSwitch = millis(); 
        firstRun = true;       
      }
    }

    updateClockDisplay(t); 
    last_t = t;            
    firstRun = false;      
  }

  delay(WDT_REFRESH_TIME); 
}

// =========================================================================
// 祝日ファイルのデフォルト作成
// =========================================================================
void createDefaultHolidaysFile() {
  if (LittleFS.exists("/holidays.bin")) {
    DEBUG_PRINTLN("[INFO] holidays.bin exists. / holidays.binは既に存在します。");
    return;
  }

  DEBUG_PRINTLN("[INFO] Creating default holidays.bin... / デフォルトの holidays.bin を作成中...");
  File file = LittleFS.open("/holidays.bin", "w");
  if (!file) {
    DEBUG_PRINTLN("[ERROR] Failed to open holidays.bin / holidays.binの作成に失敗しました。");
    return;
  }

  HolidayData defaults[] = {
    {0, 1, 26, COLOR_BLOODY_ORANGE}, 
    {0, 4, 20, COLOR_NEON_BLUE},     
    {0, 7, 3,  COLOR_BLOODY_ORANGE}, 
    {0, 1, 1,   COLOR_HOLIDAY_RED}, 
    {0, 2, 11,  COLOR_HOLIDAY_RED}, 
    {0, 2, 23,  COLOR_HOLIDAY_RED}, 
    {0, 4, 29,  COLOR_HOLIDAY_RED}, 
    {0, 5, 3,   COLOR_HOLIDAY_RED}, 
    {0, 5, 4,   COLOR_HOLIDAY_RED}, 
    {0, 5, 5,   COLOR_HOLIDAY_RED}, 
    {0, 8, 11,  COLOR_HOLIDAY_RED}, 
    {0, 11, 3,  COLOR_HOLIDAY_RED}, 
    {0, 11, 23, COLOR_HOLIDAY_RED}, 
    {2026, 1, 12, COLOR_HOLIDAY_RED}, 
    {2026, 3, 20, COLOR_HOLIDAY_RED}, 
    {2026, 7, 20, COLOR_HOLIDAY_RED}, 
    {2026, 9, 21, COLOR_HOLIDAY_RED}, 
    {2026, 9, 23, COLOR_HOLIDAY_RED}, 
    {2026, 10, 12, COLOR_HOLIDAY_RED} 
  };

  size_t count = sizeof(defaults) / sizeof(HolidayData);
  for (size_t i = 0; i < count; i++) {
    file.write((uint8_t*)&defaults[i], sizeof(HolidayData));
  }
  file.close(); 
  DEBUG_PRINTF("[OK] Saved %d records / %d 件の祝日データを LittleFS に保存しました。\n", count, count);
}

// =========================================================================
// 記念日・祝日の色判定
// =========================================================================
uint16_t checkHolidayColor(DateTime dt) {
  uint16_t defaultColor = (displayMode == 0 || displayMode == 3) ? COLOR_NEON_BLUE : COLOR_BLOODY_ORANGE;

  File file = LittleFS.open("/holidays.bin", "r");
  if (!file) return defaultColor;

  HolidayData holiday;
  uint16_t matchedColor = defaultColor;

  while (file.read((uint8_t*)&holiday, sizeof(HolidayData)) == sizeof(HolidayData)) {
    if ((holiday.year == 0 || holiday.year == dt.year()) &&
        holiday.month == dt.month() &&
        holiday.day == dt.day()) {
      matchedColor = holiday.disp_color; 
      DEBUG_PRINTF("[MATCH] Special date detected: 0x%04X / 記念日・祝日を検知: 0x%04X\n", matchedColor, matchedColor);
      break; 
    }
  }
  file.close();
  return matchedColor; 
}

// =========================================================================
// ボタン処理
// =========================================================================
void handleButtons() {
  bool btnAdjust = (digitalRead(PIN_BTN_ADJUST) == LOW);
  bool btnSelect = (digitalRead(PIN_BTN_SELECT) == LOW);
  bool btnUp     = (digitalRead(PIN_BTN_UP) == LOW);

  if (btnAdjust) {
    if (!adjustPressed) {
      adjustPressed = true;
      adjustPressStartTime = millis();
      DEBUG_PRINTLN("[BUTTON] ADJUST Down / ADJUSTボタンが押されました。");
    } else {
      if (millis() - adjustPressStartTime >= 3000) { 
        isEditMode = !isEditMode; 
        adjustPressed = false;
        tft.fillScreen(ST77XX_BLACK); 

        DEBUG_PRINTF("[MODE CHANGE] Edit Mode: %d / 時刻合わせモード: %d\n", isEditMode, isEditMode);

        if (isEditMode) {
          editDateTime = rtc.now();
          editTarget = 0; 
        } else {
          rtc.adjust(editDateTime); 
          firstRun = true; 
          lastSwitch = millis();
          DEBUG_PRINTLN("[MODE CHANGE] Saved to RTC / 新しい時刻をRTCに保存しました。");
        }
        delay(500); 
        return;
      }
    }
  } else {
    if (adjustPressed) {
      if (millis() - adjustPressStartTime < 3000) { 
        DEBUG_PRINTLN("[BUTTON] ADJUST Short Release / ADJUSTボタンの短押しを検知。");
        if (!isEditMode) {
          displayMode = (displayMode + 1) % 4; 
          DEBUG_PRINTF("[INFO] Display Mode Changed: %d / 表示モード変更: %d\n", displayMode, displayMode);
          if (displayMode == 0 || displayMode == 3) {
            displayBinaryImage("/makima.bin");
            isMakimaBg = true;
          } else {
            displayBinaryImage("/chanesouman.bin");
            isMakimaBg = false;
          }
          currentTextColor = checkHolidayColor(rtc.now()); 
          firstRun = true;
          lastSwitch = millis(); 
        }
      }
      adjustPressed = false;
    }
  }

  if (isEditMode) {
    if (btnSelect) { 
      editTarget = (editTarget + 1) % 6; 
      DEBUG_PRINTF("[EDIT] Target: %d (0:Y, 1:M, 2:D, 3:h, 4:m, 5:s) / 編集対象: %d\n", editTarget, editTarget);
      delay(200); 
    }
    if (btnUp) { 
      int y = editDateTime.year();
      int m = editDateTime.month();
      int d = editDateTime.day();
      int hh = editDateTime.hour();
      int mm = editDateTime.minute();
      int ss = editDateTime.second();

      switch (editTarget) {
        case 0: y++; if (y > 2099) y = 2026; break; 
        case 1: m = (m % 12) + 1; break;             
        case 2: d = (d % 31) + 1; break;             
        case 3: hh = (hh + 1) % 24; break;           
        case 4: mm = (mm + 1) % 60; break;           
        case 5: ss = (ss + 1) % 60; break;           
      }
      editDateTime = DateTime(y, m, d, hh, mm, ss);
      DEBUG_PRINTF("[EDIT] Increment. Buff: %04d/%02d/%02d %02d:%02d:%02d / 数値変更。バッファ: %04d/%02d/%02d %02d:%02d:%02d\n", 
                   y, m, d, hh, mm, ss, y, m, d, hh, mm, ss);
      delay(200);
    }
  }
}

// =========================================================================
// シリアルデバッグ（タイムスリップ機能）
// =========================================================================
void handleSerialDebug() {
#if DEBUG
  if (Serial.available() >= 8) {
    String input = Serial.readStringUntil('\n'); 
    input.trim(); 

    if (input.length() == 8) {
      int y = input.substring(0, 4).toInt(); 
      int m = input.substring(4, 6).toInt(); 
      int d = input.substring(6, 8).toInt(); 

      if (y >= 2000 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
        DateTime now = rtc.now();
        rtc.adjust(DateTime(y, m, d, now.hour(), now.minute(), now.second()));
        DEBUG_PRINTF("[SERIAL SYNC] Warp -> %04d/%02d/%02d / ワープ -> %04d/%02d/%02d\n", y, m, d, y, m, d);
        firstRun = true; 
        currentTextColor = checkHolidayColor(rtc.now()); 
      }
    }
  }
#endif
}

// =========================================================================
// 画像読み込み
// =========================================================================
void displayBinaryImage(const char* filename) {
  DEBUG_PRINTF("[FS] Loading: %s / 背景読み込み中: %s\n", filename, filename);
  currentBgFile = filename;
  File f = LittleFS.open(filename, "r");
  if (!f) {
    DEBUG_PRINTF("[ERROR] Open failed: %s / オープン失敗: %s\n", filename, filename);
    return;
  }

  tft.startWrite(); 
  tft.setAddrWindow(0, 0, screenWidth, screenHeight); 

  for (int y = 0; y < screenHeight; y++) {
    if (f.read((uint8_t*)lineBuffer, screenWidth * 2) != screenWidth * 2) break;
    tft.writePixels(lineBuffer, screenWidth);      
    if (y % 40 == 0) watchdog_update();            
  }
  tft.endWrite(); 
  f.close();
}

// =========================================================================
// 背景部分復元テキスト表示
// =========================================================================
void drawTextWithBackground(int16_t x, int16_t y, const char* text, const char* bgFile) {
  const int16_t h = 58;        
  const int16_t off_y = 46;    
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

// =========================================================================
// 時計描画と周期的ログ
// =========================================================================
void updateClockDisplay(DateTime t) {
  if (displayMode == 2 || displayMode == 3) {
    lastTemp = -999.0; 
    return;
  }

  char buf[32];
  u8g2_gfx.setForegroundColor(currentTextColor); 

  if (firstRun || t.day() != last_t.day()) {
    sprintf(buf, "%04d/%02d/%02d", t.year(), t.month(), t.day());
    int16_t x_date = ((screenWidth - (strlen(buf) * FontWidthOffset)) / 2) + DateX_offset;
    drawTextWithBackground(x_date, ROW_DATE, buf, currentBgFile);
    DEBUG_PRINTF("[CLOCK] Date Update: %s / 日付表示を更新: %s\n", buf, buf);
  }

  if (firstRun || t.day() != last_t.day()) {
    const char* days[] = {"Sun", "Mon", "Tues", "Wednes", "Thurs", "Fri", "Satur"};
    const char* dayStr = days[t.dayOfTheWeek()];
    int16_t x_day = ((screenWidth - (strlen(dayStr) * FontWidthOffset)) / 2) + WeekdayX_Offset;
    drawTextWithBackground(x_day, ROW_DAY, dayStr, currentBgFile);
  }

  bool isPM = (t.hour() >= 12);
  bool lastPM = (last_t.hour() >= 12);
  if (firstRun || isPM != lastPM) {
    drawTextWithBackground(COL_AMPM, ROW_TIME, isPM ? "PM" : "AM", currentBgFile);
  }

  if (firstRun || t.hour() != last_t.hour()) {
    int h12 = t.hour() % 12;
    if (h12 == 0) h12 = 12; 
    sprintf(buf, "%02d", h12);
    drawTextWithBackground(COL_HOUR, ROW_TIME, buf, currentBgFile);
  }

  if (firstRun || t.minute() != last_t.minute()) {
    sprintf(buf, "%02d", t.minute());
    drawTextWithBackground(COL_MIN, ROW_TIME, buf, currentBgFile);
    
    // 1分ごとにPCシリアルモニタへ状態を一括送信
    DEBUG_PRINTF("[STATUS REPORT] %02d:%02d | Temp: %.1fC | Pres: %.1fhPa / [状態報告] %02d:%02d | 温度: %.1fC | 気圧: %.1fhPa\n", 
                 t.hour(), t.minute(), lastTemp, lastPres, t.hour(), t.minute(), lastTemp, lastPres);
  }
  
  if (firstRun || t.second() != last_t.second()) {
    sprintf(buf, "%02d", t.second());
    drawTextWithBackground(COL_SEC, ROW_TIME, buf, currentBgFile);
  }

  static bool lastState = false;
  bool currentState = (millis() / 500) % 2 == 0;
  if (firstRun || currentState != lastState) {
    if (currentState) {
      drawTextWithBackground(COL_SEP1, ROW_TIME + SemiCollonY_offset, ":", currentBgFile);
      drawTextWithBackground(COL_SEP2, ROW_TIME + SemiCollonY_offset, ":", currentBgFile);
    } else {
      drawTextWithBackground(COL_SEP1, ROW_TIME + SemiCollonY_offset, " ", currentBgFile);
      drawTextWithBackground(COL_SEP2, ROW_TIME + SemiCollonY_offset, " ", currentBgFile);
    }
    lastState = currentState;
  }

  float currentTemp = bmp.readTemperature();
  float currentPres = bmp.readPressure() / 100.0F; 

  if (abs(currentTemp - lastTemp) >= 0.1 || abs(currentPres - lastPres) >= 0.1) {
    char envBuf[40];
    snprintf(envBuf, sizeof(envBuf), "T:%2.1fC P:%4.1fhPa ", currentTemp, currentPres);
    u8g2_gfx.setForegroundColor(COLOR_NEON_BLUE); 
    drawTextWithBackground(15, 140, envBuf, currentBgFile); 
    
    DEBUG_PRINTF("[SENSOR] Temp: %.2fC, Pres: %.2fhPa / センサー変動: 温度: %.2fC, 気圧: %.2fhPa\n", 
                 currentTemp, currentPres, currentTemp, currentPres);
    lastTemp = currentTemp;
    lastPres = currentPres;
  }
}

// =========================================================================
// 時刻合わせ画面
// =========================================================================
void drawEditScreen() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 150) return; 
  lastUpdate = millis();

  tft.fillScreen(ST77XX_BLACK); 
  u8g2_gfx.setForegroundColor(COLOR_NEON_BLUE);
  
  u8g2_gfx.setCursor(40, 40);
  u8g2_gfx.print("TIME SETTING MODE");

  char buf[40];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d", editDateTime.year(), editDateTime.month(), editDateTime.day());
  u8g2_gfx.setCursor(60, 100);
  u8g2_gfx.print(buf);

  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", editDateTime.hour(), editDateTime.minute(), editDateTime.second());
  u8g2_gfx.setCursor(85, 150);
  u8g2_gfx.print(buf);

  u8g2_gfx.setForegroundColor(COLOR_BLOODY_ORANGE);
  u8g2_gfx.setCursor(50, 210);
  switch (editTarget) {
    case 0: u8g2_gfx.print("EDIT -> [ YEAR ]"); break;
    case 1: u8g2_gfx.print("EDIT -> [ MONTH ]"); break;
    case 2: u8g2_gfx.print("EDIT -> [ DAY ]"); break;
    case 3: u8g2_gfx.print("EDIT -> [ HOUR ]"); break;
    case 4: u8g2_gfx.print("EDIT -> [ MINUTE ]"); break;
    case 5: u8g2_gfx.print("EDIT -> [ SECOND ]"); break;
  }
}
