Markdown
[日本語の取扱説明書はこちら (README_JP.md)](./README_JP.md)

# Advanced TFT Character Clock (RP2040-Powered)

This product is a high-performance desktop digital clock based on an RP2040 mini-board, integrating a high-resolution TFT display, a high-precision Real-Time Clock (RTC), and an environmental sensor. 

---

## 1. Key Features

1. **Multi-Mode Background Display**
   The user can cycle through four distinct display modes by pressing the Adjust button:
   * Mode 0: Makima Background + Text Display (ON)
   * Mode 1: Chainsaw Man Background + Text Display (ON)
   * Mode 2: Chainsaw Man Background + Text Display (OFF - Image Gallery Mode)
   * Mode 3: Makima Background + Text Display (OFF - Image Gallery Mode)
2. **On-Board Time Adjustment (3-Button Control)**
   Allows users to calibrate the calendar and time directly using the hardware buttons without requiring external computer connection.
3. **Environmental Monitoring**
   Measures ambient temperature and atmospheric pressure in real-time, updating the display smoothly upon detecting a 0.1-unit variance.
4. **Holiday & Anniversary Color Coding**
   Dynamically alters the text color on specific dates based on binary holiday data stored in the LittleFS.
5. **Hang-up Prevention (WDT Guard)**
   Monitored by a hardware Watchdog Timer (WDT) to ensure the system automatically reboots within seconds in the event of a system freeze.

---

## 2. Build Environment & Dependencies

This project can be compiled using **Arduino IDE 2.x** or the **PlatformIO** environment.

### ■ Required Libraries
| Library Name | Version | Purpose |
| :--- | :--- | :--- |
| `Adafruit_GFX_Library` | 1.11.9 or later | Core graphics rendering |
| `Adafruit_ST7789_Library` | 1.10.3 or later | TFT display hardware control |
| `U8g2_for_Adafruit_GFX` | 1.8.0 or later | Custom font and multilingual rendering |
| `RTClib` (Adafruit fork) | 2.1.4 or later | High-precision RTC (DS3231) control |
| `Adafruit_BMP280_Library` | 2.6.8 or later | Temperature & pressure sensor control |

---

## 3. Parts List

| Component Name | Qty | Remarks |
| :--- | :--- | :--- |
| RP2040 Mini Board | 1 | Microcontroller board (Hardware Verified layout) |
| 2.0-inch TFT Display Module (ST7789) | 1 | 320x240 pixels, SPI interface |
| DS3231 High-Precision RTC Module | 1 | I2C interface (Address: 0x68) |
| BMP280 Temperature & Pressure Sensor | 1 | I2C interface (Address: 0x76 or 0x77) |
| Tactile Switches | 3 | Used for mode switching & time calibration |
| Breadboard / Jumper Wires | 1 Set | For hardware prototyping & wiring |

---

## 4. Circuit Diagram (Connection Specifications)

This section details the hardware connections between the RP2040 mini-board and each peripheral device.
*Note: Connect the opposite terminal of each tactile switch to the **GND** pin located at the bottom-left of the board.*

### ■ Signal Connection List

#### (1) TFT Display (ST7789 SPI0)
* GPIO 15 ── TFT_RST (Reset)
* GPIO 16 ── TFT_DC (Data/Command)
* GPIO 17 ── TFT_CS (Chip Select)
* GPIO 18 ── TFT_SCLK (SPI0_SCK)
* GPIO 19 ── TFT_MOSI (SPI0_TX)

#### (2) Real-Time Clock (DS3231 [I2C0])
* GPIO 4  ── I2C0_SCL (Serial Clock) *Requires external pull-up resistor
* GPIO 5  ── I2C0_SDA (Serial Data)  *Requires external pull-up resistor

#### (3) Environmental Sensor (BMP280 [I2C1])
* GPIO 6  ── I2C1_SDA (Serial Data)
* GPIO 7  ── I2C1_SCL (Serial Clock)

#### (4) Control Tactile Switches (Internal Pull-Up Enabled)
* GPIO 22 ── Adjust Button (Long press for 3s to Enter/Exit Time Adjustment Mode)
* GPIO 20 ── Select Button (Cycles through adjustment fields: YY/MM/DD/hh/mm/ss)
* GPIO 21 ── Count Up Button (Increments the value of the active field by +1)

---

### ■ Wiring Diagram (Hardware Layout Mirror)

```text
                        RP2040 Mini Board (Final Wiring)
                                Circuit Diagram
                    
    [Left-Side Devices]         +---------------+         [Right-Side Devices]
                  (NC)  [--| (NC)      (15) |--]  15  ---> [TFT_RST] Display
         [TFT_DC]  <--- 16  [--| 16         14 |--]  14
         [TFT_CS]  <--- 17  [--| 17         13 |--]  13
        [TFT_SCLK] <--- 18  [--| 18         12 |--]  12
        [TFT_MOSI] <--- 19  [--| 19         11 |--]  11
       [Select BTN] <--- 20  [--| 20         10 |--]  10
     [Count Up BTN] <--- 21  [--| 21          9 |--]  9
       [Adjust BTN] <--- 22  [--| 22          8 |--]  8
                         23  [--| 23          7 |--]  7  ---> [I2C1_SCL] BMP280
                         24  [--| 24          6 |--]  6  ---> [I2C1_SDA] BMP280
                         25  [--| 25          5 |--]  5  ---> [I2C0_SDA] RTC (DS3231)
                          0  [--|  0          4 |--]  4  ---> [I2C0_SCL] RTC (DS3231)
                          1  [--|  1          3 |--]  3
                          2  [--|  2          2 |--]  2
                          3  [--|  3          1 |--]  1
                             [--|             0 |--]  0
        [Common GND] <--- GND [--| GND        5V |--]  5V ──> VCC (5V) for all devices
                 3V3 <--- 3V3 [--| 3V3       GND |--]  GND ──> GND for all devices
                             +-------+-------+
                                     |
                                   [USB-C]
5. Wallpaper Binary Generation & Upload
To optimize high-speed rendering on the LCD, image assets must be stored in LittleFS as raw binary data (.bin) formatted in RGB565 (Big-Endian). This eliminates CPU rendering overhead.

■ Image Conversion via ffmpeg
Bash
ffmpeg -i input.jpg -f rawvideo -pix_fmt rgb565be makima.bin
-pix_fmt rgb565be: Configured as RGB565 Big-Endian to align with the ST7789 display driver specifications.

6. Holiday & Anniversary Data Format
Holiday configurations are saved as a fixed-length binary file in LittleFS. Each entry consumes exactly 6 bytes.

■ Data Structure (C/C++)
C++
struct DateTime {
  uint16_t year;        // 2 bytes: Target Year (Set to 0 for recurring annual events)
  uint8_t month;        // 1 byte : Target Month (1 to 12)
  uint8_t day;          // 1 byte : Target Day (1 to 31)
  uint16_t disp_color;  // 2 bytes: Display color code for the day (RGB565 format)
};
7. Operation Guide
■ Standard Mode
Press Adjust Button (Short Press): Cycles through display modes (Mode 0 to 3) sequentially.

■ Time Adjustment Mode
Entering the Mode: Press and hold the Adjust Button for over 3 seconds.

Selecting a Unit: Press the Select Button to cycle through adjustable fields: "Year ──> Month ──> Day ──> Hour ──> Minute ──> Second".

Modifying the Value: Press the Count Up Button to increment the value of the active field.

Saving and Exiting: Press and hold the Adjust Button for over 3 seconds to write to the external RTC hardware.
