/*
 * D1 Mini (ESP8266) Morse Sender - WebSocket Client
 * Unterstützt normale Morsetaste UND Morse-Paddle (Iambic)
 * Mit WiFi-Konfigurationsportal (Access Point Mode)
 */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsClient.h>
#include <LittleFS.h>

// WiFi-Zugangsdaten (werden aus LittleFS geladen)
String wifi_ssid = "";
String wifi_password = "";

// Access Point Konfiguration
const char* ap_ssid = "MorseSender-Config";
const char* ap_password = "morse123";  // Mindestens 8 Zeichen
IPAddress ap_ip(192, 168, 4, 1);
IPAddress ap_gateway(192, 168, 4, 1);
IPAddress ap_subnet(255, 255, 255, 0);

// WT32-ETH01 Empfänger-Adresse
String websocket_server = "192.168.1.100";
uint16_t websocket_port = 81;

// Pin-Definitionen für D1 Mini
#define MORSE_KEY_PIN D1     // GPIO5 - Normale Morsetaste
#define PADDLE_DIT_PIN D2    // GPIO4 - Paddle DIT (kurz)
#define PADDLE_DAH_PIN D5    // GPIO14 - Paddle DAH (lang)
#define LED_PIN D4           // GPIO2 - Eingebaute LED (invertiert!)
#define BUZZER_PIN D6        // GPIO12 - Optional: Piezo-Summer

// Jumper für Modus-Auswahl
#define JUMPER_MODE_A D7     // GPIO13 - Jumper nach GND = Mode A
#define JUMPER_MODE_B D8     // GPIO15 - Jumper nach GND = Mode B

// Trimmer für Geschwindigkeit
#define SPEED_TRIMMER_PIN A0 // Einziger ADC Pin (10-bit: 0-1023)
#define WPM_MIN 10
#define WPM_MAX 40

// Morse-Timing
int currentWPM = 20;
unsigned long ditLength = 1200 / currentWPM;
unsigned long dahLength = ditLength * 3;
unsigned long elementGap = ditLength;
#define BUZZER_FREQ 800

// Betriebsmodi
enum OperationMode {
  MODE_STRAIGHT_KEY,
  MODE_PADDLE_A,
  MODE_PADDLE_B
};

OperationMode currentMode = MODE_STRAIGHT_KEY;

WebSocketsClient webSocket;
ESP8266WebServer server(80);

bool connected = false;
bool isTransmitting = false;
bool configMode = false;

// Zustände
bool lastKeyState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 5;

bool ditPressed = false;
bool dahPressed = false;
bool lastDitState = HIGH;
bool lastDahState = HIGH;
unsigned long ditDebounceTime = 0;
unsigned long dahDebounceTime = 0;

enum KeyerState {
  IDLE,
  DIT_ON,
  DIT_OFF,
  DAH_ON,
  DAH_OFF
};

KeyerState keyerState = IDLE;
unsigned long keyerTimer = 0;
bool ditLatched = false;
bool dahLatched = false;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WS] Verbindung getrennt");
      connected = false;
      if (!configMode) {
        digitalWrite(LED_PIN, HIGH); // LED aus (invertiert)
      }
      break;
      
    case WStype_CONNECTED:
      Serial.printf("[WS] Verbunden mit: %s\n", payload);
      connected = true;
      if (!configMode) {
        digitalWrite(LED_PIN, LOW); // LED an (invertiert)
      }
      break;
      
    case WStype_TEXT:
      Serial.printf("[WS] Empfangen: %s\n", payload);
      break;
      
    case WStype_ERROR:
      Serial.println("[WS] Fehler!");
      connected = false;
      break;
      
    default:
      break;
  }
}

// WiFi-Konfiguration laden (aus LittleFS)
bool loadWiFiConfig() {
  if (!LittleFS.begin()) {
    Serial.println("LittleFS Mount fehlgeschlagen!");
    return false;
  }
  
  if (!LittleFS.exists("/config.txt")) {
    return false;
  }
  
  File f = LittleFS.open("/config.txt", "r");
  if (!f) {
    return false;
  }
  
  wifi_ssid = f.readStringUntil('\n');
  wifi_password = f.readStringUntil('\n');
  websocket_server = f.readStringUntil('\n');
  websocket_port = f.readStringUntil('\n').toInt();
  f.close();
  
  wifi_ssid.trim();
  wifi_password.trim();
  websocket_server.trim();
  
  return (wifi_ssid.length() > 0);
}

// WiFi-Konfiguration speichern
void saveWiFiConfig(String ssid, String password, String ws_server, uint16_t ws_port) {
  File f = LittleFS.open("/config.txt", "w");
  if (!f) {
    Serial.println("Fehler beim Speichern!");
    return;
  }
  
  f.println(ssid);
  f.println(password);
  f.println(ws_server);
  f.println(ws_port);
  f.close();
  
  Serial.println("Konfiguration gespeichert!");
}

