#include "BluetoothSerial.h"

#define NUM_LEDS 5
int ledPins[NUM_LEDS]   = {13, 12, 14, 27, 26};
int inputPins[NUM_LEDS] = {25, 33, 32, 4, 15};

int currentLed = -1;
bool gameRunning = false;
unsigned long gameEndTime = 0;
int scoreCorrect = 0;
int scoreWrong = 0;
unsigned long debounceDelay = 200;

BluetoothSerial SerialBT;

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Game"); // Bluetooth name
  randomSeed(analogRead(0));

  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
    pinMode(inputPins[i], INPUT_PULLUP);
  }

  Serial.println("Waiting for Bluetooth commands...");
}

void loop() {
  // Handle Bluetooth commands
  if (SerialBT.available()) {
    String cmd = SerialBT.readStringUntil('\n');
    cmd.trim();

    if (cmd == "START") startGame(30); // default 30s
    else if (cmd.startsWith("TIMER:")) {
      int secs = cmd.substring(6).toInt();
      startGame(secs);
    }
    else if (cmd == "STOP") stopGame();
    else if (cmd == "SCORE?") sendScore();
  }

  // Game logic
  if (gameRunning) {
    if (millis() >= gameEndTime) {
      stopGame();
    } else {
      checkInput();
    }
  }
}

void startGame(int seconds) {
  scoreCorrect = 0;
  scoreWrong = 0;
  gameEndTime = millis() + (seconds * 1000UL);
  gameRunning = true;
  SerialBT.println("Game Started for " + String(seconds) + " seconds");
  nextLed();
}

void stopGame() {
  gameRunning = false;
  for (int i = 0; i < NUM_LEDS; i++) {
    digitalWrite(ledPins[i], LOW);
  }
  SerialBT.println("Game Over!");
  sendScore();
}

void sendScore() {
  SerialBT.println("Correct: " + String(scoreCorrect));
  SerialBT.println("Wrong: " + String(scoreWrong));
}

void checkInput() {
  for (int i = 0; i < NUM_LEDS; i++) {
    if (digitalRead(inputPins[i]) == LOW) {
      delay(debounceDelay);
      if (i == currentLed) {
        scoreCorrect++;
        SerialBT.println("Hit! Correct LED");
        digitalWrite(ledPins[currentLed], LOW);
        nextLed();
      } else {
        scoreWrong++;
        SerialBT.println("Wrong LED");
      }
    }
  }
}

void nextLed() {
  int next = random(NUM_LEDS);
  while (next == currentLed) {
    next = random(NUM_LEDS);
  }
  currentLed = next;
  digitalWrite(ledPins[currentLed], HIGH);
}
