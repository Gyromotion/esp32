#include <WiFi.h>
#include <esp_now.h>

#define NUM_LEDS 5
#define Sec 10   // game duration in seconds

// MAC addresses of LED nodes
uint8_t ledMacs[NUM_LEDS][6] = {
  {0x80, 0xF3, 0xDA, 0x4A, 0x6E, 0xF8}, // LED1
  {0x80, 0xF3, 0xDA, 0x42, 0x7B, 0xCC}, // LED2
  {0x80, 0xF3, 0xDA, 0x42, 0xF5, 0x28}, // LED3
  {0x80, 0xF3, 0xDA, 0x54, 0x42, 0xB4}, // LED4
  {0x80, 0xF3, 0xDA, 0x41, 0x44, 0x6C}  // LED5
};

typedef struct {
  int ledID;
  bool pressed;
  bool handshake;
} struct_message;

struct_message msg;
bool nodeReady[NUM_LEDS] = {false, false, false, false, false};

// ---------- ESP-NOW callbacks ----------
void onSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("Send status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Success" : "Fail");
}

// --- ESP-NOW receive callback ---
void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(struct_message)) return;

  struct_message *received = (struct_message*)incomingData;
  int idx = received->ledID - 1;

  if (idx >= 0 && idx < NUM_LEDS) {
    // Handshake handling
    if (received->handshake && !nodeReady[idx]) {
      nodeReady[idx] = true;
      Serial.printf("Handshake received from LED %d\n", received->ledID);

      // ✅ Immediately turn ON this LED
      int cmdOn = 1;  
      esp_err_t result = esp_now_send(ledMacs[idx], (uint8_t*)&cmdOn, sizeof(cmdOn));
      if (result == ESP_OK) {
        Serial.printf("LED %d ON command sent!\n", received->ledID);
      } else {
        Serial.printf("Failed to send ON command to LED %d\n", received->ledID);
      }
    }

    // Button pressed handling
    if (received->pressed) {
      Serial.printf("LED %d pressed!\n", received->ledID);
      nodeReady[idx] = true; // mark pressed
    }
  }
}


// ---------- Add peer ----------
bool addPeer(uint8_t mac[6]) {
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, mac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  while (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Retry adding peer...");
    delay(500);
  }
  return true;
}

// ---------- Send command ----------
void sendCommand(uint8_t mac[6], int command) {
  esp_now_send(mac, (uint8_t*)&command, sizeof(command));
}

// ---------- Game function ----------
void startGame() {
  Serial.println("Game starting...");
  unsigned long startTime = millis();
  unsigned long gameDuration = Sec * 1000;

  while (millis() - startTime < gameDuration) {
    int ledIndex = random(0, NUM_LEDS);

    sendCommand(ledMacs[ledIndex], 1); // ON
    Serial.printf("LED %d ON\n", ledIndex + 1);

    nodeReady[ledIndex] = false;
    while (!nodeReady[ledIndex]) {
      delay(50);
    }

    sendCommand(ledMacs[ledIndex], 0); // OFF
    Serial.printf("LED %d OFF (pressed)\n", ledIndex + 1);

    delay(200);
  }

  Serial.println("Game ended!");

  // Blink all once
  for (int b = 0; b < 2; b++) {
    for (int i = 0; i < NUM_LEDS; i++) sendCommand(ledMacs[i], 1);
    delay(200);
    for (int i = 0; i < NUM_LEDS; i++) sendCommand(ledMacs[i], 0);
    delay(200);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  Serial.println("Brain ESP32 Node");

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onDataRecv);

  for (int i = 0; i < NUM_LEDS; i++) {
    addPeer(ledMacs[i]);
    Serial.printf("Peer LED %d added\n", i + 1);
  }
}

void loop() {
  // Reset ready flags
  for (int i = 0; i < NUM_LEDS; i++) nodeReady[i] = false;

  // 🔄 Wait for all handshakes
  Serial.println("Waiting for all nodes...");
  while (true) {
    bool allReady = true;
    for (int i = 0; i < NUM_LEDS; i++) {
      if (!nodeReady[i]) allReady = false;
    }
    if (allReady) break;
    delay(200);
  }

  Serial.println("All nodes connected!");

  // Blink all LEDs 5 times
  for (int b = 0; b < 5; b++) {
    for (int i = 0; i < NUM_LEDS; i++) sendCommand(ledMacs[i], 0);
    delay(300);
    for (int i = 0; i < NUM_LEDS; i++) sendCommand(ledMacs[i], 1);
    delay(300);
  }

  // Turn OFF before game
  for (int i = 0; i < NUM_LEDS; i++) sendCommand(ledMacs[i], 0);

  // Start game
  startGame();

  // After game → restart handshake loop again
}
