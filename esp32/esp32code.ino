#include "FirebaseESP32.h"  // Install Firebase ESP32 library by Mobizt v4.3.4
#include <WebServer.h>
#include <WiFi.h>
#include <esp32cam.h>

#define WIFI_SSID "vivo 1983"
#define WIFI_PASSWORD "12345678"

#define FIREBASE_HOST "facedetection-92a73-default-rtdb.firebaseio.com"
#define FIREBASE_AUTH "AIzaSyA7oFE-if_vx3D4nQ4e7KKcwZo-Dej9qhQ"

String FIREBASE_BUCKET = "1RV22CS211";  // Replace with your USN

FirebaseData firebaseData;

WebServer server(80);
static auto loRes = esp32cam::Resolution::find(320, 240);
static auto midRes = esp32cam::Resolution::find(350, 530);
static auto hiRes = esp32cam::Resolution::find(800, 600);

#define SWITCH_PIN 12
#define SERVO_PIN 13  // Define the GPIO pin connected to the servo
#define FLASH_PIN 4   // Define the GPIO pin connected to the flash LED

// Define debounce variables
int lastSwitchState = HIGH;
int currentSwitchState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;  // Adjust debounce delay as needed

// Define PWM parameters
const int servoMinPulseWidth = 1000;  // Minimum pulse width in microseconds
const int servoMaxPulseWidth = 2000;  // Maximum pulse width in microseconds
const int servoFreq = 50;  // PWM frequency in Hz

void setupPWM() {
  ledcSetup(0, servoFreq, 16);  // Channel 0, 50 Hz, 16-bit resolution
  ledcAttachPin(SERVO_PIN, 0);  // Attach pin to channel 0
}

void setServoAngle(int angle) {
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;

  // Map angle to pulse width
  int pulseWidth = map(angle, 0, 180, servoMinPulseWidth, servoMaxPulseWidth);

  // Calculate duty cycle
  int dutyCycle = (pulseWidth * 65536) / 20000;  // 65536 is 2^16 for 16-bit resolution

  // Debugging information
  Serial.print("Angle: ");
  Serial.print(angle);
  Serial.print(", Pulse Width: ");
  Serial.print(pulseWidth);
  Serial.print(", Duty Cycle: ");
  Serial.println(dutyCycle);

  // Set PWM duty cycle
  ledcWrite(0, dutyCycle);
}

void serveJpg() {
  digitalWrite(FLASH_PIN, HIGH);  // Turn on the flash LED

  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    digitalWrite(FLASH_PIN, LOW);  // Turn off the flash LED
    return;
  }
  Serial.printf("CAPTURE OK %dx%d %db\n", frame->getWidth(), frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);

  digitalWrite(FLASH_PIN, LOW);  // Turn off the flash LED
}

void handleJpgLo() {
  if (!esp32cam::Camera.changeResolution(loRes)) {
    Serial.println("SET-LO-RES FAIL");
  }
  serveJpg();
}

void handleJpgHi() {
  if (!esp32cam::Camera.changeResolution(hiRes)) {
    Serial.println("SET-HI-RES FAIL");
  }
  serveJpg();
}

void handleJpgMid() {
  if (!esp32cam::Camera.changeResolution(midRes)) {
    Serial.println("SET-MID-RES FAIL");
  }
  serveJpg();
}

void setupWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  while (!Serial) ;  // Wait for serial port to initialize

  setupWiFi();

  pinMode(SWITCH_PIN, INPUT_PULLUP);  // Set the switch pin as input with internal pull-up resistor
  pinMode(FLASH_PIN, OUTPUT);         // Set the flash pin as output
  digitalWrite(FLASH_PIN, LOW);       // Ensure the flash is off initially

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  // Camera setup
  using namespace esp32cam;
  Config cfg;
  cfg.setPins(pins::AiThinker);
  cfg.setResolution(hiRes);
  cfg.setBufferCount(2);
  cfg.setJpeg(80);

  bool ok = Camera.begin(cfg);
  Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");

  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam-lo.jpg");
  Serial.println("  /cam-hi.jpg");
  Serial.println("  /cam-mid.jpg");

  server.on("/cam-lo.jpg", handleJpgLo);
  server.on("/cam-hi.jpg", handleJpgHi);
  server.on("/cam-mid.jpg", handleJpgMid);

  server.begin();

  setupPWM();  // Initialize PWM for servo control
}

void rotateServoClockwise() {
  for (int pos = 0; pos <= 180; pos += 5) {  // Sweep from 0 to 180 degrees
    setServoAngle(pos);
    delay(30);  // Shorten delay for faster rotation
  }
}

void rotateServoCounterClockwise() {
  for (int pos = 180; pos >= 0; pos -= 5) {  // Sweep from 180 to 0 degrees
    setServoAngle(pos);
    delay(30);  // Shorten delay for faster rotation
  }
}

void loop() {
  int reading = digitalRead(SWITCH_PIN);

  // If the switch state changed (due to noise or pressing)
  if (reading != lastSwitchState) {
    lastDebounceTime = millis();
  }

  // If the switch state has stabilized
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != currentSwitchState) {
      currentSwitchState = reading;

      if (currentSwitchState == LOW) {  // Switch is ON (active LOW)
        if (Firebase.setInt(firebaseData, String(FIREBASE_BUCKET) + "/entry", 1)) {
          Serial.println("Switch ON, entry set to 1");
        } else {
          Serial.println("Failed to update entry to 1");
        }
      } else {  // Switch is OFF
        if (Firebase.setInt(firebaseData, String(FIREBASE_BUCKET) + "/entry", 0)) {
          Serial.println("Switch OFF, entry set to 0");
        } else {
          Serial.println("Failed to update entry to 0");
        }
      }
    }
  }

  lastSwitchState = reading;

  server.handleClient();  // Handle image requests

  // Check Firebase for the servo control value
  if (Firebase.getInt(firebaseData, String(FIREBASE_BUCKET) + "/access")) {
    int accessValue = firebaseData.intData();
    if (accessValue == 1) {
      rotateServoClockwise();
      delay(4000);  // Add a short delay between rotations
      rotateServoCounterClockwise();
    }
  } else {
    Serial.println("Failed to read access value");
  }

  delay(100);  // Adjust delay as needed for the loop
}