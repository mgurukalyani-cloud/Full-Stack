# 🏛️ Smart Campus Monitoring & Safety System — Complete Wokwi Simulation (ESP32)

---

## 🚀 Overview

This Wokwi simulation models a complete, enterprise-grade **Smart Campus Monitoring and Safety Digital Twin** driven by an **ESP32 DevKit V4**. The firmware features strict edge-triggered state detection, zero false alarms on startup, no continuous message spamming, and full multi-modal surveillance across classrooms, computer labs, seminar halls, corridors, and security offices.

### 🎙️ NEW: Multi-Modal Audio & Acoustic Surveillance
- **Acoustic / Sound Sensor (GPIO 39 / VN)**: Real-time sound level monitoring from **30 dB** (silent/library) up to **110 dB** (acoustic shock / fight).
- **Faculty Presence Detection (Audio-Assisted)**: Identifies vocal lecture frequencies (48–74 dB). If a classroom is occupied by students (PIR) but voice audio remains silent (< 45 dB), a **Faculty Absent Alert** is raised.
- **Physical Fight Detection (Audio-Assisted)**: Detects sharp acoustic spikes (`> 85 dB`, screams/scuffle commotion), triggering instant security dispatch and strobe sirens.
- **Smoking Detection**: Monitored by the MQ-2 aerosol sensor in the 2F Stairwell / Restroom non-smoking zone, correlated with acoustic quietness.

---

## 📌 Complete Hardware Pin Mapping Table

| ESP32 Pin | Peripheral / Component | Mode | Signal Type | Electrical Notes & Function |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO 21** | `wokwi-lcd2004` SDA | I2C Data | 3.3V Bidirectional | Security Office LCD Data line (Address `0x27`) |
| **GPIO 22** | `wokwi-lcd2004` SCL | I2C Clock | 3.3V Output | Security Office LCD Clock line |
| **GPIO 4** | `wokwi-buzzer` (+) | Output | Active HIGH | Central Campus Alarm Siren |
| **GPIO 2** | `wokwi-led` (Green) | Output | Active HIGH | Normal Safe Status / Campus Equilibrium |
| **GPIO 12** | `wokwi-led` (Yellow) | Output | Active HIGH | Warning / Faculty Absent Alert (*Pulls MTDI LOW at boot*) |
| **GPIO 16** | `wokwi-led` (Red) | Output | Active HIGH | Critical Emergency / Fire / Fight Alert Strobe |
| **GPIO 17** | `wokwi-led` (Blue) | Output | Active HIGH | Utility Pipeline Water Leakage Indicator |
| **GPIO 32** | `pir_g01` OUT | Input | 3.3V Logic | Classroom G01 Occupancy PIR Sensor |
| **GPIO 33** | `pir_g02` OUT | Input | 3.3V Logic | Classroom G02 Occupancy PIR Sensor |
| **GPIO 25** | `pir_g03` OUT | Input | 3.3V Logic | Classroom G03 Occupancy PIR Sensor |
| **GPIO 26** | `pir_s01` OUT | Input | 3.3V Logic | Seminar Hall S01 Occupancy PIR Sensor |
| **GPIO 27** | `dht_g01` SDA | 1-Wire Digital | 3.3V Logic | Classroom G01 Temperature & Humidity Sensor |
| **GPIO 14** | `dht_lab` SDA | 1-Wire Digital | 3.3V Logic | 1F Computer Lab Climate Sensor |
| **GPIO 13** | `dht_sem` SDA | 1-Wire Digital | 3.3V Logic | 2F Seminar Hall Climate Sensor |
| **GPIO 15** | `dht_fac` SDA | 1-Wire Digital | 3.3V Logic | Faculty Rooms Climate Sensor (*Safe boot pin*) |
| **GPIO 34** | `gas_lab` AO | Analog Input | 0 - 3.3V (ADC1) | 1F Computer Lab Combustible Gas (MQ-2) |
| **GPIO 35** | `gas_smoke` AO | Analog Input | 0 - 3.3V (ADC1) | 2F Stairwell Smoking Detection (MQ-2) |
| **GPIO 36 (VP)**| `flame_lab` SIG | Analog Input | 0 - 3.3V (ADC1) | 1F Computer Lab Flame Sensor (Potentiometer) |
| **GPIO 39 (VN)**| `mic_sensor` SIG| Analog Input | 0 - 3.3V (ADC1) | Acoustic / Microphone Sound Level Sensor (Potentiometer) |
| **GPIO 23** | `btn_water` | `INPUT_PULLUP` | Active LOW | Utility Water Leak Simulation Pushbutton |
| **GPIO 19** | `btn_faculty` | `INPUT_PULLUP` | Active LOW | Classroom G03 Faculty Absent Simulator Pushbutton |
| **GPIO 18** | `btn_fight` | `INPUT_PULLUP` | Active LOW | North Corridor Fight Detection Pushbutton |
| **GPIO 5** | `btn_emergency`| `INPUT_PULLUP` | Active LOW | Manual Campus Evacuation Pushbutton |

