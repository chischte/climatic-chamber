/*
 * *****************************************************************************
 * CLIMATE CHAMBER CONTROLLER - IMPLEMENTATION
 * *****************************************************************************
 */

#include "controller.h"
#include "storage.h"

// --- Config (SPEEDUP, timing constants) ---

// Import constants from config.h for local use
static constexpr uint8_t SPEEDUP = Config::SPEEDUP_FACTOR;
static constexpr uint16_t RING_BUFFER_SIZE = Config::SENSOR_RING_BUFFER_SIZE;

#if !defined(SIMULATE_SENSORS)
  static constexpr bool SIMULATE_SENSORS = Config::SIMULATE_SENSORS;
#endif

// Helper: scale milliseconds by SPEEDUP, never return 0 for non-zero input
static inline unsigned long scaled(unsigned long ms) {
  if (ms == 0) return 0;
  unsigned long result = ms / SPEEDUP;
  return (result == 0) ? 1 : result;
}

// Timing constants (real-time values, will be scaled)
static constexpr unsigned long RT_SAMPLE_PERIOD_MS = 1000;          // 1 second sampling
static constexpr unsigned long RT_POLL_PERIOD_MS = 200;             // 200ms UI poll (not scaled, already fast)
static constexpr unsigned long RT_MEDIAN_SAMPLE_PERIOD_MS = 1000;   // 1000ms per median sample
static constexpr unsigned long RT_MEASURE_SWIRL_DURATION_MS = 5000; // 5s swirl before measurement
static constexpr unsigned long RT_MEDIAN_DURATION_MS = 5000;        // 5s median sampling (5 samples)
static constexpr unsigned long RT_WAIT_BETWEEN_CYCLES_MS = 60000;   // 60s wait

// Action durations (real-time)
static constexpr unsigned long RT_CO2_SWIRL_MS = 10000;
static constexpr unsigned long RT_CO2_SETTLE_MS = 20000;
static constexpr unsigned long RT_RH_DOWN_FRESHAIR_MS = 10000;
static constexpr unsigned long RT_RH_DOWN_SWIRL_MS = 10000;
static constexpr unsigned long RT_RH_DOWN_SETTLE_MS = 20000;
static constexpr unsigned long RT_RH_UP_FOGGER_MS = 5000;
static constexpr unsigned long RT_RH_UP_MIX_MS = 10000;
static constexpr unsigned long RT_RH_UP_SETTLE_MS = 120000;
static constexpr unsigned long RT_BASELINE_FRESHAIR_MS = 10000;
static constexpr unsigned long RT_BASELINE_SETTLE_MS = 10000;

// Lockout and baseline timing (real-time)
static constexpr unsigned long RT_RH_LOCKOUT_MS = 180000;       // 3 minutes
static constexpr unsigned long RT_BASELINE_INTERVAL_MS = 600000; // 10 minutes

// Thresholds (loaded from storage)
static uint16_t g_co2_setpoint = 800;  // ppm
static float g_rh_setpoint = 95.0f;     // %
static float g_temp_setpoint = 25.0f;   // °C
static constexpr float RH_HYSTERESIS = 2.0f; // Use setpoint ± 2% for high/low thresholds

// Median sample count
static constexpr uint8_t MEDIAN_SAMPLE_COUNT = 5;

// --- RingBuffer<T, 200> ---

template<typename T, uint16_t N>
class SensorRingBuffer {
private:
  T buffer[N];
  uint16_t head;
  uint16_t count;

public:
  SensorRingBuffer() : head(0), count(0) {
    for (uint16_t i = 0; i < N; i++) {
      buffer[i] = 0;
    }
  }

  void push(T value) {
    buffer[head] = value;
    head = (head + 1) % N;
    if (count < N) count++;
  }

