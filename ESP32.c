#include <WiFi.h>
#include <PubSubClient.h>

// WiFi Credentials
const char* WIFI_SSID = "TECNOSPARK4";
const char* WIFI_PASSWORD = "Amir0017";

// MQTT Broker Details
const char* MQTT_BROKER = "broker.hivemq.com";
const int MQTT_PORT = 1883;
const char* CLIENT_ID = "smartbin_017";
const char* TOPIC_FILL_LEVEL = "smartdustbin/filllevel";
const char* TOPIC_LID_STATE = "smartdustbin/lidstate";

// Ultrasonic Sensor Pins for Fill Level
const int FILL_TRIG_PIN = 19;
const int FILL_ECHO_PIN = 23;

// Ultrasonic Sensor Pins for Proximity Detection (Lid Control)
const int PROX_TRIG_PIN = 5;
const int PROX_ECHO_PIN = 2;

// Status LED Pin
const int LED_PIN = 22;

// Bin Dimensions (cm)
const float BIN_HEIGHT = 42.0;
const float DEFAULT_FILL_LEVEL = 20.0;
const float LID_PROXIMITY_THRESHOLD = 50.0; // cm

// Timing constants
const long fillPublishInterval = 10000;      // Publish fill level every 10 seconds
const long proximityCheckInterval = 200;     // Check proximity every 200ms (reduced from 300ms)

// Serial communication with Arduino for lid control
const int ARDUINO_BAUD_RATE = 9600;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastFillPublishTime = 0;
unsigned long lastProximityCheckTime = 0;

bool firstReadingSent = false;
bool isLidOpen = false;  // Track current lid state

// Forward declarations
bool publishFillLevel(float fillPercentage, bool retained);
bool publishLidState(bool isOpen, bool retained);
float measureDistance(int trigPin, int echoPin);

void setup() {
  Serial.begin(ARDUINO_BAUD_RATE);  // UART communication with Arduino UNO
  Serial.println("\n=== Smart Dustbin System ===");

  // Configure sensor pins
  pinMode(FILL_TRIG_PIN, OUTPUT);
  pinMode(FILL_ECHO_PIN, INPUT);
  pinMode(PROX_TRIG_PIN, OUTPUT);
  pinMode(PROX_ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);  // Longer keep-alive for stability
  
  // Initial delay to allow Arduino to boot
  delay(2000);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Get current time once to use for both checks
  unsigned long currentMillis = millis();
  
  // Proximity detection for lid control (more frequent checks)
  // Check proximity first for better responsiveness
  if (currentMillis - lastProximityCheckTime >= proximityCheckInterval) {
    lastProximityCheckTime = currentMillis;
    checkProximityAndControlLid();
  }
  
  // Fill level publishing at regular intervals
  if (currentMillis - lastFillPublishTime >= fillPublishInterval) {
    lastFillPublishTime = currentMillis;
    measureAndPublishFillLevel();
  }
}

void setupWiFi() {
  delay(10);
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
  int retry = 0;
  while (!mqttClient.connected() && retry < 3) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID to prevent conflicts
    String clientId = CLIENT_ID;
    clientId += String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("✅ Connected to MQTT Broker");
      
      // Send default readings with retained flag on first connect
      if (!firstReadingSent) {
        publishFillLevel(DEFAULT_FILL_LEVEL, true);
        publishLidState(isLidOpen, true);
        firstReadingSent = true;
        Serial.println("✅ Default values published with retained flag");
      }
    } else {
      Serial.print("❌ MQTT failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" - retrying...");
      retry++;
      delay(2000);
    }
  }
}

float measureDistance(int trigPin, int echoPin) {
  // Clear the trigger pin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Send 10μs pulse to trigger
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Measure the duration of the echo pulse with shorter timeout
  // Reduced from 30000 to 15000 for faster readings
  long duration = pulseIn(echoPin, HIGH, 15000);
  
  // Calculate distance (speed of sound = 0.034 cm/μs)
  float distance = duration * 0.034 / 2;

  if (duration == 0) {
    // Only log timeout for the fill level sensor to reduce serial noise
    if (trigPin == FILL_TRIG_PIN) {
      Serial.println("⚠️ Warning: Echo timeout - sensor may be disconnected or out of range");
    }
    return -1;
  }
  
  if (distance < 0.5 || distance > 400) {
    // Only log for fill level sensor
    if (trigPin == FILL_TRIG_PIN) {
      Serial.println("⚠️ Warning: Distance calculation out of valid range");
    }
    return -1;
  }

  return distance;
}

