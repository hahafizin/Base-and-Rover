#ifndef BATTERY_MONITOR_H
#define BATTERY_MONITOR_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <Preferences.h>

#define FILE_CAL_DISCHARGE    "/cal_discharge.txt"
#define FILE_CAL_CHARGE       "/cal_charge.txt"
#define FILE_PROFILE          "/profile.bin"

enum CalState { CAL_IDLE = 0, CAL_RECORDING_DISCHARGE = 1, CAL_RECORDING_CHARGE = 2 };

struct BatteryProfile {
  int totalDischargeMins;
  int totalChargeMins;
  float calDischargeCurrentMA; // <--- BARU: Simpan arus purata masa kalibrasi
  float dischargeCurve[101];
  float chargeCurve[101];
  bool valid;
};

class BatteryMonitor {
  public:
    int percentage = 50;
    int time = -1;
    String timeStr = "--";
    bool isCharging = false;
    bool isCalibrated = false;
    CalState currentCalState = CAL_IDLE;
    String statusMsg = "Menunggu Arahan...";

    BatteryMonitor() {}

    void begin() {
      if (!LittleFS.begin(true)) return;
      prefs.begin("bat_state", false);
      currentCalState = (CalState)prefs.getInt("mode", 0);
      loadProfile(); 
    }

    void factoryReset() {
      LittleFS.remove(FILE_CAL_DISCHARGE);
      LittleFS.remove(FILE_CAL_CHARGE);
      LittleFS.remove(FILE_PROFILE);
      currentCalState = CAL_IDLE;
      prefs.putInt("mode", CAL_IDLE); 
      isCalibrated = false;
    }

    void startDischarging() {
      LittleFS.remove(FILE_CAL_DISCHARGE);
      currentCalState = CAL_RECORDING_DISCHARGE;
      prefs.putInt("mode", (int)currentCalState);
      
      sumCurrent = 0; // Reset pengumpul arus
      currentCount = 0;
      
      recordData(FILE_CAL_DISCHARGE, lastVoltage);
      lastRecordTime = millis();
    }

    void startCharging() {
      if (!LittleFS.exists(FILE_CAL_DISCHARGE)) return;
      currentCalState = CAL_RECORDING_CHARGE;
      prefs.putInt("mode", (int)currentCalState);
      recordData(FILE_CAL_CHARGE, lastVoltage);
      lastRecordTime = millis();
    }

    void finishCalibration() {
      if (currentCalState != CAL_RECORDING_CHARGE) return;
      currentCalState = CAL_IDLE;
      prefs.putInt("mode", (int)currentCalState);
      processCalibrationData(); 
    }

    void getData(float voltage, float current) {
      unsigned long now = millis();
      lastVoltage = voltage; 
      // Kita guna absolute current (sentiasa positif) untuk pengiraan multiplier
      float absCurrent = abs(current);
      
      bool chargingNow = (current < -10.0); 
      if (chargingNow != isCharging) {
        isCharging = chargingNow;
        lastLatchedTime = -1; 
      }

      // Logic Kalibrasi
      if (currentCalState != CAL_IDLE) {
        if (now - lastRecordTime >= 60000) {
          lastRecordTime = now;
          if (currentCalState == CAL_RECORDING_DISCHARGE) {
            recordData(FILE_CAL_DISCHARGE, voltage);
            // Kumpul arus untuk cari purata kalibrasi
            sumCurrent += absCurrent;
            currentCount++;
          }
          else if (currentCalState == CAL_RECORDING_CHARGE) {
            recordData(FILE_CAL_CHARGE, voltage);
          }
        }
      }

      // Update Paparan (Setiap 10 saat)
      if (now - lastUpdate >= 10000) {
        lastUpdate = now;
        if (isCalibrated && currentCalState == CAL_IDLE) {
          calculateStats(voltage, absCurrent); // <--- Pass current sekali
        } else {
          percentage = constrain(map(voltage * 100, 600, 840, 0, 100), 0, 100);
          timeStr = (currentCalState == CAL_IDLE) ? "--" : "CAL";
        }
      }
    }

    void debugProfile() {
      if (!isCalibrated) {
        Serial.println("--- DEBUG: PROFILE NOT CALIBRATED IN MEMORY ---");
      }
      Serial.println("--- DATA DALAM profile.dat ---");
      Serial.print("Discharge Mins: "); Serial.println(profile.totalDischargeMins);
      Serial.print("Charge Mins:    "); Serial.println(profile.totalChargeMins);
      Serial.print("Cal Current:    "); Serial.print(profile.calDischargeCurrentMA); Serial.println(" mA");
      Serial.print("Valid Flag:     "); Serial.println(profile.valid ? "TRUE" : "FALSE");
      
      // Tengok titik voltan pertama dan terakhir
      Serial.print("Curve 0% (Min): "); Serial.print(profile.dischargeCurve[0]); Serial.println(" V");
      Serial.print("Curve 100% (Max): "); Serial.print(profile.dischargeCurve[100]); Serial.println(" V");
      Serial.println("-----------------------------");
    }

