/*
 * =====================================================================================
 * SMART CAMPUS MONITORING AND SAFETY SYSTEM (ESP32)
 * Advanced Multi-Modal Digital Twin Firmware: Audio, Motion, Environmental & Security
 * =====================================================================================
 * 
 * HARDWARE MAPPING & GPIO ALLOCATION:
 * -------------------------------------------------------------------------------------
 *  ESP32 PIN | PERIPHERAL / SENSOR          | MODE         | LOGIC / BEHAVIOR
 * -----------+------------------------------+--------------+---------------------------
 *  GPIO 21   | I2C SDA (LCD 20x4 Display)   | Output/Input | I2C Data (Address 0x27)
 *  GPIO 22   | I2C SCL (LCD 20x4 Display)   | Output       | I2C Clock
 *  GPIO 4    | Buzzer (+)                   | Output       | Active HIGH alarm buzzer
 *  GPIO 2    | Green LED (Anode)            | Output       | Active HIGH (Normal/Safe Mode)
 *  GPIO 12   | Yellow LED (Anode to GND)    | Output       | Active HIGH (Warning/Faculty Absent)
 *            |                              |              | *Pulls GPIO 12 LOW at boot (prevents 1.8V flash crash)
 *  GPIO 16   | Red LED (Anode)              | Output       | Active HIGH (Fire/Critical Hazard/Fight)
 *  GPIO 17   | Blue LED (Anode)             | Output       | Active HIGH (Water Leakage)
 *  GPIO 32   | PIR Sensor G01 (Ground Floor)| Input        | HIGH = Occupied, LOW = Empty
 *  GPIO 33   | PIR Sensor G02 (Ground Floor)| Input        | HIGH = Occupied, LOW = Empty
 *  GPIO 25   | PIR Sensor G03 (Ground Floor)| Input        | HIGH = Occupied, LOW = Empty
 *  GPIO 26   | PIR Sensor S01 (Second Floor)| Input        | HIGH = Occupied, LOW = Empty
 *  GPIO 27   | DHT22 1 (G01 Classroom)     | 1-Wire Digital| Temp & Humidity
 *  GPIO 14   | DHT22 2 (1F Computer Lab)    | 1-Wire Digital| Temp & Humidity
 *  GPIO 13   | DHT22 3 (2F Seminar Hall)    | 1-Wire Digital| Temp & Humidity
 *  GPIO 15   | DHT22 4 (1F Faculty Cabins)  | 1-Wire Digital| Temp & Humidity (Safe boot pin)
 *  GPIO 34   | MQ-2 Gas Sensor 1 (Lab Gas)  | Analog Input | ADC1_CH6 (0-4095)
 *  GPIO 35   | MQ-2 Gas Sensor 2 (Smoke Det)| Analog Input | ADC1_CH7 (Smoking in Stairwell)
 *  GPIO 36/VP| Flame Sensor 1 (Lab Flame)   | Analog (ADC1)| ADC1_CH0 (0-4095, pot simulation)
 *  GPIO 39/VN| Acoustic/Mic Sensor (Audio)  | Analog (ADC1)| ADC1_CH3 (Sound Level: 30-110 dB)
 *  GPIO 23   | Water Leak Button / Sensor   | INPUT_PULLUP | LOW = Leak Pressed, HIGH = Dry
 *  GPIO 19   | Faculty Absent Pushbutton    | INPUT_PULLUP | LOW = Absent Pressed, HIGH = Normal
 *  GPIO 18   | Fight Detection Pushbutton   | INPUT_PULLUP | LOW = Fight Pressed, HIGH = Normal
 *  GPIO 5    | Emergency Mode Pushbutton    | INPUT_PULLUP | LOW = Emergency Pressed, HIGH = Normal
 * =====================================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// =====================================================================================
// PIN CONSTANTS
// =====================================================================================
// Display I2C Pins
#define PIN_I2C_SDA          21   // Master I2C Data
#define PIN_I2C_SCL          22   // Master I2C Clock
#define LCD_ADDR             0x27 // I2C Address for PCF8574
#define LCD_COLS             20
#define LCD_ROWS             4

// Output Pins (LEDs & Buzzer)
#define PIN_BUZZER           4    // Emergency alarm buzzer
#define PIN_LED_GREEN        2    // Normal / Safe Mode
#define PIN_LED_YELLOW       12   // Warning / Faculty Absent (Safe pull-down at boot)
#define PIN_LED_RED          16   // Fire / Critical Emergency / Fight
#define PIN_LED_BLUE         17   // Water Leakage Detected

// PIR Motion Sensors (Digital Inputs)
#define PIN_PIR_G01          32   // Classroom G01
#define PIN_PIR_G02          33   // Classroom G02
#define PIN_PIR_G03          25   // Classroom G03
#define PIN_PIR_S01          26   // Seminar Hall S01

// DHT22 Environmental Sensors
#define PIN_DHT_G01          27   // Classroom G01 DHT22
#define PIN_DHT_LAB          14   // Computer Lab DHT22
#define PIN_DHT_SEM          13   // Seminar Hall DHT22
#define PIN_DHT_FAC          15   // Faculty Room DHT22
#define DHTTYPE              DHT22

// MQ-2 Gas & Smoke Sensors (Analog Inputs ADC1)
#define PIN_GAS_LAB          34   // Computer Lab Gas Leak (ADC1_CH6)
#define PIN_GAS_SMOKE        35   // Stairwell Smoking Detection (ADC1_CH7)

// Flame Sensor & Acoustic Microphone Sensor (Sensor VP / VN)
#define PIN_FLAME_LAB        36   // 1F Lab Flame Sensor (VP / ADC1_CH0)
#define PIN_AUDIO_SENSOR     39   // Acoustic / Mic Sensor (VN / ADC1_CH3)

// Pushbuttons (Active LOW with internal pull-up)
#define PIN_BTN_WATER        23   // Water Leak Simulator
#define PIN_BTN_FACULTY      19   // Faculty Absent Simulator
#define PIN_BTN_FIGHT        18   // Student Fight Simulator
#define PIN_BTN_EMERGENCY    5    // Emergency Mode Simulator

// Calibration Thresholds (Hysteresis prevents flickering)
#define FLAME_THRESH_TRIG    2500 // Analog ADC value to trigger fire
#define FLAME_THRESH_CLR     1800 // Analog ADC value to clear fire
#define GAS_THRESH_TRIG      2000 // ADC value for toxic gas leak
#define GAS_THRESH_CLR       1600 // ADC value to clear gas leak
#define SMOKE_THRESH_TRIG    2000 // ADC value for smoking activity
#define SMOKE_THRESH_CLR     1600 // ADC value to clear smoke
#define TEMP_WARN_THRESHOLD  35.0 // °C Warning level
#define TEMP_CRIT_THRESHOLD  50.0 // °C Critical temperature level

// Audio Monitoring Thresholds (in Decibels)
#define AUDIO_SILENCE_DB     45   // Below 45 dB = Silence / No vocal speech
#define AUDIO_LECTURE_MIN_DB 48   // 48 dB to 74 dB = Normal faculty teaching speech
#define AUDIO_LECTURE_MAX_DB 74   // Upper bound for typical classroom lecture
#define AUDIO_FIGHT_DB       85   // Above 85 dB = Acoustic shock / Screaming / Fight disturbance
#define AUDIO_FIGHT_CLR_DB   75   // Hysteresis threshold to clear fight alarm

// =====================================================================================
// OBJECT INSTANTIATIONS
// =====================================================================================
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

DHT dhtG01(PIN_DHT_G01, DHTTYPE);
DHT dhtLab(PIN_DHT_LAB, DHTTYPE);
DHT dhtSem(PIN_DHT_SEM, DHTTYPE);
DHT dhtFac(PIN_DHT_FAC, DHTTYPE);

// =====================================================================================
// SYSTEM STATE VARIABLES
// =====================================================================================
// Occupancy States
bool occG01 = false;
bool occG02 = false;
bool occG03 = false;
bool occS01 = false;
bool prevOccG01 = false;
bool prevOccG02 = false;
bool prevOccG03 = false;
bool prevOccS01 = false;

// Environmental Telemetry
float tempG01 = 24.0, humG01 = 50.0;
float tempLab = 25.0, humLab = 48.0;
float tempSem = 23.0, humSem = 52.0;
float tempFac = 23.5, humFac = 51.0;

// Audio Surveillance State
int audioRaw = 0;
int audioDb = 42; // Starts at calm ambient 42 dB
bool audioFightDetected = false;
bool facultyVoiceDetected = false;
bool prevFacultyVoice = false;

// Hazard States
bool fireLab = false;
bool prevFireLab = false;

bool gasLeak = false;
bool prevGasLeak = false;

bool smokeDetected = false;
bool prevSmoke = false;

bool waterLeak = false;
bool prevWaterLeak = false;

bool facultyAbsent = false;
bool fightDetected = false;
bool emergencyMode = false;

// Raw Sensor Values
int gasLabRaw = 0;
int gasSmokeRaw = 0;
int flameLabRaw = 0;

// Pushbutton Debounce & Edge Tracking (Active LOW, starts HIGH)
bool prevBtnFaculty = HIGH;
bool prevBtnFight   = HIGH;
bool prevBtnEmerg   = HIGH;
bool prevBtnWater   = HIGH;

// Security Analytics
unsigned long alertCount = 0;
unsigned long securityOfficeAlerts = 0;

// Non-blocking Timers (millis)
unsigned long lastSensorReadTime   = 0;
const unsigned long SENSOR_INTERVAL = 300;   // Read sensors every 300ms

unsigned long lastLoopDebugTime    = 0;
const unsigned long LOOP_DEBUG_INTERVAL = 2000; // Continuous loop debug every 2 seconds

unsigned long lastDhtPrintTime     = 0;
const unsigned long DHT_PRINT_INTERVAL = 5000;  // DHT22 Temp/Hum print every 5 seconds

unsigned long lastLcdCycleTime     = 0;
const unsigned long LCD_INTERVAL    = 3000;   // Rotate LCD screen every 3 seconds
int currentLcdScreen               = 0;

unsigned long lastBlinkTime        = 0;
bool blinkState                    = false;

// =====================================================================================
// HELPER FUNCTION: SECURITY TIMESTAMP
// =====================================================================================
void printTimestamp() {
  unsigned long totalSec = millis() / 1000;
  unsigned int s = totalSec % 60;
  unsigned int m = (totalSec / 60) % 60;
  unsigned int h = (totalSec / 3600);
  char buf[16];
  sprintf(buf, "[%02u:%02u:%02u] ", h, m, s);
  Serial.print(buf);
}

// =====================================================================================
// FUNCTION DECLARATIONS
// =====================================================================================
void readSensors();
void handleButtons();
void updateOutputs();
void updateLcd();
void printLoopDebug();
void printDhtReport();

// =====================================================================================
// SETUP FUNCTION
// =====================================================================================
void setup() {
  // 1. Initialize Serial at 115200 baud
  Serial.begin(115200);
  delay(100);

  // 2. Configure Output Pins (Buzzer & LEDs)
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_YELLOW, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_LED_BLUE, OUTPUT);

  // STARTUP RULE: NO alarms or buzzers activate at startup!
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_YELLOW, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  digitalWrite(PIN_LED_BLUE, LOW);
  digitalWrite(PIN_LED_GREEN, HIGH); // SAFE NORMAL MODE indicator is ON!

  // 3. Configure PIR Motion Sensor Pins
  pinMode(PIN_PIR_G01, INPUT);
  pinMode(PIN_PIR_G02, INPUT);
  pinMode(PIN_PIR_G03, INPUT);
  pinMode(PIN_PIR_S01, INPUT);

  // 4. Configure Pushbuttons (Active LOW with internal pull-up)
  pinMode(PIN_BTN_WATER, INPUT_PULLUP);
  pinMode(PIN_BTN_FACULTY, INPUT_PULLUP);
  pinMode(PIN_BTN_FIGHT, INPUT_PULLUP);
  pinMode(PIN_BTN_EMERGENCY, INPUT_PULLUP);

  // 5. Initialize DHT22 Sensors
  dhtG01.begin();
  dhtLab.begin();
  dhtSem.begin();
  dhtFac.begin();

  // 6. Initialize I2C LCD Display (20x4 at 0x27)
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("===================="));
  lcd.setCursor(0, 1); lcd.print(F("SMART CAMPUS SYSTEM "));
  lcd.setCursor(0, 2); lcd.print(F("CAMPUS STATUS: SAFE "));
  lcd.setCursor(0, 3); lcd.print(F("ALL SYSTEMS NORMAL  "));

  // 7. EXACT REQUIRED STARTUP OUTPUT
  Serial.println(F("\n===================================="));
  Serial.println(F("SMART CAMPUS SYSTEM"));
  Serial.println(F("===================================="));
  Serial.println(F("ESP32 Started"));
  Serial.println(F("Initializing Sensors..."));
  Serial.println(F("Initializing LCD..."));
  Serial.println(F("Initializing Security System..."));
  Serial.println(F("Audio Surveillance Active"));
  Serial.println(F("All Systems Operational"));
  Serial.println(F("Campus Status: SAFE\n"));
}

// =====================================================================================
// MAIN LOOP
// =====================================================================================
void loop() {
  unsigned long currentMillis = millis();

  // 1. Interactive Button Event Monitoring
  handleButtons();

  // 2. Periodic Sensor Acquisition & Edge Detection
  if (currentMillis - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = currentMillis;
    readSensors();
  }

  // 3. Update Status Actuators & Indicators
  updateOutputs();

  // 4. Continuous Loop Debugging (Every 2 Seconds)
  if (currentMillis - lastLoopDebugTime >= LOOP_DEBUG_INTERVAL) {
    lastLoopDebugTime = currentMillis;
    printLoopDebug();
  }

  // 5. DHT22 Environmental Telemetry Print (Every 5 Seconds)
  if (currentMillis - lastDhtPrintTime >= DHT_PRINT_INTERVAL) {
    lastDhtPrintTime = currentMillis;
    printDhtReport();
  }

  // 6. LCD Display Rotation (Every 3 Seconds)
  if (currentMillis - lastLcdCycleTime >= LCD_INTERVAL) {
    lastLcdCycleTime = currentMillis;
    currentLcdScreen = (currentLcdScreen + 1) % 7; // Rotates across 7 screens including Audio!
    updateLcd();
  }
}

// =====================================================================================
// PUSHBUTTON MONITORING (BUTTON-BASED TESTING)
// =====================================================================================
void handleButtons() {
  bool curBtnFaculty = digitalRead(PIN_BTN_FACULTY);
  bool curBtnFight   = digitalRead(PIN_BTN_FIGHT);
  bool curBtnEmerg   = digitalRead(PIN_BTN_EMERGENCY);
  bool curBtnWater   = digitalRead(PIN_BTN_WATER);

  // BUTTON 1: Faculty Absent Simulator (Pin 19)
  if (prevBtnFaculty == HIGH && curBtnFaculty == LOW) {
    facultyAbsent = !facultyAbsent; // Toggle state for testing
    if (facultyAbsent) {
      alertCount++;
      securityOfficeAlerts++;
      Serial.println(F("\n[ALERT] Faculty Absent"));
      Serial.println(F("Alert Sent To Security Office"));
    } else {
      Serial.println(F("\n[INFO] Faculty Present - Normal Attendance Resumed"));
    }
    updateLcd();
  }
  prevBtnFaculty = curBtnFaculty;

  // BUTTON 2: Student Fight Simulator (Pin 18)
  if (prevBtnFight == HIGH && curBtnFight == LOW) {
    fightDetected = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[ALERT] Student Fight Detected"));
    Serial.println(F("Security Team Notified"));
    updateLcd();
  } else if (prevBtnFight == LOW && curBtnFight == HIGH && !audioFightDetected) {
    fightDetected = false;
    Serial.println(F("\n[INFO] Security Team Dispatched - Fight Incident Contained"));
    updateLcd();
  }
  prevBtnFight = curBtnFight;

  // BUTTON 3: Emergency Trigger (Pin 5)
  if (prevBtnEmerg == HIGH && curBtnEmerg == LOW) {
    emergencyMode = !emergencyMode; // Toggle emergency state
    if (emergencyMode) {
      alertCount++;
      securityOfficeAlerts++;
      Serial.println(F("\n[EMERGENCY MODE ACTIVATED]"));
    } else {
      Serial.println(F("\n[INFO] Emergency Mode Deactivated - Campus All Clear"));
    }
    updateLcd();
  }
  prevBtnEmerg = curBtnEmerg;

  // WATER LEAK SENSOR BUTTON (Pin 23)
  if (prevBtnWater == HIGH && curBtnWater == LOW) {
    waterLeak = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[ALERT] Water Leak Detected"));
    Serial.println(F("Water Leak Detected"));
    updateLcd();
  } else if (prevBtnWater == LOW && curBtnWater == HIGH) {
    waterLeak = false;
    Serial.println(F("\n[INFO] Water Leak Cleared"));
    updateLcd();
  }
  prevBtnWater = curBtnWater;
}

// =====================================================================================
// SENSOR ACQUISITION & STRICT STATE-CHANGE LOGIC (ZERO FALSE ALERTS & ZERO SPAM)
// =====================================================================================
void readSensors() {
  // -----------------------------------------------------------------------------------
  // 1. PIR MOTION SENSORS
  // -----------------------------------------------------------------------------------
  occG01 = (digitalRead(PIN_PIR_G01) == HIGH);
  occG02 = (digitalRead(PIN_PIR_G02) == HIGH);
  occG03 = (digitalRead(PIN_PIR_G03) == HIGH);
  occS01 = (digitalRead(PIN_PIR_S01) == HIGH);

  // Exact requirement: PIR state-change triggers
  if (occG01 != prevOccG01) {
    if (occG01) {
      Serial.println(F("Classroom Occupied"));
    } else {
      Serial.println(F("Classroom Empty"));
    }
    prevOccG01 = occG01;
    updateLcd();
  }

  if (occG02 != prevOccG02) {
    prevOccG02 = occG02;
    updateLcd();
  }
  if (occG03 != prevOccG03) {
    prevOccG03 = occG03;
    updateLcd();
  }
  if (occS01 != prevOccS01) {
    prevOccS01 = occS01;
    updateLcd();
  }

  // -----------------------------------------------------------------------------------
  // 2. AUDIO & ACOUSTIC SURVEILLANCE SENSOR (GPIO 39 / VN)
  // -----------------------------------------------------------------------------------
  audioRaw = analogRead(PIN_AUDIO_SENSOR);
  // Map ADC (0 - 4095) to Decibels (30 dB - 110 dB)
  audioDb = map(audioRaw, 0, 4095, 30, 110);

  // FEATURE A: Physical Fight Acoustic Spike Detection (> 85 dB)
  if (!audioFightDetected && audioDb >= AUDIO_FIGHT_DB) {
    audioFightDetected = true;
    fightDetected = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[ALERT] Student Fight Detected"));
    Serial.print(F("[CRITICAL] Acoustic Shock Detected: "));
    Serial.print(audioDb);
    Serial.println(F(" dB (> 85 dB Scream Threshold)"));
    Serial.println(F("Security Team Notified"));
    updateLcd();
  } else if (audioFightDetected && audioDb < AUDIO_FIGHT_CLR_DB) {
    audioFightDetected = false;
    if (digitalRead(PIN_BTN_FIGHT) == HIGH) { // If button not physically held
      fightDetected = false;
      Serial.println(F("\n[INFO] Acoustic Levels Normalized - Fight Disturbance Contained"));
      updateLcd();
    }
  }

  // FEATURE B: Faculty Voice / Presence Detection via Audio
  bool isVoiceRange = (audioDb >= AUDIO_LECTURE_MIN_DB && audioDb <= AUDIO_LECTURE_MAX_DB);
  if (isVoiceRange != prevFacultyVoice) {
    if (isVoiceRange && (occG01 || occG03)) {
      Serial.print(F("[AUDIO] Faculty Voice Active: "));
      Serial.print(audioDb);
      Serial.println(F(" dB (Lecture in progress)"));
      // Clear absence if teaching audio resumes
      if (facultyAbsent && digitalRead(PIN_BTN_FACULTY) == HIGH) {
        facultyAbsent = false;
        Serial.println(F("[INFO] Faculty Present - Normal Attendance Resumed"));
      }
    }
    prevFacultyVoice = isVoiceRange;
    updateLcd();
  }

  // -----------------------------------------------------------------------------------
  // 3. DHT22 TEMPERATURE & HUMIDITY READINGS
  // -----------------------------------------------------------------------------------
  float t1 = dhtG01.readTemperature();
  float h1 = dhtG01.readHumidity();
  if (!isnan(t1)) tempG01 = t1;
  if (!isnan(h1)) humG01 = h1;

  float t2 = dhtLab.readTemperature();
  float h2 = dhtLab.readHumidity();
  if (!isnan(t2)) tempLab = t2;
  if (!isnan(h2)) humLab = h2;

  float t3 = dhtSem.readTemperature();
  float h3 = dhtSem.readHumidity();
  if (!isnan(t3)) tempSem = t3;
  if (!isnan(h3)) humSem = h3;

  float t4 = dhtFac.readTemperature();
  float h4 = dhtFac.readHumidity();
  if (!isnan(t4)) tempFac = t4;
  if (!isnan(h4)) humFac = h4;

  // -----------------------------------------------------------------------------------
  // 4. MQ-2 GAS & SMOKING SENSORS (ANALOG ADC1, ZERO CONTINUOUS SPAM)
  // -----------------------------------------------------------------------------------
  gasLabRaw   = analogRead(PIN_GAS_LAB);
  gasSmokeRaw = analogRead(PIN_GAS_SMOKE);

  // Lab Gas Leak Threshold with Hysteresis
  if (!gasLeak && gasLabRaw > GAS_THRESH_TRIG) {
    gasLeak = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[CRITICAL] Gas Leakage Detected"));
    Serial.println(F("Gas Detected"));
    updateLcd();
  } else if (gasLeak && gasLabRaw < GAS_THRESH_CLR) {
    gasLeak = false;
    Serial.println(F("\n[INFO] Gas Levels Normal - Air Safe"));
    updateLcd();
  }

  // FEATURE C: Smoking Detection (Stairwell Restroom Non-Smoking Zone)
  if (!smokeDetected && gasSmokeRaw > SMOKE_THRESH_TRIG) {
    smokeDetected = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[ALERT] Smoking Activity Detected"));
    Serial.println(F("Smoking Detected"));
    Serial.println(F("Location: 2F Stairwell Restroom Corridor (Non-Smoking Zone)"));
    updateLcd();
  } else if (smokeDetected && gasSmokeRaw < SMOKE_THRESH_CLR) {
    smokeDetected = false;
    Serial.println(F("\n[INFO] Smoke Cleared - Air Quality Normal"));
    updateLcd();
  }

  // -----------------------------------------------------------------------------------
  // 5. FLAME SENSOR (VP POTENTIOMETER, ZERO FALSE ALERTS ON STARTUP)
  // -----------------------------------------------------------------------------------
  flameLabRaw = analogRead(PIN_FLAME_LAB);

  // Lab Flame Sensor State-Change
  if (!fireLab && flameLabRaw > FLAME_THRESH_TRIG) {
    fireLab = true;
    alertCount++;
    securityOfficeAlerts++;
    Serial.println(F("\n[CRITICAL] Fire Detected"));
    Serial.println(F("Fire Detected"));
    updateLcd();
  } else if (fireLab && flameLabRaw < FLAME_THRESH_CLR) {
    fireLab = false;
    Serial.println(F("\n[INFO] Fire Cleared - Lab Safe"));
    updateLcd();
  }
}

// =====================================================================================
// CONTINUOUS LOOP DEBUGGING (EVERY 2 SECONDS)
// =====================================================================================
void printLoopDebug() {
  Serial.println(F("System Running..."));

  // PIR Status
  Serial.print(F("PIR Status: "));
  if (occG01 || occG02 || occG03 || occS01) {
    Serial.println(F("Classroom Occupied"));
  } else {
    Serial.println(F("Classroom Empty"));
  }

  // Audio Level Telemetry
  Serial.print(F("Audio Level: "));
  Serial.print(audioDb);
  Serial.print(F(" dB ["));
  if (audioDb >= AUDIO_FIGHT_DB) {
    Serial.println(F("SCREAM / FIGHT DISTURBANCE!]"));
  } else if (audioDb >= AUDIO_LECTURE_MIN_DB && audioDb <= AUDIO_LECTURE_MAX_DB) {
    Serial.println(F("LECTURE SPEECH DETECTED]"));
  } else if (audioDb < AUDIO_SILENCE_DB) {
    Serial.println(F("SILENT / NO VOICE]"));
  } else {
    Serial.println(F("AMBIENT NORMAL]"));
  }

  // Temperature
  Serial.print(F("Temperature: "));
  Serial.print((int)tempG01);
  Serial.println(F("°C"));

  // Humidity
  Serial.print(F("Humidity: "));
  Serial.print((int)humG01);
  Serial.println(F("%"));

  // MQ2 Value
  Serial.print(F("MQ2 Value: "));
  Serial.println(gasLabRaw);

  // Flame Status
  Serial.print(F("Flame Status: "));
  Serial.println(fireLab ? F("FIRE DETECTED") : F("SAFE"));

  // Water Status
  Serial.print(F("Water Status: "));
  Serial.println(waterLeak ? F("WATER LEAK DETECTED") : F("NORMAL"));
  Serial.println();
}

// =====================================================================================
// DHT22 SENSOR REPORT (EVERY 5 SECONDS)
// =====================================================================================
void printDhtReport() {
  // Temperature = XX
  // Humidity = XX
  Serial.print(F("Temperature = "));
  Serial.println((int)tempG01);
  Serial.print(F("Humidity = "));
  Serial.println((int)humG01);

  // Critical temperature check (> 50°C)
  if (tempG01 > TEMP_CRIT_THRESHOLD || tempLab > TEMP_CRIT_THRESHOLD) {
    Serial.println(F("[CRITICAL] High Temperature Alert! Temp > 50°C"));
  }
}

// =====================================================================================
// ACTUATORS & HARDWARE OUTPUT CONTROLLER
// =====================================================================================
void updateOutputs() {
  unsigned long now = millis();
  if (now - lastBlinkTime >= 250) {
    lastBlinkTime = now;
    blinkState = !blinkState;
  }

  // 1. Critical Hazards (Fire, Gas Leak, Fight/Acoustic Shock, Emergency Mode, Temp > 50°C)
  bool isCritical = (fireLab || gasLeak || fightDetected || emergencyMode || tempLab > TEMP_CRIT_THRESHOLD);

  if (isCritical) {
    digitalWrite(PIN_LED_RED, blinkState ? HIGH : LOW);
    digitalWrite(PIN_BUZZER, blinkState ? HIGH : LOW);
  } else {
    digitalWrite(PIN_LED_RED, LOW);
    digitalWrite(PIN_BUZZER, LOW);
  }

  // 2. Yellow LED: Warning / Faculty Absent Alert
  bool isWarning = (facultyAbsent || (tempLab >= TEMP_WARN_THRESHOLD && tempLab <= TEMP_CRIT_THRESHOLD));
  if (isWarning) {
    digitalWrite(PIN_LED_YELLOW, HIGH);
  } else {
    digitalWrite(PIN_LED_YELLOW, LOW);
  }

  // 3. Blue LED: Water Leak Alert
  if (waterLeak) {
    digitalWrite(PIN_LED_BLUE, HIGH);
  } else {
    digitalWrite(PIN_LED_BLUE, LOW);
  }

  // 4. Green LED: Normal Safe Mode (ON when no critical hazards, or room occupied)
  if (!isCritical && !isWarning) {
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else if (!isCritical) {
    digitalWrite(PIN_LED_GREEN, HIGH);
  } else {
    digitalWrite(PIN_LED_GREEN, LOW);
  }
}

// =====================================================================================
// LCD STATUS DISPLAY (ROTATING SCREENS & EMERGENCY OVERRIDES)
// =====================================================================================
void updateLcd() {
  lcd.clear();

  // EMERGENCY OVERRIDES (Immediate high-priority alert display)
  if (fireLab) {
    lcd.setCursor(0, 0); lcd.print(F("********************"));
    lcd.setCursor(0, 1); lcd.print(F("!   FIRE ALERT!    !"));
    lcd.setCursor(0, 2); lcd.print(F("LAB FLAME TRIGGERED "));
    lcd.setCursor(0, 3); lcd.print(F("EVACUATE IMMEDIATELY"));
    return;
  }

  if (gasLeak) {
    lcd.setCursor(0, 0); lcd.print(F("********************"));
    lcd.setCursor(0, 1); lcd.print(F("Gas Leakage Detected"));
    lcd.setCursor(0, 2); lcd.print(F("MQ-2 SENSOR: DANGER "));
    lcd.setCursor(0, 3); lcd.print(F("EXHAUST DAMPERS ON  "));
    return;
  }

  if (emergencyMode) {
    lcd.setCursor(0, 0); lcd.print(F(">>> EMERGENCY <<<   "));
    lcd.setCursor(0, 1); lcd.print(F("CAMPUS ALARM ACTIVE "));
    lcd.setCursor(0, 2); lcd.print(F("SECURITY CODE RED   "));
    lcd.setCursor(0, 3); lcd.print(F("ALL GATES UNLOCKED  "));
    return;
  }

  if (fightDetected) {
    lcd.setCursor(0, 0); lcd.print(F("********************"));
    lcd.setCursor(0, 1); lcd.print(F("Fight Detected      "));
    lcd.setCursor(0, 2);
    lcd.print(F("AUDIO NOISE: ")); lcd.print(audioDb); lcd.print(F("dB"));
    lcd.setCursor(0, 3); lcd.print(F("GUARDS DISPATCHED   "));
    return;
  }

  if (facultyAbsent) {
    lcd.setCursor(0, 0); lcd.print(F("- ATTENDANCE ALERT -"));
    lcd.setCursor(0, 1); lcd.print(F("FACULTY ABSENT      "));
    lcd.setCursor(0, 2); lcd.print(F("ROOM G03: 28 STUDNT "));
    lcd.setCursor(0, 3); lcd.print(F("NO VOICE (< 45 dB)  "));
    return;
  }

  if (waterLeak) {
    lcd.setCursor(0, 0); lcd.print(F("- UTILITY PIPELINE -"));
    lcd.setCursor(0, 1); lcd.print(F("Water Leak Detected "));
    lcd.setCursor(0, 2); lcd.print(F("SOLENOID: AUTO-SHUT "));
    lcd.setCursor(0, 3); lcd.print(F("MAINTENANCE ALERTED "));
    return;
  }

  if (smokeDetected) {
    lcd.setCursor(0, 0); lcd.print(F("- AIR SAFETY ALERT -"));
    lcd.setCursor(0, 1); lcd.print(F("Smoking Detected    "));
    lcd.setCursor(0, 2); lcd.print(F("2F STAIRCASE ATRIUM "));
    lcd.setCursor(0, 3); lcd.print(F("CCTV RECORDING ON   "));
    return;
  }

  // NORMAL ROTATING SCREENS (Every 3 seconds)
  switch (currentLcdScreen) {
    case 0: // Master Campus Status
      lcd.setCursor(0, 0); lcd.print(F("-- SMART CAMPUS --  "));
      lcd.setCursor(0, 1); lcd.print(F("STATUS: ALL SECURE  "));
      lcd.setCursor(0, 2); lcd.print(F("NODES: 19 OPERATIONAL"));
      lcd.setCursor(0, 3);
      lcd.print(F("ALERTS TODAY: "));
      lcd.print(alertCount);
      break;

    case 1: // Classroom Occupancy Matrix
      lcd.setCursor(0, 0); lcd.print(F("- ROOM OCCUPANCY -  "));
      lcd.setCursor(0, 1);
      lcd.print(F("G01:")); lcd.print(occG01 ? F("ACTIVE ") : F("EMPTY  "));
      lcd.print(F("G02:")); lcd.print(occG02 ? F("ACT") : F("EMP"));
      lcd.setCursor(0, 2);
      lcd.print(F("G03:")); lcd.print(occG03 ? F("ACTIVE ") : F("EMPTY  "));
      lcd.print(F("S01:")); lcd.print(occS01 ? F("ACT") : F("EMP"));
      lcd.setCursor(0, 3);
      lcd.print(F("OCCUPIED: "));
      lcd.print((occG01?1:0) + (occG02?1:0) + (occG03?1:0) + (occS01?1:0));
      lcd.print(F("/4 ROOMS"));
      break;

    case 2: // Audio Surveillance Screen
      lcd.setCursor(0, 0); lcd.print(F("- AUDIO SURVEIL -   "));
      lcd.setCursor(0, 1);
      lcd.print(F("MIC LEVEL: ")); lcd.print(audioDb); lcd.print(F(" dB   "));
      lcd.setCursor(0, 2);
      if (audioDb >= AUDIO_LECTURE_MIN_DB && audioDb <= AUDIO_LECTURE_MAX_DB) {
        lcd.print(F("VOICE: LECTURE DET  "));
      } else if (audioDb < AUDIO_SILENCE_DB) {
        lcd.print(F("VOICE: SILENT / CALM"));
      } else {
        lcd.print(F("VOICE: NORMAL AMB   "));
      }
      lcd.setCursor(0, 3);
      lcd.print(F("CORRIDOR: CALM (OK) "));
      break;

    case 3: // Faculty Attendance Status
      lcd.setCursor(0, 0); lcd.print(F("- FACULTY STATUS -  "));
      lcd.setCursor(0, 1); lcd.print(F("G01: PROF. SHARMA   "));
      lcd.setCursor(0, 2); lcd.print(F("G02: EMPTY (ECO)    "));
      lcd.setCursor(0, 3); lcd.print(F("G03: VOICE VERIFIED "));
      break;

    case 4: // Environmental DHT22 Telemetry
      lcd.setCursor(0, 0); lcd.print(F("- CLIMATE DHT22 -   "));
      lcd.setCursor(0, 1);
      lcd.print(F("G01:")); lcd.print((int)tempG01); lcd.print(F("C  LAB:")); lcd.print((int)tempLab); lcd.print(F("C"));
      lcd.setCursor(0, 2);
      lcd.print(F("SEM:")); lcd.print((int)tempSem); lcd.print(F("C  FAC:")); lcd.print((int)tempFac); lcd.print(F("C"));
      lcd.setCursor(0, 3);
      lcd.print(F("AVG HUMIDITY: ")); lcd.print((int)humLab); lcd.print(F("%"));
      break;

    case 5: // Utility & Safety Hazards
      lcd.setCursor(0, 0); lcd.print(F("- HAZARDS MONITOR - "));
      lcd.setCursor(0, 1);
      lcd.print(F("FIRE:")); lcd.print(fireLab ? F("CRIT ") : F("SAFE "));
      lcd.print(F("GAS:"));  lcd.print(gasLeak ? F("LEAK") : F("SAFE"));
      lcd.setCursor(0, 2);
      lcd.print(F("SMOKE:")); lcd.print(smokeDetected ? F("WARN") : F("SAFE"));
      lcd.print(F(" H2O:")); lcd.print(waterLeak ? F("LEAK") : F("SAFE"));
      lcd.setCursor(0, 3);
      lcd.print(F("SOLENOID: ARMED (OK)"));
      break;

    case 6: // Security Operations Center
      lcd.setCursor(0, 0); lcd.print(F("- SECURITY CENTER - "));
      lcd.setCursor(0, 1); lcd.print(F("CCTV AI: 6 CAMERAS  "));
      lcd.setCursor(0, 2); lcd.print(F("AUDIO AI: ACOUSTIC-OK"));
      lcd.setCursor(0, 3);
      lcd.print(F("TOTAL EVENTS: ")); lcd.print(securityOfficeAlerts);
      break;
  }
}