  void getAll(T *out) const {
    if (count < N) {
      // Buffer not full: fill with zeros, then real data
      uint16_t fillCount = N - count;
      for (uint16_t i = 0; i < fillCount; i++) {
        out[i] = 0;
      }
      // Copy real data oldest->newest
      uint16_t start = 0;
      for (uint16_t i = 0; i < count; i++) {
        out[fillCount + i] = buffer[(start + i) % N];
      }
    } else {
      // Buffer full: head points to oldest
      for (uint16_t i = 0; i < N; i++) {
        out[i] = buffer[(head + i) % N];
      }
    }
  }

  uint16_t size() const { return count; }
};

// --- SimSensor ---

#if SIMULATE_SENSORS

class SimSensor {
private:
  float rh;
  float rh_2;
  float temp;
  float temp_2;
  float temp_outer;
  int co2;
  int co2_2;
  unsigned long lastUpdate;
  
  // Random walk parameters
  float rhDrift;
  float rh_2Drift;
  float tempDrift;
  float temp_2Drift;
  float tempOuterDrift;
  float co2Drift;
  float co2_2Drift;
  uint8_t co2PulseCounter;
  uint8_t co2_2PulseCounter;

  float randomWalk(float current, float &drift, float min, float max, float noise, float driftSpeed) {
    // Update drift
    drift += (random(-100, 101) / 10000.0f) * driftSpeed;
    drift = constrain(drift, -0.05f, 0.05f);
    
    // Apply drift and noise
    float value = current + drift + (random(-100, 101) / 1000.0f) * noise;
    return constrain(value, min, max);
  }

public:
  SimSensor() : rh(92.0f), rh_2(90.5f), temp(25.0f), temp_2(24.0f), temp_outer(22.0f), co2(800), co2_2(820), lastUpdate(0), 
                rhDrift(0), rh_2Drift(0), tempDrift(0), temp_2Drift(0), tempOuterDrift(0), co2Drift(0), co2_2Drift(0), 
                co2PulseCounter(0), co2_2PulseCounter(0) {
    randomSeed(analogRead(0));
  }

  Sensors read() {
    unsigned long now = millis();
    if (now - lastUpdate >= scaled(RT_SAMPLE_PERIOD_MS)) {
      lastUpdate = now;
      
      // Update RH (85..99.5)
      rh = randomWalk(rh, rhDrift, 85.0f, 99.5f, 0.3f, 0.5f);
      
      // Update RH_2 (85..99.5) - slightly different values
      rh_2 = randomWalk(rh_2, rh_2Drift, 85.0f, 99.5f, 0.3f, 0.5f);
      
      // Update Temp inner (18..35)
      temp = randomWalk(temp, tempDrift, 18.0f, 35.0f, 0.2f, 0.3f);
      
      // Update Temp_2 (18..35) - slightly different values
      temp_2 = randomWalk(temp_2, temp_2Drift, 18.0f, 35.0f, 0.2f, 0.3f);
      
      // Update Temp outer (15..32) - slightly cooler range
      temp_outer = randomWalk(temp_outer, tempOuterDrift, 15.0f, 32.0f, 0.2f, 0.3f);
      
      // Update CO2 with occasional pulses (450..3000)
      co2Drift += (random(-100, 101) / 100.0f);
      co2Drift = constrain(co2Drift, -10.0f, 10.0f);
      
      int co2Value = co2 + (int)co2Drift + random(-20, 21);
      
      // Occasional CO2 pulse
      if (co2PulseCounter > 0) {
        co2Value += 500;
        co2PulseCounter--;
      } else if (random(0, 1000) < 5) { // 0.5% chance
        co2PulseCounter = 10;
      }
      
      co2 = constrain(co2Value, 450, 3000);
      
      // Update CO2_2 with slightly different values (offset by 10-30 ppm)
      co2_2Drift += (random(-100, 101) / 100.0f);
      co2_2Drift = constrain(co2_2Drift, -10.0f, 10.0f);
      
      int co2_2Value = co2_2 + (int)co2_2Drift + random(-20, 21);
      
      // Occasional CO2_2 pulse (different timing than CO2)
      if (co2_2PulseCounter > 0) {
        co2_2Value += 500;
        co2_2PulseCounter--;
      } else if (random(0, 1000) < 3) { // 0.3% chance
        co2_2PulseCounter = 10;
      }
      
      co2_2 = constrain(co2_2Value, 450, 3000);
    }
    
    return {co2, co2_2, rh, rh_2, temp, temp_2, temp_outer};
  }
};

