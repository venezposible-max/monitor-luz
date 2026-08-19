/*
  =============================================================================
  PROYECTO: Monitor de Luz e Internet (ESP8266 -> Servidor Railway)
  VERSIÓN: WiFiManager Ultra-Estable + Compatibilidad Garantizada
  =============================================================================
*/

#include <ESP8266WiFi.h>
#include <WiFiManager.h>         // Librería WiFiManager por tablatronix
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* RAILWAY_SERVER_URL = "https://monitor-luz-production.up.railway.app";

String deviceId = "";
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 60000;

void sendPingToRailway() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(250);
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String endpoint = String(RAILWAY_SERVER_URL) + "/api/ping";

  if (http.begin(client, endpoint)) {
    http.addHeader("Content-Type", "application/json");
    String jsonPayload = "{\"deviceId\":\"" + deviceId + "\",\"uptimeMs\":" + String(millis()) + "}";
    int httpCode = http.POST(jsonPayload);
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  uint32_t chipId = ESP.getChipId();
  char idBuffer[16];
  snprintf(idBuffer, sizeof(idBuffer), "ESP-%06X", chipId);
  deviceId = String(idBuffer);

  WiFiManager wifiManager;

  pinMode(0, INPUT_PULLUP);
  if (digitalRead(0) == LOW) {
    wifiManager.resetSettings();
  }

  if (!wifiManager.autoConnect("Configurar-Luz")) {
    delay(3000);
    ESP.restart();
  }

  sendPingToRailway();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(5000);
    return;
  }

  if (millis() - lastPingTime >= PING_INTERVAL || lastPingTime == 0) {
    sendPingToRailway();
    lastPingTime = millis();
  }
}
