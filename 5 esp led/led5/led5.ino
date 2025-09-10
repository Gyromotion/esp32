#include <WiFi.h>
#include <esp_now.h>

#define LED_PIN 26
#define BUTTON_PIN 15
#define LED_ID 5  // Node ID

typedef struct {
  int ledID;
  bool pressed;
  bool handshake;
} struct_message;

struct_message msg;

uint8_t brainMac[] = {0x78, 0x1C, 0x3C, 0xC9, 0xEA, 0x00};

void onSent(const esp_now_send_info_t *info, const esp_now_send_status_t status) {}

void onDataRecv(const esp_now_recv_info *info, const uint8_t *incomingData, int len) {
  if (len != sizeof(int)) return;
  int command;
  memcpy(&command, incomingData, sizeof(int));

  if (command == 1) digitalWrite(LED_PIN, HIGH);
  else digitalWrite(LED_PIN, LOW);
}

void sendHandshake() {
  msg.ledID = LED_ID;
  msg.pressed = false;
  msg.handshake = true;
  esp_now_send(brainMac, (uint8_t*)&msg, sizeof(msg));
}

void sendPressed() {
  msg.ledID = LED_ID;
  msg.pressed = true;
  msg.handshake = false;
  esp_now_send(brainMac, (uint8_t*)&msg, sizeof(msg));
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_send_cb(onSent);
  esp_now_register_recv_cb(onDataRecv);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, brainMac, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  sendHandshake();
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    sendPressed();
    delay(200);
  }
}
