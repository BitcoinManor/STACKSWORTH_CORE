//STACKSWORTH_CORE_v2.0.4-timefocus
//June 17, 2026
//Fixed: Time Focus spacing, larger weather condition, removed degree symbol hatchbox issue

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
#include <time.h>

DNSServer dnsServer;


class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;
  lgfx::Touch_XPT2046 _touch_instance;

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
      cfg.pin_miso = 12;
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

    {
      auto cfg = _touch_instance.config();

      // XPT2046 touch controller raw calibration values.
      // These are raw ADC values, not screen pixel dimensions.
      cfg.x_min      = 280;
      cfg.x_max      = 3860;
      cfg.y_min      = 340;
      cfg.y_max      = 3860;

      cfg.pin_int    = 36;     // TOUCH_IRQ
      cfg.bus_shared = false;  // Touch is NOT on the TFT SPI pins on this board
      cfg.offset_rotation = 1; // Match display rotation (landscape)

      // STACKSWORTH CORE / CYD 2.8 touch pins
      cfg.spi_host = HSPI_HOST;
      cfg.freq = 1000000;
      cfg.pin_sclk = 25;  // TOUCH_CLK
      cfg.pin_mosi = 32;  // TOUCH_DIN / MOSI
      cfg.pin_miso = 39;  // TOUCH_DO / MISO
      cfg.pin_cs   = 33;  // TOUCH_CS

      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};

LGFX tft;

// 🌍 API Endpoints & Configuration
const char* FIRMWARE_VERSION = "v2.0.4-timefocus";
const char* SATONAK_BASE = "https://satonak.bitcoinmanor.com";
const char* SATONAK_PRICE = "/api/price";
const char* SATONAK_HEIGHT = "/api/height";
const char* SATONAK_MINER = "/api/miner";
const char* SATONAK_FEE = "/api/fee";
const char* SATONAK_CHANGE24H = "/api/change24h";
const char* SATONAK_MARKETCAP = "/api/marketcap";
const char* SATONAK_SUPPLY = "/api/circsupply";
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
float latitude = 0.0;
float longitude = 0.0;
int temperature = 0;
String weatherCondition = "Unknown";
bool weatherValid = false;

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
int savedTimezone = 11;  // Portal timezone index. Default Mountain (Calgary)
bool displayEnabled[12] = {true, true, false, true, false, false, false, false, true, false, true, true}; // Default metrics

// 🔄 Fetch Intervals (milliseconds) - Prioritized for importance
const unsigned long INTERVAL_HEIGHT = 2UL * 60UL * 1000UL;     // 2 min (PRIORITY 1: Most important)
const unsigned long INTERVAL_MINER = 2UL * 60UL * 1000UL;      // 2 min (PRIORITY 1: Tied to block)
const unsigned long INTERVAL_PRICE = 5UL * 60UL * 1000UL;      // 5 min (PRIORITY 2)
const unsigned long INTERVAL_FEE = 10UL * 60UL * 1000UL;       // 10 min
const unsigned long INTERVAL_CHANGE24H = 30UL * 60UL * 1000UL; // 30 min (rate limited)
const unsigned long INTERVAL_WEATHER = 30UL * 60UL * 1000UL;   // 30 min, same as Matrix
const unsigned long INTERVAL_TIME = 60UL * 1000UL;             // 1 min (internal clock)
const unsigned long INTERVAL_NTP_SYNC = 30UL * 60UL * 1000UL;  // 30 min (prevent drift)

unsigned long lastPriceFetch = 0;
unsigned long lastHeightFetch = 0;
unsigned long lastMinerFetch = 0;
unsigned long lastFeeFetch = 0;
unsigned long lastChange24hFetch = 0;
unsigned long lastWeatherFetch = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastNtpSync = 0;

// 🆔 Device MAC ID
String macID = "";

// 🌐 AP Mode flags
bool apMode = false;
bool apMsgShown = false;

// 📱 Touch Screen Management
int currentScreen = 0;  // 0=Dashboard, 1=Block Focus, 2=Time Focus
unsigned long lastTouchTime = 0;
const unsigned long touchDebounce = 500;  // 500ms debounce
bool touchDebugMode = false;  // Set to true to see continuous touch status
bool touchWasDown = false;   // Edge-trigger touch so one press = one screen change

// 🌍 Timezone configuration (same as Matrix.ino)
const char *ntpServer = "pool.ntp.org";
const char* TIMEZONE_STRINGS[] = {
  "UTC0",                                    // UTC +0
  "GMT0BST,M3.5.0/1,M10.5.0",              // London +0/+1
  "CET-1CEST,M3.5.0,M10.5.0/3",            // Paris/Berlin +1/+2
  "EET-2EEST,M3.5.0/3,M10.5.0/4",          // Helsinki +2/+3
  "MSK-3",                                  // Moscow +3 (no DST)
  "JST-9",                                  // Tokyo +9 (no DST)
  "AEST-10AEDT,M10.1.0,M4.1.0/3",          // Sydney +10/+11
  "NZST-12NZDT,M9.5.0,M4.1.0/3",           // Auckland +12/+13
  "HST10",                                  // Hawaii -10 (no DST)
  "AKST9AKDT,M3.2.0,M11.1.0",              // Alaska -9/-8
  "PST8PDT,M3.2.0,M11.1.0",                // Pacific -8/-7
  "MST7MDT,M3.2.0,M11.1.0",                // Mountain -7/-6 (Calgary!)
  "CST6CDT,M3.2.0,M11.1.0",                // Central -6/-5
  "EST5EDT,M3.2.0,M11.1.0"                 // Eastern -5/-4
};

