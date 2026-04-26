#include <SPI.h>       // Wajib tambah untuk custom SPI pin
#include <Wire.h>      // Wajib tambah untuk custom I2C pin
#include <LoRa.h>
#include <Adafruit_BMP280.h>
#include <TinyGPSPlus.h>       // --- TAMBAHAN BARU: Library GPS ---
#include <HardwareSerial.h>    // --- TAMBAHAN BARU: Untuk Serial GPS ---
#include <Adafruit_INA219.h>   // --- TAMBAHAN BARU: Library INA219 ---
#include <Adafruit_NeoPixel.h> // --- UBAH: Tambah Library NeoPixel ---
#include "LoraManager_TX.h"
#include "BatteryPercentage.h"

// --- OBJEK SENSOR & MODUL ---
Adafruit_BMP280 bmp;
Adafruit_INA219 ina219;        // Objek INA219
TinyGPSPlus gps;               // Objek GPS
HardwareSerial SerialGPS(1);   // Guna UART1 untuk GPS
LoraManager_TX loraSys;
BatteryMonitor batMon;


// --- SETTING PIN BARU (ESP32-S3 SuperMini) ---
#define SPI_SCK  12
#define SPI_MISO 13
#define SPI_MOSI 11

#define LORA_CS  10
#define LORA_RST 6
#define LORA_IRQ 7

#define I2C_SDA  8
#define I2C_SCL  9

// --- PIN GPS BARU (Menggunakan GPIO Header sahaja) ---
#define GPS_RX 4 //sambung ke TX GPS
#define GPS_TX 5 //sambung ke RX GPS

#define BUZZER 2

// --- UBAH: Setting NeoPixel ---
#define NEOPIXEL_PIN 48 
#define NUMPIXELS 1
Adafruit_NeoPixel pixels(NUMPIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  delay(3000);
  // 1. Setup Status LED (NeoPixel)
  // --- UBAH: Init NeoPixel dan bukan pinMode biasa ---
  pixels.begin();
  pixels.setBrightness(50); // Set kecerahan (0-255). 50 cukup terang.
  pixels.clear();
  pixels.show();

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW); // Off buzzer (active low/high bergantung pada jenis)

  Serial.begin(115200);
  
  Serial.println("\n--- MULA BOOT TX BASE (S3 SUPERMINI) ---");

  // 2. Mulakan custom SPI bus
  SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, LORA_CS);

  // 3. Setup LoRa Pins & Begin
  Serial.println("Initializing LoRa...");
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);
  
  if (!LoRa.begin(915E6)) {
    Serial.println("RALAT: LoRa gagal dikesan. Semak wiring!");
    while (1) {
      // --- UBAH: Blink MERAH jika error LoRa ---
      pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Merah
      pixels.show();
      delay(100);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));   // Off
      pixels.show();
      delay(100); 
    }
  }
  Serial.println("LoRa OK!");

  // 4. Mulakan custom I2C bus untuk BMP280/INA219
  Wire.begin(I2C_SDA, I2C_SCL);

  // 5. Setup BMP280
  Serial.println("Initializing BMP280...");
  if (!bmp.begin(0x76)) { 
    Serial.println("RALAT: BMP280 tidak dikesan. Semak wiring I2C!");
  } else {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL, 
                    Adafruit_BMP280::SAMPLING_X2, 
                    Adafruit_BMP280::SAMPLING_X16, 
                    Adafruit_BMP280::FILTER_X16, 
                    Adafruit_BMP280::STANDBY_MS_500); 
    Serial.println("BMP280 OK!");
  } 

  // --- TAMBAHAN BARU: Setup INA219 ---
  Serial.println("Initializing INA219...");
  if (!ina219.begin()) {
    Serial.println("RALAT: INA219 tidak dikesan. Semak wiring I2C!");
  } else {
    Serial.println("INA219 OK!");
  }

  // --- TAMBAHAN BARU: Setup GPS Serial ---
  Serial.println("Initializing GPS...");
  // 1. Tetapkan Baudrate 115200
  SerialGPS.begin(115200, SERIAL_8N1, GPS_RX, GPS_TX); 
  delay(1000); // Tunggu modul bersedia

  Serial.println("Konfigurasi BASE bermula...");

  // 2. Set Update Rate ke 10Hz (100ms)
  SerialGPS.println("$PQTMGNSSRATE,100*39");
  delay(100);

  // 3. Track Satelit Paling Banyak (Buka GPS + GLONASS + Galileo + BDS + QZSS)
  SerialGPS.println("$PQTMGNSS,1,1,1,1,1,0*3E");
  delay(100);

  // 4. Mesej NMEA yang SAMA (Hanya RMC, GGA dan VTG diletupkan, GSV ditutup)
  // Ini mengelakkan Serial Monitor banjir dengan teks pada kelajuan 10Hz
  SerialGPS.println("$PQTMNMEOUT,1,1,0,0,0,1,0,0,0,0,0*3E");
  delay(100);

  // 5. Navigation Mode: Stationary (Penting untuk Base supaya tak drift)
  SerialGPS.println("$PQTMNAVMODE,1*3A"); 
  delay(100);

  // Simpan ke Flash Memory
  SerialGPS.println("$PQTMSAVEPAR*5A");
  delay(100);
  
  Serial.println("BASE sedia digunakan!");

  loraSys.begin();
  // INITIALIZE BATTERY MONITOR (Penting untuk load data kalibrasi)
  batMon.begin();
  batMon.debugProfile();
  // if (batMon.isCalibrated) {
  //   batMon.setCapacityMultiplier(6.0); 
  //   Serial.println("Penyelarasan Kapasiti Selesai (Multiplier: 6x)");
  // }

  digitalWrite(BUZZER, LOW); // Pastikan logic buzzer betul (LOW = Bunyi atau HIGH = Bunyi?)

  Serial.println("--- BOOT SELESAI ---");
}

