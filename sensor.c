// ESP32 Code for Ultrasonic Sensor with UART Communication
// Controls Smart Dustbin by sending commands to Arduino UNO

const int trigPin = 5;   
const int echoPin = 2;  
const int ledPin = 23;  // Optional: Status LED on ESP32

long duration, dist, average;  
long aver[3];   // Array for average measurements
bool isOpen = false;  // Track current state to avoid repeated commands

void setup() {
  Serial.begin(9600);  // UART communication with Arduino UNO
  
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);  // Optional status LED
  
  // Initial delay to allow Arduino to boot
  delay(2000);
  
  Serial.println("ESP32 Ultrasonic Sensor initialized");
  delay(1000);
}

// Function to measure distance
void measure() { 
  digitalWrite(trigPin, LOW);
  delayMicroseconds(5);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(15);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH);
  dist = (duration/2) / 29.1;    // Calculate distance in cm
}

void loop() {
  // Take average of 3 measurements
  for (int i = 0; i <= 2; i++) {
    measure();              
    aver[i] = dist;           
    delay(10);
  }
  
  dist = (aver[0] + aver[1] + aver[2]) / 3;
  
  // Send distance data to Arduino UNO via UART (once per second)
  static unsigned long lastDistanceUpdate = 0;
  if (millis() - lastDistanceUpdate > 1000) {
    Serial.print("D:");  // Distance command prefix
    Serial.println(dist);
    lastDistanceUpdate = millis();
  }
  
  // Only send open/close commands when state changes
  if (dist < 50 && !isOpen) {
    // If distance is less than 50cm and lid is closed, open it
    Serial.println("O:1");  // Open command
    digitalWrite(ledPin, HIGH);  // Optional: Turn on LED
    isOpen = true;
    delay(1000);  // Give Arduino time to process and respond
  } 
  else if (dist >= 50 && isOpen) {
    // If distance is 50cm or more and lid is open, close it
    Serial.println("O:0");  // Close command
    digitalWrite(ledPin, LOW);  // Optional: Turn off LED
    isOpen = false;
    delay(1000);  // Give Arduino time to process and respond
  }
  
  delay(300);  // Small delay before next reading
}