bool publishFillLevel(float fillPercentage, bool retained) {
  // Format with one decimal place - worked with gauge widget
  char msg[10];
  snprintf(msg, sizeof(msg), "%.1f", fillPercentage);
  
  // Use QoS 0 to match panel settings, with optional retained flag
  bool success = mqttClient.publish(TOPIC_FILL_LEVEL, msg, retained);

  Serial.print("📤 Fill Level: ");
  Serial.print(msg);
  Serial.print("% - MQTT Status: ");
  Serial.println(success ? "Published ✓" : "Failed ✗");

  return success;
}

bool publishLidState(bool isOpen, bool retained) {
  const char* msg = isOpen ? "OPEN" : "CLOSED";
  
  // Use QoS 0 with optional retained flag
  bool success = mqttClient.publish(TOPIC_LID_STATE, msg, retained);

  Serial.print("📤 Lid State: ");
  Serial.print(msg);
  Serial.print(" - MQTT Status: ");
  Serial.println(success ? "Published ✓" : "Failed ✗");

  return success;
}

void measureAndPublishFillLevel() {
  float currentHeight = measureDistance(FILL_TRIG_PIN, FILL_ECHO_PIN);
  
  // Always display distance measurement regardless of validity
  Serial.print("📏 Current height: ");
  Serial.print(currentHeight);
  Serial.println(" cm");
  
  if (currentHeight <= 0 || currentHeight > BIN_HEIGHT) {
    Serial.println("⚠️ Invalid fill level measurement, using default value");
    publishFillLevel(DEFAULT_FILL_LEVEL, false);
    return;
  }

  float fillPercentage = ((BIN_HEIGHT - currentHeight) / BIN_HEIGHT) * 100;
  
  // Ensure values stay within valid range
  if (fillPercentage < 0) fillPercentage = 0;
  if (fillPercentage > 100) fillPercentage = 100;
  
  // Display calculation details
  Serial.print("🧮 Calculation: ((");
  Serial.print(BIN_HEIGHT);
  Serial.print(" - ");
  Serial.print(currentHeight);
  Serial.print(") / ");
  Serial.print(BIN_HEIGHT);
  Serial.print(") * 100 = ");
  Serial.print(fillPercentage);
  Serial.println("%");
  
  publishFillLevel(fillPercentage, false);
}

void checkProximityAndControlLid() {
  // Take average of 3 measurements for stability
  float distances[3];
  float avgDistance = 0;
  
  for (int i = 0; i < 3; i++) {
    distances[i] = measureDistance(PROX_TRIG_PIN, PROX_ECHO_PIN);
    if (distances[i] > 0) { // Only include valid readings
      avgDistance += distances[i];
    } else {
      // If invalid reading, try again
      i--;
      delay(10);
      continue;
    }
    delay(10);
  }
  
  avgDistance /= 3;
  
  // Send distance data to Arduino UNO via UART
  Serial.print("D:");
  Serial.println(avgDistance);
  
  // Only send open/close commands when state changes
  if (avgDistance < LID_PROXIMITY_THRESHOLD && !isLidOpen) {
    // If hand is detected and lid is closed, send open command
    Serial.println("O:1");  // Open command
    digitalWrite(LED_PIN, HIGH);  // Turn on status LED
    isLidOpen = true;
    publishLidState(isLidOpen, false);  // Publish state change to MQTT
    delay(100);  // Small delay to avoid command flooding
  } 
  else if (avgDistance >= LID_PROXIMITY_THRESHOLD && isLidOpen) {
    // If no hand detected and lid is open, send close command
    Serial.println("O:0");  // Close command
    digitalWrite(LED_PIN, LOW);  // Turn off status LED
    isLidOpen = false;
    publishLidState(isLidOpen, false);  // Publish state change to MQTT
    delay(100);  // Small delay to avoid command flooding
  }
}