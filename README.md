# Climatic Chamber - Klimakammer Steuerung

Closed-loop-Steuerung von Temperatur, relativer Luftfeuchtigkeit und CO₂-Gehalt mit Frischluftverwaltung, implementiert auf einer Arduino Portenta Machine Control Plattform.

## 📋 Übersicht

Dieses Projekt implementiert ein vollständiges Steuerungssystem für eine Klimakammer mit:

- **🎯 Automatische Klimaregelung**: Non-preemptive Steuerung von RH, CO₂ und Temperatur
- **📊 Echtzeit-Monitoring**: Web-Dashboard mit Chart.js (3 Live-Diagramme)
- **🔬 Simulierte Sensoren**: 10x Speedup für schnelles Testing
- **🔄 Mess-Zyklus**: Swirl → Median-Sampling → Evaluate → Wait
- **💾 Ring-Buffer**: 200 Samples pro Signal (RH, Temp, CO2)
- **📡 WiFi & Web-API**: REST-API und Web-Interface
- **💿 Persistente Speicherung**: Flash/RAM-basiert mit Wear-Leveling

## 🎯 Hauptfunktionen

### Klimaregelung

Das System führt **prioritätsbasierte, non-preemptive Aktionen** aus:

1. **CO₂-Reduktion** (Priorität 1): Bei CO₂ > 1100 ppm
   - 10s Umwälzer (Swirler) + 20s Settle
   
2. **RH-Reduktion** (Priorität 2): Bei RH > 98%
   - 10s Frischluft + 10s Umwälzer + 20s Settle
   - Nach Aktion: RH_UP für 3 Minuten gesperrt
   
3. **RH-Erhöhung** (Priorität 3): Bei RH < 94%
   - 5s Nebler + 10s Mix (alle Outputs) + 120s Settle
   - Nach Aktion: RH_DOWN für 3 Minuten gesperrt
   
4. **Baseline-Lüftung** (Priorität 4): Wenn 10 Minuten keine Belüftung
   - 10s Frischluft + 10s Settle

⚠️ **Wichtig**: Laufende Aktionen werden NIE abgebrochen (non-preemptive)!

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
- Sampling: 100ms statt 1000ms
- Alle Aktionen/Wartezeiten durch 10 geteilt
- Realistische Random-Walk-Simulation:
  - **RH**: 85-99.5% mit Drift
  - **Temperatur**: 18-35°C mit Drift
  - **CO₂**: 450-3000 ppm mit gelegentlichen Spitzen

## 🔧 Hardware

- **Plattform**: Arduino Portenta H7 (M7 Core)
- **Board**: Portenta Machine Control
- **MCU**: STM32H747XIH6 @ 480MHz
- **RAM**: 511 KB (16.8% verwendet)
- **Flash**: 768 KB (40.5% verwendet)

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
├── main.cpp                 # Hauptprogramm (~60 Zeilen)
├── controller.h/cpp         # Klimakammer-Steuerung (650 Zeilen)
│   ├── SimSensor            # Simulierte Sensoren
│   ├── SensorRingBuffer     # 200-Sample Ring-Buffer
│   ├── Measurement SM       # Mess-Zyklus State Machine
│   ├── Action SM            # Non-preemptive Aktionen
│   └── Controller Logic     # Prioritätsbasierte Steuerung
├── credentials.h            # WiFi-Zugangsdaten (nicht in Git)
├── credentials.h.template   # Template für Zugangsdaten
├── wifi_manager.h/cpp       # WiFi-Verbindungsverwaltung
├── storage.h/cpp            # Persistente Datenspeicherung
├── web_server.h/cpp         # HTTP-Server, REST-API, Web-UI
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

### Controller (`controller.h/cpp`)

**Hauptsteuerung der Klimakammer** - vollständig non-blocking und non-preemptive.

