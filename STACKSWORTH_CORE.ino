#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>

// =====================================================
// STACKSWORTHmini
// Option B - Card layout (lowered slightly)
// =====================================================

// -------- Colors (RGB565) --------
#define BLACK     0x0000
#define WHITE     0xFFFF
#define RED       0xF800
#define GREEN     0x07E0
#define BLUE      0x001F
#define CYAN      0x07FF
#define ORANGE    0xFD20
#define GREY      0x8410
#define DARKGREY  0x4208

// -------- Pins --------
#define LCD_MOSI 6
#define LCD_SCLK 7
#define LCD_CS   14
#define LCD_DC   15
#define LCD_RST  21
#define LCD_BL   22
#define RGB_PIN  8

#define LCD_WIDTH  172
#define LCD_HEIGHT 320

// -------- RGB LED --------
#define NUM_PIXELS 1
Adafruit_NeoPixel pixel(NUM_PIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

// -------- Display --------
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI);

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  LCD_RST,
  0,
  true,
  LCD_WIDTH,
  LCD_HEIGHT,
  34,
  0,
  34,
  0
);

// -------- Placeholder values --------
String displayPrice = "$943,228";
String displayBlock = "893245";
String displayTime  = "10:42 PM";
String footerText   = "STACKSWORTH.COM";

// -------- RGB timing --------
uint16_t hueValue = 0;
unsigned long lastRGBUpdate = 0;

// =====================================================
// RGB rainbow
// =====================================================
void updateRainbowLED() {
  if (millis() - lastRGBUpdate > 20) {
    lastRGBUpdate = millis();
    pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(hueValue)));
    pixel.show();
    hueValue += 256;
  }
}

// =====================================================
// UI FUNCTIONS
// =====================================================
void drawHeader() {
  gfx->drawRoundRect(8, 18, 156, 38, 8, ORANGE);

  gfx->setTextColor(WHITE);
  gfx->setTextSize(2);
  gfx->setCursor(22, 24);
  gfx->print("STACKSWORTH");

  gfx->setTextColor(ORANGE);
  gfx->setTextSize(2);
  gfx->setCursor(78, 36);
  gfx->print("mini");
}

void drawMetricCard(int x, int y, int w, int h, const char *label, const String &value, uint16_t borderColor) {
  gfx->drawRoundRect(x, y, w, h, 10, borderColor);
  gfx->drawRoundRect(x + 1, y + 1, w - 2, h - 2, 10, DARKGREY);

  gfx->setTextColor(borderColor);
  gfx->setTextSize(1);
  gfx->setCursor(x + 10, y + 8);
  gfx->print(label);

  gfx->setTextColor(WHITE);
  gfx->setTextSize(3);
  gfx->setCursor(x + 8, y + 30);
  gfx->print(value);
}

void drawFooter(const String &text) {
  gfx->drawFastHLine(12, 292, 148, DARKGREY);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(26, 302);
  gfx->print(text);
}

void drawMainScreen() {
  gfx->fillScreen(BLACK);

  drawHeader();
  drawMetricCard(8, 66, 156, 64, "PRICE", displayPrice, WHITE);
  drawMetricCard(8, 140, 156, 64, "BLOCK", displayBlock, CYAN);
  drawMetricCard(8, 214, 156, 64, "TIME", displayTime, BLUE);
  drawFooter(footerText);
}

// =====================================================
// SETUP
// =====================================================
void setup() {
  delay(200);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  pixel.begin();
  pixel.setBrightness(40);
  pixel.clear();
  pixel.show();

  if (!gfx->begin()) {
    while (1) {
      updateRainbowLED();
      delay(10);
    }
  }

  gfx->setRotation(0);
  drawMainScreen();
}

// =====================================================
// LOOP
// =====================================================
void loop() {
  updateRainbowLED();
}
