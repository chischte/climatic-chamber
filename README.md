# Climatic Chamber - Klimakammer Steuerung

Professionelle Closed-loop-Steuerung von Temperatur, relativer Luftfeuchtigkeit und CO₂-Gehalt mit Frischluftverwaltung, implementiert auf einer Arduino Portenta Machine Control Plattform.

> 🎯 **Refactored Codebase**: Dieses Projekt folgt Clean Code Prinzipien mit zentralisierter Konfiguration, vollständiger Dokumentation und modularer Architektur. Siehe [REFACTORING.md](REFACTORING.md) für Details.

## 📋 Übersicht

Dieses Projekt implementiert ein vollständiges, professionell strukturiertes Steuerungssystem für eine Klimakammer mit:

- **🎯 Automatische Klimaregelung**: Non-preemptive Steuerung von RH, CO₂ und Temperatur
- **📊 Multi-Sensor Monitoring**: 7 Sensoren (2× CO2, 2× RH, 3× Temp) mit Ring-Buffer
- **📈 Web-Dashboard**: Real-time Charts mit Chart.js (7 Live-Diagramme + 4 Status-Anzeigen)
- **🔬 Simulierte Sensoren**: 10x Speedup für schnelles Testing
- **🔄 Mess-Zyklus**: Swirl → Median-Sampling → Evaluate → Wait
- **💾 Ring-Buffer**: 200 Samples pro Signal
- **📡 WiFi & REST-API**: Web-Interface mit JSON-API
- **💿 Persistente Speicherung**: Flash/RAM-basiert mit Wear-Leveling
- **🏗️ Clean Code**: Zentrale Konfiguration, vollständige Dokumentation, klare Struktur

## 🏆 Code Quality Features

- ✅ **Zentrale Konfiguration** (`config.h`): Alle Konstanten an einem Ort
- ✅ **Vollständige Dokumentation**: Doxygen-Style Kommentare für alle APIs
- ✅ **Keine Magic Numbers**: Alle Werte als benannte Konstanten
- ✅ **Single Responsibility**: Jedes Modul hat einen klaren Zweck
- ✅ **DRY Prinzip**: Keine Code-Duplikation
- ✅ **Beschreibende Namen**: Funktionen und Variablen klar benannt
- ✅ **Modulare Architektur**: Saubere Trennung der Zuständigkeiten

## 🎯 Hauptfunktionen

### Klimaregelung mit 7 Sensoren

Das System überwacht 7 Sensoren gleichzeitig:
- **CO2 Main + CO2 2nd**: Hauptsensor und Vergleichssensor für CO₂-Konzentration
- **RH Main + RH 2nd**: Zwei Luftfeuchtigkeitssensoren für redundante Messung
- **Temp Main + Temp 2nd**: Zwei Innensensoren für gleichmäßige Temperaturerfassung
- **Temp Outer**: Außentemperatur-Sensor für Umgebungsüberwachung

Alle Sensoren werden mit 200-Sample Ring-Buffern erfasst und im Web-Dashboard visualisiert.

### Prioritätsbasierte Steuerung

Das System führt **prioritätsbasierte, non-preemptive Aktionen** aus:

1. **CO₂-Reduktion** (Priorität 1): Bei CO₂ > Setpoint + 100ppm
   - 10s Umwälzer (Swirler) + 20s Settle
   
2. **RH-Reduktion** (Priorität 2): Bei RH > Setpoint + 2%
   - 60s Frischluft + 60s Settle
   - Nach Aktion: RH_UP für 3 Minuten gesperrt
   
3. **RH-Erhöhung** (Priorität 3): Bei RH < Setpoint - 2%
   - 3s Nebler + 10s Mix + 60s Settle
   - Nach Aktion: RH_DOWN für 3 Minuten gesperrt
   
4. **Heater-Regelung** (Independent): Kontinuierliche Temperaturregelung
   - Ein: Bei Temp < Setpoint - 1°C
   - Aus: Bei Temp >= Setpoint

⚠️ **Wichtig**: Laufende Aktionen werden NIE abgebrochen (non-preemptive)!

