# Bi-directional ESP-NOW Relay System

An end-to-end Hybrid IoT system developed as part of the **Capstone Project (Project H - Track B)**. This project demonstrates bi-directional communication between a local wireless sensor node and an internet-connected gateway node using **ESP-NOW** protocol and **MQTT**.

---

## 📌 Project Overview
This project implements a bi-directional relay control system. 

* **Sensor Node (Board A):** Reads environmental metrics (Temperature, Humidity, Light level), makes autonomous local decisions to trigger an onboard relay, sends structured telemetry to the Gateway via ESP-NOW, and listens for cloud override commands.
* **Gateway Node (Board B):** Receives local telemetry via ESP-NOW, bridges and publishes the data to a remote MQTT broker (`broker.hivemq.com`), subscribes to cloud control topics, and forwards incoming override commands back to Board A.

---

## 🏗️ System Architecture

```
+------------------------+      ESP-NOW       +------------------------+      MQTT (Wi-Fi)      +-------------------+
|  Sensor Node (Board A) | <================> |   Gateway (Board B)    | <====================> |    MQTT Broker    |
| (DHT11, LDR, Relay)    |  Bi-Directional    | (Wi-Fi Station Mode)   | (esp32/sensor/data)   |  (HiveMQ Cloud)   |
+------------------------+                    +------------------------+ (esp32/relay/control)  +-------------------+
```

1. **Uplink Dataflow:** `Board A Sensors` ➔ `ESP-NOW` ➔ `Board B Gateway` ➔ `MQTT Publish (JSON Payload)`
2. **Downlink Controlflow:** `MQTT Broker Topic` ➔ `Board B Gateway` ➔ `ESP-NOW Forwarding` ➔ `Board A Relay Override`

---

## 🛠️ Hardware Required

| Component | Quantity | Description |
| :--- | :---: | :--- |
| **ESP32 DevKit V1** | 2 | Board A (Sensor Node) & Board B (Gateway) |
| **DHT11 Sensor** | 1 | Temperature & Humidity Sensor |
| **LDR Module / Photoresistor** | 1 | Light Intensity Sensor |
| **1-Channel Relay Module** | 1 | Actuator for high-power load control |
| **Breadboards & Jumper Wires** | — | Circuit connections |
| **Micro-USB Cables** | 2 | Power & serial programming |

---

## 🔌 Wiring & Pinout Guide

### Board A (Sensor & Actuator Node)
| Peripheral | ESP32 GPIO | Description |
| :--- | :---: | :--- |
| **DHT11 Data** | `GPIO 4` | Digital temperature/humidity data |
| **LDR Signal** | `GPIO 34` | Analog light sensor input (ADC1) |
| **Relay Signal** | `GPIO 26` | Digital output for relay trigger |
| **VCC / GND** | `3.3V / GND` | Sensor power supply |

### Board B (Gateway Node)
| Connection | ESP32 Pin | Description |
| :--- | :---: | :--- |
| **Wi-Fi Connection** | Onboard Antenna | Connects to local AP for Internet access |
| **Power Input** | USB 5V | Micro-USB power source |

---

## 📚 Libraries Used

* [`esp_now.h`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html) - Native ESP-NOW protocol for fast peer-to-peer communication.
* [`WiFi.h`](https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/readme.html) - Built-in ESP32 Wi-Fi stack.
* [`PubSubClient.h`](https://github.com/knolleary/pubsubclient) - MQTT client library for IoT cloud integration.

---

## 📂 Repository Structure

```
├── Gateway_Node_BoardB/
│   └── Gateway_Node_BoardB.ino    # ESP32 Gateway firmware (ESP-NOW to MQTT)
├── Sensor_Node_BoardA/
│   └── Sensor_Node_BoardA.ino     # ESP32 Sensor firmware (Sensors, Relay & ESP-NOW)
├── docs/
│   └── circuit_diagram.png        # System architecture and circuit wiring schematic
└── README.md                      # Project documentation
```

---

## 🚀 How to Run
1. Obtain the **MAC Address** of Board A and update `nodeAddress[]` in Board B's code.
2. Flash `Sensor_Node_BoardA.ino` to the Sensor Node.
3. Flash `Gateway_Node_BoardB.ino` to the Gateway Node.
4. Monitor serial logs at **115200 baud** for connection and message verification.
5. Use an MQTT client (e.g., *MQTT Explorer*) to inspect telemetry on `esp32/sensor/data` or send control commands to `esp32/relay/control`.