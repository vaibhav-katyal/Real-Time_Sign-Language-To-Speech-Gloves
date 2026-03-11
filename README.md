# Real-Time Sign Language to Speech Glove

An ESP32-based wearable glove that translates hand gestures into speech in real time. The system uses four MPU6050 sensors (three on fingers, one on the wrist) connected through a PCA9548A I2C multiplexer to detect finger bends and wrist motions, then serves a live web dashboard that speaks the recognized gestures aloud.

---

## 📸 Project Photos

<!-- Add your project photos to an "images" folder and update the paths below -->

| | |
|---|---|
| ![Glove Overview](images/glove_overview.jpg) | ![Circuit Wiring](images/circuit_wiring.jpg) |
| *Full glove assembly* | *Circuit and wiring* |

| | |
|---|---|
| ![Web Dashboard](images/web_dashboard.jpg) | ![Sensors Close-up](images/sensors_closeup.jpg) |
| *Web dashboard on phone* | *MPU6050 sensor placement* |

> **To add your photos:** Create an `images/` folder in the project directory, place your photos there, and update the filenames above.

---

## Features

- **Finger Bend Detection** — Gyroscope spike detection on index, middle, and ring fingers
- **Wrist Motion Recognition** — Detects nods, waves, shakes, bows, circles, and jerks
- **Web Dashboard** — Real-time UI served over WiFi with live sensor data
- **Text-to-Speech** — Browser-based speech synthesis reads detected gestures aloud
- **Priority System** — Wrist gestures take priority; speech blocking prevents overlapping outputs
- **No External App Required** — Works on any device with a browser (phone, tablet, laptop)

---

## Supported Gestures

### Finger Gestures

| Finger | Gesture | Message |
|--------|---------|---------|
| Index  | Bend    | "I am feeling sick" |
| Middle | Bend    | "I am feeling thirsty" |
| Ring   | Bend    | "I love you" |

### Wrist Gestures

| Motion | Gesture | Message |
|--------|---------|---------|
| Nod (up-down) | Help Nod | "Help Me!" |
| Bow (forward-back) | Hi Bow | "Hi" |
| Wave (left-right) | Name Wave | "My name is Vaibhav" |
| Fast Shake | How Shake | "How are you?" |
| Rapid Circle | Emergency Circle | "EMERGENCY! Call 911!" |
| Sharp Up Jerks | Call Help Jerk | "CALL HELP!" |
| Wobble | Maybe Wobble | "Maybe" *(text only, no speech)* |

---

## Hardware Required

| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32 Dev Board | 1 | Microcontroller with WiFi |
| MPU6050 (GY-521) | 4 | Gyroscope + accelerometer sensors |
| PCA9548A I2C Multiplexer | 1 | Allows multiple MPU6050s on the same I2C bus |
| Glove | 1 | Wearable base |
| Jumper Wires | — | Connections |
| Breadboard or PCB | 1 | Circuit assembly |
| USB Cable | 1 | Power and programming |

---

## Circuit Diagram

```
ESP32                PCA9548A
┌──────┐            ┌──────────┐
│ GPIO 21 (SDA) ────┤ SDA      │
│ GPIO 22 (SCL) ────┤ SCL      │
│ 3.3V ─────────────┤ VCC      │
│ GND ──────────────┤ GND      │
└──────┘            │          │
                    │ CH0 ─────── MPU6050 (Index Finger)
                    │ CH2 ─────── MPU6050 (Middle Finger)
                    │ CH3 ─────── MPU6050 (Ring Finger)
                    └──────────┘

ESP32 ── I2C (direct, no mux) ── MPU6050 (Wrist)
```

> The wrist MPU6050 is connected directly to the ESP32 I2C bus (PCA9548A channel set to 0x00 to deselect all channels).

---

## Software Setup

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) (v2.0+ recommended)
- ESP32 board package installed in Arduino IDE
- Required libraries (all included with ESP32 core):
  - `WiFi.h`
  - `WebServer.h`
  - `Wire.h`

### Installation

1. **Clone or download** this repository
2. **Open** `signlangcode.ino` in Arduino IDE
3. **Update WiFi credentials** in the code:
   ```cpp
   const char* ssid = "your_wifi_name";
   const char* password = "your_wifi_password";
   ```
4. **Select board:** Tools → Board → ESP32 Dev Module
5. **Select port:** Tools → Port → (your ESP32 COM port)
6. **Upload** the sketch

### Usage

1. Power on the ESP32 (USB or battery)
2. Open the Serial Monitor (115200 baud) to see the assigned IP address
3. Open the IP address in any browser on the same WiFi network
4. The web dashboard will display live sensor data and speak detected gestures

---

## How It Works

1. **Sensor Reading** — The ESP32 reads gyroscope and accelerometer data from all four MPU6050 sensors at ~20 Hz
2. **Finger Detection** — A spike in gyroscope data above 50°/s indicates a finger bend. The bend state is held for 1.5 seconds before resetting
3. **Wrist Detection** — A rolling history of 30 wrist gyro samples is analyzed for patterns (direction changes, dominant axes, value ranges) to classify motions
4. **Priority System** — Wrist gestures are checked first. When a gesture triggers speech, new detections are blocked for 3 seconds to prevent overlap
5. **Web Interface** — The ESP32 hosts a web server. The dashboard polls `/data` every 200ms and uses the Web Speech API to speak recognized gestures

---

## Project Structure

```
signlangcode/
├── signlangcode.ino   # Main Arduino sketch (all logic + web UI)
├── images/            # Project photos (add your own)
│   ├── glove_overview.jpg
│   ├── circuit_wiring.jpg
│   ├── web_dashboard.jpg
│   └── sensors_closeup.jpg
└── README.md          # This file
```

---

## Customization

- **Add new gestures** — Modify `checkFingerGestures()` or `checkWristMotionGestures()` with new detection patterns
- **Change messages** — Update the text strings in `updateGesture()` calls
- **Adjust sensitivity** — Tune `SPIKE_THRESHOLD` (default 50°/s) and wrist motion thresholds
- **Change speech timing** — Modify `MIN_SPEECH_DURATION` (default 3000ms) and `FINGER_CHECK_INTERVAL` (default 2000ms)

---

## Troubleshooting

| Issue | Solution |
|-------|----------|
| No WiFi connection | Verify SSID and password; ensure 2.4 GHz network |
| Sensors not detected | Check I2C wiring and PCA9548A channel assignments |
| False gesture triggers | Increase `SPIKE_THRESHOLD` or wrist motion thresholds |
| No speech output | Click anywhere on the web page first (browser requires user interaction to enable speech) |
| Erratic readings | Ensure solid solder joints; keep wires short |

---

## License

This project is open source and available for educational and personal use.

---

## Acknowledgments

Built for assistive communication — enabling non-verbal individuals to express needs and emotions through hand gestures.