### Web-Dashboard mit Setpoint-Steuerung

Moderne Web-Oberfläche mit:
- **3 Setpoint-Boxen**: CO2 (400-10000 ppm), RH (82-96%), Temp (18-32°C)
- **Adjustierbare Sollwerte**: Buttons zum direkten Anpassen
- **7 Sensor-Charts**: Multi-Sensor-Anzeige mit Legenden
  - CO2: 2 Linien (Main/2nd) - rot/pink
  - RH: 2 Linien (Main/2nd) - blau/hellblau
  - Temp: 3 Linien (Main/2nd/Outer) - grün/mittelgrün/hellgrün
- **4 Status-Charts**: Fogger, Swirler, FreshAir, Heater (ON/OFF Anzeige)
- **200 Datenpunkte**: ~10 Minuten Verlauf bei 3s Sampling
- **Auto-Refresh**: Alle 3 Sekunden
- **Timestamps**: HH:mm:ss auf x-Achse

### Mess-Zyklus

```
MEASURE_SWIRL (5s) → MEASURE_MEDIAN (10 Samples) → EVALUATE → WAIT (60s) → ⟳
```

- **Swirl**: Umwälzer für gleichmäßige Durchmischung
- **Median**: 10 Messungen über 5s, Median-Filter gegen Ausreißer
- **Evaluate**: Controller entscheidet über nötige Aktion
- **Wait**: Wartezeit bis zum nächsten Zyklus

### Simulierte Sensoren (10x Speedup)

Für schnelles Testing läuft das System **10x schneller als Echtzeit**:
- Sampling: 300ms statt 3000ms
- Alle Aktionen/Wartezeiten durch 10 geteilt
- Realistische Random-Walk-Simulation für alle 7 Sensoren:
  - **RH**: 85-99.5% mit Drift (beide Sensoren leicht unterschiedlich)
  - **Temperatur**: 18-35°C mit Drift (3 Sensoren mit verschiedenen Offsets)
  - **CO₂**: 450-3000 ppm mit gelegentlichen Spitzen (2 Sensoren)

## 🔧 Hardware

- **Plattform**: Arduino Portenta H7 (M7 Core)
- **Board**: Portenta Machine Control
- **MCU**: STM32H747XIH6 @ 480MHz
- **RAM**: 511 KB (19.3% verwendet: 100,848 bytes)
- **Flash**: 768 KB (43.0% verwendet: 338,080 bytes)

### Hardware-Anschlüsse (TODO)

Die IO-Wrapper-Funktionen müssen noch an die tatsächliche Hardware angepasst werden:

```cpp
// In controller.cpp, Zeilen ~302-318
static void setSwirler(bool on) {
  // TODO: Hardware-Pin für Umwälzer setzen
}

static void setFreshAir(bool on) {
  // TODO: Hardware-Pin für Frischluft-Ventil setzen
}

static void setFogger(bool on) {
  // TODO: Hardware-Pin für Nebler setzen
}
```

## 📁 Projektstruktur

```
src/
├── config.h                 # 🆕 Zentrale Konfiguration (ALLE Konstanten)
├── main.cpp                 # Hauptprogramm (~80 Zeilen, dokumentiert)
├── controller.h/cpp         # Klimakammer-Steuerung (~830 Zeilen)
│   ├── SimSensor            # Simulierte 7-Sensor-Umgebung
│   ├── SensorRingBuffer     # Template für 200-Sample Ring-Buffer
│   ├── Measurement SM       # Mess-Zyklus State Machine
│   ├── Action SM            # Non-preemptive Aktionen
│   ├── Heater Control       # Unabhängige Heizungsregelung
│   └── Controller Logic     # Prioritätsbasierte Steuerung
├── credentials.h            # WiFi-Zugangsdaten (nicht in Git)
├── credentials.h.template   # Template für Zugangsdaten
├── wifi_manager.h/cpp       # WiFi-Verbindungsverwaltung
├── storage.h/cpp            # Persistente Datenspeicherung + Setpoints
├── web_server.h/cpp         # HTTP-Server, REST-API, Web-UI (~490 Zeilen)
└── flash_ringbuffer.h/cpp   # Low-Level Flash/RAM Ring-Buffer

lib/
└── Arduino_PortentaMachineControl/  # Hardware-Library

docs/
├── REFACTORING.md           # 🆕 Clean Code Dokumentation
└── README.md                # Diese Datei
```

