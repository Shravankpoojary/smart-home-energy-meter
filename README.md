# smart-home-energy-meter
### Major Project | ACS712 + STM32 Nucleo + ESP8266 + ThingSpeak

A smart AC energy monitor built with an STM32F446RE Nucleo board, ACS712 Hall-effect sensor, and an ESP8266 module (via AT commands) for local LCD visualization and cloud data logging.

## Overview
A real-time AC mains current monitoring system that measures the current drawnby a home appliance using an ACS712 Hall-effect current sensor. The STM32F446RE Nucleo board processes 1000 ADC samples per cycle to compute RMS current, peakcurrent, and apparent power. Readings are displayed live on a 16×2 I²C LCD anduploaded to the ThingSpeak cloud platform every 20 seconds via an ESP8266 NodeMCU.

## How It Works
1. ACS712 sensor clamps around the live wire and outputs an analog voltage proportional to current
2. STM32 reads the analog signal on PA0 using its 12-bit ADC (1000 samples per reading)
3. DC offset (midpoint bias) is calculated first, then subtracted from each sample
4. RMS current, peak current, and apparent power (VA) are computed
5. Results are shown on the 16×2 LCD in real time
6. STM32 sends the three values to ESP8266 over UART3 (PB10) as binary packets at 9600 baud
7. ESP8266 parses the packets and uploads data to ThingSpeak via WiFi every 20 seconds.
   
## Hardware Required
- STM32F446RE Nucleo-64 development board
- ACS712 current sensor module (20 A variant)
- ESP8266 NodeMCU (v1.0 / ESP-12E)
- 16×2 LCD with I²C backpack (PCF8574, address 0x27)
- Breadboard and jumper wires
- USB cables for programming both boards
- 5 V / 3.3 V power supply

## Key Parameters
| Parameter         | Value              |
|-------------------|--------------------|
| ADC Resolution    | 12-bit (0–4095)    |
| Reference Voltage | 3.3 V              |
| Samples per cycle | 1000               |
| Noise threshold   | 0.25 A             |
| UART baud rate    | 9600               |
| Upload interval   | 20 seconds         |
| Mains voltage     | 230 V              |


## Wiring
### ACS712 → STM32
| ACS712 Pin | STM32 Pin      |
|------------|----------------|
| VCC        | 5V             |
| GND        | GND            |
| OUT        | A0 (PA0)       |

### LCD (I²C) → STM32
| LCD Pin | STM32 Pin |
|---------|-----------|
| SDA     | PB7       |
| SCL     | PB6       |
| VCC     | 5V        |
| GND     | GND       |

### STM32 → ESP8266 (UART)
| STM32 Pin       | NodeMCU Pin   |
|-----------------|---------------|
| PB10 (UART3 TX) | D6 (GPIO12)   |
| GND             | GND           |
> **Note:** GND must be shared between STM32 and NodeMCU for UART communication to work.

## Packet Protocol (STM32 → ESP8266)
All values are scaled ×100 and sent as 4-byte binary frames at 9600 baud.
| Packet      | Byte 0 (Header) | Byte 1 | Byte 2 | Byte 3 (Footer) |
|-------------|-----------------|--------|--------|-----------------|
| RMS Current | `0xAA`          | MSB    | LSB    | `0x55`          |
| Peak Current| `0xBB`          | MSB    | LSB    | `0x66`          |
| Apparent Power | `0xCC`       | MSB    | LSB    | `0x77`          |
**Example:** RMS = 1.23 A → scaled value = 123 → `[0xAA][0x00][0x7B][0x55]`

## Software Required
- Arduino IDE (with STM32duino core for STM32 and ESP8266 core for NodeMCU)
- Libraries:
  - `Wire.h` (built-in)
  - `LiquidCrystal_I2C` (by Frank de Brabander)
  - `SoftwareSerial` (built-in with ESP8266 core)
  - `ESP8266WiFi` (built-in with ESP8266 core)
  - `ThingSpeak` (by MathWorks)
  
## Cloud Platform
- ThingSpeak IoT — https://thingspeak.com
- Field 1 → RMS Current (A)
- Field 2 → Peak Current (A)
- Field 3 → Apparent Power (VA)