unsigned long previousMillis = 0;

void loop() {
  // --- 1. BACA DATA GPS ---
  while (SerialGPS.available() > 0) {
    char c = SerialGPS.read();
    gps.encode(c);
    // Serial.print(c);
  }
  bool baseGpsFix = gps.location.isValid();
  float hdop = gps.hdop.isValid() ? gps.hdop.hdop() : 4.0; 
  double baseLat = gps.location.isValid() ? gps.location.lat() : 0.0;
  double baseLon = gps.location.isValid() ? gps.location.lng() : 0.0;

  // Serial.println(String(gps.satellites.value()));


  // --- 2. BACA DATA INA219 & KIRA BATERI ---
  float busVoltage = ina219.getBusVoltage_V();
  float current_mA = ina219.getCurrent_mA();
  batMon.getData(busVoltage, current_mA);
  int baseBattery = batMon.percentage;

    unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;
    // Serial.print("\nTime Str:" + batMon.timeStr);Serial.println(", Current:" + String(-current_mA, 0) + "mA\n");
    // Serial.println(String(gps.location.lat()));
  }


  // --- 3. BACA DATA BMP280 ---
  float baseAlt = bmp.readAltitude(1014); 

  // --- 4. HANTAR SEMUA DATA KE LORA SYSTEM ---
  loraSys.run(baseGpsFix, hdop, baseBattery, baseAlt, baseLat, baseLon);

  // --- 5. KAWALAN LED STATUS (NEOPIXEL) ---
  // Format warna: pixels.Color(RED, GREEN, BLUE) - Nilai 0-255
  
  // --- KAWALAN LED STATUS (NEOPIXEL) ---
  if (loraSys.isConnected) {
    
    if (loraSys.status) {
      // Status TRUE: Warna BIRU (Kekal seperti asal)
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); 
    } 
    else {
      // Status FALSE: Warna ikut RSSI (Hijau -> Merah)
      
      // 1. Dapatkan RSSI terkini
      int rssi = LoRa.packetRssi(); 

      // 2. Clamp/Hadkan nilai RSSI supaya tak lari dari julat -20 hingga -120
      // Kalau lebih -20 (terlampau dekat), kekal -20. Kalau kurang -120, kekal -120.
      if (rssi > -20) rssi = -20;
      if (rssi < -120) rssi = -120;

      // 3. Kira nilai Merah & Hijau guna function map()
      // Bila RSSI -20 (Kuat): Merah 0, Hijau 255
      // Bila RSSI -120 (Lemah): Merah 255, Hijau 0
      
      int redVal = map(rssi, -20, -120, 0, 255);   // Semakin lemah, semakin merah
      int greenVal = map(rssi, -20, -120, 255, 0); // Semakin lemah, hijau hilang

      // 4. Set warna pixel
      pixels.setPixelColor(0, pixels.Color(redVal, greenVal, 0)); 
    }
    
  } else {
    // Jika disconnected: Warna MERAH SOLID
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
  }
  
  pixels.show(); // Update LED
}