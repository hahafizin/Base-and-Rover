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

## Schematics
<img width="3300" height="2550" alt="Base Schematic" src="https://github.com/user-attachments/assets/bcbeca97-ea46-42f9-a79c-68b0efb911cf" />
<img width="3300" height="2550" alt="Rover Schematic" src="https://github.com/user-attachments/assets/78af9db3-1a71-40d7-ad3d-2fc98291df3c" />

