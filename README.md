# Smart Bin System
A smart waste bin system using ESP32 and Arduino Uno with ultrasonic sensors for both fill level monitoring and touchless opening.
## Overview
This project implements a smart bin with two key features:

#### Fill Level Monitoring: Tracks how full the bin is and reports data to an MQTT dashboard
#### Touchless Opening: Detects hand presence to automatically open the bin lid via servo motor

## Hardware Components

1. ESP32 microcontroller
2. Arduino Uno
3. 2× Ultrasonic sensors (HC-SR04)
4. Servo motor
5. Power supply
6. Connecting wires
7. Smart bin enclosure

## System Architecture
### ESP32 Functions

1. Controls both ultrasonic sensors
2. Measures bin fill level using one ultrasonic sensor
3. Detects hand presence using the second ultrasonic sensor
4. Sends fill level data to MQTT dashboard
5. Communicates with Arduino Uno via UART when hand is detected

### Arduino Uno Functions

1. Programmed in AVR Assembly
2. Receives hand detection signal from ESP32 via UART
3. Controls servo motor to open/close bin lid

## Software Setup
### ESP32 Code
The ESP32 code is written in Arduino framework and handles:

### WiFi connection
MQTT client configuration and publishing
Ultrasonic sensor readings
UART communication with Arduino Uno

### Arduino Uno Code
The Arduino code is written in AVR Assembly and handles:

1. UART communication with ESP32
2. Servo motor control logic

## Installation & Setup

1. Clone this repository
2. Install required libraries:

PubSubClient (for MQTT)
WiFi library for ESP32


### Configure the ESP32:

1. Update WiFi credentials
2. Configure MQTT broker address and credentials
3. Set pin configurations for ultrasonic sensors
4. Configure UART parameters


5. Upload AVR Assembly code to Arduino Uno
6. Upload ESP32 code
7. Wire components according to the pin configuration in the code and in connections.txt