#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

#define SW_BLACK   0x0000
#define SW_WHITE   0xFFFF
#define SW_ORANGE  0xFD20
#define SW_CYAN    0x07FF
#define SW_DARK    0x0841
#define SW_PANEL   0x1082

void drawCard(int x, int y, int w, int h, const char* title, const char* value) {
  tft.fillRoundRect(x, y, w, h, 10, SW_PANEL);
  tft.drawRoundRect(x, y, w, h, 10, SW_ORANGE);

  tft.setTextColor(SW_CYAN, SW_PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(title, x + 10, y + 8, 2);

  tft.setTextColor(SW_WHITE, SW_PANEL);
  tft.drawString(value, x + 10, y + 32, 4);
}

void drawCoreUI() {
  tft.fillScreen(SW_BLACK);

  // Header
  tft.fillRoundRect(8, 8, 224, 46, 10, SW_PANEL);
  tft.drawRoundRect(8, 8, 224, 46, 10, SW_ORANGE);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SW_ORANGE, SW_PANEL);
  tft.drawString("STACKSWORTH", 120, 21, 2);

  tft.setTextColor(SW_WHITE, SW_PANEL);
  tft.drawString("CORE", 120, 41, 4);

  // Main price block
  tft.fillRoundRect(8, 62, 224, 72, 12, SW_PANEL);
  tft.drawRoundRect(8, 62, 224, 72, 12, SW_CYAN);

  tft.setTextColor(SW_CYAN, SW_PANEL);
  tft.drawString("BITCOIN PRICE", 120, 78, 2);

  tft.setTextColor(SW_WHITE, SW_PANEL);
  tft.drawString("$108,420", 120, 111, 4);

  // Metric cards
  drawCard(8, 144, 106, 64, "BLOCK", "893179");
  drawCard(126, 144, 106, 64, "FEE", "12 sat/vB");

  drawCard(8, 218, 106, 64, "MINER", "Foundry");
  drawCard(126, 218, 106, 64, "SATS/USD", "923");

  // Footer
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(SW_ORANGE, SW_BLACK);
  tft.drawString("Bitcoin's Pulse at a Glance", 120, 306, 2);
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);

  tft.init();
  tft.setRotation(0);   // portrait: 240 x 320
  tft.setTextWrap(false);

  drawCoreUI();

  Serial.println("STACKSWORTH CORE display test loaded.");
}

void loop() {
}