## Block Diagram
<p align="center">
  <img width="1488" height="683" alt="ACS712 Current Monitor Block Diagram" src="https://github.com/user-attachments/assets/e94602c7-0b33-4191-95f5-90b4df127e03" />
  <br>
  <em>Figure 1: Block diagram of the ACS712 AC Current Monitor system.</em>
</p>


## System Setup Snapshot

<p align="center">
<img width="4080" height="1836" alt="IMG_20260501_153227" src="https://github.com/user-attachments/assets/051a21e5-b5bd-4af7-bd3f-81712901f642" />
  <br>
  <em>Figure 2: Physical hardware implementation of the Smart Energy Meter, showing the integrated STM32 Nucleo, ESP8266 NodeMCU, ACS712 current sensor, and 16x2 LCD display.</em>
</p>

  
## Setup Instructions
### STM32 (Sender)
1. Install STM32duino core in Arduino IDE
2. Install `LiquidCrystal_I2C` library via Library Manager
3. Open `ACS712_STM32.ino`
4. Adjust the following if needed:
   - `SENSITIVITY` → `0.185f` for 5 A module, `0.100f` for 20 A (default), `0.066f` for 30 A
   - `LCD_ADDRESS` → change to `0x3F` if LCD stays blank
   - `MAINS_VOLTAGE` → set to your local mains voltage (default `230.0f`)
5. Select board: **Nucleo-64 / STM32F446RE**
6. Upload the code

### ESP8266 (Receiver + WiFi Upload)
1. Install ESP8266 board core in Arduino IDE
2. Install `ThingSpeak` library via Library Manager
3. Open `ACS712_ESP8266.ino`
4. Replace the following with your own credentials:
   - `YOUR_WIFI_SSID` → your WiFi network name
   - `YOUR_WIFI_PASSWORD` → your WiFi password
   - `TS_CHANNEL_ID` → your ThingSpeak Channel ID
   - `TS_WRITE_API_KEY` → your ThingSpeak Write API Key
  
<p align="center">
  <img width="784" height="336" alt="ThingSpeak Credentials Location" src="https://github.com/user-attachments/assets/90ddd28a-8a26-428a-a25e-998c389549e9" />
  <br>
  <em>Figure 3: Locating ThingSpeak Credentials for Firmware.</em>
</p>

5. Select board: **NodeMCU 1.0 (ESP-12E Module)**
6. Upload the code
   
### ThingSpeak
1. Create a free account at https://thingspeak.com
2. Create a new channel with 3 fields:
   - Field 1: RMS Current (A)
   - Field 2: Peak Current (A)
   - Field 3: Apparent Power (VA)
3. Copy the Channel ID and Write API Key into the ESP8266 sketch

## Expected Output

**STM32 Serial Monitor and ESP8266 Serial Monitor (115200 baud):**

<p align="center">
  <img width="100%" alt="STM32 and ESP8266 Serial Monitor Logs" src="https://github.com/user-attachments/assets/4962d468-ea57-4052-a725-d3423b7d338d" />
  <br>
  <em>Figure 4: Serial monitor outputs from both the STM32 and ESP8266 modules displaying the parsed RMS current, peak current, and apparent power.</em>
</p>

**ThingSpeak Cloud Dashboard:**

<p align="center">
  <img width="100%" alt="ThingSpeak Cloud Dashboard showing Current and Power" src="YOUR_IMAGE_URL_HERE" />
  <br>
  <em>Figure 5: ThingSpeak cloud dashboard visualizing real-time telemetry from the ESP8266, including live gauges for RMS current, Peak current, and Apparent Power.</em>
</p>

**LCD Display:**

<p align="center">
  <img width="100%" alt="16x2 LCD Display showing RMS and Power" src="https://github.com/user-attachments/assets/66478931-dd2a-457c-ac2d-4bbfea7b868e" />
  <br>
  <em>Figure 6: Local 16x2 I2C LCD interface displaying real-time RMS current (A), Peak current (A), and Apparent Power (VA).</em>
</p>


