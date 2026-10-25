#include <WiFi.h>
#include <esp_now.h>
#include <PubSubClient.h>

const char* ssid = "Giza-NTI";
const char* password = "NT!##22N7";
const char* mqtt_server = "broker.hivemq.com";

WiFiClient espClient;
PubSubClient client(espClient);

// Replace with MAC address from sender board's serial output
uint8_t nodeAddress[] = {0x84, 0x1F, 0xE8, 0x16, 0xDE, 0x38};

// ─── MESSAGE STRUCTURE
typedef struct struct_message {
    float temp;
    float hum;
    int ldrVal;
    int relayState;
    int source;
} struct_message;

struct_message incomingReadings;
struct_message outgoingCommand;

// Receive data from the Node via ESP-NOW and publish it to MQTT
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  
  Serial.printf("Received -> Temp: %.2f | Hum: %.2f | LDR: %d | Relay: %d\n", 
                incomingReadings.temp, incomingReadings.hum, 
                incomingReadings.ldrVal, incomingReadings.relayState);

  char payload[150];
  snprintf(payload, sizeof(payload), "{\"temp\":%.2f, \"hum\":%.2f, \"ldr\":%d, \"relay\":%d}", 
           incomingReadings.temp, incomingReadings.hum, incomingReadings.ldrVal, incomingReadings.relayState);
  
// Upload data to the cloud
client.publish("esp32/sensor/data", payload);
}

// Receive remote control commands from the cloud via MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.print("MQTT Command received [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "esp32/relay/control") {
    int command = message.toInt();
    outgoingCommand.relayState = command;

// Wirelessly forward the command to the Sensor Node via ESP-NOW
    esp_err_t result = esp_now_send(nodeAddress, (uint8_t *) &outgoingCommand, sizeof(outgoingCommand));
    if (result == ESP_OK) {
      Serial.println("Override command successfully forwarded to Node via ESP-NOW!");
    } else {
      Serial.println("Failed to forward command");
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT Broker...");
    if (client.connect("ESP32GatewayClient_Nouran")) {
      Serial.println("Connected!");
      client.subscribe("esp32/relay/control"); // Subscribe to the control topic
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nGateway WiFi Connected.");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, nodeAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add node peer");
    return;
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