**Konfiguration** (in `controller.h/cpp`):
```cpp
#define SIMULATE_SENSORS 1              // 1 = Simulation, 0 = echte Sensoren
static constexpr uint8_t SPEEDUP = 10;  // Speedup-Faktor (10 = 10x schneller)

// Schwellwerte
static constexpr int CO2_THRESHOLD = 1100;        // ppm
static constexpr float RH_HIGH_THRESHOLD = 98.0f; // %
static constexpr float RH_LOW_THRESHOLD = 94.0f;  // %
```

**API:**
```cpp
controller_init();                          // Initialisierung (in setup())
controller_tick();                          // Periodischer Tick (in loop())
controller_get_last200(rh, temp, co2);     // Letzte 200 Samples abrufen
```

**Features:**
- ✅ Vollständig non-blocking (nur millis(), kein delay())
- ✅ Non-preemptive Actions (laufende Aktionen nie abbrechen)
- ✅ Drift-free Scheduling (nextMs += period)
- ✅ Median-Filter (10 Samples) gegen Ausreißer
- ✅ Prioritätsbasierte Steuerung (4 Prioritätsstufen)
- ✅ Lockout-Mechanismus (3 min nach RH-Aktionen)
- ✅ Baseline-Lüftung (alle 10 min)

**Timing (bei SPEEDUP=10):**
- Sampling: 100ms (statt 1s)
- Mess-Zyklus Start: alle 6s (statt 60s)
- Median-Sampling: 500ms für 10 Samples (statt 5s)
- Aktionen: 1-12s (statt 10-120s)

### Web Server (`web_server.h/cpp`)

HTTP-Server mit Web-UI und REST-API.

**Endpoints:**

| Endpoint | Methode | Beschreibung |
|----------|---------|--------------|
| `/` | GET | **Klimakammer-Dashboard** mit 3 Chart.js-Diagrammen |
| `/api/last200` | GET | JSON-API: Letzte 200 Samples (RH, Temp, CO2) |
| `/old` | GET | Legacy Counter-Interface |
| `/inc` | POST | Legacy: Counter incrementieren |

**API-Beispiel:**
```bash
# Letzte 200 Samples abrufen
curl http://<ip-adresse>/api/last200

# Response (JSON):
{
  "rh": [0,0,...,92.3,92.5],      # 200 Werte, oldest→newest
  "temp": [0,0,...,24.8,25.1],    # 200 Werte
  "co2": [0,0,...,890,905]        # 200 Werte
}
```

**Web-UI Features:**
- 📊 3 Echtzeit-Diagramme (RH, Temp, CO2)
- 🔄 Auto-Refresh alle 200ms
- 📱 Responsive Design
- 🎨 Chart.js via CDN (keine lokalen Dateien)
- ⚡ Keine Animationen (Performance)

**Screenshot:**
```
┌─────────────────────────────────────────┐
│ Climate Chamber Monitor                 │
│ Current: RH=92.3% | Temp=25.1°C | CO2=905 ppm │
├─────────────────────────────────────────┤
│ ┌───────────────────────────────────┐   │
│ │  RH (%)   Chart                   │   │
│ └───────────────────────────────────┘   │
│ ┌───────────────────────────────────┐   │
│ │  Temp (°C) Chart                  │   │
│ └───────────────────────────────────┘   │
│ ┌───────────────────────────────────┐   │
│ │  CO2 (ppm) Chart                  │   │
│ └───────────────────────────────────┘   │
└─────────────────────────────────────────┘
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

## 🛠️ Entwicklung & Debugging

### Code-Struktur

Das Hauptprogramm (`main.cpp`) ist bewusst minimal gehalten:

```cpp
void setup() {
  storage_init();                     // Speicher initialisieren
  storage_load();                     // Persistierte Daten laden
  controller_init();                  // Klimakammer-Controller initialisieren
  wifi_init(WIFI_SSID, WIFI_PASS);   // WiFi verbinden
}

void loop() {
  controller_tick();    // Klimakammer-Steuerung
  wifi_tick();          // WiFi-Status überwachen
  web_server_handle();  // HTTP-Requests bearbeiten
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
