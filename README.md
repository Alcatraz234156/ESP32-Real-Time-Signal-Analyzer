# ESP32-Real-Time-Signal-Analyzer

ESP32-based real-time signal acquisition and analysis system with OLED UI, Python/NumPy DSP, FFT, harmonic analysis, THD and live spectrogram visualization.

## Hardware Used

| Component | Purpose |
|---|---|
| ESP32 Development Board | Main controller, ADC sampling and serial communication |
| 0.96" 128×64 SSD1306 OLED | On-device menu and live measurement display |
| 10 kΩ Potentiometer | Menu navigation |
| 10 kΩ Resistor | ADC input pull-down on GPIO34 to prevent floating readings when no signal is connected |
| ON/OFF Rocker Switch | Menu selection / navigation |
| Breadboard | Circuit prototyping |
| Jumper Wires | Electrical connections |
| USB Cable | ESP32 power, programming and serial communication |
| PC/Laptop | Python-based signal processing and visualization |

### Pin Configuration

| Function | ESP32 Pin |
|---|---:|
| Signal Input / ADC | GPIO 34 |
| ADC Pull-down | GPIO 34 → 10 kΩ → GND |
| Potentiometer | GPIO 35 |
| Menu Switch | GPIO 27 |
| OLED SDA | GPIO 32 |
| OLED SCL | GPIO 33 |

## Software & Libraries

| Software / Library | Purpose |
|---|---|
| Arduino IDE | ESP32 firmware development and uploading |
| Python 3 | PC-side signal processing and visualization |
| NumPy | Numerical processing, FFT, harmonic analysis and THD calculations |
| Matplotlib | Real-time oscilloscope, FFT spectrum and spectrogram visualization |
| PySerial | Serial communication between the ESP32 and PC |
| Adafruit GFX Library | Graphics primitives for the OLED interface |
| Adafruit SSD1306 Library | SSD1306 OLED control |
| Git & GitHub | Version control and project documentation |

### Programming Languages

- **C++** — ESP32 firmware
- **Python** — PC-side DSP, analysis and visualization
