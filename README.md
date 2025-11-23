# 📡 Remote Morse Key System

Ein WebSocket-basiertes Remote-Morse-System mit Token-Authentifizierung für sichere Verbindungen.
Befindet sich noch in der Entwicklung, aber mal als Alpha Version zum Testen.

## 🎯 Übersicht

Dieses Projekt ermöglicht die Remote-Steuerung einer Morsetaste über ein Netzwerk (WiFi/Ethernet). Der Sender sendet Morse-Signale über WebSocket an einen Empfänger, der diese in Echtzeit ausgibt.

### Features

- ✅ **WebSocket-Kommunikation** mit niedriger Latenz (<10ms)
- ✅ **Token-Authentifizierung** für sichere Verbindungen
- ✅ **WiFi-Konfigurationsportal** für einfaches Setup
- ✅ **Web-Interface** für Status und Steuerung
- ✅ **Straight Key & Iambic Paddle** Support (Mode A & B)
- ✅ **Einstellbare Geschwindigkeit** (10-40 WPM) via Trimmer
- ✅ **Hardware-Jumper** für Moduswechsel
- ✅ **Auto-Reconnect** mit manueller Steuerung

## 🔧 Hardware

### Sender (D1 Mini / ESP8266)

**Komponenten:**
- LOLIN D1 Mini (ESP8266)
- Morsetaste oder Iambic Paddle
- 10kΩ Potentiometer (für WPM-Einstellung)
- Optional: Piezo-Summer für lokales Feedback
- Optional: 2x Jumper für Moduswahl

**Pin-Belegung:**

| Pin | Funktion | Beschreibung |
|-----|----------|--------------|
| D1 (GPIO5) | Morse Key | Normale Morsetaste |
| D2 (GPIO4) | Paddle DIT | Paddle linker Hebel (kurz) |
| D4 (GPIO2) | LED | Status-LED (invertiert) |
| D5 (GPIO14) | Paddle DAH | Paddle rechter Hebel (lang) |
| D6 (GPIO12) | Buzzer | Piezo-Summer (optional) |
| D7 (GPIO13) | Jumper A | Mode A aktivieren |
| D8 (GPIO15) | Jumper B | Mode B aktivieren |
| A0 | Trimmer | Geschwindigkeitsregelung |

**Verkabelung:**
```
Morsetaste:     GPIO5 <--[Taster]--> GND
Paddle DIT:     GPIO4 <--[Taster]--> GND
Paddle DAH:     GPIO14 <--[Taster]--> GND
Trimmer:        A0 <--[Schleifer]
                3.3V <--[Pin 1]
                GND <--[Pin 3]
Buzzer:         GPIO12 <--[+Piezo]--> GND
```

### Empfänger (WT32-ETH01)

**Komponenten:**
- WT32-ETH01 Ethernet Modul
- Ethernet-Kabel
- Ausgang: LED, Relais, oder andere Schaltung

**Pin-Belegung:**

| Pin | Funktion |
|-----|----------|
| GPIO2 | Morse-Ausgang |
| ETH | Ethernet (intern verdrahtet) |

## 📦 Software-Voraussetzungen

### Arduino IDE Setup

1. **Arduino IDE** (Version 1.8.x oder 2.x)
2. **ESP8266 Board Package**:
   - Boardverwalter-URL: `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - Board: "LOLIN(WEMOS) D1 R2 & mini"
3. **ESP32 Board Package** (für WT32-ETH01):
   - Boardverwalter-URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Board: "ESP32 Dev Module"

### Benötigte Bibliotheken

**Für D1 Mini (Sender):**
- `ESP8266WiFi` (in ESP8266 Core enthalten)
- `ESP8266WebServer` (in ESP8266 Core enthalten)
- `LittleFS` (in ESP8266 Core enthalten)
- `WebSockets` by Markus Sattler (via Library Manager)

**Für WT32-ETH01 (Empfänger):**
- `ETH` (in ESP32 Core enthalten)
- `WiFi` (in ESP32 Core enthalten)
- `WebServer` (in ESP32 Core enthalten)
- `WebSockets` by Markus Sattler (via Library Manager)

## 🚀 Installation

### 1. Sender (D1 Mini) einrichten

1. **Code hochladen:**
   - Öffne `D1_Mini_Sender/D1_Mini_Sender.ino`
   - Board: "LOLIN(WEMOS) D1 R2 & mini"
   - Hochladen

2. **Erstkonfiguration:**
   - D1 Mini startet als Access Point: `MorseSender-Config`
   - Passwort: `morse123`
   - Verbinde dich mit dem WiFi
   - Öffne Browser: `http://192.168.4.1`
   - Konfiguriere:
     - WiFi-Netzwerk & Passwort
     - WebSocket Server IP (z.B. `192.168.1.100`)
     - WebSocket Port (Standard: `81`)
     - Authentifizierungs-Token (ändere `morse2024`!)
   - Speichern → D1 Mini startet neu

