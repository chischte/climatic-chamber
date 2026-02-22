# Climatic Chamber - Klimakammer Steuerung

Closed-loop-Steuerung von Temperatur, relativer Luftfeuchtigkeit und CO₂-Gehalt mit Frischluftverwaltung, implementiert auf einer Arduino Portenta Machine Control Plattform.

## 📋 Übersicht

Dieses Projekt implementiert ein Steuerungssystem für eine Klimakammer mit folgenden Funktionen:

- **WiFi-Konnektivität**: Automatische Verbindung mit konfigurierbarem WLAN
- **Web-Interface**: HTTP-Server mit REST-API und Web-GUI
- **Persistente Datenspeicherung**: Ring-Buffer mit Flash/RAM-Speicherung und Wear-Leveling
- **Modulare Architektur**: Saubere Trennung von WiFi, Storage und Web-Server-Logik

## 🔧 Hardware

- **Plattform**: Arduino Portenta H7 (M7 Core)
- **Board**: Portenta Machine Control
- **MCU**: STM32H747XIH6 @ 480MHz
- **RAM**: 511 KB
- **Flash**: 768 KB

## 📁 Projektstruktur

```
src/
├── main.cpp                 # Hauptprogramm (~59 Zeilen, nur High-Level-Logik)
├── credentials.h            # WiFi-Zugangsdaten (nicht in Git)
├── credentials.h.template   # Template für Zugangsdaten
├── wifi_manager.h/cpp       # WiFi-Verbindungsverwaltung
├── storage.h/cpp            # Datenpersistenz mit Ring-Buffer
├── web_server.h/cpp         # HTTP-Server und REST-API
└── flash_ringbuffer.h/cpp   # Low-Level Flash/RAM Ring-Buffer

lib/
└── Arduino_PortentaMachineControl/  # Hardware-Library
```

## 🚀 Installation & Setup

### 1. PlatformIO installieren
```bash
# Via VS Code Extension oder pip
pip install platformio
```

### 2. Projekt klonen
```bash
git clone <repository-url>
cd climatic-chamber
```

### 3. WiFi-Zugangsdaten konfigurieren
```bash
# Template kopieren
cp src/credentials.h.template src/credentials.h

# Zugangsdaten eintragen
# Bearbeite src/credentials.h und trage deine WLAN-Daten ein
```

**credentials.h Beispiel:**
```cpp
static constexpr const char *WIFI_SSID = "dein-wlan-name";
static constexpr const char *WIFI_PASS = "dein-wlan-passwort";
```

### 4. Kompilieren und hochladen
```bash
# Kompilieren
platformio run

# Kompilieren und hochladen
platformio run --target upload

# Serial Monitor öffnen (115200 baud)
platformio device monitor
```

## 📚 Module

### WiFi Manager (`wifi_manager.h/cpp`)

Verwaltet die WiFi-Verbindung und den Web-Server.

**Konfiguration** (in `wifi_manager.h`):
- `WIFI_SERVER_PORT`: Port für Web-Server (Standard: 80)
- `WIFI_MAX_RETRIES`: Maximale Verbindungsversuche (Standard: 3)
- `WIFI_ATTEMPT_TIMEOUT_MS`: Timeout pro Versuch (Standard: 20s)
- `WIFI_RETRY_DELAY_MS`: Wartezeit zwischen Versuchen (Standard: 2s)
- `WIFI_HEARTBEAT_MS`: Intervall für Status-Updates (Standard: 30s)

**API:**
```cpp
wifi_init(ssid, pass);  // Initialisierung und Verbindung
wifi_tick();            // Periodische Statusüberwachung (in loop() aufrufen)
wifi_get_server();      // Server-Instanz abrufen
```

### Storage (`storage.h/cpp`)

Persistente Datenspeicherung mit automatischem Ring-Buffer auf Flash oder RAM.

**Features:**
- Automatische Flash-Erkennung (QSPI) mit RAM-Fallback
- Ring-Buffer mit 100 Slots à 64 Bytes
- CRC8-Checksummen für Datenintegrität
- Wear-Leveling durch Append-Only-Writes
- Auto-Persistierung nach 5 Sekunden Inaktivität

**Konfiguration** (in `flash_ringbuffer.h`):
- `RING_BUFFER_NUM_SLOTS`: Anzahl Slots (Standard: 100)
- `RING_BUFFER_SLOT_SIZE`: Größe pro Slot (Standard: 64 Bytes)

**API:**
```cpp
storage_init();                        // Initialisierung
storage_load();                        // Daten vom Flash/RAM laden
storage_tick();                        // Auto-Persistierung (in loop() aufrufen)
storage_increment_value(index);        // Wert incrementieren
storage_set_value(index, value);       // Wert setzen
storage_get_values();                  // Werte auslesen
storage_save_now();                    // Sofort speichern (force)
```

**Datenstruktur:**
- 10 × 16-bit Werte (z.B. für Sensordaten, Counter, etc.)
- Sequenznummern für Versionierung
- CRC8-Checksummen

### Web Server (`web_server.h/cpp`)

HTTP-Server mit Web-GUI und REST-API.

**Endpoints:**
- `GET /` - HTML-Oberfläche mit Counter
- `GET /api/values` - JSON-API: Alle Werte auslesen
- `POST /api/increment` - JSON-API: Counter incrementieren

**Beispiel API-Requests:**
```bash
# Werte auslesen
curl http://<ip-adresse>/api/values
# Response: {"values":[42,0,0,0,0,0,0,0,0,0]}

# Counter incrementieren
curl -X POST http://<ip-adresse>/api/increment
# Response: {"values":[43,0,0,0,0,0,0,0,0,0]}
```

## 🔒 Sicherheit

- **credentials.h ist in .gitignore**: Zugangsdaten werden nicht versioniert
- **credentials.h.template**: Enthält Platzhalter für andere Entwickler
- Template in Git committen, echte Credentials lokal halten

## 🛠️ Entwicklung

### Code-Struktur

Das Hauptprogramm (`main.cpp`) ist bewusst minimal gehalten:

```cpp
void setup() {
  storage_init();      // Speicher initialisieren
  storage_load();      // Persistierte Daten laden
  wifi_init(WIFI_SSID, WIFI_PASS);  // WiFi verbinden
}

void loop() {
  wifi_tick();         // WiFi-Status überwachen
  web_server_handle(); // HTTP-Requests bearbeiten
  storage_tick();      // Auto-Persistierung
}
```

Alle Implementierungsdetails sind in separate, fokussierte Module ausgelagert.

### Debugging

Serial Monitor Ausgaben (115200 baud):
- WiFi-Verbindungsstatus
- IP-Adresse nach erfolgreicher Verbindung
- Storage-Operationen (Laden/Speichern)
- Ring-Buffer-Status

### Erweiterungen

**Neue Sensordaten speichern:**
```cpp
// In main.cpp
float temperature = readTemperatureSensor();
storage_set_value(1, (uint16_t)(temperature * 10));  // Index 1, Wert * 10 für Kommastelle
```

**Neue API-Endpoints:**
Endpoints in `web_server.cpp` erweitern und Handler implementieren.

## 📝 Lizenz

[Lizenz hier einfügen]

## 👥 Autoren

[Autoren hier einfügen]

## 🐛 Bekannte Einschränkungen

- WiFi-Retry verwendet blocking `delay()` (2s pro Retry)
- Ring-Buffer ist aktuell auf 10 uint16-Werte beschränkt
- Nur ein Web-Client gleichzeitig wird unterstützt
