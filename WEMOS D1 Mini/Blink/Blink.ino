#include <WiFi.h>
#include "time.h"
#include <TFT_eSPI.h>
#include "unihiker_k10.h"

UNIHIKER_K10 k10;
TFT_eSPI tft = TFT_eSPI();

const char* ssid = "Capgemini_4G";
const char* password = "MN704116";

const char* ntpServer = "pool.ntp.org";
long gmtOffset_sec = 19800;   // India
int daylightOffset_sec = 0;

int prev_sec = -1;

void setup() {
  Serial.begin(115200);

  k10.begin();          // IMPORTANT for K10 power & screen
  k10.initScreen(2);    // rotation

  tft.init();           // TFT init AFTER k10.initScreen
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(40, 100);
  tft.print("Connecting WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  tft.fillScreen(TFT_BLACK);

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

void loop() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo)) {
    return;
  }

  if (timeinfo.tm_sec != prev_sec) {
    prev_sec = timeinfo.tm_sec;

    tft.fillScreen(TFT_BLACK);

    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(5);
    tft.setCursor(40, 70);

    char timeStr[9];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    tft.print(timeStr);

    tft.setTextSize(2);
    tft.setCursor(90, 150);

    char dateStr[11];
    strftime(dateStr, sizeof(dateStr), "%d-%m-%Y", &timeinfo);
    tft.print(dateStr);
  }

  delay(200);
}