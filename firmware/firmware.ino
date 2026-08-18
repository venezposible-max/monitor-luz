/*
  =============================================================================
  PROYECTO: Monitor de Luz e Internet (ESP8266 -> Servidor Railway)
  VERSIÓN: Portal Cautivo Instantáneo (Detección de EEPROM Limpia/Basura)
  =============================================================================
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <EEPROM.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

const char* RAILWAY_SERVER_URL = "https://monitor-luz-production.up.railway.app";

#define EEPROM_SIZE 96
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);

DNSServer dnsServer;
ESP8266WebServer webServer(80);

String ssid = "";
String password = "";
String deviceId = "";

unsigned long lastPingTime = 0;
const unsigned long PING_INTERVAL = 60000;
bool configMode = false;
bool lastConnectFailed = false;

bool isValidSSID(String s) {
  if (s.length() == 0 || s.length() > 32) return false;
  for (unsigned int i = 0; i < s.length(); i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 32 || c > 126) return false;
  }
  return true;
}

void loadCredentials() {
  EEPROM.begin(EEPROM_SIZE);
  char ssidBuf[33] = {0};
  char passBuf[65] = {0};

  for (int i = 0; i < 32; ++i) {
    byte b = EEPROM.read(i);
    ssidBuf[i] = (b >= 32 && b <= 126) ? char(b) : 0;
  }
  for (int i = 0; i < 64; ++i) {
    byte b = EEPROM.read(32 + i);
    passBuf[i] = (b >= 32 && b <= 126) ? char(b) : 0;
  }

  ssid = String(ssidBuf);
  password = String(passBuf);
  ssid.trim();
  password.trim();

  if (!isValidSSID(ssid)) {
    ssid = "";
    password = "";
  }
}

void saveCredentials(String qssid, String qpass) {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < EEPROM_SIZE; ++i) EEPROM.write(i, 0);

  for (int i = 0; i < qssid.length(); ++i) EEPROM.write(i, qssid[i]);
  for (int i = 0; i < qpass.length(); ++i) EEPROM.write(32 + i, qpass[i]);
  
  EEPROM.commit();
}

void handleRoot() {
  int n = WiFi.scanNetworks();
  
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<style>body{font-family:sans-serif;background:#0d1117;color:#fff;padding:16px;text-align:center}"
                ".card{background:#161b22;padding:20px;border-radius:16px;max-width:360px;margin:auto;border:1px solid #30363d}"
                ".net-item{background:#0d1117;border:1px solid #30363d;padding:10px 14px;border-radius:8px;margin:6px 0;text-align:left;cursor:pointer;display:flex;justify-content:space-between;align-items:center;font-size:0.9rem}"
                ".net-item:hover{border-color:#58a6ff;background:#1f242c}"
                "input{width:100%;padding:12px;margin:10px 0;border-radius:8px;border:1px solid #30363d;background:#0d1117;color:#fff;box-sizing:border-box;font-size:0.95rem}"
                "button{width:100%;padding:14px;background:#238636;color:#fff;border:none;border-radius:8px;font-weight:bold;cursor:pointer;font-size:1rem;margin-top:8px}</style>"
                "<script>function sel(s){document.getElementById('s').value=s;document.getElementById('p').focus();}</script>"
                "</head><body><div class='card'><h2>⚡ Configurar Luz</h2>"
                "<p style='color:#10b981;font-weight:bold;font-size:0.9rem;margin-bottom:8px'>ID Dispositivo: " + deviceId + "</p>";

  if (lastConnectFailed) {
    html += "<p style='color:#ef4444;background:rgba(239,68,68,0.15);padding:8px;border-radius:6px;font-size:0.85rem;border:1px solid #ef4444;'>⚠️ No se pudo conectar a la red previa. Verifica la clave e inténtalo de nuevo.</p>";
  }

  html += "<p style='color:#8b949e;font-size:0.85rem;text-align:left;margin-bottom:6px'>Toca tu red WiFi para seleccionarla:</p>"
          "<div style='max-height:150px;overflow-y:auto;margin-bottom:12px;'>";

  if (n <= 0) {
    html += "<p style='color:#8b949e'>Buscando redes cercanas...</p>";
  } else {
    for (int i = 0; i < n; ++i) {
      String netName = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      String signalStr = rssi > -65 ? "📶 Excelente" : (rssi > -80 ? "📶 Buena" : "📶 Débil");
      html += "<div class='net-item' onclick=\"sel('" + netName + "')\"><span>" + netName + "</span><small style='color:#8b949e'>" + signalStr + "</small></div>";
    }
  }

  html += "</div>"
          "<form action='/save' method='POST'>"
          "<input type='text' id='s' name='s' placeholder='Nombre del WiFi (SSID)' required><br>"
          "<input type='password' id='p' name='p' placeholder='Contraseña de tu WiFi' required><br>"
          "<button type='submit'>GUARDAR Y OBTENER ENLACE 🚀</button>"
          "</form></div></body></html>";

  webServer.send(200, "text/html", html);
}

void handleSave() {
  if (webServer.hasArg("s") && webServer.hasArg("p")) {
    String testSsid = webServer.arg("s");
    String testPass = webServer.arg("p");
    testSsid.trim();
    testPass.trim();

    saveCredentials(testSsid, testPass);
    String myUrl = String(RAILWAY_SERVER_URL) + "/?id=" + deviceId;

    String successHtml = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'>"
                         "<style>body{font-family:sans-serif;background:#0d1117;color:#fff;padding:20px;text-align:center}"
                         ".card{background:#161b22;padding:24px;border-radius:16px;max-width:340px;margin:auto;border:1px solid #30363d}"
                         "input{width:100%;padding:12px;margin:12px 0;border-radius:8px;border:1px solid #30363d;background:#0d1117;color:#60a5fa;box-sizing:border-box;font-family:monospace;font-size:0.85rem;text-align:center}"
                         "button{width:100%;padding:14px;background:#3b82f6;color:#fff;border:none;border-radius:8px;font-weight:bold;cursor:pointer;font-size:0.95rem}</style>"
                         "<script>function copyUrl(){var copyText=document.getElementById('u');copyText.select();copyText.setSelectionRange(0,99999);navigator.clipboard.writeText(copyText.value);document.getElementById('b').textContent='¡COPIADO! ✅';}</script>"
                         "</head><body><div class='card'>"
                         "<h2>¡Conexión Guardada! 🎉</h2>"
                         "<p style='color:#10b981;font-weight:bold;'>Dispositivo: " + deviceId + "</p>"
                         "<p style='color:#8b949e;font-size:0.85rem'>Este es tu enlace único de monitoreo. Cópialo o guárdalo ahora:</p>"
                         "<input type='text' id='u' value='" + myUrl + "' readonly>"
                         "<button id='b' onclick='copyUrl()'>📋 COPIAR ENLACE</button>"
                         "<p style='color:#e5c07b;font-size:0.8rem;margin-top:16px'>La placa se conectará a '" + testSsid + "' en unos segundos...</p>"
                         "</div></body></html>";

    webServer.send(200, "text/html", successHtml);
    delay(3500);
    ESP.restart();
  }
}

void startConfigPortal() {
  configMode = true;
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("Configurar-Luz");

  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/", handleRoot);
  webServer.on("/save", handleSave);
  webServer.onNotFound(handleRoot);
  webServer.begin();

  Serial.println("\n[Portal Cautivo] Red activa inmediatamente: 'Configurar-Luz'");
}

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

  pinMode(0, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  uint32_t chipId = ESP.getChipId();
  char idBuffer[16];
  snprintf(idBuffer, sizeof(idBuffer), "ESP-%06X", chipId);
  deviceId = String(idBuffer);

  if (digitalRead(0) == LOW) {
    saveCredentials("", "");
    delay(1000);
    startConfigPortal();
    return;
  }

  loadCredentials();

  if (ssid.length() == 0 || !isValidSSID(ssid)) {
    startConfigPortal();
  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());

    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40) {
      delay(500);
      if (tries % 4 == 0) {
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
      }
      tries++;
    }

    digitalWrite(LED_BUILTIN, HIGH);

    if (WiFi.status() == WL_CONNECTED) {
      sendPingToRailway();
    } else {
      lastConnectFailed = true;
      startConfigPortal();
    }
  }
}

void loop() {
  if (configMode) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    return;
  }

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
