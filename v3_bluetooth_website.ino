#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#define SERVICE_UUID        "12345678-1234-1234-1234-123456789abc"
#define CHARACTERISTIC_UUID "87654321-4321-4321-4321-cba987654321"

// LED pins
const int ledPins[] = {13, 12, 14, 27, 26};
const int numLEDs = 5;

// Button pins
const int buttonPins[] = {25, 33, 32, 4, 15};

// BLE globals
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// Game state
bool gameRunning = false;
unsigned long gameStartTime = 0;
unsigned long gameDuration = 0;
int currentLED = -1;
int score = 0;

// Debounce
unsigned long lastButtonPress[5] = {0, 0, 0, 0, 0};
const unsigned long debounceDelay = 150;

// Forward declarations
void startGame(int duration);
void stopGame();

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) override { 
    deviceConnected = true; 
    Serial.println("BLE client connected");
  }
  void onDisconnect(BLEServer* pServer) override { 
    deviceConnected = false; 
    Serial.println("BLE client disconnected");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    String rx = String(pChar->getValue().c_str());
    Serial.print("Received: ");
    Serial.println(rx);

    if (rx.length() == 0) return;

    if (rx.startsWith("START:")) {
      int duration = rx.substring(6).toInt();
      startGame(duration);
    } else if (rx == "STOP") {
      stopGame();
    }
  }
};

void startGame(int duration) {
  if (!deviceConnected) {
    Serial.println("Can't start - no BLE client");
    return;
  }

  gameRunning = true;
  gameStartTime = millis();
  gameDuration = (unsigned long)duration * 1000UL;
  score = 0;
  currentLED = -1;

  Serial.print("Game started for ");
  Serial.print(duration);
  Serial.println(" seconds");

  String s = "SCORE:";
  s += score;
  pCharacteristic->setValue(s);
  pCharacteristic->notify();

  selectRandomLED(); // Light the first LED
}

void stopGame() {
  if (!gameRunning) return;
  gameRunning = false;
  turnOffAllLEDs();

  String f = "FINAL:";
  f += score;
  pCharacteristic->setValue(f);
  pCharacteristic->notify();

  String clear = "LED:-1"; // no LED active
  pCharacteristic->setValue(clear);
  pCharacteristic->notify();

  Serial.print("Game stopped. Final score: ");
  Serial.println(score);
}

void notifyScore() {
  if (!deviceConnected) return;
  String msg = "SCORE:";
  msg += score;
  pCharacteristic->setValue(msg);
  pCharacteristic->notify();
  Serial.println("Sent: " + msg);
}

void notifyLED(int ledIndex) {
  if (!deviceConnected) return;
  String msg = "LED:";
  msg += ledIndex;
  pCharacteristic->setValue(msg);
  pCharacteristic->notify();
  Serial.println("Sent: " + msg);
}

void notifyWrong() {
  if (!deviceConnected) return;
  pCharacteristic->setValue("WRONG");
  pCharacteristic->notify();
  Serial.println("Sent: WRONG");
}

void turnOffAllLEDs() {
  for (int i = 0; i < numLEDs; i++) digitalWrite(ledPins[i], LOW);
}

void selectRandomLED() {
  turnOffAllLEDs();
  int newLED;
  do {
    newLED = random(0, numLEDs);
  } while (newLED == currentLED && numLEDs > 1);
  currentLED = newLED;
  digitalWrite(ledPins[currentLED], HIGH);
  notifyLED(currentLED);
  Serial.print("LED ");
  Serial.print(currentLED);
  Serial.println(" active");
}

void handleGameLogic() {
  if (!gameRunning) return;

  unsigned long now = millis();

  // End game when time is up
  if (now - gameStartTime >= gameDuration) {
    stopGame();
    return;
  }

  // Wait for correct button press
  for (int i = 0; i < numLEDs; i++) {
    if (digitalRead(buttonPins[i]) == LOW) { // grounded
      if (now - lastButtonPress[i] > debounceDelay) {
        lastButtonPress[i] = now;
        Serial.print("Button ");
        Serial.print(i);
        Serial.println(" pressed");
        if (i == currentLED) {
          score++;
          notifyScore();
          selectRandomLED(); // choose next LED only after correct press
        } else {
          notifyWrong();
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  for (int i = 0; i < numLEDs; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  randomSeed(analogRead(0));

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

  pServer->getAdvertising()->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();

  Serial.println("BLE server started. Waiting for client...");
}

void loop() {
  handleGameLogic();
  delay(10);
}
