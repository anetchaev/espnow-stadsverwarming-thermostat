#include <ESP8266WiFi.h>

void setup() {
    Serial.begin(115200);
    delay(500);

    // Set Wi-Fi to Station Mode to initialize the STA MAC interface
    WiFi.mode(WIFI_STA);
    delay(100);

    uint8_t mac[6];
    WiFi.macAddress(mac);

    // Format as C++ byte array string
    char macArrayStr[40];
    snprintf(macArrayStr, sizeof(macArrayStr), "{0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X}",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    Serial.println("\n==========================================");
    Serial.println("NodeMCU (ESP8266) STA MAC Address");
    Serial.println("==========================================");
    Serial.print("Standard format : ");
    Serial.println(WiFi.macAddress());
    Serial.print("C++ Array format: ");
    Serial.println(macArrayStr);
    Serial.println("==========================================");
    Serial.println("Copy the C++ Array format into your M5Stack sketch.");
}

void loop() {
    delay(5000);
}
