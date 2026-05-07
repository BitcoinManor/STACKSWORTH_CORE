//STACKSWORTH_CORE_v1.0.1

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

  // Main price card (wider, more to the left)
  tft.fillRoundRect(10, 50, 220, 120, 12, TFT_BLACK);

  // Cyan accent border (bottom + right)
  tft.drawLine(10, 170, 230, 170, TFT_CYAN);
  tft.drawLine(230, 50, 230, 170, TFT_CYAN);

  // Labels
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);

  tft.setCursor(20, 60);
  tft.print("BITCOIN PRICE");

  // Price (larger area for 6 digits)
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);

  tft.setCursor(20, 95);
  tft.print("$67,842");

  // 24h change percentage
  tft.setTextColor(TFT_GREEN);  // Will be green or red based on API
  tft.setTextSize(2);
  tft.setCursor(20, 140);
  tft.print("+2.34%");

  // Right metrics
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);

  tft.setCursor(245, 55);
  tft.print("BLOCK");

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(245, 80);
  tft.print("842471");

  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(245, 125);
  tft.print("FEE");

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(245, 150);
  tft.print("23");
  tft.setTextSize(1);
  tft.setCursor(245, 165);
  tft.print("sat/vB");
}

void loop()
{
}
