#ifndef BATTERY_PERCENTAGE_H
#define BATTERY_PERCENTAGE_H

#include <Arduino.h>

class BatteryPercentage {
  private:
    // ==========================================
    // DATA PROFIL KALIBRASI (HARDCODED)
    // ==========================================
    const int TOTAL_DISCHARGE_MINS = 1030;
    const int TOTAL_CHARGE_MINS = 230;
    const float CAL_CURRENT_MA = 131.8961;

    // 51 Titik Voltan (0% hingga 100% dalam gandaan 2%)
    const float DISCHARGE_CURVE[51] = {
      5.93, 6.50, 6.79, 6.82, 6.85, 6.88, 6.96, 7.02, 7.02, 7.05, 
      7.07, 7.08, 7.13, 7.16, 7.16, 7.17, 7.19, 7.20, 7.21, 7.22, 
      7.24, 7.25, 7.29, 7.29, 7.30, 7.39, 7.42, 7.44, 7.47, 7.49, 
      7.52, 7.55, 7.57, 7.60, 7.63, 7.66, 7.68, 7.72, 7.74, 7.77, 
      7.81, 7.84, 7.88, 7.90, 7.94, 7.97, 8.00, 8.03, 8.07, 8.11, 8.16
    };

    const float CHARGE_CURVE[51] = {
      6.51, 6.51, 6.51, 7.20, 7.20, 7.23, 7.23, 7.36, 7.36, 7.36, 
      7.43, 7.43, 7.46, 7.46, 7.49, 7.49, 7.51, 7.51, 7.51, 7.54, 
      7.54, 7.54, 7.54, 7.59, 7.59, 7.62, 7.62, 7.62, 7.66, 7.66, 
      7.70, 7.70, 7.74, 7.74, 7.74, 7.80, 7.80, 7.86, 7.86, 7.92, 
      7.92, 7.98, 7.98, 7.98, 8.08, 8.08, 8.15, 8.15, 8.30, 8.30, 8.36
    };

    // ==========================================
    // PEMBOLEHUBAH SISTEM
    // ==========================================
    unsigned long lastUpdate = 0;
    int lastLatchedPercent = -1;
    int lastLatchedTime = -1;

    // Fungsi Kiraan Peratusan Smooth (Linear Interpolation)
    float getRawPercentage(float voltage, bool charging) {
      const float* curve = charging ? CHARGE_CURVE : DISCHARGE_CURVE;
      
      // Had bawah dan atas
      if (voltage <= curve[0]) return 0.0;
      if (voltage >= curve[50]) return 100.0;

      // Cari kedudukan voltan dalam array
      for (int i = 0; i < 50; i++) {
        if (voltage >= curve[i] && voltage <= curve[i+1]) {
          // Elak error bahagi dengan sifar kalau ada titik data statik
          if (curve[i+1] == curve[i]) return i * 2.0; 
          
          // Formula: Nilai Asas (i * 2%) + Baki perpuluhan
          float pct = (i * 2.0) + 2.0 * ((voltage - curve[i]) / (curve[i+1] - curve[i]));
          return pct;
        }
      }
      return 50.0; // Fallback keselamatan
    }

  public:
    int percentage = 50;
    int time = -1;
    String timeStr = "--";
    bool isCharging = false;

    // Panggil fungsi ni dalam loop utama awak
    void getData(float voltage, float current) {
      unsigned long now = millis();
      float absCurrent = abs(current);
      
      // Penentuan Mode Cas (Negatif = Arus Masuk)
      bool chargingNow = (current < -10.0); 

      // Jika status berubah (Contoh: cucuk/cabut charger)
      // Kita RESET kunci (latch) supaya ia boleh kira semula
      if (chargingNow != isCharging) {
        isCharging = chargingNow;
        // lastLatchedPercent = -1; 
        lastLatchedTime = -1;
      }

      // Update bacaan setiap 5 saat
      if (now - lastUpdate >= 5000) { 
        lastUpdate = now;

        // 1. KIRA PERATUSAN
        float rawPct = getRawPercentage(voltage, isCharging);
        int currentPct = round(rawPct);

        // --- LOGIK ANTI-LOMPAT (STRICT LATCHING) ---
        if (lastLatchedPercent == -1) lastLatchedPercent = currentPct;

        if (isCharging) {
          // Tengah cas: Peratus hanya boleh KEKAL atau NAIK
          if (currentPct >= lastLatchedPercent) lastLatchedPercent = currentPct;
        } else {
          // Tengah guna: Peratus hanya boleh KEKAL atau TURUN
          if (currentPct <= lastLatchedPercent) lastLatchedPercent = currentPct;
        }
        percentage = lastLatchedPercent;


        // 2. KIRA BAKI MASA (COUNTDOWN)
        int rawTime = 0;
        
        if (isCharging) {
          // Kiraan baki masa untuk penuh
          rawTime = TOTAL_CHARGE_MINS * ((100.0 - percentage) / 100.0);
        } else {
          // --- AUTO-MULTIPLIER UNTUK ROVER & BASE ---
          float multiplier = 1.0;
          if (absCurrent > 10.0) { 
            multiplier = CAL_CURRENT_MA / absCurrent;
          } else {
            multiplier = CAL_CURRENT_MA / 10.0; // Hadkan untuk elak jam jadi tak terhingga
          }
          
          rawTime = (TOTAL_DISCHARGE_MINS * multiplier) * (percentage / 100.0);
        }

        // Masa hanya boleh makin sedikit (Strict Countdown)
        if (lastLatchedTime == -1) lastLatchedTime = rawTime;
        if (rawTime < lastLatchedTime) lastLatchedTime = rawTime; 
        time = lastLatchedTime;

        // 3. FORMAT PAPARAN MASA
        int h = time / 60;
        int m = time % 60;
        timeStr = String(h) + "h " + String(m) + "m";
        
        // Override jika penuh/kosong
        if (percentage == 100 && isCharging) timeStr = "FULL";
        if (percentage == 0 && !isCharging) timeStr = "EMPTY";
      }
    }
};

#endif