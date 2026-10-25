/*
 *          This ESP32 node reads environmental telemetry from a DHT11 sensor and an LDR light 
 *          sensor, executes local automated or manual relay control decisions, and transmits 
 *          data wirelessly via ESP-NOW to the Gateway (Board B). It also receives bi-directional 
 *          override commands from the gateway to switch between manual and automatic modes.
  */
#include <WiFi.h>
#include <esp_now.h>
#include <DHT.h>

//  Pin Definitions
#define DHTPIN 4
#define DHTTYPE DHT11
#define LDR_PIN 34
#define RELAY_PIN 26

DHT dht(DHTPIN, DHTTYPE);

// MAC Address of Board B (Gateway)
uint8_t gatewayAddress[] = {
  0x44, 0x1D, 0x64, 0xF4, 0x16, 0xBC
};

// Automatic Mode Settings (Mode 2) 
// Relay turns ON if temperature is higher than this value
const float TEMP_ON_THRESHOLD_C = 30.0;

// Relay turns ON if light level is low
// With wiring configuration (LDR to 3.3V and 10k to GND): darkness = lower raw reading
const int LIGHT_ON_THRESHOLD_RAW = 2000;

// true if HIGH triggers the relay, false if LOW triggers it
const bool RELAY_ACTIVE_HIGH = true;

// This structure must match the structure in Board B exactly
typedef struct struct_message {
  float temp;
  float hum;
  int ldrVal;
  int relayState;  // Command state: 0, 1, or 2
  int source;
} struct_message;

struct_message myData;

// Modes: 0 = Manual OFF, 1 = Manual ON, 2 = Automatic
volatile int pendingMode = 2;
volatile bool modeCommandReceived = false;
int currentMode = 2;

bool relayIsOn = false;

// Last valid temperature and humidity readings
float lastGoodTemp = 0;
float lastGoodHum = 0;
bool hasGoodReading = false;

unsigned long lastReadTime = 0;
const unsigned long READ_INTERVAL_MS = 5000;

void setRelay(bool turnOn) {
  relayIsOn = turnOn;
  if (RELAY_ACTIVE_HIGH) {
    digitalWrite(RELAY_PIN, turnOn ? HIGH : LOW);
  } else {
    digitalWrite(RELAY_PIN, turnOn ? LOW : HIGH);
  }
}

// Some DHT11 sensors return a wrong negative temperature value (e.g., -21 instead of 20).
// This function fixes the sign if needed. If the reading is valid, it leaves it as is.
float readTempFixed() {
  float t = dht.readTemperature();
  if (!isnan(t) && t < 0) {
    t = round(-1.0 - t);
  }
  return t;
}

// Receive incoming commands (ESP32 Arduino Core 3.x)
void OnDataRecv(const esp_now_recv_info_t *info,
                const uint8_t *incomingBytes,
                int len) {
  if (info == nullptr || incomingBytes == nullptr) return;
  if (len != sizeof(struct_message)) return;

  struct_message receivedData;
  memcpy(&receivedData, incomingBytes, sizeof(receivedData));

  if (receivedData.relayState < 0 || receivedData.relayState > 2) return;

  pendingMode = receivedData.relayState;
  modeCommandReceived = true;
}

// Transmission status callback (ESP32 Arduino Core 3.x)
void OnDataSent(const wifi_tx_info_t *txInfo,
                esp_now_send_status_t status) {
  (void)txInfo;
  Serial.print("ESP-NOW Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LDR_PIN, INPUT);
  analogReadResolution(12);

  setRelay(false);

  dht.begin();
  delay(2000);  // Wait for DHT11 sensor to stabilize

  WiFi.mode(WIFI_STA);

  Serial.print("Board A MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Initialization Failed.");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, gatewayAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add Gateway as peer.");
    return;
  }

  lastReadTime = millis();
  Serial.println("Board A is ready.");
}

void loop() {
  // Apply the latest command received from Board B
  if (modeCommandReceived) {
    noInterrupts();
    int newMode = pendingMode;
    modeCommandReceived = false;
    interrupts();

    currentMode = newMode;

    Serial.print("New Mode: ");
    if (currentMode == 0) {
      Serial.println("Manual OFF");
    } else if (currentMode == 1) {
      Serial.println("Manual ON");
    } else {
      Serial.println("Automatic");
    }

    // Read sensors immediately after any mode change (without waiting for 5 seconds)
    lastReadTime = millis() - READ_INTERVAL_MS;
  }

  unsigned long now = millis();
  if (now - lastReadTime < READ_INTERVAL_MS) {
    return;
  }
  lastReadTime = now;

  // 1) Read Sensors First
  float humidity = dht.readHumidity();
  float temperature = readTempFixed();
  int lightValue = analogRead(LDR_PIN);

  // DHT11 measures between 0 and 50 degrees; any reading outside this range is considered invalid
  bool dhtValid = !isnan(humidity) && !isnan(temperature) &&
                  temperature >= 0 && temperature <= 50 &&
                  humidity >= 0 && humidity <= 100;

  if (dhtValid) {
    lastGoodTemp = temperature;
    lastGoodHum = humidity;
    hasGoodReading = true;
  } else {
    Serial.println("Invalid DHT reading; temperature will not be used for decision making.");
  }

  //  2) Make Decision Based on Mode 
  if (currentMode == 0) {
    relayIsOn = false;
  } else if (currentMode == 1) {
    relayIsOn = true;
  } else {
    // Automatic: High temperature or low light
    bool hot = dhtValid && (temperature > TEMP_ON_THRESHOLD_C);
    bool dark = (lightValue >= LIGHT_ON_THRESHOLD_RAW);
    relayIsOn = hot || dark;

    Serial.print("Auto -> High Temperature: ");
    Serial.print(hot ? "Yes" : "No");
    Serial.print(" | Low Light: ");
    Serial.println(dark ? "Yes" : "No");
  }

  setRelay(relayIsOn);

  //  3) Send Data to Board B 
  myData.temp = hasGoodReading ? lastGoodTemp : 0;
  myData.hum = hasGoodReading ? lastGoodHum : 0;
  myData.ldrVal = lightValue;
  myData.relayState = relayIsOn ? 1 : 0;
  myData.source = currentMode;

  Serial.print("Temp: ");
  Serial.print(myData.temp);
  Serial.print(" C | Humidity: ");
  Serial.print(myData.hum);
  Serial.print(" % | Light raw: ");
  Serial.print(lightValue);
  Serial.print(" | Mode: ");
  Serial.print(currentMode);
  Serial.print(" | Relay: ");
  Serial.println(relayIsOn ? "ON" : "OFF");

  esp_err_t result = esp_now_send(
    gatewayAddress,
    reinterpret_cast<const uint8_t *>(&myData),
    sizeof(myData)
  );

  if (result != ESP_OK) {
    Serial.print("Error sending data: ");
    Serial.println(result);
  }
}
