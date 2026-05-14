//STACKSWORTH_COREv1.0.6

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:

  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();

      cfg.spi_host = VSPI_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = false;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = 14;
      cfg.pin_mosi = 13;
      cfg.pin_miso = -1;
      cfg.pin_dc   = 2;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();

      cfg.pin_cs           = 15;
      cfg.pin_rst          = -1;
      cfg.pin_busy         = -1;

      cfg.panel_width      = 240;
      cfg.panel_height     = 320;
      cfg.offset_x         = 0;
      cfg.offset_y         = 0;
      cfg.offset_rotation  = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;

      cfg.readable         = false;
      cfg.invert           = false;
      cfg.rgb_order        = false;
      cfg.dlen_16bit       = false;
      cfg.bus_shared       = true;

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();

      cfg.pin_bl = 21;
      cfg.invert = false;
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting CYD Display Test...");
  
  tft.init();
  Serial.println("Display initialized");

  tft.setBrightness(255);

  tft.setRotation(1);

  tft.fillScreen(TFT_BLACK);

  // Subtle grid
  for (int x = 0; x < 320; x += 20)
  {
    tft.drawLine(x, 0, x, 240, 0x2104);
  }

  for (int y = 0; y < 240; y += 20)
  {
    tft.drawLine(0, y, 320, y, 0x2104);
  }

  // Draw logo - Orange hexagon with S
  int hexX = 25;
  int hexY = 15;
  int hexSize = 12;
  
  // Draw hexagon
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = hexX + hexSize * cos(angle1);
    int y1 = hexY + hexSize * sin(angle1);
    int x2 = hexX + hexSize * cos(angle2);
    int y2 = hexY + hexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
  }
  
  // Draw S inside hexagon
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(hexX - 6, hexY - 8);
  tft.print("S");

  // Header: STACKSWORTH CORE
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(45, 8);
  tft.print("STACKSWORTH");
  
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(185, 8);
  tft.print("CORE");

  // Main price card (moved up closer to header)
  tft.fillRoundRect(10, 35, 220, 105, 12, TFT_BLACK);

  // Cyan accent border (bottom + right)
  tft.drawLine(10, 140, 230, 140, TFT_CYAN);
  tft.drawLine(230, 35, 230, 140, TFT_CYAN);

  // Labels
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);

  tft.setCursor(20, 43);
  tft.print("BITCOIN PRICE");

  // Price (larger area for 6 digits)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);

  tft.setCursor(20, 75);
  tft.print("$67,842");

  // 24h change percentage
  tft.setTextColor(TFT_GREEN);  // Will be green or red based on API
  tft.setTextSize(2);
  tft.setCursor(20, 115);
  tft.print("+2.34%");

  // Right metrics - BLOCK, MINER, FEE stacked
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);

  // BLOCK (moved up closer to top)
  tft.setCursor(245, 38);
  tft.print("BLOCK");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(245, 58);
  tft.print("842471");

  // MINER (more space below BLOCK)
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(245, 93);
  tft.print("MINER");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(245, 113);
  tft.print("FoundryUSA");

  // FEE (more space below MINER)
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(245, 133);
  tft.print("FEE");
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(245, 153);
  tft.print("23");
  tft.setTextSize(1);
  tft.setCursor(270, 157);
  tft.print("sat/vB");

  // Date and Time display (between cyan border and orange bottom bar)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.print("Mon, May 5  8:42PM");

  // Bottom data bar - 4 sections with orange border (closer to footer)
  int barY = 175;
  int barHeight = 48;
  int section1Width = 91;
  int section2Width = 91;
  int section3Width = 91;
  int section4Width = 47;
  
  // Draw orange border rectangle
  tft.drawRect(0, barY, 320, barHeight, TFT_ORANGE);
  
  // Draw vertical dividers
  tft.drawLine(section1Width, barY, section1Width, barY + barHeight, TFT_ORANGE);
  tft.drawLine(section1Width + section2Width, barY, section1Width + section2Width, barY + barHeight, TFT_ORANGE);
  tft.drawLine(section1Width + section2Width + section3Width, barY, section1Width + section2Width + section3Width, barY + barHeight, TFT_ORANGE);

  // Section 1: MARKET CAP
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(5, barY + 5);
  tft.print("MARKET CAP");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(5, barY + 22);
  tft.print("$1.32T");

  // Section 2: SATS/USD
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(section1Width + 5, barY + 5);
  tft.print("SATS/USD");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(section1Width + 5, barY + 22);
  tft.print("1473");

  // Section 3: SUPPLY
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(section1Width + section2Width + 5, barY + 5);
  tft.print("SUPPLY");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(section1Width + section2Width + 5, barY + 22);
  tft.print("19.8M");

  // Section 4: LIVE indicator (smaller box)
  int liveX = section1Width + section2Width + section3Width;
  tft.setTextColor(TFT_CYAN);  // Cyan when connected, will be red when not
  tft.setTextSize(1);
  tft.setCursor(liveX + 8, barY + 5);
  tft.print("LIVE");
  
  // Draw cyan pulse circle (more space below text)
  tft.fillCircle(liveX + 24, barY + 32, 5, TFT_CYAN);

  // Footer
  tft.setTextColor(0x528A);  // Dim gray
  tft.setTextSize(1);
  tft.setCursor(90, 228);
  tft.print("Built By STACKSWORTH");
}

void loop()
{
}