const char* TIMEZONE_NAMES[] = {
  "UTC (+0)", "London (+0/+1)", "Paris (+1/+2)", "Helsinki (+2/+3)",
  "Moscow (+3)", "Tokyo (+9)", "Sydney (+10/+11)", "Auckland (+12/+13)",
  "Hawaii (-10)", "Alaska (-9/-8)", "Pacific (-8/-7)", "Mountain (-7/-6)",
  "Central (-6/-5)", "Eastern (-5/-4)"
};

#define NUM_TIMEZONES (sizeof(TIMEZONE_STRINGS) / sizeof(TIMEZONE_STRINGS[0]))


String mapWeatherCode(int code)
{
  if (code == 0) return "Sunny";
  else if (code == 1) return "Mostly Sunny";
  else if (code == 2) return "Partly Cloudy";
  else if (code == 3) return "Cloudy";
  else if (code >= 45 && code <= 48) return "Foggy";
  else if (code >= 51 && code <= 57) return "Drizzle";
  else if (code >= 61 && code <= 67) return "Rain";
  else if (code >= 71 && code <= 77) return "Snowy";
  else if (code >= 80 && code <= 82) return "Showers";
  else if (code >= 85 && code <= 86) return "Snow Showers";
  else if (code >= 95 && code <= 99) return "Thunderstorm";
  return "Unknown";
}


int getTimezoneIndex() {
  // Current portal saves indexes 0-13. Older builds saved UTC offsets, so keep compatibility.
  if (savedTimezone >= 0 && savedTimezone < (int)NUM_TIMEZONES) return savedTimezone;
  if (savedTimezone == -10) return 8;
  if (savedTimezone == -9) return 9;
  if (savedTimezone == -8) return 10;
  if (savedTimezone == -7) return 11;
  if (savedTimezone == -6) return 12;
  if (savedTimezone == -5) return 13;
  return 11; // Mountain default
}

void applyTimezone() {
  int tzIndex = getTimezoneIndex();
  configTzTime(TIMEZONE_STRINGS[tzIndex], ntpServer);
  Serial.printf("🕒 Timezone configured: %s (%s)\n", TIMEZONE_NAMES[tzIndex], TIMEZONE_STRINGS[tzIndex]);
  lastNtpSync = millis();
}

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

bool wifiConnected = false;
bool initialSetupDone = false;
unsigned long lastWiFiCheck = 0;
const unsigned long wifiCheckInterval = 5000; // Check every 5 seconds

