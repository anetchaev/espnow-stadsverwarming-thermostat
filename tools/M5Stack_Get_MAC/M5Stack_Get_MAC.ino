#include <M5Stack.h>
#include <WiFi.h>

void setup() {
    M5.begin();
    M5.Power.begin();
    Serial.begin(115200);

    // Set Wi-Fi to Station Mode to initialize the STA MAC interface
    WiFi.mode(WIFI_STA);
    delay(100);

    uint8_t mac[6];
    WiFi.macAddress(mac);

    // Format as C++ byte array string
    char macArrayStr[40];
    snprintf(macArrayStr, sizeof(macArrayStr), "{0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Output to Serial Monitor
    Serial.println("\n--- M5Stack (ESP32) STA MAC Address ---");
    Serial.print("Standard format : ");
    Serial.println(WiFi.macAddress());
    Serial.print("C++ Array format: ");
    Serial.println(macArrayStr);

    // Output directly to M5Stack LCD Screen
    M5.Lcd.clear(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setTextSize(2);

    M5.Lcd.setCursor(10, 20);
    M5.Lcd.println("M5Stack STA MAC:");

    M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    M5.Lcd.setCursor(10, 55);
    M5.Lcd.println(WiFi.macAddress());

    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.setCursor(10, 110);
    M5.Lcd.println("Copy for NodeMCU code:");

    M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    M5.Lcd.setCursor(10, 145);
    M5.Lcd.println(macArrayStr);
}

void loop() {
    delay(1000);
}
