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

// -----------------------------------------------------------------------------
// PORTAL CAUTIVO HTML (AP CONFIGURAR-LUZ)
// -----------------------------------------------------------------------------
void handleRoot() {
  String myUrl = String(RAILWAY_SERVER_URL) + "/?id=" + deviceId;

  String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#0d1117;color:#fff;padding:20px;text-align:center}"
                ".card{background:#161b22;padding:20px;border-radius:14px;max-width:340px;margin:auto;border:1px solid #30363d}"
                "input{width:100%;padding:10px;margin:8px 0;border-radius:6px;border:1px solid #30363d;background:#0d1117;color:#fff;box-sizing:border-box}"
                "button{width:100%;padding:12px;background:#238636;color:#fff;border:none;border-radius:6px;font-weight:bold;cursor:pointer}</style></head><body>"
                "<div class='card'><h2>⚡ Configurar Luz</h2>"
                "<p style='color:#10b981;font-weight:bold;font-size:0.9rem;margin-bottom:4px'>ID: " + deviceId + "</p>"
                "<p style='color:#8b949e;font-size:0.85rem'>Ingresa el WiFi de tu casa:</p>"
                "<form action='/save' method='POST'>"
                "<input type='text' name='s' placeholder='Nombre del WiFi (SSID)' required><br>"
                "<input type='password' name='p' placeholder='Contraseña' required><br>"
                "<button type='submit'>GUARDAR Y CONECTAR</button>"
                "</form></div></body></html>";
  webServer.send(200, "text/html", html);
}

void handleSave() {
  if (webServer.hasArg("s") && webServer.hasArg("p")) {
    String newSsid = webServer.arg("s");
    String newPass = webServer.arg("p");
    saveCredentials(newSsid, newPass);

    String myUrl = String(RAILWAY_SERVER_URL) + "/?id=" + deviceId;

    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
                  "<style>body{font-family:sans-serif;background:#0d1117;color:#fff;padding:20px;text-align:center}"
                  ".card{background:#161b22;padding:24px;border-radius:16px;max-width:340px;margin:auto;border:1px solid #30363d}"
                  "input{width:100%;padding:10px;margin:10px 0;border-radius:8px;border:1px solid #30363d;background:#0d1117;color:#60a5fa;box-sizing:border-box;font-family:monospace;font-size:0.85rem;text-align:center}"
                  "button{width:100%;padding:12px;background:#3b82f6;color:#fff;border:none;border-radius:8px;font-weight:bold;cursor:pointer;font-size:0.95rem}</style>"
                  "<script>function copyUrl(){var copyText=document.getElementById('u');copyText.select();copyText.setSelectionRange(0,99999);navigator.clipboard.writeText(copyText.value);document.getElementById('b').textContent='¡COPIADO! ✅';}</script>"
                  "</head><body><div class='card'>"
                  "<h2>¡WiFi Guardado! 🎉</h2>"
                  "<p style='color:#10b981;font-weight:bold;'>Dispositivo: " + deviceId + "</p>"
                  "<p style='color:#8b949e;font-size:0.85rem'>Este es tu enlace único de monitoreo. Cópialo o guárdalo ahora:</p>"
                  "<input type='text' id='u' value='" + myUrl + "' readonly>"
                  "<button id='b' onclick='copyUrl()'>📋 COPIAR ENLACE</button>"
                  "<p style='color:#e5c07b;font-size:0.8rem;margin-top:16px'>La placa se reiniciará en unos segundos...</p>"
                  "</div></body></html>";

    webServer.send(200, "text/html", html);
    delay(4000);
    ESP.restart();
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