// 🕐 DST Detection Function (North American rules)
// DST starts: Second Sunday in March at 2:00 AM
// DST ends: First Sunday in November at 2:00 AM
bool isDST(int month, int day, int dayOfWeek) {
  // No DST in Jan, Feb, Dec
  if (month < 3 || month > 11) return false;
  // DST always active Apr-Oct
  if (month > 3 && month < 11) return true;
  
  // Calculate for March and November
  int previousSunday = day - dayOfWeek;
  
  // In March, DST starts on second Sunday (day 8-14)
  if (month == 3) {
    return previousSunday >= 8;
  }
  
  // In November, DST ends on first Sunday (day 1-7)
  if (month == 11) {
    return previousSunday < 1;
  }
  
  return false;
}

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
    
    // API returns raw number, not JSON
    payload.trim();
    btcPrice = (int)payload.toFloat();
    
    if (btcPrice > 0) {
      satsPerDollar = (int)(100000000.0 / btcPrice);
      Serial.printf("💰 Price: %s%d | Sats/$: %d\n", getCurrencySymbol().c_str(), btcPrice, satsPerDollar);
      http.end();
      return true;
    } else {
      Serial.println("❌ Price is 0 or invalid");
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
    
    // API returns raw number, not JSON
    payload.trim();
    blockHeight = payload.toInt();
    
    if (blockHeight > 0) {
      Serial.printf("📦 Block Height: %d\n", blockHeight);
      http.end();
      return true;
    } else {
      Serial.println("❌ Block height is 0 or invalid");
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
    
    // API returns raw text, not JSON
    payload.trim();
    if (payload.length() > 0) {
      minerName = payload;
      Serial.println("⛏️ Miner: " + minerName);
      http.end();
      return true;
    } else {
      Serial.println("❌ Miner name is empty");
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
    
    // API returns raw number, not JSON
    payload.trim();
    feeRate = payload.toInt();
    
    if (feeRate > 0) {
      Serial.printf("⚡ Fee: %d sat/vB\n", feeRate);
      http.end();
      return true;
    } else {
      Serial.println("❌ Fee rate is 0 or invalid");
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
    
    // Check if response is an error (JSON with "error" field)
    if (payload.indexOf("error") >= 0) {
      Serial.println("⚠️ API returned error (likely rate limited)");
      http.end();
      return false;
    }
    
    // API returns raw number when successful
    payload.trim();
    btcChange24h = payload.toFloat();
    
    Serial.printf("📈 24H Change: %.2f%%\n", btcChange24h);
    http.end();
    return true;
  } else if (httpCode == 429) {
    Serial.println("⚠️ Rate limited (429) - will retry later");
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// 💼 Fetch Market Cap from SATONAK
bool fetchMarketCapFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_MARKETCAP) + "?fiat=" + savedCurrency;
  
  Serial.print("💼 Fetching market cap from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Market cap response: " + payload);
    
    payload.trim();
    
    // Check if it's an error response
    if (payload.indexOf("error") >= 0 || payload == "na") {
      Serial.println("⚠️ Market cap endpoint unavailable");
      http.end();
      return false;
    }
    
    if (payload.length() > 0) {
      marketCap = payload;
      Serial.println("💼 Market Cap: " + marketCap);
      http.end();
      return true;
    }
  } else if (httpCode == 404) {
    Serial.println("⚠️ Market cap endpoint not found (404) - may not be implemented");
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}

// 📊 Fetch Circulating Supply from SATONAK
bool fetchSupplyFromSatonak() {
  if (WiFi.status() != WL_CONNECTED) return false;
  
  HTTPClient http;
  String url = String(SATONAK_BASE) + String(SATONAK_SUPPLY);
  
  Serial.print("📊 Fetching supply from SATONAK: ");
  Serial.println(url);
  
  http.begin(url);
  http.setTimeout(5000);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Supply response: " + payload);
    
    payload.trim();
    
    // Check if it's an error response
    if (payload.indexOf("error") >= 0 || payload == "na") {
      Serial.println("⚠️ Supply endpoint unavailable");
      http.end();
      return false;
    }
    
    // Remove commas for validation
    String cleanPayload = payload;
    cleanPayload.replace(",", "");
    long supplyVal = cleanPayload.toInt();
    
    // Sanity check: should be between 0 and 21M
    if (supplyVal > 0 && supplyVal <= 21000000) {
      circSupply = payload;  // Keep commas for display
      Serial.println("📊 Supply: " + circSupply);
      http.end();
      return true;
    } else {
      Serial.println("❌ Supply value out of range: " + String(supplyVal));
    }
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
  }
  
  http.end();
  return false;
}


// 🌎 Fetch latitude/longitude from saved city using the same Matrix approach
void fetchLatLonFromCity()
{
  if (WiFi.status() != WL_CONNECTED || savedCity.length() == 0) {
    Serial.println("⚠️ Skipping lat/lon fetch - WiFi or city missing.");
    return;
  }

  HTTPClient http;
  String cityParam = savedCity;
  cityParam.replace(" ", "%20");
  String url = "https://nominatim.openstreetmap.org/search?city=" + cityParam + "&format=json&limit=1";

  Serial.print("🌎 Fetching city coordinates: ");
  Serial.println(url);

  http.setTimeout(3000);
  http.setConnectTimeout(2000);
  http.useHTTP10(true);
  http.setReuse(false);
  http.begin(url);
  http.addHeader("User-Agent", "STACKSWORTH-CORE/2.0.3 (bitcoinmanor.com)");

  int httpResponseCode = http.GET();
  if (httpResponseCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload);

    if (!err && doc.size() > 0) {
      String latStr = doc[0]["lat"] | "";
      String lonStr = doc[0]["lon"] | "";
      latitude = latStr.toFloat();
      longitude = lonStr.toFloat();
      Serial.printf("✅ City coordinates: %.6f, %.6f\n", latitude, longitude);
    } else {
      Serial.println("❌ City lookup JSON parse failed or returned no results.");
    }
  } else {
    Serial.println("❌ City lookup failed, HTTP code: " + String(httpResponseCode));
  }

  http.end();
}

// 🌡️ Fetch weather from Open-Meteo, ported from MATRIX
bool fetchWeather()
{
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ No WiFi connection, skipping weather fetch.");
    return false;
  }

  if (savedCity.length() == 0) {
    Serial.println("❌ City not set, skipping weather fetch.");
    return false;
  }

  if (latitude == 0.0 && longitude == 0.0) {
    fetchLatLonFromCity();
    delay(150);
  }

  if (latitude == 0.0 && longitude == 0.0) {
    Serial.println("⚠️ No valid lat/lon, skipping weather fetch.");
    weatherValid = false;
    return false;
  }

  String weatherURL = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 6) +
                      "&longitude=" + String(longitude, 6) +
                      "&current=temperature_2m,weather_code&timezone=auto";

  HTTPClient http;
  http.setTimeout(3000);
  http.setConnectTimeout(2000);
  http.useHTTP10(true);
  http.setReuse(false);
  http.begin(weatherURL);

  Serial.print("🌡️ Fetching weather: ");
  Serial.println(weatherURL);

  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      float temp = doc["current"]["temperature_2m"] | 0.0;
      int weatherCode = doc["current"]["weather_code"] | -1;
      temperature = (int)round(temp);
      weatherCondition = mapWeatherCode(weatherCode);
      weatherValid = true;
      Serial.printf("✅ Updated Weather: %d°C | %s\n", temperature, weatherCondition.c_str());
      http.end();
      return true;
    }
    Serial.println("❌ Failed to parse weather JSON");
  } else {
    Serial.println("❌ Weather fetch failed, HTTP code: " + String(httpCode));
  }

  http.end();
  weatherValid = false;
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
  
  if (true) {
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
  } else {
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.setCursor(20, 115);
    tft.print("---");
    Serial.println("📈 24h change display hidden");
  }
}