3. **Status-Webseite:**
   - Nach Neustart über `http://[D1-Mini-IP]` erreichbar
   - Zeigt Verbindungsstatus, Modus, WPM
   - Connect/Disconnect Buttons
   - Link zur Konfiguration

### 2. Empfänger (WT32-ETH01) einrichten

1. **Token anpassen:**
   - Öffne `WT32_Receiver/WT32_Receiver.ino`
   - Ändere `String wsAuthToken = "morse2024";` auf dein Token
   - **Wichtig**: Token muss auf Sender und Empfänger identisch sein!

2. **Netzwerk-Einstellungen** (optional):
   ```cpp
   bool useDHCP = true;  // DHCP (Standard)
   // Oder statische IP:
   IPAddress local_ip(192, 168, 1, 100);
   ```

3. **Code hochladen:**
   - Board: "ESP32 Dev Module"
   - Hochladen

4. **Status-Webseite:**
   - IP wird im Serial Monitor angezeigt
   - Browser: `http://[WT32-IP]`
   - Zeigt verbundene Clients, Nachrichten-Zähler

## 🎮 Bedienung

### Modi wechseln

**Hardware (Jumper):**
- Kein Jumper: **Straight Key** (normale Morsetaste)
- GPIO13 → GND: **Iambic Mode A**
- GPIO15 → GND: **Iambic Mode B**

**Software (Serial Monitor):**
- `1` = Straight Key
- `2` = Iambic Mode A
- `3` = Iambic Mode B
- `r` = Konfiguration zurücksetzen

### Geschwindigkeit einstellen

Drehe den Trimmer am A0 für 10-40 WPM.

### Verbindung steuern

**Via Webseite:**
- Button "Verbinden" → Stellt WebSocket-Verbindung her
- Button "Trennen" → Trennt Verbindung (kein Auto-Reconnect)

**Automatisch:**
- Bei gespeicherter Konfiguration verbindet sich der Sender automatisch beim Start

## 🔐 Sicherheit

### Token-Authentifizierung

Das System verwendet Token-basierte Authentifizierung:

1. **Client verbindet** → Server sendet `AUTH_REQUIRED`
2. **Client sendet Token** → Format: `AUTH:your_token_here`
3. **Server prüft Token:**
   - ✅ Korrekt → `AUTH_OK` → Verbindung erlaubt
   - ❌ Falsch → `AUTH_FAILED` → Verbindung getrennt

### Sicherheitshinweise

⚠️ **Wichtig:**
- Ändere das Standard-Token `morse2024`!
- WebSocket ist **unverschlüsselt** (kein TLS)
- Nutze nur in **vertrauenswürdigen Netzwerken**
- Token muss auf beiden Geräten identisch sein
- Token mindestens 8 Zeichen (empfohlen: 16+)

### Best Practices

- Verwende ein langes, zufälliges Token
- Ändere das Token regelmäßig
- Nutze ein separates VLAN für IoT-Geräte
- Für öffentliche Netzwerke: VPN verwenden

## 🌐 API

### Sender API-Endpoints

```
GET  /              → Status-Webseite
GET  /config        → Konfigurations-Portal
GET  /scan          → WiFi-Netzwerke scannen (JSON)
POST /save          → Konfiguration speichern
GET  /api/status    → Status abrufen (JSON)
POST /api/connect   → WebSocket verbinden
POST /api/disconnect → WebSocket trennen
```