  private:
    Preferences prefs;
    BatteryProfile profile;
    unsigned long lastUpdate = 0, lastRecordTime = 0;
    float lastVoltage = 0;
    float sumCurrent = 0;
    int currentCount = 0;
    int lastLatchedPercent = -1, lastLatchedTime = -1;

    void recordData(const char* fname, float val) {
      File f = LittleFS.open(fname, "a");
      if (f) { f.println(val); f.close(); }
    }

    void loadProfile() {
      if (LittleFS.exists(FILE_PROFILE)) {
        File file = LittleFS.open(FILE_PROFILE, "r");
        file.read((uint8_t*)&profile, sizeof(BatteryProfile));
        file.close();
        if (profile.valid) {
            isCalibrated = true;
            Serial.println("Calibration Status: VALID");
        } else {
            Serial.println("Calibration Status: INVALID (Check data!)");
        }
        Serial.println("profiles.dat OK");
      } else Serial.println("profiles.dat not exist");
    }

    void processCalibrationData() {
      int dL = countLines(FILE_CAL_DISCHARGE);
      int cL = countLines(FILE_CAL_CHARGE);
      if (dL < 5 || cL < 5) return;

      profile.totalDischargeMins = dL;
      profile.totalChargeMins = cL;
      // Kira purata arus masa discharge tadi
      profile.calDischargeCurrentMA = (currentCount > 0) ? (sumCurrent / currentCount) : 300.0;

      buildCurve(FILE_CAL_DISCHARGE, profile.dischargeCurve, dL, true);
      buildCurve(FILE_CAL_CHARGE, profile.chargeCurve, cL, false);
      profile.valid = true;
      
      File f = LittleFS.open(FILE_PROFILE, "w");
      f.write((uint8_t*)&profile, sizeof(BatteryProfile));
      f.close();
      isCalibrated = true;
      LittleFS.remove(FILE_CAL_DISCHARGE);
      LittleFS.remove(FILE_CAL_CHARGE);
    }

    void calculateStats(float voltage, float actualCurrent) {
      float* curve = isCharging ? profile.chargeCurve : profile.dischargeCurve;
      int closestIndex = 0;
      float minDiff = 100.0;
      for (int i = 0; i <= 100; i++) {
        float diff = abs(voltage - curve[i]);
        if (diff < minDiff) { minDiff = diff; closestIndex = i; }
      }
      int rawPct = closestIndex;

      // Strict Percentage
      if (lastLatchedPercent == -1) lastLatchedPercent = rawPct;
      if (isCharging) { if (rawPct >= lastLatchedPercent) lastLatchedPercent = rawPct; }
      else { if (rawPct <= lastLatchedPercent) lastLatchedPercent = rawPct; }
      percentage = lastLatchedPercent;

      // --- AUTO MULTIPLIER LOGIC ---
      float multiplier = 1.0;
      if (!isCharging && actualCurrent > 5.0) {
        // Multiplier = Arus Kalibrasi / Arus Sekarang
        multiplier = profile.calDischargeCurrentMA / actualCurrent;
      }

      int rawTime = isCharging ? 
        profile.totalChargeMins * ((100.0 - percentage) / 100.0) : 
        (profile.totalDischargeMins * multiplier) * (percentage / 100.0);
      
      if (lastLatchedTime == -1) lastLatchedTime = rawTime;
      if (rawTime < lastLatchedTime) lastLatchedTime = rawTime;
      time = lastLatchedTime;

      int h = time / 60; int m = time % 60;
      timeStr = String(h) + "h " + String(m) + "m";
      if (percentage == 100 && isCharging) timeStr = "FULL";
      if (percentage == 0 && !isCharging) timeStr = "EMPTY";
    }

    int countLines(const char* fn) {
      File f = LittleFS.open(fn, "r"); if (!f) return 0;
      int c = 0; while (f.available()) if (f.read() == '\n') c++;
      f.close(); return c;
    }

    void buildCurve(const char* fn, float* out, int tot, bool isD) {
       File f = LittleFS.open(fn, "r");
       for (int i = 0; i <= 100; i++) {
          int target = isD ? tot - (i * tot / 100) : (i * tot / 100);
          if (target >= tot) target = tot - 1;
          f.seek(0); int cur = 0;
          while (f.available()) {
             String s = f.readStringUntil('\n');
             if (cur == target) { out[i] = s.toFloat(); break; }
             cur++;
          }
       }
       f.close();
    }
};

#endif