// Update block height display (more vertical space now)
void updateBlockHeightDisplay() {
  // Clear block area (wider to prevent stray characters)
  tft.fillRect(238, 60, 82, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(238, 60);
  
  tft.print(String(blockHeight));
  Serial.println("📦 Updated block height: " + String(blockHeight));
}

// Update miner name display (dynamic sizing for long words)
void updateMinerDisplay() {
  // Clear miner area (larger and wider to prevent stray characters)
  tft.fillRect(238, 98, 82, 36, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  
  if (true) {
    // Check if miner name has a space (two words)
    int spaceIndex = minerName.indexOf(' ');
    
    if (spaceIndex > 0 && spaceIndex < minerName.length() - 1) {
      // Two-word name - split into two lines
      String word1 = minerName.substring(0, spaceIndex);
      String word2 = minerName.substring(spaceIndex + 1);
      
      // Determine size based on original length (before truncation)
      int word1Size = (word1.length() > 6) ? 1 : 2;
      int word2Size = (word2.length() > 6) ? 1 : 2;
      
      // Truncate if too long
      if (word1.length() > 6) word1 = word1.substring(0, 6);
      if (word2.length() > 6) word2 = word2.substring(0, 6);
      
      // Print with dynamic sizing
      tft.setTextSize(word1Size);
      tft.setCursor(238, 98);
      tft.print(word1);
      
      tft.setTextSize(word2Size);
      tft.setCursor(238, 114);
      tft.print(word2);
      
      Serial.println("⛏️ Updated miner: " + word1 + " " + word2);
    } else {
      // Single word - check for CamelCase, then hyphenate if over 6 characters
      if (minerName.length() > 6) {
        int splitPoint = -1;
        
        // Look for capital letter in middle (CamelCase like "SpiderPool")
        for (int i = 1; i < minerName.length() - 1; i++) {
          if (isupper(minerName.charAt(i))) {
            splitPoint = i;
            break;
          }
        }
        
        // If no CamelCase found, split at middle
        if (splitPoint == -1) {
          splitPoint = (minerName.length() + 1) / 2;
        }
        
        String word1 = minerName.substring(0, splitPoint);
        String word2 = minerName.substring(splitPoint);
        
        // Print both parts in size 2 (readable size, no hyphen needed for natural splits)
        tft.setTextSize(2);
        tft.setCursor(238, 98);
        tft.print(word1);
        
        tft.setTextSize(2);
        tft.setCursor(238, 114);
        tft.print(word2);
        
        Serial.println("⛏️ Updated miner: " + word1 + " " + word2 + " (split, size 2)");
      } else {
        // 6 characters or less - fits in one line, size 2
        tft.setTextSize(2);
        tft.setCursor(238, 98);
        tft.print(minerName);
        Serial.println("⛏️ Updated miner: " + minerName + " (size 2)");
      }
    }
  } else {
    tft.setTextSize(2);
    tft.setCursor(238, 98);
    tft.print("---");
    Serial.println("⛏️ Miner display hidden");
  }
}

// Update fee rate display (now on bottom bar - narrower section)
void updateFeeDisplay() {
  int barY = 175;
  int section1Width = 142;
  
  // Clear fee area on bottom bar
  tft.fillRect(section1Width + 5, barY + 24, 95, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(section1Width + 5, barY + 24);
  
  tft.print(String(feeRate));
  tft.setTextSize(1);
  tft.setCursor(section1Width + 25, barY + 28);
  tft.print("sat/vB");
  Serial.println("⚡ Updated fee: " + String(feeRate) + " sat/vB");
}

// Update sats per dollar display (first section of bottom bar)
void updateSatsPerDollarDisplay() {
  int barY = 175;
  
  // Clear sats/USD area
  tft.fillRect(5, barY + 24, 130, 20, TFT_BLACK);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(5, barY + 24);
  
  tft.print(String(satsPerDollar));
  Serial.println("💎 Updated sats/$: " + String(satsPerDollar));
}

// Market cap removed from display (no longer used)

// Supply removed from display (no longer used)

// Update date/time display
void updateDateTimeDisplay() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return;
  }
  
  // Clear date/time area
  tft.fillRect(10, 150, 230, 20, TFT_BLACK);
  
  // Format components separately (ESP32-compatible)
  char dayOfWeek[10];
  char monthDay[10];
  char timeOnly[10];
  
  strftime(dayOfWeek, sizeof(dayOfWeek), "%a", &timeinfo);      // "Mon"
  strftime(monthDay, sizeof(monthDay), "%b %d", &timeinfo);     // "May 05"
  strftime(timeOnly, sizeof(timeOnly), "%I:%M%p", &timeinfo);   // "08:42PM"
  
  // Strip leading zeros like MATRIX does
  char* time = timeOnly;
  if (time[0] == '0') time++;  // "8:42PM" instead of "08:42PM"
  
  // Build final string: "Mon, May 05  8:42PM"
  char timeStr[30];
  snprintf(timeStr, sizeof(timeStr), "%s, %s  %s", dayOfWeek, monthDay, time);
  
  // Check DST status for logging (month is 0-11, tm_wday is 0-6 where 0=Sunday)
  bool dstActive = isDST(timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_wday);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 150);
  tft.print(timeStr);
  
  Serial.printf("🕒 Updated time: %s (DST: %s)\n", timeStr, dstActive ? "Yes" : "No");
}