### Neue config.h - Zentrale Konfiguration

Alle System-Konstanten sind jetzt in `src/config.h` organisiert:

```cpp
namespace Config {
  // System
  constexpr bool SIMULATE_SENSORS = true;
  constexpr uint8_t SPEEDUP_FACTOR = 10;
  constexpr unsigned long SERIAL_BAUD_RATE = 115200;
  
  // Data Collection
  constexpr uint16_t SENSOR_RING_BUFFER_SIZE = 200;
  constexpr unsigned long SAMPLE_INTERVAL_MS = 3000;
  
  // Timing (scaled by SPEEDUP_FACTOR)
  constexpr unsigned long MEDIAN_DURATION_MS = 5000;
  constexpr unsigned long SWIRL_DURATION_MS = 10000;
  constexpr unsigned long FRESHAIR_DURATION_MS = 60000;
  constexpr unsigned long FOGGER_DURATION_MS = 3000;
  
  // Sensor Simulation
  namespace Simulation {
    constexpr int CO2_MIN = 450;
    constexpr int CO2_MAX = 3000;
    constexpr float RH_MIN = 85.0f;
    constexpr float RH_MAX = 99.5f;
    // ... weitere Konstanten
  }
  
  // Control Parameters
  namespace CO2 {
    constexpr uint16_t SETPOINT_MIN = 400;
    constexpr uint16_t SETPOINT_MAX = 10000;
    constexpr uint16_t SETPOINT_DEFAULT = 400;
  }
  
  namespace Humidity {
    constexpr float SETPOINT_MIN = 82.0f;
    constexpr float SETPOINT_MAX = 96.0f;
    constexpr float HYSTERESIS_BAND = 2.0f;
  }
  
  namespace Temperature {
    constexpr float SETPOINT_MIN = 18.0f;
    constexpr float SETPOINT_MAX = 32.0f;
    constexpr float HEATER_ON_THRESHOLD = 1.0f;
  }
  
  // Web Interface
  namespace WebUI {
    constexpr uint16_t CHART_HEIGHT_PX = 150;
    constexpr uint16_t STATUS_CHART_HEIGHT_PX = 40;
    
    namespace Colors {
      constexpr const char* CO2_MAIN = "f44336";
      constexpr const char* RH_MAIN = "2196F3";
      constexpr const char* TEMP_MAIN = "4CAF50";
      // ... weitere Farben
    }
  }
}
```

**Vorteile:**
- ✅ Keine Magic Numbers mehr im Code
- ✅ Einfach anpassbare Parameter
- ✅ Übersichtliche Kategorisierung
- ✅ Namensräume verhindern Konflikte
- ✅ Bessere Testbarkeit

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

## 📚 Module (Refactored & Documented)

### Controller (`controller.h/cpp`)

**Hauptsteuerung der Klimakammer** - vollständig non-blocking, non-preemptive und professionell dokumentiert.

**Refactoring Highlights:**
- ✅ Vollständige Doxygen-Dokumentation aller Public APIs
- ✅ Verwendung von `Config::` Konstanten (keine Magic Numbers)
- ✅ Verbesserte Funktionsnamen (`applySpeedupFactor()` statt `scaled()`)
- ✅ Klare Trennung von Sensor-Simulation und Controller-Logik
- ✅ 7-Sensor-Unterstützung mit individuellen Ring-Buffern

**Konfiguration** (in `config.h`):
```cpp
namespace Config {
  constexpr bool SIMULATE_SENSORS = true;
  constexpr uint8_t SPEEDUP_FACTOR = 10;
  
  namespace CO2 {
    constexpr uint16_t SETPOINT_DEFAULT = 400;
    constexpr int CONTROL_THRESHOLD = 100;
  }
  
  namespace Humidity {
    constexpr float HYSTERESIS_BAND = 2.0f;
  }
  
  namespace Temperature {
    constexpr float HEATER_ON_THRESHOLD = 1.0f;
  }
}
```

