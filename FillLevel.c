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

// Ultrasonic Sensor Pins
const int TRIG_PIN = 19;
const int ECHO_PIN = 18;

// Bin Dimensions (cm)
const float BIN_HEIGHT = 6.0;
const float DEFAULT_FILL_LEVEL = 10.0;

WiFiClient espClient;
PubSubClient mqttClient(espClient);

unsigned long lastPublishTime = 0;
const long publishInterval = 10000;  // Publish every 10 seconds
bool firstReadingSent = false;

// Forward declaration of the publishFillLevel function
bool publishFillLevel(float fillPercentage, bool retained = false);

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Smart Dustbin System ===");

  // Configure sensor pins
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  setupWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setKeepAlive(60);  // Longer keep-alive for stability
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Publish at regular intervals
  unsigned long currentMillis = millis();
  if (currentMillis - lastPublishTime >= publishInterval) {
    lastPublishTime = currentMillis;
    
    // Always measure and display distance regardless of publishing
    float distance = measureDistance();
    Serial.print("📏 Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    
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
      
      // Send default reading with retained flag on first connect
      if (!firstReadingSent) {
        bool success = publishFillLevel(DEFAULT_FILL_LEVEL, true);
        if (success) {
          firstReadingSent = true;
          Serial.println("✅ Default fill level published with retained flag");
        }
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

float measureDistance() {
  // Clear the trigger pin
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Send 10μs pulse to trigger
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Measure the duration of the echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  
  // Calculate distance (speed of sound = 0.034 cm/μs)
  float distance = duration * 0.034 / 2;

  // Print raw duration for debugging
  Serial.print("📊 Echo duration: ");
  Serial.print(duration);
  Serial.println(" microseconds");

  if (duration == 0) {
    Serial.println("⚠️ Warning: Echo timeout - sensor may be disconnected or out of range");
    return -1;
  }
  
  if (distance <= 0 || distance > 400) {
    Serial.println("⚠️ Warning: Distance calculation out of valid range");
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

void measureAndPublishFillLevel() {
  float currentHeight = measureDistance();
  
  // Always display distance measurement regardless of validity
  Serial.print("📏 Current height: ");
  Serial.print(currentHeight);
  Serial.println(" cm");
  
  if (currentHeight <= 0 || currentHeight > BIN_HEIGHT) {
    Serial.println("⚠️ Invalid fill level measurement, using default value");
    publishFillLevel(DEFAULT_FILL_LEVEL);
    return;
  }

  // Calculate fill percentage (empty bin = 0%, full bin = 100%)
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
  
  publishFillLevel(fillPercentage);
}