**Status JSON Beispiel:**
```json
{
  "connected": true,
  "authenticated": true,
  "wifi": "MyNetwork",
  "server": "192.168.1.100:81",
  "mode": "Paddle A",
  "wpm": 25
}
```

### Empfänger API-Endpoints

```
GET /              → Status-Webseite
GET /api/status    → Status abrufen (JSON)
```

**Status JSON Beispiel:**
```json
{
  "ip": "192.168.1.100",
  "mac": "AA:BB:CC:DD:EE:FF",
  "dhcp": true,
  "totalMessages": 1234,
  "lastMessage": 123456789,
  "keyState": false,
  "uptime": 123456789,
  "clients": [
    {
      "ip": "192.168.1.50",
      "uptime": 60000,
      "connected": true
    }
  ],
  "clientCount": 1
}
```

### WebSocket-Protokoll

**Nachrichten (Sender → Empfänger):**
- `AUTH:token` - Authentifizierung
- `DOWN` - Taste gedrückt
- `UP` - Taste losgelassen

**Nachrichten (Empfänger → Sender):**
- `AUTH_REQUIRED` - Authentifizierung erforderlich
- `AUTH_OK` - Authentifizierung erfolgreich
- `AUTH_FAILED` - Authentifizierung fehlgeschlagen

## 🐛 Troubleshooting

### Sender verbindet sich nicht

- ✅ WiFi-Credentials korrekt?
- ✅ Server-IP erreichbar? (ping testen)
- ✅ Token auf beiden Seiten identisch?
- ✅ Serial Monitor checken für Fehlermeldungen

### Empfänger erhält keine Signale

- ✅ WebSocket-Server gestartet? (Check Serial Monitor)
- ✅ Firewall blockiert Port 81?
- ✅ Client authentifiziert? (Check Status-Webseite)

### LED blinkt nicht / Morse funktioniert nicht

- ✅ Pins korrekt verdrahtet?
- ✅ Taster zwischen Pin und GND?
- ✅ Pull-up aktiviert? (im Code standard)
- ✅ D1 Mini LED ist invertiert (LOW = AN)

### Config-Portal nicht erreichbar

- ✅ Mit WiFi `MorseSender-Config` verbunden?
- ✅ IP `192.168.4.1` im Browser
- ✅ Passwort `morse123` verwendet?
- ✅ LED blinkt 3x beim Start des Config-Modus?

### Speicher voll / Upload-Fehler

**Für ESP8266:**
- Flash Size: "4MB (FS:2MB OTA:~1019KB)" einstellen
- LittleFS formatieren: `LittleFS.format()`

## 📊 Technische Spezifikationen

### Latenz

- **Lokales Netzwerk**: < 10ms
- **WLAN → Ethernet**: 5-15ms typisch
- **Internet (mit gutem Ping)**: 20-100ms

### Reichweite

- **WiFi**: Abhängig vom Router (typisch 20-50m indoor)
- **Ethernet**: Bis 100m (Cat5e/Cat6)

### Stromverbrauch

- **D1 Mini (Sender)**: ~80mA @ 5V (WiFi aktiv)
- **WT32-ETH01 (Empfänger)**: ~200mA @ 5V (Ethernet aktiv)

## 🔄 Updates & Erweiterungen

### Geplante Features

- [ ] OTA (Over-The-Air) Updates
- [ ] Morse-Decoder im Empfänger
- [ ] Aufnahme & Wiedergabe von Morse-Sequenzen
- [ ] TLS/SSL-Verschlüsselung

### Mitmachen

Contributions sind willkommen! Bitte:
1. Fork das Repository
2. Erstelle einen Feature-Branch
3. Commit deine Änderungen
4. Erstelle einen Pull Request

## 📄 Lizenz

Dieses Projekt ist Open Source und steht unter der MIT-Lizenz.

## 🙏 Credits

- **WebSockets Library** by Markus Sattler
- **ESP8266/ESP32 Arduino Core** by Espressif
- Entwickelt für die Amateurfunk-Community

## 📞 Support

Bei Fragen oder Problemen:
- GitHub Issues erstellen
- Serial Monitor Ausgabe mitschicken
- Hardware-Setup beschreiben

---

**73 & Happy Morsing!** 📻✨