**Public API** (vollständig dokumentiert):
```cpp
/**
 * @brief Initialize the climate controller
 * Must be called once during setup before any other controller functions.
 */
void controller_init();

/**
 * @brief Execute one iteration of the control loop
 * Call this repeatedly in the main loop.
 */
void controller_tick();

/**
 * @brief Get last 200 samples from primary sensors
 * @param rh_out    Output array (200 elements)
 * @param temp_out  Output array (200 elements)
 * @param co2_out   Output array (200 elements)
 */
void controller_get_last200(float *rh_out, float *temp_out, int *co2_out);

/**
 * @brief Get last 200 samples from additional sensors
 * @param co2_2_out      Secondary CO2 (200 elements)
 * @param rh_2_out       Secondary humidity (200 elements)
 * @param temp_2_out     Secondary temperature (200 elements)
 * @param temp_outer_out Outer temperature (200 elements)
 */
void controller_get_additional_sensors(int *co2_2_out, float *rh_2_out, 
                                       float *temp_2_out, float *temp_outer_out);

// Setpoint Management (mit Range-Checks)
void controller_set_co2_setpoint(uint16_t ppm);      // 400-10000 ppm
void controller_set_rh_setpoint(float percent);      // 82-96 %
void controller_set_temp_setpoint(float celsius);    // 18-32 °C
```

**Features:**
- ✅ Vollständig non-blocking (nur millis(), kein delay())
- ✅ Non-preemptive Actions (laufende Aktionen nie abbrechen)
- ✅ Drift-free Scheduling (nextMs += period)
- ✅ Median-Filter (10 Samples) gegen Ausreißer
- ✅ Prioritätsbasierte Steuerung (4 Prioritätsstufen)
- ✅ Unabhängige Heizungsregelung (1°C Hysterese)
- ✅ 7-Sensor Multi-Line Charts mit Legenden

**Timing (bei SPEEDUP_FACTOR=10):**
- Sampling: 300ms (statt 3s)
- Mess-Zyklus: Kontinuierlich
- Median-Sampling: 500ms für 10 Samples (statt 5s)
- Aktionen: 0.3-12s (statt 3-120s)

### Web Server (`web_server.h/cpp`)

HTTP-Server mit modernem Dashboard und REST-API.

**Refactoring Highlights:**
- ✅ Zentrale Chart-Konfiguration mit `Config::WebUI::Colors`
- ✅ JSON-API mit strukturierter Datenausgabe (11 Arrays für 7 Sensoren + 4 Outputs)
- ✅ Responsive Design mit flexiblem Grid-Layout
- ✅ Multi-Dataset Charts mit Legenden
- ✅ Cache-Mechanismus für Performance (1s Cache)

**Endpoints:**

| Endpoint | Methode | Beschreibung |
|----------|---------|--------------|
| `/` | GET | **Klimakammer-Dashboard** mit 11 Diagrammen |
| `/api/last200` | GET | JSON-API: Alle Sensoren + Outputs (200 Samples je) |

**API-Beispiel:**
```bash
# Letzte 200 Samples abrufen
curl http://<ip-adresse>/api/last200

# Response (JSON):
{
  "co2": [0,0,...,890,905],           # 200 Werte, oldest→newest
  "co2_2": [0,0,...,900,915],         # Zweiter CO2-Sensor
  "rh": [0,0,...,92.3,92.5],          # Hauptsensor RH
  "rh_2": [0,0,...,90.8,91.1],        # Zweiter RH-Sensor
  "temp": [0,0,...,24.8,25.1],        # Hauptsensor Temp
  "temp_2": [0,0,...,23.9,24.2],      # Zweiter Temp-Sensor
  "temp_outer": [0,0,...,21.5,21.8],  # Außentemperatur
  "fogger": [0,0,...,0,1],            # Nebel-Status
  "swirler": [0,0,...,1,1],           # Umwälzer-Status
  "freshair": [0,0,...,0,0],          # Frischluft-Status
  "heater": [0,0,...,1,1],            # Heizung-Status
  "time": 12345,                      # Uptime in Sekunden
  "setpoints": {
    "co2": 400,
    "rh": 96.0,
    "temp": 30.0
  }
}
```

