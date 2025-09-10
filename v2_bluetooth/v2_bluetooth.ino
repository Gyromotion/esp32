#include "BLEDevice.h"
#include "BLEServer.h"
#include "BLEUtils.h"
#include "BLE2902.h"

// BLE Configuration - Must match web app
#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

// Pin Definitions
const int LED_PINS[] = {2, 4, 5, 18, 19};        // LED pins (Red, Green, Blue, Yellow, Purple)
const int BUTTON_PINS[] = {12, 13, 14, 27, 26};  // Button pins corresponding to LEDs
const int NUM_LEDS = 5;

// Game Variables
bool gameActive = false;
int currentLED = -1;
int score = 0;
unsigned long gameStartTime = 0;
unsigned long gameDuration = 30000; // Default 30 seconds
unsigned long lastLEDChange = 0;
unsigned long ledChangeInterval = 1500; // Change LED every 1.5 seconds

// BLE Variables
BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Button debouncing
unsigned long lastButtonPress[NUM_LEDS] = {0};
const unsigned long debounceDelay = 50;

// Function declarations
void startGame(int duration);
void stopGame();
void endGame();
void handleGameLogic();
void changeToRandomLED();
void checkButtons();
void handleButtonPress(int buttonIndex);
void sendData(String data);
void testLEDs();

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected");
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String value = pCharacteristic->getValue().c_str();
      Serial.println("Received: " + value);
      
      if (value.startsWith("START:")) {
        int duration = value.substring(6).toInt();
        startGame(duration);
      } else if (value == "STOP") {
        stopGame();
      }
    }
};

void setup() {
  Serial.begin(115200);
  Serial.println("ESP32 Reaction Game Starting...");

  // Initialize pins
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(LED_PINS[i], OUTPUT);
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    digitalWrite(LED_PINS[i], LOW);
  }

  // Initialize BLE
  BLEDevice::init("ESP32_Reaction_Game");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);
  BLEDevice::startAdvertising();
  
  Serial.println("BLE Server started, waiting for connections...");
  
  // Test LEDs on startup
  testLEDs();
}

void loop() {
  // Handle BLE connection changes
  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    Serial.println("Start advertising");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  // Game logic
  if (gameActive) {
    handleGameLogic();
    checkButtons();
    
    // Check if game time is up
    if (millis() - gameStartTime >= gameDuration) {
      endGame();
    }
  }
  
  delay(10);
}

void startGame(int duration) {
  if (!deviceConnected) return;
  
  Serial.println("Starting game for " + String(duration) + " seconds");
  
  gameActive = true;
  score = 0;
  gameDuration = duration * 1000; // Convert to milliseconds
  gameStartTime = millis();
  lastLEDChange = millis();
  
  // Turn off all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  
  // Start with first LED
  changeToRandomLED();
  
  // Send initial score
  sendData("SCORE:" + String(score));
}

void stopGame() {
  if (!gameActive) return;
  
  Serial.println("Game stopped");
  endGame();
}

void endGame() {
  gameActive = false;
  currentLED = -1;
  
  // Turn off all LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], LOW);
  }
  
  // Send final score
  sendData("FINAL:" + String(score));
  sendData("LED:0"); // Clear active LED display
  
  Serial.println("Game ended. Final score: " + String(score));
}

void handleGameLogic() {
  // Change LED at intervals
  if (millis() - lastLEDChange >= ledChangeInterval) {
    changeToRandomLED();
    lastLEDChange = millis();
  }
}

void changeToRandomLED() {
  // Turn off current LED
  if (currentLED >= 0) {
    digitalWrite(LED_PINS[currentLED], LOW);
  }
  
  // Choose new random LED (different from current)
  int newLED;
  do {
    newLED = random(0, NUM_LEDS);
  } while (newLED == currentLED && NUM_LEDS > 1);
  
  currentLED = newLED;
  digitalWrite(LED_PINS[currentLED], HIGH);
  
  // Send LED update to web app
  sendData("LED:" + String(currentLED + 1)); // Send 1-based index
  
  Serial.println("LED " + String(currentLED + 1) + " is now active");
}

void checkButtons() {
  for (int i = 0; i < NUM_LEDS; i++) {
    // Check if button is pressed (with debouncing)
    if (digitalRead(BUTTON_PINS[i]) == LOW && 
        millis() - lastButtonPress[i] > debounceDelay) {
      
      lastButtonPress[i] = millis();
      handleButtonPress(i);
    }
  }
}

void handleButtonPress(int buttonIndex) {
  Serial.println("Button " + String(buttonIndex + 1) + " pressed");
  
  if (buttonIndex == currentLED) {
    // Correct button pressed
    score++;
    sendData("SCORE:" + String(score));
    Serial.println("Correct! Score: " + String(score));
    
    // Immediately change to new LED
    changeToRandomLED();
    lastLEDChange = millis();
    
    // Increase difficulty by reducing interval slightly
    if (ledChangeInterval > 800) {
      ledChangeInterval -= 50;
    }
    
  } else {
    // Wrong button pressed
    sendData("WRONG");
    Serial.println("Wrong button!");
  }
}

void sendData(String data) {
  if (deviceConnected) {
    pCharacteristic->setValue(data.c_str());
    pCharacteristic->notify();
    Serial.println("Sent: " + data);
  }
}

void testLEDs() {
  Serial.println("Testing LEDs...");
  
  // Light up each LED in sequence
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(LED_PINS[i], HIGH);
    delay(200);
    digitalWrite(LED_PINS[i], LOW);
    delay(100);
  }
  
  // Flash all LEDs
  for (int flash = 0; flash < 3; flash++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(LED_PINS[i], HIGH);
    }
    delay(200);
    for (int i = 0; i < NUM_LEDS; i++) {
      digitalWrite(LED_PINS[i], LOW);
    }
    delay(200);
  }
  
  Serial.println("LED test complete");
}