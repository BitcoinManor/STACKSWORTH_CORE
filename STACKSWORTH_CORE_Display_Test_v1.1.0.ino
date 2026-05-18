//STACKSWORTH_CORE_v1.1.0
//May 17,2026

#include <LovyanGFX.hpp>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <FS.h>
#include <SPIFFS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <Update.h>
#include "esp_wifi.h"
#include <DNSServer.h>

DNSServer dnsServer;


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

// 🌍 API Endpoints & Configuration
const char* FIRMWARE_VERSION = "v1.1.0";
const char* SATONAK_BASE = "https://satonak.bitcoinmanor.com";
const char* SATONAK_PRICE = "/api/price";
const char* SATONAK_HEIGHT = "/api/height";
const char* SATONAK_MINER = "/api/miner";
const char* SATONAK_FEE = "/api/fee";
const char* SATONAK_CHANGE24H = "/api/change24h";
const char* SATONAK_MARKETCAP = "/api/marketcap";
const char* SATONAK_SUPPLY = "/api/supply";
const char* UPDATE_URL = "https://satonak.bitcoinmanor.com/firmware/stacksworth-core.bin";
const char* VERSION_CHECK_URL = "https://satonak.bitcoinmanor.com/api/version";

// 📊 Global Data Variables
int btcPrice = 0;
int blockHeight = 0;
int feeRate = 0;
int satsPerDollar = 0;
float btcChange24h = 0.0;
String minerName = "Unknown";
String marketCap = "Unknown";
String circSupply = "Unknown";

// ⚙️ Saved Settings
Preferences prefs;
AsyncWebServer server(80);
String savedSSID = "";
String savedPassword = "";
String savedCity = "";
String savedCurrency = "USD";
String savedTempUnit = "F";
String savedDeviceName = "";
uint8_t savedBrightness = 128;
int savedTimezone = 10;  // Default Pacific
bool displayEnabled[12] = {true, true, false, true, false, false, false, false, true, false, true, true}; // Default metrics

// 🔄 Fetch Intervals (milliseconds)
const unsigned long INTERVAL_PRICE = 5UL * 60UL * 1000UL;      // 5 min
const unsigned long INTERVAL_HEIGHT = 2UL * 60UL * 1000UL;     // 2 min
const unsigned long INTERVAL_MINER = 6UL * 60UL * 1000UL;      // 6 min
const unsigned long INTERVAL_FEE = 9UL * 60UL * 1000UL;        // 9 min
const unsigned long INTERVAL_CHANGE24H = 8UL * 60UL * 1000UL;  // 8 min

unsigned long lastPriceFetch = 0;
unsigned long lastHeightFetch = 0;
unsigned long lastMinerFetch = 0;
unsigned long lastFeeFetch = 0;
unsigned long lastChange24hFetch = 0;

// 🆔 Device MAC ID
String macID = "";

// 🌐 AP Mode flags
bool apMode = false;
bool apMsgShown = false;

// Get short MAC address for device identification
String getShortMAC() {
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
  char shortID[7];
  sprintf(shortID, "%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(shortID);
}

// Get currency symbol based on saved currency
String getCurrencySymbol() {
  if (savedCurrency == "USD") return "$";
  if (savedCurrency == "CAD") return "$";
  if (savedCurrency == "EUR") return "€";
  if (savedCurrency == "GBP") return "£";
  if (savedCurrency == "JPY") return "¥";
  if (savedCurrency == "AUD") return "$";
  if (savedCurrency == "CHF") return "Fr";
  if (savedCurrency == "CNY") return "¥";
  if (savedCurrency == "SEK") return "kr";
  if (savedCurrency == "NOK") return "kr";
  return "$";
}

// Format number with commas (e.g., 67842 -> "67,842")
String formatWithCommas(int number) {
  String numStr = String(number);
  String result = "";
  int len = numStr.length();
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0) {
      result += ",";
    }
    result += numStr[i];
  }
  return result;
}

const char* ssid = "SM-S918W0853";
const char* password = "MySamsungPhone!!!";

bool wifiConnected = false;
bool initialSetupDone = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 5000; // Check every 5 seconds

