/*
  =============================================================================
  PROYECTO: Monitor de Luz e Internet en Casa (ESP8266 -> Servidor Railway)
  =============================================================================
  1. Si no hay WiFi guardado, crea el punto de acceso "Configurar-Luz".
  2. El usuario ingresa la red y clave de su casa en su celular.
  3. La placa genera su ID Único (ej. ESP-A1B2C3).
  4. Envía un latido HTTP POST a Railway cada 60 segundos.
  =============================================================================
*/

#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         // Librería: "WiFiManager" por tzapu
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

// =============================================================================
// REEMPLAZA CON LA URL DE TU SERVIDOR EN RAILWAY
// Ejemplo: "https://tu-monitor-luz.up.railway.app"
// =============================================================================
const char* RAILWAY_SERVER_URL = "https://monitor-luz-production.up.railway.app";
// =============================================================================

String deviceId = "";
unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 60000; // 60 segundos

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n============================================");
  Serial.println(" INICIANDO MONITOR DE LUZ (RAILWAY VERSION)");
  Serial.println("============================================");

  // 1. Obtener ID Único del ESP8266
  uint32_t chipId = ESP.getChipId();
  char idBuffer[16];
  snprintf(idBuffer, sizeof(idBuffer), "ESP-%06X", chipId);
  deviceId = String(idBuffer);

  Serial.println("[ID Único Dispositivo]: " + deviceId);

  // 2. Configurar Portal Cautivo ("Configurar-Luz")
  WiFiManager wifiManager;
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));

  Serial.println("[WiFi] Si no se conecta automáticamente, conéctate al WiFi: 'Configurar-Luz'");

  if (!wifiManager.autoConnect("Configurar-Luz")) {
    Serial.println("[WiFi] Error de conexión. Reiniciando...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("\n[WiFi] ¡CONECTADO CON ÉXITO!");
  Serial.print("[WiFi] IP asignada: ");
  Serial.println(WiFi.localIP());

  // 3. Imprimir enlace directo
  String estadoUrl = String(RAILWAY_SERVER_URL) + "/estado.html?id=" + deviceId;
  Serial.println("\n============================================");
  Serial.println(" ¡LISTO! Guarda este enlace en tu teléfono:");
  Serial.println(" 👉 " + estadoUrl);
  Serial.println("============================================\n");

  sendPingToRailway();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Conexión perdida. Reconectando...");
    WiFi.reconnect();
    delay(5000);
    return;
  }

  if (millis() - lastPingTime >= PING_INTERVAL || lastPingTime == 0) {
    sendPingToRailway();
    lastPingTime = millis();
  }
}

void sendPingToRailway() {
  // Parpadear el LED azul integrado durante 5 segundos (10 ciclos de 250ms encendido / 250ms apagado)
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 10; i++) {
    digitalWrite(LED_BUILTIN, LOW);  // Encender LED azul (Active LOW)
    delay(250);
    digitalWrite(LED_BUILTIN, HIGH); // Apagar LED azul
    delay(250);
  }

  WiFiClientSecure client;
  client.setInsecure(); // Omitir verificación SSL estricta en ESP8266

  HTTPClient http;
  String endpoint = String(RAILWAY_SERVER_URL) + "/api/ping";

  Serial.print("[HTTP] Enviando latido a Railway... ");

  if (http.begin(client, endpoint)) {
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"deviceId\":\"" + deviceId + "\",\"uptimeMs\":" + String(millis()) + "}";
    int httpCode = http.POST(jsonPayload);

    if (httpCode > 0) {
      Serial.printf("Respuesta Railway: %d OK\n", httpCode);
    } else {
      Serial.printf("Error HTTP: %s\n", http.errorToString(httpCode).c_str());
    }

    http.end();
  } else {
    Serial.println("No se pudo iniciar la conexión HTTPS.");
  }
}
