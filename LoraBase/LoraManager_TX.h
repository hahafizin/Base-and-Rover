#ifndef LORA_MANAGER_TX_H
#define LORA_MANAGER_TX_H

#include <Arduino.h>
#include <LoRa.h>

// --- STRUKTUR PENAPIS KALMAN ---
struct SimpleKalmanFilter {
  double err_measure;
  double err_estimate;
  double q;
  double current_estimate;
  double last_estimate;
  double kalman_gain;

  SimpleKalmanFilter(double mea_e, double est_e, double q_val) {
    err_measure = mea_e;
    err_estimate = est_e;
    q = q_val;
    current_estimate = 0;
    last_estimate = 0;
  }

  double updateEstimate(double mea) {
    if (last_estimate == 0.0) {
      last_estimate = mea;
      current_estimate = mea;
      return mea;
    }
    kalman_gain = err_estimate / (err_estimate + err_measure);
    current_estimate = last_estimate + kalman_gain * (mea - last_estimate);
    err_estimate = (1.0 - kalman_gain) * err_estimate + fabs(last_estimate - current_estimate) * q;
    last_estimate = current_estimate;
    return current_estimate;
  }
};

// --- STRUKTUR DATA BINARI (JIMAT SAIZ) ---
struct __attribute__((packed)) TelemetryPacket {
  char type;          // 'T' (1 byte)
  uint8_t flags;      // Bit 0: Fix, Bit 1: IsCalibrating, Bit 2: IsCalDone (1 byte)
  uint8_t hdop;       
  uint8_t battery;    
  int16_t alt;        
  int32_t lat;        
  int32_t lon;        
};

class LoraManager_TX {
  private:
    unsigned long lastTxTime = 0;
    unsigned long lastAckTime = 0;
    
    unsigned long txInterval = 5000;  // Tetap 5 saat untuk Long Range
    unsigned long currentTimeout = 15000; // 3x interval
    unsigned long _statusTimer = 0;

    bool _hasReceivedFirstAck = false;

    // --- VARIABEL KALIBRASI (BASE) ---
    bool _isCalibrating = false;
    bool _isCalDone = false;
    unsigned long _calStartTime = 0;
    double _latSum = 0.0;
    double _lonSum = 0.0;
    int _calCount = 0;
    double _avgLat = 0.0;
    double _avgLon = 0.0;

    // --- KALMAN FILTER (HEAVY) UNTUK BASE ---
    SimpleKalmanFilter baseLatKF{0.00002, 0.00002, 0.000001};
    SimpleKalmanFilter baseLonKF{0.00002, 0.00002, 0.000001};

  public:
    bool isConnected = false;
    int RSSI = 0; 
    bool status = false;

    void begin() {
      // --- HARDCODE: EXTREME LONG RANGE ---
      LoRa.setTxPower(20);            
      LoRa.setSpreadingFactor(12);    
      LoRa.setSignalBandwidth(125E3); 
      LoRa.setCodingRate4(8);         
      
      _hasReceivedFirstAck = false;
      status = false; 
    }

    void run(bool isgpsfix, float hdop, int batteryPercentage, float currentAlt, double currentLat, double currentLon) {
      if (status && (millis() - _statusTimer >= 100)) status = false;

      // --- 1. TERIMA ACK & ARAHAN DARI ROVER ---
      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        String incoming = "";
        while (LoRa.available()) incoming += (char)LoRa.read();
        
        // Logik Standard ACK: Protokol disederhanakan ("A,-95" / "C,-95" / "R,-95")
        if (incoming.startsWith("A,") || incoming.startsWith("C,") || incoming.startsWith("R,")) {
          int firstComma = incoming.indexOf(',');

          if (firstComma != -1) {
            lastAckTime = millis();
            _hasReceivedFirstAck = true;
            status = true;
            _statusTimer = millis();

            // Baca nilai RSSI terus selepas koma pertama
            RSSI = incoming.substring(firstComma + 1).toInt();
          }
        } 
        
        // --- TERIMA ARAHAN CALIBRATE ('C') DARI ROVER ---
        if (incoming.startsWith("C,")) {
             Serial.println("\n[TX] *** CALIBRATION COMMAND RECEIVED! Starting 5-Min Average ***");
             _isCalibrating = true;
             _isCalDone = false;
             _calStartTime = millis();
             _latSum = 0.0; _lonSum = 0.0; _calCount = 0;
        }

        // --- TERIMA ARAHAN RESTART ('R') ---
        if (incoming.startsWith("R,")) {
             delay(150); LoRa.beginPacket(); LoRa.print("R,CONFIRM"); LoRa.endPacket(); 
             delay(500); ESP.restart(); 
        }
      }

      if (!_hasReceivedFirstAck || (millis() - lastAckTime > currentTimeout)) {
        isConnected = false;
      } else {
        isConnected = true;
      }

      // --- 2. PENAPIS KALMAN & AVERAGING LOGIC ---
      double filteredLat = 0.0;
      double filteredLon = 0.0;
      
      if (currentLat != 0.0 && currentLon != 0.0) {
          filteredLat = baseLatKF.updateEstimate(currentLat);
          filteredLon = baseLonKF.updateEstimate(currentLon);
      }

      if (_isCalibrating) {
          if (isgpsfix && filteredLat != 0.0) {
              _latSum += filteredLat;
              _lonSum += filteredLon;
              _calCount++;
          }
          
          if (millis() - _calStartTime >= 300000) { // 5 Minit
              _isCalibrating = false;
              _isCalDone = true;
              if (_calCount > 0) {
                  _avgLat = _latSum / _calCount;
                  _avgLon = _lonSum / _calCount;
                  Serial.println("[TX] 5-Min Averaging Done! New Base Locked.");
              } else {
                  _avgLat = filteredLat; // Fallback kalau tiada data
                  _avgLon = filteredLon;
                  Serial.println("[TX] Averaging Failed (No GPS Fix). Using last known.");
              }
          }
      }

      // --- 3. HANTAR DATA ---
      if (millis() - lastTxTime >= txInterval) {
        lastTxTime = millis();

        TelemetryPacket packet;
        packet.type = 'T';
        
        packet.flags = (isgpsfix ? 1 : 0) | (_isCalibrating ? 2 : 0) | (_isCalDone ? 4 : 0);
        packet.hdop = (uint8_t)(hdop * 10); 
        packet.battery = (uint8_t)batteryPercentage;
        packet.alt = (int16_t)currentAlt;
        
        if (_isCalDone) {
            packet.lat = (int32_t)(_avgLat * 1000000.0);
            packet.lon = (int32_t)(_avgLon * 1000000.0);
        } else {
            packet.lat = (int32_t)(filteredLat * 1000000.0); 
            packet.lon = (int32_t)(filteredLon * 1000000.0);
        }

        if (LoRa.beginPacket() != 0) {
          LoRa.write((uint8_t*)&packet, sizeof(packet));
          LoRa.endPacket();
        }
      }
    }
};

#endif