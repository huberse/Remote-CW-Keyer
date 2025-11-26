/*
 * WT32-ETH01 Morse Empfänger - WebSocket Server
 * Empfängt Morse-Signale über WebSocket und steuert einen Ausgang
 * Mit Webinterface für Status-Anzeige und Morse-Decoder
 */

#include <ETH.h>
#include <WebSocketsServer.h>
#include <WebServer.h>

// Pin-Definitionen für WT32-ETH01
#define OUTPUT_PIN 2       // GPIO2 für Morse-Ausgang (LED, Relais, etc.)

// Netzwerk-Konfiguration
IPAddress local_ip(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

WebSocketsServer webSocket = WebSocketsServer(81);
WebServer server(80);

bool ethConnected = false;
bool useDHCP = true;

// WebSocket Authentifizierung
String wsAuthToken = "morse2024";

// Status-Variablen
struct ClientInfo {
  IPAddress ip;
  unsigned long connectedSince;
  bool isConnected;
  bool isAuthenticated;
};

ClientInfo clients[WEBSOCKETS_SERVER_CLIENT_MAX];
unsigned long totalMessages = 0;
unsigned long lastMessageTime = 0;
bool currentKeyState = false;

// Morse Decoder Variablen
String decodedText = "";
String currentMorseSequence = "";
unsigned long keyDownTime = 0;
unsigned long keyUpTime = 0;
unsigned long lastKeyChangeTime = 0;
const unsigned long dotThreshold = 200;
const unsigned long dashThreshold = 600;
const unsigned long charGap = 800;
const unsigned long wordGap = 2000;

// Morse Code Tabelle
struct MorseCode {
  const char* pattern;
  char letter;
};

const MorseCode morseTable[] = {
  {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
  {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
  {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
  {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
  {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
  {"--..", 'Z'},
  {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'},
  {".....", '5'}, {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'},
  {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {".----.", '\''}, 
  {"-.-.--", '!'}, {"-..-.", '/'}, {"-.--.", '('}, {"-.--.-", ')'}, 
  {".-...", '&'}, {"---...", ':'}, {"-.-.-.", ';'}, {"-...-", '='}, 
  {".-.-.", '+'}, {"-....-", '-'}, {"..--.-", '_'}, {".-..-.", '"'}, 
  {"...-..-", '$'}, {".--.-.", '@'},
  {NULL, '\0'}
};

char decodeMorseSequence(String sequence) {
  for (int i = 0; morseTable[i].pattern != NULL; i++) {
    if (sequence == morseTable[i].pattern) {
      return morseTable[i].letter;
    }
  }
  return '?';
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("ETH Started");
      ETH.setHostname("wt32-morse-rx");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.print("ETH IP: ");
      Serial.println(ETH.localIP());
      Serial.print("WebSocket Server läuft auf: ws://");
      Serial.print(ETH.localIP());
      Serial.println(":81");
      ethConnected = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      ethConnected = false;
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      ethConnected = false;
      break;
    default:
      break;
  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Client getrennt\n", num);
      digitalWrite(OUTPUT_PIN, LOW);
      currentKeyState = false;
      if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
        clients[num].isConnected = false;
        clients[num].isAuthenticated = false;
      }
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Client verbunden von %d.%d.%d.%d (noch nicht authentifiziert)\n", num, ip[0], ip[1], ip[2], ip[3]);
        if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
          clients[num].ip = ip;
          clients[num].connectedSince = millis();
          clients[num].isConnected = true;
          clients[num].isAuthenticated = false;
        }
        webSocket.sendTXT(num, "AUTH_REQUIRED");
      }
      break;
      
    case WStype_TEXT:
      {
        String message = String((char*)payload);
        
        if (num < WEBSOCKETS_SERVER_CLIENT_MAX && !clients[num].isAuthenticated) {
          if (message.startsWith("AUTH:")) {
            String receivedToken = message.substring(5);
            if (receivedToken == wsAuthToken) {
              clients[num].isAuthenticated = true;
              webSocket.sendTXT(num, "AUTH_OK");
              Serial.printf("[%u] Client authentifiziert!\n", num);
            } else {
              webSocket.sendTXT(num, "AUTH_FAILED");
              Serial.printf("[%u] Authentifizierung fehlgeschlagen!\n", num);
              webSocket.disconnect(num);
            }
            return;
          } else {
            webSocket.sendTXT(num, "AUTH_REQUIRED");
            webSocket.disconnect(num);
            return;
          }
        }
        
        Serial.printf("[%u] Empfangen: %s\n", num, payload);
        totalMessages++;
        lastMessageTime = millis();
        
        if (message == "DOWN") {
          digitalWrite(OUTPUT_PIN, HIGH);
          currentKeyState = true;
          
          keyDownTime = millis();
          unsigned long upDuration = keyDownTime - keyUpTime;
          
          if (upDuration > wordGap && currentMorseSequence.length() > 0) {
            char decodedChar = decodeMorseSequence(currentMorseSequence);
            decodedText += decodedChar;
            decodedText += " ";
            currentMorseSequence = "";
            Serial.println("Morse Word: " + decodedText);
          } else if (upDuration > charGap && currentMorseSequence.length() > 0) {
            char decodedChar = decodeMorseSequence(currentMorseSequence);
            decodedText += decodedChar;
            currentMorseSequence = "";
            Serial.println("Morse Char: " + decodedText);
          }
          
          Serial.println(">>> Taste GEDRÜCKT");
        } 
        else if (message == "UP") {
          digitalWrite(OUTPUT_PIN, LOW);
          currentKeyState = false;
          
          keyUpTime = millis();
          unsigned long downDuration = keyUpTime - keyDownTime;
          
          if (downDuration > 0) {
            if (downDuration < dotThreshold) {
              currentMorseSequence += ".";
              Serial.println("Dit: " + currentMorseSequence);
            } else if (downDuration < dashThreshold) {
              currentMorseSequence += "-";
              Serial.println("Dah: " + currentMorseSequence);
            }
          }
          
          Serial.println(">>> Taste LOSGELASSEN");
        }
      }
      break;
      
    case WStype_BIN:
      break;
      
    case WStype_ERROR:
    case WStype_FRAGMENT_TEXT_START:
    case WStype_FRAGMENT_BIN_START:
    case WStype_FRAGMENT:
    case WStype_FRAGMENT_FIN:
      break;
  }
}

void handleRoot() {
  String html = F("<!DOCTYPE html><html><head><meta charset='UTF-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1.0'>");
  html += F("<title>Morse Empfänger</title><style>");
  html += F("*{margin:0;padding:0;box-sizing:border-box}");
  html += F("body{font-family:Arial;background:#667eea;min-height:100vh;padding:20px}");
  html += F(".container{max-width:900px;margin:0 auto}");
  html += F(".card{background:white;padding:20px;border-radius:10px;margin-bottom:15px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}");
  html += F("h1{color:#333;font-size:24px;margin-bottom:5px}");
  html += F(".grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:15px}");
  html += F(".status{text-align:center}");
  html += F(".label{color:#666;font-size:11px;text-transform:uppercase;margin-bottom:5px}");
  html += F(".value{color:#333;font-size:20px;font-weight:bold}");
  html += F("table{width:100%;border-collapse:collapse}");
  html += F("th{background:#f5f5f5;padding:10px;text-align:left;font-size:11px}");
  html += F("td{padding:10px;border-bottom:1px solid #eee}");
  html += F(".badge{padding:3px 10px;border-radius:15px;font-size:10px;background:#d4edda;color:#155724}");
  html += F(".decoder{background:#f8f9fa;padding:20px;border-radius:8px;min-height:120px;font-family:monospace;font-size:24px;line-height:1.6;word-wrap:break-word;border:2px solid #dee2e6}");
  html += F(".decoder-title{font-size:14px;color:#666;margin-bottom:10px;font-family:Arial}");
  html += F(".clear-btn{background:#dc3545;color:white;border:none;padding:8px 15px;border-radius:5px;cursor:pointer;font-size:12px;margin-top:10px}");
  html += F(".clear-btn:hover{background:#c82333}");
  html += F("</style></head><body><div class='container'>");
  html += F("<div class='card'><h1>📡 Morse Empfänger</h1><p style='color:#666'>WT32-ETH01 Status</p></div>");
  html += F("<div class='card'><div class='grid'>");
  html += F("<div class='status'><div class='label'>IP Adresse</div><div class='value' id='ip'>-</div></div>");
  html += F("<div class='status'><div class='label'>WebSocket Port</div><div class='value'>81</div></div>");
  html += F("<div class='status'><div class='label'>Clients</div><div class='value' id='cnt'>0</div></div>");
  html += F("<div class='status'><div class='label'>Nachrichten</div><div class='value' id='msg'>0</div></div>");
  html += F("</div></div>");
  html += F("<div class='card'><div class='decoder-title'>📝 Morse Decoder:</div>");
  html += F("<div class='decoder' id='decoded'>Warte auf Morse-Signale...</div>");
  html += F("<button class='clear-btn' onclick='clearDecoder()'>🗑️ Löschen</button></div>");
  html += F("<div class='card'><h2 style='margin-bottom:15px'>Verbundene Clients</h2>");
  html += F("<table><thead><tr><th>#</th><th>IP Adresse</th><th>Status</th><th>Verbunden seit</th></tr></thead>");
  html += F("<tbody id='tbl'><tr><td colspan='4' style='text-align:center;color:#999'>Keine Clients</td></tr></tbody></table></div>");
  html += F("<p style='text-align:center;color:white;margin-top:15px;font-size:13px'>⟳ Auto-Refresh alle 2s</p>");
  html += F("</div><script>");
  html += F("function fmt(ms){const s=Math.floor(ms/1000),m=Math.floor(s/60),h=Math.floor(m/60);");
  html += F("return h>0?h+'h '+(m%60)+'m':m>0?m+'m '+(s%60)+'s':s+'s'}");
  html += F("function clearDecoder(){fetch('/api/clear').then(()=>document.getElementById('decoded').textContent='');}");
  html += F("function upd(){fetch('/api/status').then(r=>r.json()).then(d=>{");
  html += F("document.getElementById('ip').textContent=d.ip;");
  html += F("document.getElementById('cnt').textContent=d.clientCount;");
  html += F("document.getElementById('msg').textContent=d.totalMessages;");
  html += F("const dec=document.getElementById('decoded');");
  html += F("if(d.decoded && d.decoded.length>0){dec.textContent=d.decoded;}");
  html += F("const tb=document.getElementById('tbl');");
  html += F("if(d.clients.length===0)tb.innerHTML='<tr><td colspan=\"4\" style=\"text-align:center;color:#999\">Keine Clients</td></tr>';");
  html += F("else tb.innerHTML=d.clients.map((c,i)=>`<tr><td>${i+1}</td><td>${c.ip}</td>");
  html += F("<td><span class=\"badge\">${c.connected?'Verbunden':'Getrennt'}</span></td><td>${fmt(c.uptime)}</td></tr>`).join('')");
  html += F("}).catch(e=>console.error(e))}upd();setInterval(upd,2000);");
  html += F("</script></body></html>");
  
  server.send(200, "text/html", html);
}

void handleStatus() {
  String json = "{";
  json += "\"ip\":\"" + ETH.localIP().toString() + "\",";
  json += "\"mac\":\"" + ETH.macAddress() + "\",";
  json += "\"dhcp\":" + String(useDHCP ? "true" : "false") + ",";
  json += "\"totalMessages\":" + String(totalMessages) + ",";
  json += "\"lastMessage\":" + String(lastMessageTime) + ",";
  json += "\"keyState\":" + String(currentKeyState ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis()) + ",";
  json += "\"decoded\":\"" + decodedText + "\",";
  
  int activeClients = 0;
  json += "\"clients\":[";
  bool firstClient = true;
  for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    if (clients[i].isConnected) {
      activeClients++;
      if (!firstClient) json += ",";
      json += "{";
      json += "\"ip\":\"" + clients[i].ip.toString() + "\",";
      json += "\"uptime\":" + String(millis() - clients[i].connectedSince) + ",";
      json += "\"connected\":true";
      json += "}";
      firstClient = false;
    }
  }
  json += "],";
  json += "\"clientCount\":" + String(activeClients);
  json += "}";
  
  server.send(200, "application/json", json);
}

void handleClear() {
  decodedText = "";
  currentMorseSequence = "";
  server.send(200, "text/plain", "OK");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n=================================");
  Serial.println("WT32-ETH01 Morse Empfänger");
  Serial.println("=================================\n");
  
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, LOW);
  
  for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    clients[i].isConnected = false;
    clients[i].isAuthenticated = false;
  }
  
  WiFi.onEvent(WiFiEvent);
  
  if (useDHCP) {
    Serial.println("Starte Ethernet mit DHCP...");
    ETH.begin();
  } else {
    Serial.println("Starte Ethernet mit statischer IP...");
    ETH.begin();
    delay(100);
    ETH.config(local_ip, gateway, subnet);
  }
  
  Serial.println("Warte auf Ethernet-Verbindung...");
  int timeout = 0;
  while (!ethConnected && timeout < 100) {
    delay(100);
    timeout++;
    if (timeout % 10 == 0) Serial.print(".");
  }
  Serial.println();
  
  if (!ethConnected) {
    Serial.println("\n⚠️ WARNUNG: Ethernet-Verbindung konnte nicht hergestellt werden!");
    Serial.println("Prüfe:");
    Serial.println("- Ist das Ethernet-Kabel angeschlossen?");
    Serial.println("- Ist der Switch/Router eingeschaltet?");
    Serial.println("- Sitzt das Kabel richtig?");
    Serial.println("\nSystem läuft trotzdem weiter...\n");
  }
  
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  
  server.on("/", handleRoot);
  server.on("/api/status", handleStatus);
  server.on("/api/clear", handleClear);
  server.begin();
  
  Serial.println("\nSystem bereit!");
  if (ethConnected) {
    Serial.println("WebSocket Server: ws://" + ETH.localIP().toString() + ":81");
    Serial.println("Web Interface: http://" + ETH.localIP().toString());
    Serial.println("Auth Token: " + wsAuthToken);
  }
  Serial.println("Warte auf Verbindungen...\n");
}

void loop() {
  webSocket.loop();
  server.handleClient();
  
  if (currentMorseSequence.length() > 0 && !currentKeyState) {
    unsigned long timeSinceLastKey = millis() - keyUpTime;
    if (timeSinceLastKey > charGap) {
      char decodedChar = decodeMorseSequence(currentMorseSequence);
      decodedText += decodedChar;
      currentMorseSequence = "";
      Serial.println("Auto-Decode: " + decodedText);
    }
  }
}