static SimSensor g_simSensor;

#endif

// Read sensors (simulated or real)
static Sensors readSensors3() {
#if SIMULATE_SENSORS
  return g_simSensor.read();
#else
  // TODO: Read real sensors here
  // For now, return dummy values
  return {500, 520, 50.0f, 51.0f, 20.0f, 19.5f, 18.0f};
#endif
}

// --- Median helpers (N=10, even median = average of middle two) ---

template<typename T>
static void sortArray(T *arr, uint8_t n) {
  for (uint8_t i = 0; i < n - 1; i++) {
    for (uint8_t j = 0; j < n - i - 1; j++) {
      if (arr[j] > arr[j + 1]) {
        T temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
}

static float medianFloat(float *values, uint8_t n) {
  if (n == 0) return 0.0f;
  float sorted[MEDIAN_SAMPLE_COUNT];
  for (uint8_t i = 0; i < n; i++) {
    sorted[i] = values[i];
  }
  sortArray(sorted, n);
  
  if (n % 2 == 0) {
    // Even: average of middle two
    return (sorted[n/2 - 1] + sorted[n/2]) / 2.0f;
  } else {
    return sorted[n/2];
  }
}

static int medianInt(int *values, uint8_t n) {
  if (n == 0) return 0;
  int sorted[MEDIAN_SAMPLE_COUNT];
  for (uint8_t i = 0; i < n; i++) {
    sorted[i] = values[i];
  }
  sortArray(sorted, n);
  
  if (n % 2 == 0) {
    return (sorted[n/2 - 1] + sorted[n/2]) / 2;
  } else {
    return sorted[n/2];
  }
}

// --- Actions + Context + IO wrapper ---

enum ActionType {
  ACTION_NONE,
  ACTION_CO2,
  ACTION_RH_DOWN,
  ACTION_RH_UP,
  ACTION_BASELINE
};

enum ActionStage {
  STAGE_IDLE,
  STAGE_CO2_SWIRL,
  STAGE_CO2_SETTLE,
  STAGE_RH_DOWN_FRESHAIR,
  STAGE_RH_DOWN_SWIRL,
  STAGE_RH_DOWN_SETTLE,
  STAGE_RH_UP_FOGGER,
  STAGE_RH_UP_MIX,
  STAGE_RH_UP_SETTLE,
  STAGE_BASELINE_FRESHAIR,
  STAGE_BASELINE_SETTLE
};

struct ActionContext {
  ActionType currentAction;
  ActionStage currentStage;
  unsigned long stageStartMs;
  unsigned long rhUpLockoutUntilMs;
  unsigned long rhDownLockoutUntilMs;
  unsigned long lastVentilationMs;
  
  ActionContext() : currentAction(ACTION_NONE), currentStage(STAGE_IDLE),
                    stageStartMs(0), rhUpLockoutUntilMs(0), 
                    rhDownLockoutUntilMs(0), lastVentilationMs(0) {}
};

// Output state tracking
static bool g_swirlerState = false;
static bool g_freshAirState = false;
static bool g_foggerState = false;
static bool g_heaterState = false;

// IO Wrapper (adapt to your hardware)
static void setSwirler(bool on) {
  g_swirlerState = on;
  // TODO: Implement real hardware control
  Serial.print("Swirler: ");
  Serial.println(on ? "ON" : "OFF");
}

static void setFreshAir(bool on) {
  g_freshAirState = on;
  // TODO: Implement real hardware control
  Serial.print("FreshAir: ");
  Serial.println(on ? "ON" : "OFF");
}

static void setFogger(bool on) {
  g_foggerState = on;
  // TODO: Implement real hardware control
  Serial.print("Fogger: ");
  Serial.println(on ? "ON" : "OFF");
}

static void setHeater(bool on) {
  g_heaterState = on;
  // TODO: Implement real hardware control
  Serial.print("Heater: ");
  Serial.println(on ? "ON" : "OFF");
}

static void allOutputsOff() {
  setSwirler(false);
  setFreshAir(false);
  setFogger(false);
  setHeater(false);
}

// --- Controller (non-preemptive) ---

static ActionContext g_actionCtx;

// Start an action (only if no action is running)
static void startAction(ActionType action) {
  if (g_actionCtx.currentAction != ACTION_NONE) {
    return; // Action already running, don't preempt
  }
  
  g_actionCtx.currentAction = action;
  g_actionCtx.stageStartMs = millis();
  
  switch (action) {
    case ACTION_CO2:
      g_actionCtx.currentStage = STAGE_CO2_SWIRL;
      setSwirler(true);
      Serial.println("Action: CO2 - SWIRL");
      break;
      
    case ACTION_RH_DOWN:
      g_actionCtx.currentStage = STAGE_RH_DOWN_FRESHAIR;
      setFreshAir(true);
      g_actionCtx.lastVentilationMs = millis();
      Serial.println("Action: RH_DOWN - FRESHAIR");
      break;
      
    case ACTION_RH_UP:
      g_actionCtx.currentStage = STAGE_RH_UP_FOGGER;
      setFogger(true);
      Serial.println("Action: RH_UP - FOGGER");
      break;
      
    case ACTION_BASELINE:
      g_actionCtx.currentStage = STAGE_BASELINE_FRESHAIR;
      setFreshAir(true);
      g_actionCtx.lastVentilationMs = millis();
      Serial.println("Action: BASELINE - FRESHAIR");
      break;
      
    default:
      break;
  }
}

// Tick action state machine
static void actionTick() {
  if (g_actionCtx.currentAction == ACTION_NONE) {
    return;
  }
  
  unsigned long now = millis();
  unsigned long elapsed = now - g_actionCtx.stageStartMs;
  
  switch (g_actionCtx.currentStage) {
    // --- CO2 Action ---
    case STAGE_CO2_SWIRL:
      if (elapsed >= scaled(RT_CO2_SWIRL_MS)) {
        setSwirler(false);
        g_actionCtx.currentStage = STAGE_CO2_SETTLE;
        g_actionCtx.stageStartMs = now;
        Serial.println("Action: CO2 - SETTLE");
      }
      break;
      
    case STAGE_CO2_SETTLE:
      if (elapsed >= scaled(RT_CO2_SETTLE_MS)) {
        allOutputsOff();
        g_actionCtx.currentAction = ACTION_NONE;
        g_actionCtx.currentStage = STAGE_IDLE;
        Serial.println("Action: CO2 - COMPLETE");
      }
      break;
      
    // --- RH_DOWN Action ---
    case STAGE_RH_DOWN_FRESHAIR:
      if (elapsed >= scaled(RT_RH_DOWN_FRESHAIR_MS)) {
        setFreshAir(false);
        setSwirler(true);
        g_actionCtx.currentStage = STAGE_RH_DOWN_SWIRL;
        g_actionCtx.stageStartMs = now;
        Serial.println("Action: RH_DOWN - SWIRL");
      }
      break;
      
    case STAGE_RH_DOWN_SWIRL:
      if (elapsed >= scaled(RT_RH_DOWN_SWIRL_MS)) {
        setSwirler(false);
        g_actionCtx.currentStage = STAGE_RH_DOWN_SETTLE;
        g_actionCtx.stageStartMs = now;
        Serial.println("Action: RH_DOWN - SETTLE");
      }
      break;
      
    case STAGE_RH_DOWN_SETTLE:
      if (elapsed >= scaled(RT_RH_DOWN_SETTLE_MS)) {
        allOutputsOff();
        g_actionCtx.rhUpLockoutUntilMs = now + scaled(RT_RH_LOCKOUT_MS);
        g_actionCtx.currentAction = ACTION_NONE;
        g_actionCtx.currentStage = STAGE_IDLE;
        Serial.println("Action: RH_DOWN - COMPLETE (RH_UP locked for 3 min)");
      }
      break;
      
    // --- RH_UP Action ---
    case STAGE_RH_UP_FOGGER:
      if (elapsed >= scaled(RT_RH_UP_FOGGER_MS)) {
        setSwirler(true);
        setFreshAir(true);
        // Fogger stays on
        g_actionCtx.currentStage = STAGE_RH_UP_MIX;
        g_actionCtx.stageStartMs = now;
        g_actionCtx.lastVentilationMs = now;
        Serial.println("Action: RH_UP - MIX");
      }
      break;
      
    case STAGE_RH_UP_MIX:
      if (elapsed >= scaled(RT_RH_UP_MIX_MS)) {
        allOutputsOff();
        g_actionCtx.currentStage = STAGE_RH_UP_SETTLE;
        g_actionCtx.stageStartMs = now;
        Serial.println("Action: RH_UP - SETTLE");
      }
      break;
      
    case STAGE_RH_UP_SETTLE:
      if (elapsed >= scaled(RT_RH_UP_SETTLE_MS)) {
        allOutputsOff();
        g_actionCtx.rhDownLockoutUntilMs = now + scaled(RT_RH_LOCKOUT_MS);
        g_actionCtx.currentAction = ACTION_NONE;
        g_actionCtx.currentStage = STAGE_IDLE;
        Serial.println("Action: RH_UP - COMPLETE (RH_DOWN locked for 3 min)");
      }
      break;
      
    // --- Baseline Action ---
    case STAGE_BASELINE_FRESHAIR:
      if (elapsed >= scaled(RT_BASELINE_FRESHAIR_MS)) {
        setFreshAir(false);
        g_actionCtx.currentStage = STAGE_BASELINE_SETTLE;
        g_actionCtx.stageStartMs = now;
        Serial.println("Action: BASELINE - SETTLE");
      }
      break;
      
    case STAGE_BASELINE_SETTLE:
      if (elapsed >= scaled(RT_BASELINE_SETTLE_MS)) {
        allOutputsOff();
        g_actionCtx.currentAction = ACTION_NONE;
        g_actionCtx.currentStage = STAGE_IDLE;
        Serial.println("Action: BASELINE - COMPLETE");
      }
      break;
      
    default:
      break;
  }
}

// Evaluate sensors and decide on action (only if no action running)
static void controllerEvaluate(const Sensors &medianSensors) {
  if (g_actionCtx.currentAction != ACTION_NONE) {
    return; // Don't evaluate while action is running
  }
  
  unsigned long now = millis();
  
  // Priority 1: CO2 > setpoint
  if (medianSensors.co2 > g_co2_setpoint) {
    Serial.print("Controller: CO2 high (");
    Serial.print(medianSensors.co2);
    Serial.print(" ppm, setpoint=");
    Serial.print(g_co2_setpoint);
    Serial.println(") -> CO2 action");
    startAction(ACTION_CO2);
    return;
  }
  
  // Priority 2: RH > setpoint+hysteresis and RH_DOWN unlocked
  float rhHighThreshold = g_rh_setpoint + RH_HYSTERESIS;
  if (medianSensors.rh > rhHighThreshold && now >= g_actionCtx.rhDownLockoutUntilMs) {
    Serial.print("Controller: RH high (");
    Serial.print(medianSensors.rh);
    Serial.print(" %, threshold=");
    Serial.print(rhHighThreshold);
    Serial.println(") -> RH_DOWN action");
    startAction(ACTION_RH_DOWN);
    return;
  }
  
  // Priority 3: RH < setpoint-hysteresis and RH_UP unlocked
  float rhLowThreshold = g_rh_setpoint - RH_HYSTERESIS;
  if (medianSensors.rh < rhLowThreshold && now >= g_actionCtx.rhUpLockoutUntilMs) {
    Serial.print("Controller: RH low (");
    Serial.print(medianSensors.rh);
    Serial.print(" %, threshold=");
    Serial.print(rhLowThreshold);
    Serial.println(") -> RH_UP action");
    startAction(ACTION_RH_UP);
    return;
  }
  
  // Priority 4: Baseline (no ventilation for 10 minutes)
  if (g_actionCtx.lastVentilationMs > 0 && 
      (now - g_actionCtx.lastVentilationMs) >= scaled(RT_BASELINE_INTERVAL_MS)) {
    Serial.println("Controller: Baseline due (no ventilation for 10 min)");
    startAction(ACTION_BASELINE);
    return;
  }
  
  // Otherwise: wait
}

// --- Measurement state machine (MEASURE_SWIRL / MEASURE_MEDIAN / EVALUATE / WAIT) ---

enum MeasureStage {
  MEASURE_IDLE,
  MEASURE_SWIRL,
  MEASURE_MEDIAN,
  MEASURE_EVALUATE,
  MEASURE_WAIT
};

struct MeasureContext {
  MeasureStage stage;
  unsigned long stageStartMs;
  unsigned long nextSampleMs;
  uint8_t sampleIndex;
  float rhSamples[MEDIAN_SAMPLE_COUNT];
  float tempSamples[MEDIAN_SAMPLE_COUNT];
  int co2Samples[MEDIAN_SAMPLE_COUNT];
  
  MeasureContext() : stage(MEASURE_IDLE), stageStartMs(0), nextSampleMs(0), sampleIndex(0) {}
};

static MeasureContext g_measureCtx;

static void measurementTick() {
  unsigned long now = millis();
  
  switch (g_measureCtx.stage) {
    case MEASURE_IDLE:
      // Start first measurement cycle
      g_measureCtx.stage = MEASURE_SWIRL;
      g_measureCtx.stageStartMs = now;
      setSwirler(true);
      Serial.println("Measurement: SWIRL");
      break;
      
    case MEASURE_SWIRL:
      if (now - g_measureCtx.stageStartMs >= scaled(RT_MEASURE_SWIRL_DURATION_MS)) {
        setSwirler(false);
        g_measureCtx.stage = MEASURE_MEDIAN;
        g_measureCtx.stageStartMs = now;
        g_measureCtx.nextSampleMs = now;
        g_measureCtx.sampleIndex = 0;
        Serial.println("Measurement: MEDIAN sampling");
      }
      break;
      
    case MEASURE_MEDIAN:
      if (now >= g_measureCtx.nextSampleMs && g_measureCtx.sampleIndex < MEDIAN_SAMPLE_COUNT) {
        Sensors s = readSensors3();
        g_measureCtx.rhSamples[g_measureCtx.sampleIndex] = s.rh;
        g_measureCtx.tempSamples[g_measureCtx.sampleIndex] = s.temp;
        g_measureCtx.co2Samples[g_measureCtx.sampleIndex] = s.co2;
        g_measureCtx.sampleIndex++;
        g_measureCtx.nextSampleMs += scaled(RT_MEDIAN_SAMPLE_PERIOD_MS);
        
        Serial.print("  Sample ");
        Serial.print(g_measureCtx.sampleIndex);
        Serial.print("/");
        Serial.print(MEDIAN_SAMPLE_COUNT);
        Serial.print(": RH=");
        Serial.print(s.rh);
        Serial.print(" Temp=");
        Serial.print(s.temp);
        Serial.print(" CO2=");
        Serial.println(s.co2);
      }
      
      if (g_measureCtx.sampleIndex >= MEDIAN_SAMPLE_COUNT) {
        g_measureCtx.stage = MEASURE_EVALUATE;
        Serial.println("Measurement: EVALUATE");
      }
      break;
      
    case MEASURE_EVALUATE:
      {
        Sensors medianSensors;
        medianSensors.rh = medianFloat(g_measureCtx.rhSamples, MEDIAN_SAMPLE_COUNT);
        medianSensors.temp = medianFloat(g_measureCtx.tempSamples, MEDIAN_SAMPLE_COUNT);
        medianSensors.co2 = medianInt(g_measureCtx.co2Samples, MEDIAN_SAMPLE_COUNT);
        
        Serial.print("Median: RH=");
        Serial.print(medianSensors.rh);
        Serial.print(" Temp=");
        Serial.print(medianSensors.temp);
        Serial.print(" CO2=");
        Serial.println(medianSensors.co2);
        
        controllerEvaluate(medianSensors);
        
        g_measureCtx.stage = MEASURE_WAIT;
        g_measureCtx.stageStartMs = now;
        Serial.println("Measurement: WAIT");
      }
      break;
      
    case MEASURE_WAIT:
      if (now - g_measureCtx.stageStartMs >= scaled(RT_WAIT_BETWEEN_CYCLES_MS)) {
        g_measureCtx.stage = MEASURE_SWIRL;
        g_measureCtx.stageStartMs = now;
        setSwirler(true);
        Serial.println("Measurement: SWIRL (new cycle)");
      }
      break;
  }
}

// --- Ring buffers for plotting ---

static SensorRingBuffer<float, RING_BUFFER_SIZE> g_rhBuffer;
static SensorRingBuffer<float, RING_BUFFER_SIZE> g_rh_2Buffer;
static SensorRingBuffer<float, RING_BUFFER_SIZE> g_tempBuffer;
static SensorRingBuffer<float, RING_BUFFER_SIZE> g_temp_2Buffer;
static SensorRingBuffer<float, RING_BUFFER_SIZE> g_tempOuterBuffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_co2Buffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_co2_2Buffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_foggerBuffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_swirlerBuffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_freshAirBuffer;
static SensorRingBuffer<int, RING_BUFFER_SIZE> g_heaterBuffer;

// Sample tick: read sensors and add to ring buffers
static unsigned long g_nextSampleMs = 0;

static void sampleTick() {
  unsigned long now = millis();
  if (now >= g_nextSampleMs) {
    Sensors s = readSensors3();
    g_rhBuffer.push(s.rh);
    g_rh_2Buffer.push(s.rh_2);
    g_tempBuffer.push(s.temp);
    g_temp_2Buffer.push(s.temp_2);
    g_tempOuterBuffer.push(s.temp_outer);
    g_co2Buffer.push(s.co2);
    g_co2_2Buffer.push(s.co2_2);
    g_foggerBuffer.push(g_foggerState ? 1 : 0);
    g_swirlerBuffer.push(g_swirlerState ? 1 : 0);
    g_freshAirBuffer.push(g_freshAirState ? 1 : 0);
    g_heaterBuffer.push(g_heaterState ? 1 : 0);
    
    // Drift-free scheduling
    if (g_nextSampleMs == 0) {
      g_nextSampleMs = now + scaled(Config::SAMPLE_INTERVAL_MS);
    } else {
      g_nextSampleMs += scaled(Config::SAMPLE_INTERVAL_MS);
    }
  }
}

// Heater control: independent temperature regulation with 1°C hysteresis
static void heaterTick() {
  static unsigned long lastCheckMs = 0;
  unsigned long now = millis();
  
  // Check every second (scaled)
  if (now - lastCheckMs < scaled(Config::HEATER_CHECK_INTERVAL_MS)) return;
  lastCheckMs = now;
  
  Sensors s = readSensors3();
  
  // Hysteresis: turn on if temp < setpoint - 1, turn off if temp >= setpoint
  if (!g_heaterState && s.temp < (g_temp_setpoint - 1.0f)) {
    setHeater(true);
    Serial.print("Heater: ON (temp=");
    Serial.print(s.temp, 1);
    Serial.print(", setpoint=");
    Serial.print(g_temp_setpoint, 1);
    Serial.println(")!");
  } else if (g_heaterState && s.temp >= g_temp_setpoint) {
    setHeater(false);
    Serial.print("Heater: OFF (temp=");
    Serial.print(s.temp, 1);
    Serial.print(", setpoint=");
    Serial.print(g_temp_setpoint, 1);
    Serial.println(")!");
  }
}

// --- Public API ---

void controller_init() {
  Serial.println("Controller: Initializing...");
  allOutputsOff();
  g_nextSampleMs = 0;
  g_measureCtx.stage = MEASURE_IDLE;
  g_actionCtx.currentAction = ACTION_NONE;
  g_actionCtx.lastVentilationMs = millis(); // Start baseline timer
  
  // Load setpoints from storage
  g_co2_setpoint = storage_get_co2_setpoint();
  g_rh_setpoint = storage_get_rh_setpoint();
  g_temp_setpoint = storage_get_temp_setpoint();
  
  Serial.print("CO2 Setpoint: ");
  Serial.print(g_co2_setpoint);
  Serial.println(" ppm");
  Serial.print("RH Setpoint: ");
  Serial.print(g_rh_setpoint);
  Serial.println(" %");
  Serial.print("Temp Setpoint: ");
  Serial.print(g_temp_setpoint);
  Serial.println(" °C");
  
  Serial.print("SPEEDUP: ");
  Serial.println(SPEEDUP);
  Serial.println("Controller: Ready");
}

void controller_tick() {
  sampleTick();
  measurementTick();
  actionTick();
  heaterTick();
}

void controller_get_last200(float *rh_out, float *temp_out, int *co2_out) {
  g_rhBuffer.getAll(rh_out);
  g_tempBuffer.getAll(temp_out);
  g_co2Buffer.getAll(co2_out);
}

void controller_get_additional_sensors(int *co2_2_out, float *rh_2_out, float *temp_2_out, float *temp_outer_out) {
  g_co2_2Buffer.getAll(co2_2_out);
  g_rh_2Buffer.getAll(rh_2_out);
  g_temp_2Buffer.getAll(temp_2_out);
  g_tempOuterBuffer.getAll(temp_outer_out);
}

void controller_get_outputs(int *fogger_out, int *swirler_out, int *freshair_out) {
  g_foggerBuffer.getAll(fogger_out);
  g_swirlerBuffer.getAll(swirler_out);
  g_freshAirBuffer.getAll(freshair_out);
}

void controller_get_heater(int *heater_out) {
  g_heaterBuffer.getAll(heater_out);
}

// CO2 Setpoint management
void controller_set_co2_setpoint(uint16_t ppm) {
  storage_set_co2_setpoint(ppm);
  g_co2_setpoint = storage_get_co2_setpoint(); // Get clamped value
  Serial.print("Controller: CO2 setpoint changed to ");
  Serial.print(g_co2_setpoint);
  Serial.println(" ppm");
}

uint16_t controller_get_co2_setpoint() {
  return g_co2_setpoint;
}

// RH Setpoint management
void controller_set_rh_setpoint(float percent) {
  storage_set_rh_setpoint(percent);
  g_rh_setpoint = storage_get_rh_setpoint(); // Get clamped value
  Serial.print("Controller: RH setpoint changed to ");
  Serial.print(g_rh_setpoint);
  Serial.println(" %");
}

float controller_get_rh_setpoint() {
  return g_rh_setpoint;
}

// Temperature Setpoint management
void controller_set_temp_setpoint(float celsius) {
  storage_set_temp_setpoint(celsius);
  g_temp_setpoint = storage_get_temp_setpoint(); // Get clamped value
  Serial.print("Controller: Temp setpoint changed to ");
  Serial.print(g_temp_setpoint);
  Serial.println(" °C");
}

float controller_get_temp_setpoint() {
  return g_temp_setpoint;
}