// HTML für Config-Portal (kompakt)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Morse Config</title><style>
body{font-family:Arial;max-width:500px;margin:50px auto;padding:20px;background:#f0f0f0}
.box{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#333;text-align:center;margin-bottom:20px}
label{display:block;margin-top:15px;color:#555;font-weight:bold}
select,input{width:100%;padding:10px;margin-top:5px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box}
input[type="number"]{width:30%}
button{width:100%;padding:12px;margin-top:20px;background:#007bff;color:white;border:none;border-radius:5px;cursor:pointer}
button:hover{background:#0056b3}
.info{background:#e7f3ff;padding:15px;border-radius:5px;margin-bottom:20px}
.help{font-size:11px;color:#666;margin-top:3px}
</style></head><body><div class="box">
<h1>📡 Morse Sender</h1>
<div class="info">WiFi & WebSocket Server konfigurieren</div>
<div id="net" style="text-align:center;padding:20px">Suche Netzwerke...</div>
<form action="/save" method="POST">
<label>WiFi Netzwerk:</label>
<select id="ssid" name="ssid" required><option value="">-- Wählen --</option></select>
<label>WiFi Passwort:</label>
<input type="password" name="password" placeholder="Passwort">
<label>WebSocket Server:</label>
<input type="text" name="ws_server" value="192.168.1.100" required>
<div class="help">IP oder Hostname</div>
<label>WebSocket Port:</label>
<input type="number" name="ws_port" value="81" min="1" max="65535" required>
<button type="submit">💾 Speichern</button>
</form></div>
<script>
fetch('/scan').then(r=>r.json()).then(d=>{
const n=document.getElementById('net'),s=document.getElementById('ssid');
if(d.networks && d.networks.length>0){
n.innerHTML='<b>Gefundene Netzwerke</b>';
s.innerHTML='<option value="">-- Wählen --</option>';
d.networks.forEach(w=>{
const o=document.createElement('option');
o.value=w.ssid;
o.textContent=w.ssid+' ('+w.rssi+' dBm)';
s.appendChild(o);
});
}else n.innerHTML='<p style="color:red">Keine Netzwerke gefunden</p>';
}).catch(e=>document.getElementById('net').innerHTML='<p style="color:red">Fehler</p>');
</script></body></html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleScan() {
  Serial.println("Scanne WiFi...");
  int n = WiFi.scanNetworks();
  
  String json = "{\"networks\":[";
  for (int i = 0; i < n; i++) {
    if (i > 0) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]}";
  
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.hasArg("ssid") && server.hasArg("ws_server") && server.hasArg("ws_port")) {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String ws_server = server.arg("ws_server");
    uint16_t ws_port = server.arg("ws_port").toInt();
    
    if (ws_port < 1 || ws_port > 65535) {
      server.send(400, "text/plain", "Ungültiger Port!");
      return;
    }
    
    saveWiFiConfig(ssid, password, ws_server, ws_port);
    
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='5;url=/'>";
    html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#f0f0f0}";
    html += ".box{background:white;padding:30px;border-radius:10px;display:inline-block}</style>";
    html += "</head><body><div class='box'><h2>✅ Gespeichert!</h2>";
    html += "<p>WiFi: <b>" + ssid + "</b></p>";
    html += "<p>Server: <b>" + ws_server + ":" + String(ws_port) + "</b></p>";
    html += "<p>Neustart in 5s...</p></div></body></html>";
    
    server.send(200, "text/html", html);
    delay(2000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Fehlende Parameter!");
  }
}

void startConfigMode() {
  configMode = true;
  Serial.println("\n=== KONFIGURATIONS-MODUS ===");
  
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ap_ip, ap_gateway, ap_subnet);
  WiFi.softAP(ap_ssid, ap_password);
  
  Serial.println("AP gestartet!");
  Serial.println("SSID: " + String(ap_ssid));
  Serial.println("Passwort: " + String(ap_password));
  Serial.println("URL: http://192.168.4.1");
  Serial.println("===========================\n");
  
  for (int i = 0; i < 6; i++) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    delay(200);
  }
  
  server.on("/", handleRoot);
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

bool startClientMode() {
  Serial.println("\nVerbinde mit: " + wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi verbunden!");
    Serial.println("IP: " + WiFi.localIP().toString());
    return true;
  } else {
    Serial.println("\nVerbindung fehlgeschlagen!");
    return false;
  }
}

void sendMorseDown() {
  if (connected) {
    webSocket.sendTXT("DOWN");
  }
  digitalWrite(LED_PIN, LOW); // LED an (invertiert)
  #ifdef BUZZER_PIN
    tone(BUZZER_PIN, BUZZER_FREQ);
  #endif
  isTransmitting = true;
}

void sendMorseUp() {
  if (connected) {
    webSocket.sendTXT("UP");
  }
  digitalWrite(LED_PIN, HIGH); // LED aus (invertiert)
  #ifdef BUZZER_PIN
    noTone(BUZZER_PIN);
  #endif
  isTransmitting = false;
}

void handleStraightKey() {
  bool currentKeyState = digitalRead(MORSE_KEY_PIN);
  
  if (currentKeyState != lastKeyState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (currentKeyState == LOW && lastKeyState == HIGH) {
      Serial.println(">>> GEDRÜCKT");
      sendMorseDown();
    }
    else if (currentKeyState == HIGH && lastKeyState == LOW) {
      Serial.println(">>> LOSGELASSEN");
      sendMorseUp();
    }
    lastKeyState = currentKeyState;
  }
}

void readPaddleInputs() {
  bool currentDit = digitalRead(PADDLE_DIT_PIN);
  bool currentDah = digitalRead(PADDLE_DAH_PIN);
  
  if (currentDit != lastDitState) {
    ditDebounceTime = millis();
  }
  if ((millis() - ditDebounceTime) > debounceDelay) {
    ditPressed = (currentDit == LOW);
  }
  lastDitState = currentDit;
  
  if (currentDah != lastDahState) {
    dahDebounceTime = millis();
  }
  if ((millis() - dahDebounceTime) > debounceDelay) {
    dahPressed = (currentDah == LOW);
  }
  lastDahState = currentDah;
}

void handleIambicModeA() {
  readPaddleInputs();
  unsigned long currentTime = millis();
  
  switch(keyerState) {
    case IDLE:
      if (ditPressed) {
        keyerState = DIT_ON;
        keyerTimer = currentTime + ditLength;
        sendMorseDown();
      } else if (dahPressed) {
        keyerState = DAH_ON;
        keyerTimer = currentTime + dahLength;
        sendMorseDown();
      }
      break;
      
    case DIT_ON:
      if (currentTime >= keyerTimer) {
        keyerState = DIT_OFF;
        keyerTimer = currentTime + elementGap;
        sendMorseUp();
        if (dahPressed) dahLatched = true;
      }
      break;
      
    case DIT_OFF:
      if (currentTime >= keyerTimer) {
        if (dahLatched || dahPressed) {
          keyerState = DAH_ON;
          keyerTimer = currentTime + dahLength;
          sendMorseDown();
          dahLatched = false;
        } else if (ditPressed) {
          keyerState = DIT_ON;
          keyerTimer = currentTime + ditLength;
          sendMorseDown();
        } else {
          keyerState = IDLE;
        }
      }
      break;
      
    case DAH_ON:
      if (currentTime >= keyerTimer) {
        keyerState = DAH_OFF;
        keyerTimer = currentTime + elementGap;
        sendMorseUp();
        if (ditPressed) ditLatched = true;
      }
      break;
      
    case DAH_OFF:
      if (currentTime >= keyerTimer) {
        if (ditLatched || ditPressed) {
          keyerState = DIT_ON;
          keyerTimer = currentTime + ditLength;
          sendMorseDown();
          ditLatched = false;
        } else if (dahPressed) {
          keyerState = DAH_ON;
          keyerTimer = currentTime + dahLength;
          sendMorseDown();
        } else {
          keyerState = IDLE;
        }
      }
      break;
  }
}

void handleIambicModeB() {
  readPaddleInputs();
  unsigned long currentTime = millis();
  
  switch(keyerState) {
    case IDLE:
      if (ditPressed) {
        keyerState = DIT_ON;
        keyerTimer = currentTime + ditLength;
        sendMorseDown();
      } else if (dahPressed) {
        keyerState = DAH_ON;
        keyerTimer = currentTime + dahLength;
        sendMorseDown();
      }
      break;
      
    case DIT_ON:
      if (dahPressed) dahLatched = true;
      if (currentTime >= keyerTimer) {
        keyerState = DIT_OFF;
        keyerTimer = currentTime + elementGap;
        sendMorseUp();
      }
      break;
      
    case DIT_OFF:
      if (currentTime >= keyerTimer) {
        if (dahLatched || dahPressed) {
          keyerState = DAH_ON;
          keyerTimer = currentTime + dahLength;
          sendMorseDown();
          dahLatched = false;
        } else if (ditPressed) {
          keyerState = DIT_ON;
          keyerTimer = currentTime + ditLength;
          sendMorseDown();
        } else {
          keyerState = IDLE;
        }
      }
      break;
      
    case DAH_ON:
      if (ditPressed) ditLatched = true;
      if (currentTime >= keyerTimer) {
        keyerState = DAH_OFF;
        keyerTimer = currentTime + elementGap;
        sendMorseUp();
      }
      break;
      
    case DAH_OFF:
      if (currentTime >= keyerTimer) {
        if (ditLatched || ditPressed) {
          keyerState = DIT_ON;
          keyerTimer = currentTime + ditLength;
          sendMorseDown();
          ditLatched = false;
        } else if (dahPressed) {
          keyerState = DAH_ON;
          keyerTimer = currentTime + dahLength;
          sendMorseDown();
        } else {
          keyerState = IDLE;
        }
      }
      break;
  }
}

void updateSpeedFromTrimmer() {
  int adcValue = analogRead(SPEED_TRIMMER_PIN); // 0-1023 (10-bit)
  
  static int lastWPM = currentWPM;
  int newWPM = map(adcValue, 0, 1023, WPM_MIN, WPM_MAX);
  
  if (abs(newWPM - lastWPM) > 1) {
    currentWPM = newWPM;
    lastWPM = newWPM;
    
    ditLength = 1200 / currentWPM;
    dahLength = ditLength * 3;
    elementGap = ditLength;
    
    static int lastPrintedWPM = 0;
    if (abs(currentWPM - lastPrintedWPM) >= 2) {
      Serial.println(">>> WPM: " + String(currentWPM));
      lastPrintedWPM = currentWPM;
    }
  }
}

OperationMode readModeFromJumpers() {
  bool jumperA = (digitalRead(JUMPER_MODE_A) == LOW);
  bool jumperB = (digitalRead(JUMPER_MODE_B) == LOW);
  
  if (jumperB) return MODE_PADDLE_B;
  else if (jumperA) return MODE_PADDLE_A;
  else return MODE_STRAIGHT_KEY;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=== D1 Mini Morse Sender ===\n");
  
  // Pins
  pinMode(MORSE_KEY_PIN, INPUT_PULLUP);
  pinMode(PADDLE_DIT_PIN, INPUT_PULLUP);
  pinMode(PADDLE_DAH_PIN, INPUT_PULLUP);
  pinMode(JUMPER_MODE_A, INPUT_PULLUP);
  pinMode(JUMPER_MODE_B, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED aus
  #ifdef BUZZER_PIN
    pinMode(BUZZER_PIN, OUTPUT);
  #endif
  
  currentMode = readModeFromJumpers();
  updateSpeedFromTrimmer();
  
  Serial.println("Modus: " + String(currentMode == MODE_STRAIGHT_KEY ? "Straight Key" : 
                                     currentMode == MODE_PADDLE_A ? "Paddle A" : "Paddle B"));
  Serial.println("WPM: " + String(currentWPM));
  
  // LittleFS initialisieren
  if (!LittleFS.begin()) {
    Serial.println("LittleFS formatieren...");
    LittleFS.format();
    LittleFS.begin();
  }
  
  bool hasConfig = loadWiFiConfig();
  
  if (!hasConfig) {
    Serial.println("⚠️ Keine Konfiguration!");
    startConfigMode();
  } else {
    if (startClientMode()) {
      Serial.println("WebSocket: " + websocket_server + ":" + String(websocket_port));
      webSocket.begin(websocket_server.c_str(), websocket_port, "/");
      webSocket.onEvent(webSocketEvent);
      webSocket.setReconnectInterval(5000);
      Serial.println("\nSystem bereit!");
      Serial.println("Sende 'r' zum Reset\n");
    } else {
      startConfigMode();
    }
  }
}

unsigned long lastJumperCheck = 0;
unsigned long lastSpeedCheck = 0;

void loop() {
  if (configMode) {
    server.handleClient();
    return;
  }
  
  webSocket.loop();
  
  if (millis() - lastJumperCheck > 500) {
    lastJumperCheck = millis();
    OperationMode newMode = readModeFromJumpers();
    if (newMode != currentMode) {
      currentMode = newMode;
      keyerState = IDLE;
      sendMorseUp();
      Serial.println("\n>>> Modus: " + String(currentMode == MODE_STRAIGHT_KEY ? "Straight Key" : 
                                              currentMode == MODE_PADDLE_A ? "Paddle A" : "Paddle B"));
    }
  }
  
  if (millis() - lastSpeedCheck > 100) {
    lastSpeedCheck = millis();
    updateSpeedFromTrimmer();
  }
  
  if (Serial.available() > 0) {
    char input = Serial.read();
    if (input == 'r' || input == 'R') {
      Serial.println("\n>>> Reset...");
      LittleFS.remove("/config.txt");
      delay(1000);
      ESP.restart();
    }
  }
  
  switch(currentMode) {
    case MODE_STRAIGHT_KEY:
      handleStraightKey();
      break;
    case MODE_PADDLE_A:
      handleIambicModeA();
      break;
    case MODE_PADDLE_B:
      handleIambicModeB();
      break;
  }
  
  yield(); // Wichtig für ESP8266!
}