**Web-Dashboard Features:**
- 📊 **11 Echtzeit-Diagramme**:
  - 3 Sensor-Charts mit Multi-Line (2-3 Sensoren pro Chart)
  - 4 Status-Charts (Binary ON/OFF mit Stepped-Line)
- 🎛️ **3 Setpoint-Boxen**: Direkte Anpassung von CO2, RH, Temp
- 🔄 **Auto-Refresh**: Alle 3 Sekunden
- 📱 **Responsive Design**: Flexibles Grid-Layout
- 🎨 **Chart.js 4.4.0**: Professional charts mit Legenden
- ⏱️ **Timestamps**: HH:mm:ss Format auf X-Achse
- 📍 **Current Values**: Aktuelle Werte in Setpoint-Boxen
- 🎯 **Farbkodiert**: Consistent color scheme aus config.h

**Chart-Konfiguration** (in `config.h`):
```cpp
namespace Config::WebUI {
  constexpr uint16_t CHART_HEIGHT_PX = 150;
  constexpr uint16_t STATUS_CHART_HEIGHT_PX = 40;
  
  namespace Colors {
    constexpr const char* CO2_MAIN = "f44336";        // Rot
    constexpr const char* CO2_SECONDARY = "e91e63";   // Pink
    constexpr const char* RH_MAIN = "2196F3";         // Blau
    constexpr const char* RH_SECONDARY = "64B5F6";    // Hellblau
    constexpr const char* TEMP_MAIN = "4CAF50";       // Grün
    constexpr const char* TEMP_SECONDARY = "66BB6A";  // Mittelgrün
    constexpr const char* TEMP_OUTER = "8BC34A";      // Hellgrün
    constexpr const char* FOGGER = "9C27B0";          // Lila
    constexpr const char* SWIRLER = "FF9800";         // Orange
    constexpr const char* FRESHAIR = "00BCD4";        // Cyan
    constexpr const char* HEATER = "FF5722";          // Rot-Orange
  }
}
```


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

## 🔒 Sicherheit

- **credentials.h ist in .gitignore**: Zugangsdaten werden nicht versioniert
- **credentials.h.template**: Enthält Platzhalter für andere Entwickler
- Template in Git committen, echte Credentials lokal halten

## 🏗️ Clean Code Refactoring

Das Projekt wurde nach **Clean Code Prinzipien** refaktoriert. Details siehe [REFACTORING.md](REFACTORING.md).

### Wichtigste Verbesserungen

**1. Zentrale Konfiguration** (`config.h`)
- Alle Magic Numbers in benannte Konstanten extrahiert
- Organisiert in Namensräumen (`Config::`, `Config::Simulation::`, `Config::WebUI::`)
- Einfach anpassbar für verschiedene Szenarien

**2. Vollständige Dokumentation**
- Alle Public APIs mit Doxygen-Kommentaren
- Parameter und Rückgabewerte dokumentiert
- Modul-Dokumentation mit Architektur-Übersicht

**3. Verbesserte Namen**
- `scaled()` → `applySpeedupFactor()` (selbsterklärend)
- `readSensors3()` → `readCurrentSensors()` (keine mystischen Zahlen)
- Keine Abkürzungen ohne klare Bedeutung

**4. Modulare Struktur**
- Single Responsibility: Jedes Modul hat einen klaren Zweck
- Klare Trennung von Zuständigkeiten
- Einfach testbare Komponenten

**5. Keine Code-Duplikation**
- Ring-Buffer als wiederverwendbares Template
- Einheitliche Sensor-Simulation
- Gemeinsame Chart-Konfiguration

### Code Quality Metriken

| Metrik | Vorher | Nachher |
|--------|--------|---------|
| Magic Numbers | ~45 | 0 |
| Undokumentierte APIs | ~25 | 0 |
| Konfig-Locations | 5 Dateien | 1 Datei |
| Dokumentationsstil | Inkonsistent | Doxygen |
| Code-Duplikation | Mehrfach | Minimal |