// ═══════════════════════════════════════════════════════════════════════════
//   SCREEN DRAWING FUNCTIONS (3-Screen Carousel)
// ═══════════════════════════════════════════════════════════════════════════

// Draw screen indicator dots at bottom with footer text
void drawScreenIndicators() {
  int dotY = 228;
  int dotSpacing = 20;
  int startX = 255;  // Move dots to the right side
  
  // Footer text on left
  tft.setTextColor(0x528A);  // Dim gray
  tft.setTextSize(1);
  tft.setCursor(5, 228);
  tft.print("Built By BitcoinManor.com");
  
  // Dots on right
  for (int i = 0; i < 3; i++) {
    int dotX = startX + (i * dotSpacing);
    uint16_t color = (i == currentScreen) ? TFT_ORANGE : 0x4208;  // Orange for active, dim gray for inactive
    tft.fillCircle(dotX, dotY, 3, color);
  }
}

// Screen 1: Dashboard (current main screen)
void drawScreen1() {
  tft.fillScreen(TFT_BLACK);
  
  // Subtle grid background
  for (int x = 0; x < 320; x += 20) {
    tft.drawLine(x, 0, x, 240, 0x2104);
  }
  for (int y = 0; y < 240; y += 20) {
    tft.drawLine(0, y, 320, y, 0x2104);
  }
  
  // Logo and header
  int logoHexX = 42;
  int logoHexY = 15;
  int logoHexSize = 12;
  
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = logoHexX + logoHexSize * cos(angle1);
    int y1 = logoHexY + logoHexSize * sin(angle1);
    int x2 = logoHexX + logoHexSize * cos(angle2);
    int y2 = logoHexY + logoHexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_ORANGE);
  }
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(logoHexX - 4, logoHexY - 7);
  tft.print("S");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(65, 8);
  tft.print("STACKSWORTH");
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(205, 8);
  tft.print("CORE");
  
  // Main price card
  tft.fillRoundRect(10, 35, 220, 105, 12, TFT_BLACK);
  tft.drawLine(10, 140, 230, 140, TFT_CYAN);
  tft.drawLine(230, 35, 230, 140, TFT_CYAN);
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(20, 43);
  tft.print("BITCOIN PRICE");
  
  updatePriceDisplay();
  updateChange24hDisplay();
  
  // Right side metrics
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(238, 40);
  tft.print("BLOCK");
  updateBlockHeightDisplay();
  
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(238, 80);
  tft.print("MINER");
  updateMinerDisplay();
  
  // Date/Time
  updateDateTimeDisplay();
  
  // Bottom bar
  int barY = 175;
  int barHeight = 48;
  int section1Width = 142;
  int section2Width = 110;
  
  tft.drawRect(0, barY, 320, barHeight, TFT_ORANGE);
  tft.drawLine(section1Width, barY, section1Width, barY + barHeight, TFT_ORANGE);
  tft.drawLine(section1Width + section2Width, barY, section1Width + section2Width, barY + barHeight, TFT_ORANGE);
  
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  tft.setCursor(5, barY + 5);
  tft.print("SATS/USD");
  updateSatsPerDollarDisplay();
  
  tft.setTextColor(TFT_ORANGE);
  tft.setCursor(section1Width + 5, barY + 5);
  tft.print("FEE");
  updateFeeDisplay();
  
  updateLiveIndicator();
  
  // Screen indicators with footer
  drawScreenIndicators();
}

// Screen 2: Block Focus - Large block height with miner
void drawScreen2() {
  tft.fillScreen(TFT_BLACK);
  
  // Different grid pattern for visual distinction
  for (int x = 0; x < 320; x += 30) {
    tft.drawLine(x, 0, x, 240, 0x2104);
  }
  for (int y = 0; y < 240; y += 30) {
    tft.drawLine(0, y, 320, y, 0x2104);
  }
  
  // Logo and header (smaller)
  int logoHexX = 25;
  int logoHexY = 12;
  int logoHexSize = 10;
  
  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = logoHexX + logoHexSize * cos(angle1);
    int y1 = logoHexY + logoHexSize * sin(angle1);
    int x2 = logoHexX + logoHexSize * cos(angle2);
    int y2 = logoHexY + logoHexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_GREEN);
  }
  
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(1);
  tft.setCursor(logoHexX - 2, logoHexY - 3);
  tft.print("S");
  
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(45, 8);
  tft.print("BLOCK FOCUS");
  
  // Large block height display
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 50);
  tft.print("BLOCK HEIGHT");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(8);
  String blockStr = String(blockHeight);
  int blockWidth = blockStr.length() * 48;  // Approximate width per char at size 8
  int blockX = (320 - blockWidth) / 2;  // Center it
  tft.setCursor(blockX, 85);
  tft.print(blockStr);
  
  // Miner below in green
  tft.setTextColor(TFT_GREEN);
  tft.setTextSize(2);
  tft.setCursor(10, 165);
  tft.print("MINED BY:");
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(3);
  
  // Auto-hyphenate long single-word miner names
  String displayMiner = minerName;
  if (minerName.indexOf(' ') < 0 && minerName.length() > 10) {
    // Single word over 10 chars - hyphenate to fit better
    int splitPoint = (minerName.length() + 1) / 2;
    displayMiner = minerName.substring(0, splitPoint) + "-" + minerName.substring(splitPoint);
  } else if (displayMiner.length() > 12) {
    displayMiner = displayMiner.substring(0, 12);
  }
  
  int minerWidth = displayMiner.length() * 18;
  int minerX = (320 - minerWidth) / 2;
  tft.setCursor(minerX, 190);
  tft.print(displayMiner);
  
  // Screen indicators with footer
  drawScreenIndicators();
}

