# ESP32 LoRa-Based Land Surveying Assistant (Base & Rover System)

An advanced, DIY geospatial tool designed to assist land surveyors in locating coordinates, verifying datums, and performing field navigation. This system utilizes a **Base and Rover** configuration to mitigate GPS drift and provide high-precision relative positioning via LoRa communication.

## ✨ Key Features
* **Differential GNSS Logic (Tare Function):** Minimizes "GPS Drift" by calculating relative offsets between the Base and Rover.
* **Visual Navigation (CDI Mode):** Graphical Course Deviation Indicator (CDI) to guide users along a path between two points.
* **Geospatial Data Management:** * Parse and visualize `.kml` files directly on a 3.5" TFT screen.
    * Integrated Web Server for CSV/KML file uploads and downloads via Wi-Fi.
* **Real-time Telemetry:** Monitor Battery Health (INA219), Atmospheric Pressure (BMP280), and Compass Heading (HMC5883L).

## 🛠 Hardware Stack
| Component | Specification |
| :--- | :--- |
| **Microcontroller** | ESP32-S3 (N16R8) |
| **Display** | 3.5” TFT SPI (480x320) + Touchscreen |
| **LoRa Module** | Semtech SX1276 (433/915 MHz) |
| **GPS Module** | NEO-M8N with Active Antenna |
| **Sensors** | HMC5883L (Magnetometer), BMP280 (Barometer) |

## 📂 Project Structure
* `/src/Navigation.h` - Core distance and bearing algorithms.
* `/src/DisplayCTE.h` - Logic for the graphical navigation indicator.
* `/src/KMLFile.h` - Web interface and file system handlers.
* `/src/TouchButton.h` - Custom UI library for touch interactions.

## 🚀 Getting Started
1. Clone this repository.
2. Install required libraries: `TFT_eSPI`, `TinyGPS++`, `LoRa`, `AsyncTCP`.
3. Configure your pin mapping in `User_Setup.h` (TFT_eSPI).
4. Upload to your ESP32-S3 boards.

## Schematics and PCB Design
Base
<img width="3300" height="2550" alt="Base Schematic" src="https://github.com/user-attachments/assets/bcbeca97-ea46-42f9-a79c-68b0efb911cf" />
<img width="2037" height="1077" alt="Base PCB Design" src="https://github.com/user-attachments/assets/db9f5cf8-dfb9-4b50-87d6-b746cd009c29" />

Rover
<img width="3300" height="2550" alt="Rover Schematic" src="https://github.com/user-attachments/assets/78af9db3-1a71-40d7-ad3d-2fc98291df3c" />
<img width="2390" height="1153" alt="Rover PCB Design" src="https://github.com/user-attachments/assets/7bc40b55-f03d-46c2-a865-e09314ac346d" />

Gallery
<img width="2560" height="1920" alt="6303269277942878080_119" src="https://github.com/user-attachments/assets/32fa7c99-1725-4a0c-aa23-947023beda0b" />
<img width="2560" height="1920" alt="6303269277942878075_119" src="https://github.com/user-attachments/assets/18be4b27-6df1-4403-947b-bedb86e15e65" />
<img width="2560" height="1920" alt="6303269277942878079_119" src="https://github.com/user-attachments/assets/852b9f62-16a5-4a8e-a337-c95f4ebbeda3" />
<img width="2560" height="1920" alt="6303269277942878078_119" src="https://github.com/user-attachments/assets/f43657c5-85f0-414a-a133-0cd50a87ec9b" />
<img width="2560" height="1920" alt="6303269277942878077_119" src="https://github.com/user-attachments/assets/66cb0d62-2534-4510-a86b-62438787bfa2" />
<img width="2560" height="1920" alt="6303269277942878076_119" src="https://github.com/user-attachments/assets/02e198dc-7cf0-48ce-b38c-7291877054f5" />
<img width="2560" height="1920" alt="6303269277942878074_119" src="https://github.com/user-attachments/assets/a2dc804b-df22-407c-afc7-0bc959dcdf81" />



