#ifndef MAGNETOMETER_H
#define MAGNETOMETER_H

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <Preferences.h>
#include <math.h>

// --- KONFIGURASI ---
const float HARDCODED_DECLINATION_RAD = 0.0; 
const char* PREFS_NAMESPACE = "rover_mag"; 
const float INTERFERENCE_THRESHOLD = 0.40; // 20% ralat gangguan magnetik
const float MIN_SWING_THRESHOLD = 20.0;    // Beza max-min untuk tentukan kalibrasi berjaya

class Magnetometer {
  private:
    Adafruit_HMC5883_Unified mag;
    Preferences preferences;
    
    // Pembolehubah Kalibrasi
    float _xOffset = 0;
    float _yOffset = 0;
    float _expectedStrength = 0; // Untuk kesan kalibrasi lari
    int32_t _sensorID;

    // State Machine Variables
    unsigned long _calStartTime = 0;
    float _calMinX, _calMaxX;
    float _calMinY, _calMaxY;

    // Helper: Degree <-> Radian
    float toRadians(float degree) { return degree * (M_PI / 180.0); }
    float toDegrees(float radian) { return radian * (180.0 / M_PI); }

    // Helper: Baca dan betulkan orientasi sensor (180 deg)
    void readCorrectedMag(float &x, float &y, float &z) {
      sensors_event_t event;
      mag.getEvent(&event);

      // Pembetulan orientasi 
      x = -event.magnetic.y;
      y = event.magnetic.x;
      z = event.magnetic.z; 
    }

    // Helper: Kira kekuatan medan magnet 3D
    float getCurrentStrength() {
      float mx, my, mz;
      readCorrectedMag(mx, my, mz);
      // Abaikan mz, guna X dan Y sahaja untuk monitor gangguan
      return sqrt(sq(mx) + sq(my)); 
    }

    // Helper: Load Data dari Flash Memory
    void loadCalibration() {
      if (preferences.begin(PREFS_NAMESPACE, true)) {
        _xOffset = preferences.getFloat("x_off", 0.0);
        _yOffset = preferences.getFloat("y_off", 0.0);
        _expectedStrength = preferences.getFloat("exp_str", 0.0);
        isCalibrated = preferences.getBool("is_cal", false);
        preferences.end();
        
        Serial.println("[MAG] Data Loaded!");
      } else {
        Serial.println("[MAG] No calibration data found.");
      }
    }

    // Helper: Simpan Data ke Flash Memory
    void saveCalibration() {
      if (preferences.begin(PREFS_NAMESPACE, false)) {
        preferences.putFloat("x_off", _xOffset);
        preferences.putFloat("y_off", _yOffset);
        preferences.putFloat("exp_str", _expectedStrength);
        preferences.putBool("is_cal", true);
        preferences.end();
        Serial.println("[MAG] Calibration SAVED to Flash!");
      }
    }

  public:
    // --- PUBLIC VARIABLES ---
    int countdown = 0;           // Baki masa kalibrasi (saat)
    bool calibrateNow = false;   // True jika user perlu buat kalibrasi
    bool isSuccessful = false;   // True jika kalibrasi terakhir berjaya (cukup pusingan)
    bool isCalibrating = false;  // True semasa proses kalibrasi 10s berjalan
    bool isCalibrated = false;   // True jika sistem mempunyai data kalibrasi yang sah

    Magnetometer(int32_t sensorID = 12345) : mag(sensorID), _sensorID(sensorID) {}

    bool begin() {
      if (!mag.begin()) {
        Serial.println("[MAG] Error: HMC5883 sensor not found!");
        return false; 
      }
      
      loadCalibration();
      checkCalibrationStatus(); // Tentukan nilai 'calibrateNow'
      
      return true;
    }

