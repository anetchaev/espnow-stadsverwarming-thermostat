#include <M5Stack.h>
#include <M5UnitENV.h>
#include <WiFi.h>
#include <esp_now.h>

SHT3X sht30;

// --- CONFIGURATION ---
// Replace with your NodeMCU Receiver STA MAC Address
uint8_t receiverAddress[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// Calibration offset in Celsius (Reference Temp - Raw Sensor Temp)
const float TEMP_OFFSET = -0.3; 

// Thermostat State Variables
float targetTemp      = 21.5;
float currentTemp     = 20.0;
float currentHumidity = 0.0;
bool requestHeat      = false;

// UI Partial-Rendering Cache Registers
float lastDrawnTarget   = 0.0;
float lastDrawnCurrent  = 0.0;
float lastDrawnHumidity = 0.0;
bool lastDrawnHeatState = false;

// Radio Link State Registers
bool isRadioLinkStable    = true;
bool lastDrawnLinkState   = false;
bool forceInitialIconDraw = true;

// Non-Blocking Time-Slicing Registers
unsigned long lastRadioTx    = 0;
unsigned long lastSensorRead = 0;

const unsigned long RADIO_TX_INTERVAL    = 30000; // Transmit ESP-NOW packet every 30 seconds
const unsigned long SENSOR_READ_INTERVAL = 5000;  // Sample SHT30 sensor every 5 seconds
const unsigned long UI_BOOT_FORCE_WINDOW = 2000;  // Force initial UI draw window (ms)

// ESP-NOW Payload Structure (Must match Receiver)
struct __attribute__((packed)) DataPacket {
    bool request_heat;
};
DataPacket packet;

// Render geometric radio connection status icon in bottom-right corner
void drawConnectionSymbol(bool connected) {
    int bx = 280; // Base X coordinate
    int by = 220; // Base Y coordinate
    
    // Clear previous icon bounding box
    M5.Lcd.fillRect(bx - 15, by - 25, 40, 35, TFT_BLACK);
    
    if (connected) {
        // Green Radar Arcs (Connected)
        M5.Lcd.fillCircle(bx, by, 3, TFT_GREEN);
        M5.Lcd.drawCircle(bx, by, 9, TFT_GREEN);
        M5.Lcd.drawCircle(bx, by, 16, TFT_GREEN);
        M5.Lcd.fillRect(bx - 20, by, 40, 20, TFT_BLACK); // Clip bottom half
    } else {
        // Red Alert Triangle with 'X' (Disconnected)
        M5.Lcd.fillTriangle(bx, by - 22, bx - 14, by, bx + 14, by, TFT_RED);
        M5.Lcd.fillTriangle(bx, by - 16, bx - 10, by - 2, bx + 10, by - 2, TFT_BLACK);
        
        // Encapsulate local font size change and restore global state immediately
        M5.Lcd.setTextSize(2);
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.setCursor(bx - 5, by - 14);
        M5.Lcd.print("X");
		
        // Restore global TexSize, which was changed in to 2
        M5.Lcd.setTextSize(3); 
    }
}

// ESP-NOW Transmission Callback
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    isRadioLinkStable = (status == ESP_NOW_SEND_SUCCESS);
}

void setup() {
    M5.begin();
    M5.Power.begin();
    
    // Initialize I2C Bus on Port A (SDA: 21, SCL: 22)
    Wire.begin(21, 22);
    if (!sht30.begin(&Wire, SHT3X_I2C_ADDR, 21, 22, 400000U)) {
        M5.Lcd.println("Error: ENV III Sensor Not Found!");
        while(1);
    }

    M5.Lcd.setTextSize(3);  
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        M5.Lcd.println("Error Initializing ESP-NOW");
        return;
    }
    esp_now_register_send_cb(OnDataSent);
    
    // Register Receiver Peer
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, receiverAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Perform immediate sensor read at boot
    if (sht30.update()) {
        currentTemp = sht30.cTemp + TEMP_OFFSET;
        currentHumidity = sht30.humidity;
    }
    requestHeat = (currentTemp <= (targetTemp - 0.2));

    // Draw Static UI Labels Once
    M5.Lcd.clear(TFT_BLACK);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    
    M5.Lcd.setCursor(10, 20);  M5.Lcd.print("Current: ");
    M5.Lcd.setCursor(10, 60);  M5.Lcd.print("Target:  ");
    M5.Lcd.setCursor(10, 130); M5.Lcd.print("Humidity: ");
}

void loop() {
    M5.update();

    // 1. FAST TRACK: Poll tactile buttons continuously
    if (M5.BtnA.wasPressed()) { targetTemp -= 0.5; }
    if (M5.BtnC.wasPressed()) { targetTemp += 0.5; }

    unsigned long currentMillis = millis();

    // 2. MEDIUM TRACK: Read physical climate sensor every 5 seconds
    if (currentMillis - lastSensorRead >= SENSOR_READ_INTERVAL) {
        lastSensorRead = currentMillis;
        if (sht30.update()) {
            currentTemp = sht30.cTemp + TEMP_OFFSET;
            currentHumidity = sht30.humidity;
        }
    }

    // Evaluate Thermostat Hysteresis (+/- 0.2C deadband)
    if (currentTemp <= (targetTemp - 0.2)) { requestHeat = true; }
    if (currentTemp >= (targetTemp + 0.2)) { requestHeat = false; }

    // 3. SLOW TRACK: Transmit control payload every 30 seconds
    if (currentMillis - lastRadioTx >= RADIO_TX_INTERVAL) {
        lastRadioTx = currentMillis;
        packet.request_heat = requestHeat;
        esp_now_send(receiverAddress, (uint8_t *) &packet, sizeof(packet));
    }

    // 4. STATE-DRIVEN PARTIAL UI RENDERING
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    
    if (currentTemp != lastDrawnCurrent) {
        M5.Lcd.setCursor(210, 20);
        M5.Lcd.printf("%.1fC ", currentTemp);
        lastDrawnCurrent = currentTemp;
    }

    if (targetTemp != lastDrawnTarget) {
        M5.Lcd.setCursor(210, 60);
        M5.Lcd.printf("%.1fC ", targetTemp);
        lastDrawnTarget = targetTemp;
    }

    if (currentHumidity != lastDrawnHumidity) {
        M5.Lcd.setCursor(210, 130);
        M5.Lcd.printf("%2.1f%%  ", currentHumidity);
        lastDrawnHumidity = currentHumidity;
    }

    if (requestHeat != lastDrawnHeatState || currentMillis < UI_BOOT_FORCE_WINDOW) {
        M5.Lcd.setCursor(10, 200);
        if (requestHeat) {
            M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK);
            M5.Lcd.print("BOILER: ON   ");
        } else {
            M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
            M5.Lcd.print("BOILER: OFF  ");
        }
        lastDrawnHeatState = requestHeat;
    }

    if (isRadioLinkStable != lastDrawnLinkState || forceInitialIconDraw) {
        drawConnectionSymbol(isRadioLinkStable);
        lastDrawnLinkState = isRadioLinkStable;
        forceInitialIconDraw = false;
    }

    delay(10); 
}
