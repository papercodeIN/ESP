#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <time.h>

// ----- LCD -----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ----- I2C pins (change if needed) -----
#define SDA_PIN D4
#define SCL_PIN D5

// ----- Wi-Fi -----
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";

// ----- NTP / Timezone -----
// IST = UTC + 5:30
const long GMT_OFFSET_SEC = 19800;
const int DAYLIGHT_OFFSET_SEC = 0;

const char* NTP_SERVER = "pool.ntp.org";

const char* DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

String centerText(const String& text, int width) {
  if (text.length() >= (size_t)width) {
    return text.substring(0, width);
  }
  int left = (width - text.length()) / 2;
  int right = width - text.length() - left;
  String out = "";
  for (int i = 0; i < left; i++) out += " ";
  out += text;
  for (int i = 0; i < right; i++) out += " ";
  return out;
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);
  delay(500);

  Serial.begin(115200);
  delay(200);

  Serial.println("WiFi scan...");
  int n = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", n);
  for (int i = 0; i < n; i++) {
    Serial.printf("  %d: %s (%ddBm)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
  }

  Serial.printf("Connecting to SSID: %s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");

  for (int i = 0; i < 30; i++) {
    wl_status_t st = WiFi.status();
    Serial.printf("Try %02d: status=%d\n", i + 1, st);
    if (st == WL_CONNECTED) break;
    delay(500);
  }

  Serial.printf("Final status=%d, IP=%s\n", WiFi.status(), WiFi.localIP().toString().c_str());

  lcd.clear();
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi OK");
  } else {
    lcd.print("WiFi failed");
  }
  delay(1500);
}

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();

  connectWiFi();
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
}

void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NTP failed");
    delay(2000);
    return;
  }

  int hour24 = timeinfo.tm_hour;
  int hour12 = hour24 % 12;
  if (hour12 == 0) hour12 = 12;
  const char* ampm = (hour24 < 12) ? "AM" : "PM";

  char timeBuf[9]; // "HH:MM AM"
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", hour12, timeinfo.tm_min, ampm);

  char dateBuf[16]; // "DD-MM-YY Ddd"
  snprintf(dateBuf, sizeof(dateBuf), "%02d-%02d-%02d %s",
           timeinfo.tm_mday,
           timeinfo.tm_mon + 1,
           (timeinfo.tm_year + 1900) % 100,
           DAYS[timeinfo.tm_wday]);

  lcd.setCursor(0, 0);
  lcd.print(centerText(String(timeBuf), 16));
  lcd.setCursor(0, 1);
  lcd.print(centerText(String(dateBuf), 16));

  delay(1000);
}