// ═══════════════════════════════════════════════════════════════════════════
// 🌐 SATONAK API FETCH FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// 💰 Fetch Bitcoin Price from SATONAK
bool fetchPriceFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_PRICE) + "?fiat=" + savedCurrency;
  
  Serial.print("📊 Fetching price from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Price response: " + payload);
    
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      btcPrice = doc["price"].as<int>();
      if (btcPrice > 0) {
        satsPerDollar = (int)(100000000.0 / btcPrice);
        Serial.printf("💰 Price: %s%d | Sats/$: %d\n", getCurrencySymbol().c_str(), btcPrice, satsPerDollar);
        http.end();
        return true;
      }
    } else {
      Serial.println("❌ JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// 📦 Fetch Block Height from SATONAK
bool fetchHeightFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_HEIGHT);
  
  Serial.print("📦 Fetching height from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Height response: " + payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      blockHeight = doc["height"].as<int>();
      if (blockHeight > 0) {
        Serial.printf("📦 Block Height: %d\n", blockHeight);
        http.end();
        return true;
      }
    } else {
      Serial.println("❌ JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ⛏️ Fetch Miner Name from SATONAK
bool fetchMinerFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_MINER);
  
  Serial.print("⛏️ Fetching miner from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Miner response: " + payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      String miner = doc["miner"].as<String>();
      if (miner.length() > 0) {
        minerName = miner;
        Serial.println("⛏️ Miner: " + minerName);
        http.end();
        return true;
      }
    } else {
      Serial.println("❌ JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ⚡ Fetch Fee Rate from SATONAK
bool fetchFeeFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_FEE);
  
  Serial.print("⚡ Fetching fee from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Fee response: " + payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      feeRate = doc["fee"].as<int>();
      if (feeRate > 0) {
        Serial.printf("⚡ Fee: %d sat/vB\n", feeRate);
        http.end();
        return true;
      }
    } else {
      Serial.println("❌ JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// 📈 Fetch 24H Change from SATONAK
bool fetchChange24hFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_CHANGE24H) + "?fiat=" + savedCurrency;
  
  Serial.print("📈 Fetching 24h change from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Change response: " + payload);
    
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error) {
      btcChange24h = doc["change24h"].as<float>();
      Serial.printf("📈 24H Change: %.2f%%\n", btcChange24h);
      http.end();
      return true;
    } else {
      Serial.println("❌ JSON parse error: " + String(error.c_str()));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// 🔄 OTA UPDATE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Check for firmware updates
String checkForUpdates() {
  if (WiFi.status() != WL_CONNECTED) return "";
  
  HTTPClient http;
  http.begin(VERSION_CHECK_URL);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String latestVersion = http.getString();
    latestVersion.trim();
    http.end();
    
    Serial.println("📡 Latest version: " + latestVersion);
    Serial.println("📡 Current version: " + String(FIRMWARE_VERSION));
    
    return latestVersion;
  }
  
  http.end();
  return "";
}

// Perform OTA firmware update
bool performOTAUpdate() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  Serial.println("🔄 Starting OTA update...");
  
  HTTPClient http;
  http.begin(UPDATE_URL);
  http.setTimeout(30000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    int contentLength = http.getSize();
    Serial.printf("📦 Firmware size: %d bytes\n", contentLength);
    
    if (contentLength > 0) {
      bool canBegin = Update.begin(contentLength);
      
      if (canBegin) {
        WiFiClient* stream = http.getStreamPtr();
        size_t written = Update.writeStream(*stream);
        
        if (written == contentLength) {
          Serial.println("✅ Firmware written successfully");
        } else {
          Serial.printf("❌ Only wrote %d of %d bytes\n", written, contentLength);
        }
        
        if (Update.end()) {
          if (Update.isFinished()) {
            Serial.println("✅ Update complete. Rebooting...");
            http.end();
            delay(1000);
            ESP.restart();
            return true;
          } else {
            Serial.println("❌ Update not finished");
          }
        } else {
          Serial.printf("❌ Update error: %s\n", Update.errorString());
        }
      } else {
        Serial.println("❌ Not enough space for update");
      }
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// 📺 DISPLAY UPDATE FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

// Update Bitcoin price display
void updatePriceDisplay() {
  // Clear price area
  tft.fillRect(20, 75, 200, 32, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(4);
  tft.setCursor(20, 75);
  
  String priceStr = getCurrencySymbol() + formatWithCommas(btcPrice);
  tft.print(priceStr);
  
  Serial.println("💰 Updated price display: " + priceStr);
}

// Update 24h change display
void updateChange24hDisplay() {
  // Clear change area
  tft.fillRect(20, 115, 200, 20, TFT_BLACK);
  
  // Set color based on positive/negative
  if (btcChange24h >= 0) {
    tft.setTextColor(TFT_GREEN);
  } else {
    tft.setTextColor(TFT_RED);
  }
  
  tft.setTextSize(2);
  tft.setCursor(20, 115);
  
  String changeStr = (btcChange24h >= 0 ? "+" : "") + String(btcChange24h, 2) + "%";
  tft.print(changeStr);
  
  Serial.println("📈 Updated change display: " + changeStr);
}

// Update block height display
void updateBlockHeightDisplay() {
  // Clear block area
  tft.fillRect(245, 58, 70, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(245, 58);
  tft.print(String(blockHeight));
  
  Serial.println("📦 Updated block height: " + String(blockHeight));
}

// Update miner name display
void updateMinerDisplay() {
  // Clear miner area
  tft.fillRect(245, 113, 70, 10, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(245, 113);
  
  // Truncate miner name if too long
  String displayMiner = minerName;
  if (displayMiner.length() > 10) {
    displayMiner = displayMiner.substring(0, 10);
  }
  tft.print(displayMiner);
  
  Serial.println("⛏️ Updated miner: " + displayMiner);
}

// Update fee rate display
void updateFeeDisplay() {
  // Clear fee area
  tft.fillRect(245, 153, 25, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(245, 153);
  tft.print(String(feeRate));
  
  Serial.println("⚡ Updated fee: " + String(feeRate) + " sat/vB");
}

// Update sats per dollar display
void updateSatsPerDollarDisplay() {
  int barY = 175;
  int section1Width = 91;
  
  // Clear sats/USD area
  tft.fillRect(section1Width + 5, barY + 22, 80, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(section1Width + 5, barY + 22);
  tft.print(String(satsPerDollar));
  
  Serial.println("💎 Updated sats/$: " + String(satsPerDollar));
}

// ═══════════════════════════════════════════════════════════════════════════
// 📺 DISPLAY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

void updateLiveIndicator()
{
  // Bottom data bar position
  int barY = 175;
  int section1Width = 91;
  int section2Width = 91;
  int section3Width = 91;
  int liveX = section1Width + section2Width + section3Width;
  
  // Clear the LIVE indicator area
  tft.fillRect(liveX + 1, barY + 1, 46, 46, TFT_BLACK);
  
  if (wifiConnected)
  {
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setCursor(liveX + 8, barY + 5);
    tft.print("LIVE");
    tft.fillCircle(liveX + 24, barY + 32, 5, TFT_CYAN);
  }
  else
  {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(liveX + 2, barY + 5);
    tft.print("OFFLINE");
    tft.fillCircle(liveX + 24, barY + 32, 5, TFT_RED);
  }
}

void showConnectingAnimation(int dotCount)
{
  // Clear the connecting message area
  tft.fillRect(60, 160, 200, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 160);
  tft.print("Connecting");
  
  // Animated dots
  for (int i = 0; i < dotCount; i++)
  {
    tft.print(".");
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 🌐 WiFi CONNECTION & AP MODE
// ═══════════════════════════════════════════════════════════════════════════

// Start Access Point mode for WiFi configuration
void startAccessPoint() {
  Serial.println("🚀 Starting Access Point mode...");
  
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.disconnect(true);
  WiFi.setAutoReconnect(false);
  delay(100);
  WiFi.mode(WIFI_AP);
  
  macID = getShortMAC();
  String apSSID = "SW-CORE-" + macID;
  WiFi.softAP(apSSID.c_str());
  
  apMode = true;
  apMsgShown = false;
  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("🌍 AP IP address: ");
  Serial.println(myIP);
  Serial.print("📶 AP SSID: ");
  Serial.println(apSSID);
  
  // Start DNS server for captive portal
  dnsServer.start(53, "*", myIP);
  Serial.println("🚀 DNS Server started for captive portal");
  
  // Show AP mode message on display
  tft.fillScreen(TFT_BLACK);
  
  // Draw hexagon logo at top center
  int logoHexX = 160;  // Center of screen (320/2)
  int logoHexY = 30;
  int logoHexSize = 18;
  
  // Draw hexagon
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = logoHexX + logoHexSize * cos(angle1);
    int y1 = logoHexY + logoHexSize * sin(angle1);
    int x2 = logoHexX + logoHexSize * cos(angle2);
    int y2 = logoHexY + logoHexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
    tft.drawLine(x1+1, y1+1, x2+1, y2+1, TFT_ORANGE); // Thicker lines
  }
  
  // Draw S inside hexagon
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(3);
  tft.setCursor(logoHexX - 9, logoHexY - 12);
  tft.print("S");
  
  // STACKSWORTH CORE below logo
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(50, 60);
  tft.print("STACKSWORTH");
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(185, 60);
  tft.print("CORE");
  
  // SETUP MODE heading
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(3);
  tft.setCursor(60, 90);
  tft.print("SETUP MODE");
  
  // Instructions
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(20, 125);
  tft.print("Connect to WiFi:");
  
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(60, 150);
  tft.print(apSSID);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(25, 180);
  tft.print("Portal will auto-open.");
  tft.setCursor(25, 193);
  tft.print("If not, please visit:");
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(65, 208);
  tft.print(myIP.toString());
  
  // Footer
  tft.setTextColor(0x528A);  // Dim gray
  tft.setTextSize(1);
  tft.setCursor(50, 228);
  tft.print("Built by Bitcoin Manor 2026");
}

void connectWiFi()
{
  WiFi.mode(WIFI_STA);
  
  // Use saved credentials if available, otherwise use hardcoded defaults
  String connectSSID = (savedSSID.length() > 0) ? savedSSID : String(ssid);
  String connectPassword = (savedPassword.length() > 0) ? savedPassword : String(password);
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(connectSSID);
  
  WiFi.begin(connectSSID.c_str(), connectPassword.c_str());

  Serial.print("Connecting");

  unsigned long startAttempt = millis();
  int dotCount = 0;

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 20000)
  {
    Serial.print(".");
    
    // Update animation every 500ms
    showConnectingAnimation(dotCount % 4);
    dotCount++;
    
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    wifiConnected = true;

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    // Clear connecting message
    tft.fillRect(60, 160, 200, 20, TFT_BLACK);
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.setCursor(80, 160);
    tft.print("Connected!");
    delay(1000);
    
    // Update indicator if main UI is already shown
    if (initialSetupDone)
    {
      updateLiveIndicator();
    }

    if (MDNS.begin("core"))
    {
      Serial.println("mDNS started: http://core.local");
      MDNS.addService("http", "tcp", 80);
    }
    else
    {
      Serial.println("mDNS failed");
    }
  }
  else
  {
    wifiConnected = false;
    Serial.println();
    Serial.println("WiFi connection failed - starting AP mode");
    
    // Connection failed - start AP mode
    startAccessPoint();
  }
}

// ═══════════════════════════════════════════════════════════════════════════
// 🌐 WEB SERVER FUNCTIONS & ROUTES
// ═══════════════════════════════════════════════════════════════════════════

// Initialize web server routes
void setupWebServer() {
  // Serve main portal page (compressed)
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/core.html.gz", "text/html");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  
  // Device info endpoint
  server.on("/deviceinfo", HTTP_GET, [](AsyncWebServerRequest *request) {
    String json = "{\"macid\":\"" + macID + "\"}";
    request->send(200, "application/json", json);
  });
  
  // Identify device (blink display)
  server.on("/identify", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("🔍 Device identification requested");
    
    // Blink the display 3 times
    for (int i = 0; i < 3; i++) {
      tft.setBrightness(255);
      delay(300);
      tft.setBrightness(50);
      delay(300);
    }
    tft.setBrightness(savedBrightness);
    
    request->send(200, "text/plain", "OK");
  });
  
  // Check for firmware updates
  server.on("/checkupdate", HTTP_GET, [](AsyncWebServerRequest *request) {
    String latestVersion = checkForUpdates();
    
    if (latestVersion.length() > 0 && latestVersion != FIRMWARE_VERSION) {
      String json = "{\"updateAvailable\":true,\"latestVersion\":\"" + latestVersion + "\"}";
      request->send(200, "application/json", json);
    } else {
      String json = "{\"updateAvailable\":false}";
      request->send(200, "application/json", json);
    }
  });
  
  // Perform OTA update
  server.on("/doupdate", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Update starting...");
    
    // Perform update in a separate task to avoid blocking
    delay(1000);
    performOTAUpdate();
  });
  
  // Save settings and reboot
  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("💾 Saving settings from web portal...");
    
    // Extract all form data
    if (request->hasParam("ssid", true)) {
      savedSSID = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("password", true)) {
      savedPassword = request->getParam("password", true)->value();
    }
    if (request->hasParam("city", true)) {
      savedCity = request->getParam("city", true)->value();
    }
    if (request->hasParam("timezone", true)) {
      savedTimezone = request->getParam("timezone", true)->value().toInt();
    }
    if (request->hasParam("currency", true)) {
      savedCurrency = request->getParam("currency", true)->value();
    }
    if (request->hasParam("tempunit", true)) {
      savedTempUnit = request->getParam("tempunit", true)->value();
    }
    if (request->hasParam("devicename", true)) {
      savedDeviceName = request->getParam("devicename", true)->value();
    }
    if (request->hasParam("brightness", true)) {
      savedBrightness = request->getParam("brightness", true)->value().toInt();
    }
    
    // Save display options
    for (int i = 0; i < 12; i++) {
      String paramName = "show" + String(i);
      if (request->hasParam(paramName, true)) {
        displayEnabled[i] = (request->getParam(paramName, true)->value() == "1");
      }
    }
    
    // Save to preferences
    prefs.begin("stacksworth", false);
    prefs.putString("ssid", savedSSID);
    prefs.putString("password", savedPassword);
    prefs.putString("city", savedCity);
    prefs.putInt("timezone", savedTimezone);
    prefs.putString("currency", savedCurrency);
    prefs.putString("tempunit", savedTempUnit);
    prefs.putString("devicename", savedDeviceName);
    prefs.putUChar("brightness", savedBrightness);
    
    // Save display options
    for (int i = 0; i < 12; i++) {
      String key = "show" + String(i);
      prefs.putBool(key.c_str(), displayEnabled[i]);
    }
    
    prefs.end();
    
    Serial.println("✅ Settings saved!");
    Serial.println("🔄 Rebooting to apply new WiFi settings...");
    
    request->send(200, "text/plain", "Settings saved. Rebooting...");
    
    delay(1000);
    ESP.restart();
  });
  
  // Catch-all route for captive portal (redirect all to root)
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (apMode) {
      // In AP mode, redirect everything to the portal
      AsyncWebServerResponse *response = request->beginResponse(SPIFFS, "/core.html.gz", "text/html");
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
    } else {
      request->send(404, "text/plain", "Not found");
    }
  });
  
  server.begin();
  Serial.println("✅ Web server started on port 80");
}

// Load saved settings from preferences
void loadSavedSettings() {
  prefs.begin("stacksworth", true);  // read-only
  
  savedSSID = prefs.getString("ssid", "");
  savedPassword = prefs.getString("password", "");
  savedCity = prefs.getString("city", "");
  savedTimezone = prefs.getInt("timezone", 10);  // Default Pacific
  savedCurrency = prefs.getString("currency", "USD");
  savedTempUnit = prefs.getString("tempunit", "F");
  savedDeviceName = prefs.getString("devicename", "");
  savedBrightness = prefs.getUChar("brightness", 128);
  
  // Load display options
  for (int i = 0; i < 12; i++) {
    String key = "show" + String(i);
    displayEnabled[i] = prefs.getBool(key.c_str(), (i == 0 || i == 1 || i == 3 || i == 8 || i == 10 || i == 11));
  }
  
  prefs.end();
  
  Serial.println("📂 Loaded settings:");
  Serial.println("  SSID: " + savedSSID);
  Serial.println("  City: " + savedCity);
  Serial.println("  Currency: " + savedCurrency);
  Serial.println("  Device Name: " + savedDeviceName);
  Serial.printf("  Brightness: %d\n", savedBrightness);
}

//SETUP

void setup()
{
  Serial.begin(115200);
  Serial.println("🚀 Starting STACKSWORTH CORE v1.1.0...");
  
  // 🆔 Get device MAC ID
  WiFi.mode(WIFI_STA);
  macID = getShortMAC();
  Serial.println("🆔 Device MAC ID: " + macID);
  
  // 📂 Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ SPIFFS mount failed! Formatting...");
    SPIFFS.format();
    if (!SPIFFS.begin(true)) {
      Serial.println("❌ SPIFFS format failed!");
    } else {
      Serial.println("✅ SPIFFS formatted and mounted");
    }
  } else {
    Serial.println("✅ SPIFFS mounted");
  }
  
  // 💾 Load saved settings
  loadSavedSettings();
  
  // Initialize display FIRST
  tft.init();
  Serial.println("📺 Display initialized");

  tft.setBrightness(savedBrightness);
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  
  // Draw splash screen - hex logo and text on same line
  int splashY = 100;
  int hexX = 30;
  int hexY = splashY;
  int hexSize = 18;
  
  // Draw hexagon
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = hexX + hexSize * cos(angle1);
    int y1 = hexY + hexSize * sin(angle1);
    int x2 = hexX + hexSize * cos(angle2);
    int y2 = hexY + hexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
    tft.drawLine(x1+1, y1+1, x2+1, y2+1, TFT_ORANGE); // Thicker lines
  }
  
  // Draw S inside hexagon
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(3);
  tft.setCursor(hexX - 9, hexY - 12);
  tft.print("S");
  
  // STACKSWORTH on same line as logo
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  tft.setCursor(60, splashY - 12);
  tft.print("STACKSWORTH");
  
  // CORE below on its own line
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(240, splashY + 18);
  tft.print("CORE");
  
  delay(1500);  // Show splash for 1.5 seconds
  
  // Show "CHECKING HARDWARE" loading screen
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(30, 100);
  tft.print("CHECKING HARDWARE");
  
  // Animated loading dots
  tft.setTextColor(TFT_CYAN);
  for (int i = 0; i < 3; i++) {
    tft.setCursor(95 + (i * 40), 130);
    tft.print(".");
    delay(300);
  }
  
  delay(500);
  
  // 🌐 Smart WiFi Connection Logic (matches MATRIX behavior)
  if (savedSSID.length() > 0) {
    // Has saved credentials - try to connect
    Serial.println("📡 Found saved WiFi credentials, attempting connection...");
    connectWiFi();
  } else {
    // No saved credentials - go directly to AP mode
    Serial.println("⚠️ No saved WiFi credentials found, starting Access Point...");
    startAccessPoint();
  }
  
  // 🌐 Start web server (works in both STA and AP mode)
  setupWebServer();
  if (wifiConnected) {
    Serial.println("🌐 Web portal available at: http://core.local");
  } else if (apMode) {
    Serial.println("🌐 Web portal available at: " + WiFi.softAPIP().toString());
  }
  
  // Only draw main UI if NOT in AP mode
  if (!apMode) {
    // Clear screen and draw full UI
    tft.fillScreen(TFT_BLACK);

    // Subtle grid background
    for (int x = 0; x < 320; x += 20)
    {
      tft.drawLine(x, 0, x, 240, 0x2104);
    }

    for (int y = 0; y < 240; y += 20)
    {
      tft.drawLine(0, y, 320, y, 0x2104);
    }

  // Draw logo - Orange hexagon with S
  int logoHexX = 25;
  int logoHexY = 15;
  int logoHexSize = 12;
  
  // Draw hexagon
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = logoHexX + logoHexSize * cos(angle1);
    int y1 = logoHexY + logoHexSize * sin(angle1);
    int x2 = logoHexX + logoHexSize * cos(angle2);
    int y2 = logoHexY + logoHexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
  }
  
  // Draw S inside hexagon
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(logoHexX - 6, logoHexY - 8);
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

  // Cyan accent border (bottom + right side)
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

  // Section 4: LIVE indicator (smaller box) - show WiFi status
  int liveX = section1Width + section2Width + section3Width;
  
  // Initial indicator (will update dynamically in loop)
  updateLiveIndicator();

  // Footer
  tft.setTextColor(0x528A);  // Dim gray
  tft.setTextSize(1);
  tft.setCursor(90, 228);
  tft.print("Built By STACKSWORTH");
  
  // 🚀 Initial data fetch if WiFi is connected
  if (wifiConnected) {
    Serial.println("🔄 Fetching initial Bitcoin data...");
    
    // Fetch all data on startup with small delays between calls
    if (fetchPriceFromSatonak()) {
      updatePriceDisplay();
      updateSatsPerDollarDisplay();
      lastPriceFetch = millis();
    }
    delay(500);  // Small delay between API calls
    
    if (fetchHeightFromSatonak()) {
      updateBlockHeightDisplay();
      lastHeightFetch = millis();
    }
    delay(500);
    
    if (fetchMinerFromSatonak()) {
      updateMinerDisplay();
      lastMinerFetch = millis();
    }
    delay(500);
    
    if (fetchFeeFromSatonak()) {
      updateFeeDisplay();
      lastFeeFetch = millis();
    }
    delay(500);
    
    if (fetchChange24hFromSatonak()) {
      updateChange24hDisplay();
      lastChange24hFetch = millis();
    }
    
    Serial.println("✅ Initial data fetch complete!");
  } else {
    Serial.println("⚠️ No WiFi - showing placeholder data");
  }
  
  } // End of if (!apMode) block
  
  initialSetupDone = true;
}

void loop()
{
  // 📡 Process DNS requests when in AP mode (for captive portal)
  if (apMode) {
    dnsServer.processNextRequest();
  }
  
  // Check WiFi status periodically and reconnect if needed
  if (millis() - lastWiFiCheck >= wifiCheckInterval)
  {
    lastWiFiCheck = millis();
    
    // Skip WiFi checks if in AP mode
    if (apMode) {
      return;
    }
    
    bool wasConnected = wifiConnected;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    
    // If status changed, update the indicator
    if (wasConnected != wifiConnected)
    {
      Serial.print("WiFi status changed: ");
      Serial.println(wifiConnected ? "Connected" : "Disconnected");
      updateLiveIndicator();
    }
    
    // Try to reconnect if disconnected
    if (!wifiConnected)
    {
      Serial.println("Attempting WiFi reconnection...");
      WiFi.disconnect();
      
      // Use saved credentials
      String connectSSID = (savedSSID.length() > 0) ? savedSSID : String(ssid);
      String connectPassword = (savedPassword.length() > 0) ? savedPassword : String(password);
      WiFi.begin(connectSSID.c_str(), connectPassword.c_str());
      
      // Wait up to 5 seconds for connection
      unsigned long startAttempt = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 5000)
      {
        delay(100);
      }
      
      if (WiFi.status() == WL_CONNECTED)
      {
        wifiConnected = true;
        Serial.println("Reconnected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        updateLiveIndicator();
      }
    }
  }
  
  // 🌐 Staggered API data fetching (only if WiFi connected)
  if (wifiConnected) {
    unsigned long now = millis();
    
    // Fetch price every 5 minutes
    if (now - lastPriceFetch >= INTERVAL_PRICE) {
      if (fetchPriceFromSatonak()) {
        lastPriceFetch = now;
        updatePriceDisplay();
        updateSatsPerDollarDisplay();
      }
    }
    
    // Fetch block height every 2 minutes
    if (now - lastHeightFetch >= INTERVAL_HEIGHT) {
      if (fetchHeightFromSatonak()) {
        lastHeightFetch = now;
        updateBlockHeightDisplay();
      }
    }
    
    // Fetch miner every 6 minutes
    if (now - lastMinerFetch >= INTERVAL_MINER) {
      if (fetchMinerFromSatonak()) {
        lastMinerFetch = now;
        updateMinerDisplay();
      }
    }
    
    // Fetch fee every 9 minutes
    if (now - lastFeeFetch >= INTERVAL_FEE) {
      if (fetchFeeFromSatonak()) {
        lastFeeFetch = now;
        updateFeeDisplay();
      }
    }
    
    // Fetch 24h change every 8 minutes
    if (now - lastChange24hFetch >= INTERVAL_CHANGE24H) {
      if (fetchChange24hFromSatonak()) {
        lastChange24hFetch = now;
        updateChange24hDisplay();
      }
    }
  }
}