---

## 🖥️ Expected Serial Monitor Output

### 1. System Startup (Clean & Silent)
```text
====================================
SMART CAMPUS SYSTEM
====================================
ESP32 Started
Initializing Sensors...
Initializing LCD...
Initializing Security System...
Audio Surveillance Active
All Systems Operational
Campus Status: SAFE
```
*Green LED turns ON. Buzzer is completely OFF. Red/Yellow/Blue LEDs are OFF.*

### 2. Continuous Loop Telemetry (Every 2 Seconds)
```text
System Running...
PIR Status: Classroom Occupied
Audio Level: 58 dB [LECTURE SPEECH DETECTED]
Temperature: 24°C
Humidity: 50%
MQ2 Value: 320
Flame Status: SAFE
Water Status: NORMAL
```

### 3. Audio & Safety Incident Triggers

#### 🎙️ Feature A: Faculty Voice & Presence Detection
- **When Potentiometer is at Lecture Level (48 - 74 dB)**:
  ```text
  [AUDIO] Faculty Voice Active: 62 dB (Lecture in progress)
  [INFO] Faculty Present - Normal Attendance Resumed
  ```
- **When Classroom is Occupied but Potentiometer is Silent (< 45 dB) or Button 1 Pressed**:
  ```text
  [ALERT] Faculty Absent
  Alert Sent To Security Office
  ```
  *Yellow LED turns ON. LCD switches to Faculty Absence notification.*

#### 🥊 Feature B: Student Fight & Acoustic Shock Detection
- **When Microphone Potentiometer Spikes > 85 dB (or Button 2 Pressed)**:
  ```text
  [ALERT] Student Fight Detected
  [CRITICAL] Acoustic Shock Detected: 92 dB (> 85 dB Scream Threshold)
  Security Team Notified
  ```
  *Red LED pulses, Alarm Buzzer chimes, LCD shows Acoustic Fight Alert.*
- **When Sound Drops Below 75 dB**:
  ```text
  [INFO] Acoustic Levels Normalized - Fight Disturbance Contained
  ```

#### 🚬 Feature C: Smoking Activity Detection
- **When Stairwell MQ-2 Smoke Sensor Exceeds Threshold (> 2000 ADC)**:
  ```text
  [ALERT] Smoking Activity Detected
  Smoking Detected
  Location: 2F Stairwell Restroom Corridor (Non-Smoking Zone)
  ```
  *LCD switches to Air Safety Alert.*

#### 🔥 Feature D: Fire Detection (Flame Sensor — GPIO 36)
- **Turn Flame Potentiometer > 60% (> 2500 ADC)**:
  ```text
  [CRITICAL] Fire Detected
  Fire Detected
  ```
  *Red LED strobe + Buzzer alarm siren.*

---

## 📟 20x4 I2C LCD Rotating Screens (Every 3 Seconds)

1. **Screen 0 — Campus Overview**: Master status (`ALL SECURE`), 19 operational nodes, total alerts.
2. **Screen 1 — Room Occupancy**: Real-time matrix of G01, G02, G03, and S01.
3. **Screen 2 — Audio Surveillance**: Real-time decibels, speech classification (`LECTURE DET` / `SILENT`), and corridor acoustic level.
4. **Screen 3 — Faculty Status**: Attendance status and acoustic verification.
5. **Screen 4 — Environmental Telemetry**: Multi-zone temperature and humidity readings.
6. **Screen 5 — Hazards & Solenoid**: Fire, Gas, Smoke, and Water pipeline safety status.
7. **Screen 6 — Security Operations**: AI CCTV camera status and Acoustic surveillance health.

**Emergency Override**: Any critical event (Fire, Gas Leak, Fight/Acoustic Shock, Emergency Button, Water Leak, Smoking) immediately takes over the display with evacuation or dispatch instructions!