// Screen 3: Time Focus - Large date/time with compact footer
void drawScreen3() {
  tft.fillScreen(TFT_BLACK);

  // Diagonal grid pattern for visual distinction
  for (int i = -240; i < 320; i += 40) {
    tft.drawLine(i, 0, i + 240, 240, 0x2104);
  }

  // Logo and header (smaller)
  int logoHexX = 25;
  int logoHexY = 12;
  int logoHexSize = 10;

  for (int i = 0; i < 6; i++) {
    float angle1 = i * 60 * PI / 180;
    float angle2 = (i + 1) * 60 * PI / 180;
    int x1 = logoHexX + logoHexSize * cos(angle1);
    int y1 = logoHexY + logoHexSize * sin(angle1);
    int x2 = logoHexX + logoHexSize * cos(angle2);
    int y2 = logoHexY + logoHexSize * sin(angle2);
    tft.drawLine(x1, y1, x2, y2, TFT_CYAN);
  }

  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(logoHexX - 2, logoHexY - 3);
  tft.print("S");

  tft.setTextColor(TFT_WHITE);
  tft.setCursor(45, 8);
  tft.print("TIME FOCUS");

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char dayOfWeek[10];
    strftime(dayOfWeek, sizeof(dayOfWeek), "%A", &timeinfo);

    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(3);
    int dayWidth = strlen(dayOfWeek) * 18;
    int dayX = (320 - dayWidth) / 2;
    if (dayX < 5) dayX = 5;
    tft.setCursor(dayX, 32);
    tft.print(dayOfWeek);

    char timeOnly[10];
    strftime(timeOnly, sizeof(timeOnly), "%I:%M%p", &timeinfo);
    char* time = timeOnly;
    if (time[0] == '0') time++;

    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(6);
    int timeWidth = strlen(time) * 36;
    int timeX = (320 - timeWidth) / 2;
    if (timeX < 0) timeX = 0;
    tft.setCursor(timeX, 70);
    tft.print(time);

    char monthDay[15];
    strftime(monthDay, sizeof(monthDay), "%B %d, %Y", &timeinfo);

    // Raised date line under the main time
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(2);
    int dateWidth = strlen(monthDay) * 12;
    int dateX = (320 - dateWidth) / 2;
    if (dateX < 5) dateX = 5;
    tft.setCursor(dateX, 128);
    tft.print(monthDay);
  }

  // City + temp line. Avoid the degree symbol because the default TFT font shows it as a box.
  String cityLine = savedCity.length() ? savedCity : "Location";
  if (weatherValid) {
    int displayTemp = temperature;
    if (savedTempUnit == "F") displayTemp = (int)round((temperature * 9.0 / 5.0) + 32);
    cityLine += "  ";
    if (displayTemp >= 0) cityLine += "+";
    cityLine += String(displayTemp);
    cityLine += savedTempUnit;   // Example: +68F, not +68°F
  } else {
    cityLine += "  --";
    cityLine += savedTempUnit;
  }

  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(2);
  int cityWidth = cityLine.length() * 12;
  int cityX = (320 - cityWidth) / 2;
  if (cityX < 5) cityX = 5;
  tft.setCursor(cityX, 148);
  tft.print(cityLine);

  // Larger weather condition line
  String conditionLine = weatherValid ? weatherCondition : "Weather Syncing";
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(2);
  int conditionWidth = conditionLine.length() * 12;
  int conditionX = (320 - conditionWidth) / 2;
  if (conditionX < 5) conditionX = 5;
  tft.setCursor(conditionX, 170);
  tft.print(conditionLine);

  // Compact footer with block and price, separated from weather content
  tft.drawLine(0, 192, 320, 192, TFT_CYAN);

  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(10, 198);
  tft.print("BLOCK:");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 209);
  tft.print(String(blockHeight));

  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setCursor(145, 198);
  tft.print("PRICE:");
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(145, 209);
  String priceStr = getCurrencySymbol() + formatWithCommas(btcPrice);
  tft.print(priceStr);

  drawScreenIndicators();
}

// Refresh current screen with latest data
void refreshCurrentScreen() {
  switch(currentScreen) {
    case 0:
      drawScreen1();
      break;
    case 1:
      drawScreen2();
      break;
    case 2:
      drawScreen3();
      break;
  }
}

// ═══════════════════════════════════════════════════════════════════════════
//  📺 DISPLAY FUNCTIONS
// ═══════════════════════════════════════════════════════════════════════════