### Für Entwickler

Beim Hinzufügen neuer Features:

1. **Constants zu config.h** hinzufügen
2. **Doxygen-Kommentare** für neue Public Functions
3. **Beschreibende Namen** verwenden
4. **DRY-Prinzip** beachten (Don't Repeat Yourself)
5. **Single Responsibility** pro Modul/Funktion

## 🛠️ Entwicklung & Debugging

### Code-Struktur

Das Hauptprogramm (`main.cpp`) ist bewusst minimal gehalten und gut strukturiert:

```cpp
/**
 * @brief Initialize all subsystems
 * Order: Serial → Storage → Controller → WiFi
 */
void setup() {
  Serial.begin(Config::SERIAL_BAUD_RATE);
  Serial.println(F("=== Climatic Chamber Control System ==="));
  
  Serial.print(F("Storage... "));
  storage_init();
  storage_load();
  Serial.println(F("OK"));
  
  Serial.print(F("Controller... "));
  controller_init();
  Serial.println(F("OK"));
  
  Serial.print(F("WiFi... "));
  wifi_init(WIFI_SSID, WIFI_PASS);
  Serial.println(F("OK"));
  
  Serial.println(F("=== System Ready ==="));
}

/**
 * @brief Main control loop - non-blocking tick functions
 */
void loop() {
  controller_tick();    // Climate control
  wifi_tick();          // WiFi status
  web_server_handle();  // HTTP requests
  storage_tick();       // Auto-Persistierung
}
```

Alle Implementierungsdetails sind in separate, fokussierte Module ausgelagert.

### Serial Monitor Debug-Ausgaben

Bei 115200 baud zeigt der Serial Monitor:

**WiFi & Netzwerk:**
```
WiFi: Connecting to mueschbache
WiFi: WL_CONNECTED
Connected! IP: 192.168.1.42
```

**Controller Initialisierung:**
```
Controller: Initializing...
SPEEDUP: 10
Controller: Ready
```

**Mess-Zyklus:**
```
Measurement: SWIRL
Measurement: MEDIAN sampling
  Sample 1/10: RH=92.3 Temp=24.8 CO2=890
  Sample 2/10: RH=92.5 Temp=24.9 CO2=895
  ...
  Sample 10/10: RH=92.8 Temp=25.1 CO2=905
Median: RH=92.5 Temp=24.9 CO2=900
Measurement: WAIT
```

**Controller-Aktionen:**
```
Controller: CO2 high (1150 ppm) -> CO2 action
Swirler: ON
Action: CO2 - SWIRL
Action: CO2 - SETTLE
Swirler: OFF
Action: CO2 - COMPLETE
```

**Storage-Operationen:**
```
Initializing Ring Buffer (6400 bytes)...
Flash block device not available; using RAM ring buffer
Ring Buffer initialized: 100 slots available
Loaded data from slot 42 (seq=123, values[0]=5)
```

### Hardware-Integration

Um echte Sensoren zu verwenden:

1. **Sensoren aktivieren** in `controller.cpp`:
   ```cpp
   #define SIMULATE_SENSORS 0  // Echte Sensoren verwenden
   ```

2. **Sensor-Leselogik implementieren**:
   ```cpp
   static Sensors readSensors3() {
   #if SIMULATE_SENSORS
     return g_simSensor.read();
   #else
     // Ihre Sensor-Implementierung hier
     Sensors s;
     s.rh = MachineControl.analog_in.read(0);    // Beispiel
     s.temp = MachineControl.temp_probes.read(0); // Beispiel
     s.co2 = readCO2Sensor();                     // Ihre Funktion
     return s;
   #endif
   }
   ```

3. **IO-Pins konfigurieren** in `controller.cpp`:
   ```cpp
   static void setSwirler(bool on) {
     digitalWrite(SWIRLER_PIN, on ? HIGH : LOW);
     Serial.print("Swirler: ");
     Serial.println(on ? "ON" : "OFF");
   }
   
   // Analog für setFreshAir() und setFogger()
   ```

### Erweiterungen

**Schwellwerte anpassen:**
```cpp
// In controller.cpp
static constexpr int CO2_THRESHOLD = 1200;        // Von 1100→1200
static constexpr float RH_HIGH_THRESHOLD = 95.0f; // Von 98→95
static constexpr float RH_LOW_THRESHOLD = 90.0f;  // Von 94→90
```

**Speedup ändern:**
```cpp
// controller.cpp oder controller.h
static constexpr uint8_t SPEEDUP = 1;  // Real-time
static constexpr uint8_t SPEEDUP = 5;  // 5x schneller
static constexpr uint8_t SPEEDUP = 100; // 100x schneller (sehr schnell!)
```

**Aktionsdauern anpassen:**
```cpp
// In controller.cpp (Real-time-Werte, werden automatisch durch SPEEDUP geteilt)
static constexpr unsigned long RT_CO2_SWIRL_MS = 20000;  // 20s statt 10s
static constexpr unsigned long RT_RH_UP_SETTLE_MS = 180000; // 3min statt 2min
```

## 🧪 Testing & Inbetriebnahme

### 1. Simulation testen (ohne Hardware)

```bash
# Upload code
platformio run --target upload

# Monitor öffnen
platformio device monitor

# Browser öffnen mit angezeigter IP
# Diagramme sollten sofort aktualisieren
```

**Erwartetes Verhalten:**
- Charts zeigen realistische Random-Walk-Werte
- Controller reagiert auf simulierte Schwellwertüberschreitungen
- Serial Monitor zeigt Mess-Zyklen und Aktionen

### 2. Hardware-Integration

```cpp
// controller.cpp
#define SIMULATE_SENSORS 0  // Ändern
static constexpr uint8_t SPEEDUP = 1;  // Real-time für echte Hardware
```

### 3. Performance-Check

Aktueller RAM/Flash-Verbrauch:
- **RAM**: 16.8% (88 KB / 524 KB)
  - Ring-Buffers: 3 × 200 × 4 Bytes = 2.4 KB
  - Stack/Heap: ~85 KB
- **Flash**: 40.5% (319 KB / 786 KB)

## 📝 Lizenz

[Lizenz hier einfügen]

## 👥 Autoren

[Autoren hier einfügen]

## 🐛 Bekannte Einschränkungen

### Controller
- ✅ Vollständig non-blocking implementiert
- ✅ Keine dynamische Heap-Allokation
- ⚠️ Nur Simulation - echte Sensoren müssen noch integriert werden
- ⚠️ IO-Wrapper mit Dummy-Implementierung (nur Serial-Debug)

### WiFi & Netzwerk
- ⚠️ WiFi-Retry verwendet blocking `delay()` (2s pro Retry)  
  → Nur während der Initialisierung in `setup()`, nicht in `loop()`
- ⚠️ Nur ein Web-Client gleichzeitig wird unterstützt
- ⚠️ Keine HTTPS-Unterstützung

### Datenspeicherung
- ✅ Ring-Buffer mit 10 uint16-Werten für persistente Daten
- ℹ️ Sensor-Daten (200 Samples) werden **nicht** persistent gespeichert (RAM-only)
- ℹ️ Nach Neustart startet Ring-Buffer bei 0 (vorgesehen)

### Web-UI
- ⚠️ Chart.js wird von CDN geladen (benötigt Internetverbindung)
- ⚠️ JSON-Serialisierung für 200 Samples kann bei langsamen Clients ~1-2s dauern
- ℹ️ Keine Authentifizierung/Autorisierung

## 🚀 Roadmap

- [ ] Integration echter Sensoren (RH, Temp, CO2)
- [ ] Hardware-Pins für Outputs konfigurieren
- [ ] Optional: Datenlogging auf SD-Karte
- [ ] Optional: MQTT für externe Monitoring-Systeme
- [ ] Optional: PID-Controller für präzisere Regelung
- [ ] Optional: Web-UI ohne CDN (lokale Chart.js-Kopie)
