#include <ESP8266WiFi.h>
#include <espnow.h>

// --- HARDWARE PINOUT CONFIGURATION ---
#define RELAY_PIN     5  // D1 (GPIO5): 5V Relay Control (Set relay jumper to 'H' High-Active)
#define ALARM_LED_PIN 4  // D2 (GPIO4): External Red Failsafe Warning LED (via 270 Ohm resistor)
#define LED_GREEN     2  // D4 (GPIO2): Onboard Green Status LED

// --- TRANSMITTER MAC ADDRESSES ---
// Replace MAC_FLOOR_2 with your M5Stack STA MAC Address
const uint8_t MAC_FLOOR_2[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}; 
const uint8_t MAC_FLOOR_3[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // Optional 3rd floor expansion slot

// Zone State Flags
bool heat_floor_2 = false;
bool heat_floor_3 = false;

// Failsafe Timeout Registers
unsigned long lastPacket_F2 = 0;
unsigned long lastPacket_F3 = 0;
const unsigned long TIMEOUT_MS = 180000; // 3-minute safety timeout (180,000 ms)

// ESP-NOW Payload Structure (Must match Transmitter)
struct __attribute__((packed)) DataPacket {
    bool request_heat;
};

// Helper function to validate incoming sender MAC address
bool compareMac(const uint8_t * mac1, const uint8_t * mac2) {
    for (int i = 0; i < 6; i++) {
        if (mac1[i] != mac2[i]) return false;
    }
    return true;
}

// ESP-NOW Receive Callback
void onDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
    if (len != sizeof(DataPacket)) return;
    
    DataPacket packet;
    memcpy(&packet, incomingData, sizeof(packet));
    
    if (compareMac(mac, MAC_FLOOR_2)) {
        heat_floor_2 = packet.request_heat;
        lastPacket_F2 = millis();
    } 
    else if (compareMac(mac, MAC_FLOOR_3)) {
        heat_floor_3 = packet.request_heat;
        lastPacket_F3 = millis();
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize Outputs in Safe Default States
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);     // Relay OFF (High-Active)
    
    pinMode(ALARM_LED_PIN, OUTPUT);
    digitalWrite(ALARM_LED_PIN, LOW); // Red Warning LED OFF
    
    pinMode(LED_GREEN, OUTPUT);
    digitalWrite(LED_GREEN, HIGH);    // Green Status LED ON
    
    // Initialize ESP-NOW in Station Mode
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != 0) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }
    
    esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
    esp_now_register_recv_cb(onDataRecv);
    
    lastPacket_F2 = millis();
    lastPacket_F3 = millis();
}

void loop() {
    unsigned long currentMillis = millis();
    bool isLinkBroken = false;
    
    // 1. CHECK FAILSAFE TIMEOUTS
    if (currentMillis - lastPacket_F2 > TIMEOUT_MS) {
        heat_floor_2 = false;
        isLinkBroken = true;
    }
    
    // Uncomment the block below when adding a 3rd floor transmitter:
    /*
    if (currentMillis - lastPacket_F3 > TIMEOUT_MS) {
        heat_floor_3 = false;
        isLinkBroken = true;
    }
    */
    
    // 2. MASTER RELAY CONTROL (Logical OR across active zones)
    if (heat_floor_2 || heat_floor_3) {
        digitalWrite(RELAY_PIN, HIGH); // Close relay contacts (Boiler/Valve ON)
    } else {
        digitalWrite(RELAY_PIN, LOW);  // Open relay contacts (Boiler/Valve OFF)
    }

    // 3. VISUAL STATUS DIAGNOSTICS
    if (isLinkBroken) {
        digitalWrite(ALARM_LED_PIN, HIGH); // Red Warning LED ON
        digitalWrite(LED_GREEN, LOW);      // Green Status LED OFF
    } else {
        digitalWrite(ALARM_LED_PIN, LOW);  // Red Warning LED OFF
        digitalWrite(LED_GREEN, HIGH);     // Green Status LED ON
    }
    
    delay(1000); // Execute main control loop once per second
}