void updateLiveIndicator()
{
  // Bottom data bar position - LIVE section is wider now for text
  int barY = 175;
  int section1Width = 142;
  int section2Width = 110;
  int liveX = section1Width + section2Width;
  
  // Clear the LIVE indicator area (now 68px wide)
  tft.fillRect(liveX + 1, barY + 1, 66, 46, TFT_BLACK);
  
  if (wifiConnected)
  {
    tft.setTextColor(TFT_CYAN);
    tft.setTextSize(1);
    tft.setCursor(liveX + 20, barY + 15);
    tft.print("LIVE");
    tft.fillCircle(liveX + 34, barY + 32, 5, TFT_CYAN);
  }
  else
  {
    tft.setTextColor(TFT_RED);
    tft.setTextSize(1);
    tft.setCursor(liveX + 10, barY + 15);
    tft.print("OFFLINE");
    tft.fillCircle(liveX + 34, barY + 32, 5, TFT_RED);
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
  tft.setCursor(logoHexX - 7, logoHexY - 11);
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
  
  // Use saved credentials (AP mode will be started if none exist)
  String connectSSID = savedSSID;
  String connectPassword = savedPassword;
  
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
      if (savedTimezone < 0 || savedTimezone >= (int)NUM_TIMEZONES) savedTimezone = getTimezoneIndex();
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
    
    
    // Save to preferences. If portal is reopened and WiFi fields are left blank, keep existing credentials.
    prefs.begin("stacksworth", false);
    savedSSID.trim();
    savedPassword.trim();
    if (savedSSID.length() == 0) savedSSID = prefs.getString("ssid", "");
    if (savedPassword.length() == 0) savedPassword = prefs.getString("password", "");
    prefs.putString("ssid", savedSSID);
    prefs.putString("password", savedPassword);
    prefs.putString("city", savedCity);
    prefs.putInt("timezone", savedTimezone);
    prefs.putString("currency", savedCurrency);
    prefs.putString("tempunit", savedTempUnit);
    prefs.putString("devicename", savedDeviceName);
    prefs.putUChar("brightness", savedBrightness);
    
    
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
  savedTimezone = prefs.getInt("timezone", 11);  // Default Mountain index
  savedCurrency = prefs.getString("currency", "USD");
  savedTempUnit = prefs.getString("tempunit", "F");
  savedDeviceName = prefs.getString("devicename", "");
  savedBrightness = prefs.getUChar("brightness", 128);
  
  // CORE now uses a fixed curated dashboard; old checkbox prefs are ignored.
  for (int i = 0; i < 12; i++) {
    displayEnabled[i] = true;
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
  Serial.println("🚀 Starting STACKSWORTH CORE " + String(FIRMWARE_VERSION) + "...");
  Serial.println("📱 Features: Touch Screen | Auto DST | Dynamic Sizing");
  
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
  tft.setRotation(1);  // Landscape mode
  tft.fillScreen(TFT_BLACK);
  
  // Initialize touch controller
  pinMode(36, INPUT);  // Explicitly set IRQ pin as input
  
  if (tft.touch()) {
    Serial.println("✅ Touch controller found");
    
    // Try to calibrate touch (some controllers need this)
    uint16_t calData[8];
    tft.setTouchCalibrate(calData);  // Use default calibration
    Serial.println("🔧 Touch calibration applied");
  } else {
    Serial.println("❌ Touch controller NOT found - hardware issue?");
  }
  
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
    // 🚀 Initial data fetch if WiFi is connected
    if (wifiConnected) {
      Serial.println("🔄 Fetching initial Bitcoin data...");
      
      // 🌍 Configure timezone using portal timezone index (auto-handles DST)
      applyTimezone();
      
      // Fetch all data before drawing screens
      fetchPriceFromSatonak();
      lastPriceFetch = millis();
      delay(500);
      
      fetchHeightFromSatonak();
      lastHeightFetch = millis();
      delay(500);
      
      fetchMinerFromSatonak();
      lastMinerFetch = millis();
      delay(500);
      
      fetchFeeFromSatonak();
      lastFeeFetch = millis();
      delay(1000);
      
      fetchChange24hFromSatonak();
      lastChange24hFetch = millis();
      delay(500);

      fetchWeather();
      lastWeatherFetch = millis();
      
      Serial.println("✅ Initial data fetch complete!");
    }
    
    // Draw initial screen (Dashboard)
    drawScreen1();
    Serial.println("📱 Touch screen enabled - tap to cycle through screens");
    
    // Test touch initialization - try multiple times
    Serial.println("🔍 Touch controller test:");
    Serial.println("🔧 Touch pins: CS=33, IRQ=36, shared SPI (MOSI=13, MISO=12, SCK=14)");
    
    bool touchWorking = false;
    for (int i = 0; i < 5; i++) {
      uint16_t testX, testY;
      if (tft.getTouch(&testX, &testY)) {
        Serial.printf("✅ Touch detected! X=%d, Y=%d\n", testX, testY);
        touchWorking = true;
        break;
      }
      delay(100);
    }
    
    if (!touchWorking) {
      Serial.println("⚠️ Touch not detected at startup (normal - waiting for first touch)");
      Serial.println("💡 Try tapping screen now...");
    }
  }
  
  initialSetupDone = true;
}

void loop()
{
  // 📡 Process DNS requests when in AP mode (for captive portal)
  if (apMode) {
    dnsServer.processNextRequest();
    return;  // Skip everything else in AP mode
  }
  
  // 📱 Handle touch screen input
  // Edge-triggered: one physical tap advances one screen, holding does not keep cycling.
  uint16_t touchX, touchY;
  bool isTouched = tft.getTouch(&touchX, &touchY);

  static unsigned long lastDebugPrint = 0;
  if (touchDebugMode && millis() - lastDebugPrint > 1000) {
    lastDebugPrint = millis();
    int irqState = digitalRead(36);
    Serial.printf("🔍 Touch status: %s | IRQ pin: %d\n", isTouched ? "TOUCHED" : "not touched", irqState);
    if (isTouched) {
      Serial.printf("   Coordinates: X=%d, Y=%d\n", touchX, touchY);
    }
  }

  if (isTouched && !touchWasDown) {
    unsigned long now = millis();

    if (now - lastTouchTime > touchDebounce) {
      lastTouchTime = now;
      touchWasDown = true;

      currentScreen = (currentScreen + 1) % 3;
      Serial.printf("📱 Touch ACCEPTED! Coordinates: (%d, %d) -> Switching to screen %d\n", touchX, touchY, currentScreen);
      refreshCurrentScreen();
    }
  } else if (!isTouched) {
    touchWasDown = false;
  }

  // Check WiFi status periodically and reconnect if needed
  if (millis() - lastWiFiCheck >= wifiCheckInterval)
  {
    lastWiFiCheck = millis();
    
    bool wasConnected = wifiConnected;
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    
    // If status changed, update the indicator
    if (wasConnected != wifiConnected)
    {
      Serial.print("WiFi status changed: ");
      Serial.println(wifiConnected ? "Connected" : "Disconnected");
      
      // Only update LIVE indicator if on dashboard screen
      if (currentScreen == 0) {
        updateLiveIndicator();
      }
    }
    
    // Try to reconnect if disconnected
    if (!wifiConnected)
    {
      Serial.println("Attempting WiFi reconnection...");
      WiFi.disconnect();
      
      // Use saved credentials only
      String connectSSID = savedSSID;
      String connectPassword = savedPassword;
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
        
        // Only update LIVE indicator if on dashboard screen
        if (currentScreen == 0) {
          updateLiveIndicator();
        }
      }
    }
  }
  
  // 🌐 Staggered API data fetching (only if WiFi connected)
  if (wifiConnected) {
    unsigned long now = millis();
    
    // Re-sync NTP every 30 minutes to prevent clock drift
    if (now - lastNtpSync >= INTERVAL_NTP_SYNC) {
      applyTimezone();
      lastNtpSync = now;
    }
    
    // Update time only on screens that actually contain time.
    // This prevents the dashboard time rectangle from drawing over Block Focus.
    if (now - lastTimeUpdate >= INTERVAL_TIME) {
      lastTimeUpdate = now;
      if (currentScreen == 0) {
        updateDateTimeDisplay();
      } else if (currentScreen == 2) {
        drawScreen3();
      }
    }
    
    // Fetch weather every 30 minutes if a city is saved
    if (now - lastWeatherFetch >= INTERVAL_WEATHER) {
      if (fetchWeather()) {
        lastWeatherFetch = now;
        if (currentScreen == 2) {
          drawScreen3();
        }
      } else {
        lastWeatherFetch = now; // avoid hammering endpoint if unavailable
      }
    }

    // Fetch price every 5 minutes
    if (now - lastPriceFetch >= INTERVAL_PRICE) {
      if (fetchPriceFromSatonak()) {
        lastPriceFetch = now;
        
        // Update displays based on active screen
        if (currentScreen == 0) {
          updatePriceDisplay();
          updateSatsPerDollarDisplay();
        } else if (currentScreen == 2) {
          drawScreen3();  // Refresh footer with new price
        }
      }
    }
    
    // Fetch block height every 2 minutes
    if (now - lastHeightFetch >= INTERVAL_HEIGHT) {
      if (fetchHeightFromSatonak()) {
        lastHeightFetch = now;
        
        // Update displays based on active screen
        if (currentScreen == 0) {
          updateBlockHeightDisplay();
        } else if (currentScreen == 1) {
          drawScreen2();  // Refresh block focus screen
        } else if (currentScreen == 2) {
          drawScreen3();  // Refresh footer with new block
        }
      }
    }
    
    // Fetch miner every 6 minutes
    if (now - lastMinerFetch >= INTERVAL_MINER) {
      if (fetchMinerFromSatonak()) {
        lastMinerFetch = now;
        
        // Update displays based on active screen
        if (currentScreen == 0) {
          updateMinerDisplay();
        } else if (currentScreen == 1) {
          drawScreen2();  // Refresh block focus with new miner
        }
      }
    }
    
    // Fetch fee every 9 minutes
    if (now - lastFeeFetch >= INTERVAL_FEE) {
      if (fetchFeeFromSatonak()) {
        lastFeeFetch = now;
        
        // Only update if on dashboard
        if (currentScreen == 0) {
          updateFeeDisplay();
        }
      }
    }
    
    // Fetch 24h change every 30 minutes (rate limited by API)
    if (now - lastChange24hFetch >= INTERVAL_CHANGE24H) {
      if (fetchChange24hFromSatonak()) {
        lastChange24hFetch = now;
        
        // Only update if on dashboard
        if (currentScreen == 0) {
          updateChange24hDisplay();
        }
      }
    }
  }
}