    // Fungsi untuk check adakah kawasan sekarang ada gangguan magnetik
    // Boleh dipanggil berkala jika mahu sentiasa monitor
    void checkCalibrationStatus() {
      if (!isCalibrated || _expectedStrength == 0) {
        calibrateNow = true;
        return;
      }
      
      float currentStr = getCurrentStrength();
      float diff = abs(currentStr - _expectedStrength) / _expectedStrength;
      
      if (diff > INTERFERENCE_THRESHOLD) {
        calibrateNow = true; // Gangguan dikesan, perlu calibrate
        // Serial.println("[MAG] Amaran: Gangguan magnetik dikesan!");
      } else {
        calibrateNow = false;
      }
    }

    // 1. Mula proses kalibrasi
    void startCalibration() {
      Serial.println("[MAG] Starting Calibration (10s)...");
      _calMinX = 10000; _calMaxX = -10000;
      _calMinY = 10000; _calMaxY = -10000;
      
      _calStartTime = millis();
      isCalibrating = true;
      isSuccessful = false; 
      countdown = 10;
    }

    // 2. Loop update (Panggil sentiasa dalam loop utama)
    // Return TRUE apabila proses 10 saat baru sahaja TAMAT.
    bool update() {
      if (!isCalibrating) return false;

      unsigned long elapsed = millis() - _calStartTime;

      // Jika masa < 10 saat (10000ms)
      if (elapsed < 10000) {
        // Update variable countdown untuk UI
        countdown = 10 - (elapsed / 1000);
        if (countdown < 0) countdown = 0;

        float magX, magY, magZ;
        readCorrectedMag(magX, magY, magZ); 

        // Cari nilai Min/Max
        if (magX < _calMinX) _calMinX = magX;
        if (magX > _calMaxX) _calMaxX = magX;
        if (magY < _calMinY) _calMinY = magY;
        if (magY > _calMaxY) _calMaxY = magY;
        
        return false; // Proses masih berjalan
      } 
      else {
        // Masa tamat!
        countdown = 0;
        isCalibrating = false;

        // Semak adakah user betul-betul pusing sensor (Swing Detection)
        float swingX = _calMaxX - _calMinX;
        float swingY = _calMaxY - _calMinY;

        if (swingX < MIN_SWING_THRESHOLD || swingY < MIN_SWING_THRESHOLD) {
          isSuccessful = false;
          calibrateNow = true; // Kekal true supaya user buat lagi
          Serial.println("[MAG] GAGAL: Pusingan tidak mencukupi!");
          return true; // Return true tanda proses dah tamat (walaupun gagal)
        }

        // Jika berjaya, kira Offset
        _xOffset = (_calMaxX + _calMinX) / 2.0;
        _yOffset = (_calMaxY + _calMinY) / 2.0;
        
        _expectedStrength = getCurrentStrength(); // Ambil benchmark baru
        
        isCalibrated = true;
        isSuccessful = true;
        calibrateNow = false; // Dah tak perlu calibrate lagi

        saveCalibration();

        Serial.println("[MAG] Calibration Done & Successful.");
        return true; 
      }
    }

    int northHeading() {
      float magX, magY, magZ;
      readCorrectedMag(magX, magY, magZ); 

      // Tolak offset calibration
      float headingX = magX - _xOffset;
      float headingY = magY - _yOffset;

      float heading = atan2(headingY, headingX);
      heading += HARDCODED_DECLINATION_RAD;

      // Normalize
      if (heading < 0) heading += 2 * M_PI;
      if (heading > 2 * M_PI) heading -= 2 * M_PI;

      return (int)toDegrees(heading);
    }

    int headingTo(float currLat, float currLon, float targetLat, float targetLon) {
      float lat1 = toRadians(currLat);
      float lon1 = toRadians(currLon);
      float lat2 = toRadians(targetLat);
      float lon2 = toRadians(targetLon);

      float dLon = lon2 - lon1;

      float y = sin(dLon) * cos(lat2);
      float x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(dLon);
      
      float bearing = atan2(y, x);

      float bearingDeg = toDegrees(bearing);
      if (bearingDeg < 0) {
        bearingDeg += 360;
      }

      return (int)bearingDeg;
    }
};

#endif