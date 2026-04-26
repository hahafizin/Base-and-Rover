#ifndef LORA_MANAGER_RX_H
#define LORA_MANAGER_RX_H

#include <Arduino.h>
#include <LoRa.h>
#include <Preferences.h>
#include <math.h>

struct __attribute__((packed)) TelemetryPacket {
  char type;          
  uint8_t flags;      
  uint8_t hdop;       
  uint8_t battery;    
  int16_t alt;        
  int32_t lat;        
  int32_t lon;        
};

struct Coordinate {
  double lat;
  double lon;
};

struct DistanceData {
  double cm;
  double m;
  double km;
};

class LoraManager_RX {
  private:
    Preferences preferences;
    float altOffset = 0.0;
    
    // --- VARIABEL BARU: KOORDINAT BASE YANG DISIMPAN DALAM RAM ---
    double savedBaseLat = 0.0;
    double savedBaseLon = 0.0;

    // --- VARIABEL UNTUK MANUAL OFFSET ROVER ---
    double _userLatOffset = 0.0;
    double _userLonOffset = 0.0;
    
    unsigned long lastRxTime = 0;
    unsigned long currentTimeout = 20000; 
    
    bool _pendingRestart = false;       
    bool _pendingCalibration = false;   

    bool _lastIsConnected = false; 
    bool _hasReceivedFirstPacket = false; 
    unsigned long _statusTimer = 0;
    unsigned long _roverCalStartTime = 0;

    // --- VARIABEL UNTUK PRESSURE TREND ---
    float _lastPressure = 0.0;
    unsigned long _lastPressureTime = 0;
    int _currentTrend = 0;

    double calculateFlatEarthPythagoras(double lat1, double lon1, double lat2, double lon2) {
      double deltaY = (lat2 - lat1) * 111320.0;
      double deltaX = (lon2 - lon1) * 111320.0 * cos(lat1 * PI / 180.0);
      return sqrt((deltaX * deltaX) + (deltaY * deltaY)); 
    }

    void forceSyncPacket() {
      if (LoRa.beginPacket() != 0) {
        LoRa.print("A,"); LoRa.print(RSSI); 
        LoRa.endPacket(); 
      }
      delay(150);
    }

    void sendAck() {
      digitalWrite(1, HIGH);
      if (LoRa.beginPacket() != 0) {
        if (_pendingRestart) LoRa.print("R,"); 
        else if (_pendingCalibration) LoRa.print("C,");
        else LoRa.print("A,");
        
        LoRa.print(RSSI); 
        LoRa.endPacket(true); 
        
        if (_pendingRestart) _pendingRestart = false; 
        if (_pendingCalibration) _pendingCalibration = false; 
      }
      digitalWrite(1, LOW);
    }

  public:
    float taredAlt = 0.0; float rxAlt = 0.0; float txAlt = 0.0;
    Coordinate rxCoordinate = {0.0, 0.0};
    Coordinate txCoordinate = {0.0, 0.0};
    DistanceData getDistance = {0.0, 0.0, 0.0};
    
    int averagingStatus = 0; 
    int averagingCountdown = 0; 

    int RSSI = 0; float SNR = 0.0;
    bool isConnected = false; bool status = false; 
    bool hasBeenConnected = false;
    
    // --- FLAG MANUAL OFFSET ---
    bool isOffsetDone = false; 

    unsigned long lastConnected = 0; 
    int baseBatteryLevel = 0; bool isGpsBaseFix = false; float baseHdop = 0.0; 

    void begin() {
      Serial.println("[RX] Initializing LoraManager_RX...");
      preferences.begin("lora-rx", true); 
      altOffset = preferences.getFloat("altOff", 0.0);
      
      // Load koordinat Base terus ke RAM semasa boot
      savedBaseLat = preferences.getDouble("baseLat", 0.0);
      savedBaseLon = preferences.getDouble("baseLon", 0.0);
      preferences.end();
      
      _pendingRestart = false; 
      _pendingCalibration = false;
      status = false;
      hasBeenConnected = false;
      averagingStatus = 0; 
      averagingCountdown = 0;

      _lastPressure = 0.0;
      _lastPressureTime = millis();
      _currentTrend = 0;

      // Reset Manual Offset ke 0 setiap kali restart
      _userLatOffset = 0.0;
      _userLonOffset = 0.0;
      isOffsetDone = false;

      LoRa.setTxPower(20); 
      LoRa.setSpreadingFactor(12); 
      LoRa.setSignalBandwidth(125E3); 
      LoRa.setCodingRate4(8);

      Serial.println("\n[RX] --- STARTING SYNC (LONG RANGE MODE) ---");
      forceSyncPacket();
    }

    void calibrate() {
        _pendingCalibration = true;
        averagingStatus = 0; 
        Serial.println("[RX] >>> CMD: Sending Calibration Request to Base...");
    }

    int pressureTrend(float currentPressure) {
        if (_lastPressure == 0.0) {
            _lastPressure = currentPressure;
            _lastPressureTime = millis();
            _currentTrend = 0;
            return _currentTrend;
        }

        if (millis() - _lastPressureTime >= 1200000) {
            float bezaTekanan = currentPressure - _lastPressure;
            
            if (bezaTekanan > 0.5) {
                _currentTrend = 1;  
            } else if (bezaTekanan < -0.5) {
                _currentTrend = -1; 
            } else {
                _currentTrend = 0;  
            }

            _lastPressure = currentPressure;
            _lastPressureTime = millis();
        }
        
        return _currentTrend;
    }

    // --- FUNGSI BARU: MANUAL OFFSET COORDINATE ---
    void addOffset(double lat, double lon) {
        _userLatOffset += lat;
        _userLonOffset += lon;
        isOffsetDone = true;
        Serial.println("[RX] Manual offset applied to Rover coordinates.");
    }

    // --- FUNGSI BARU: BUANG OFFSET & KEMBALI KE ASAL ---
    void removeOffset() {
        if (isOffsetDone) {
            // Tolak nilai offset dari koordinat semasa dengan serta-merta
            rxCoordinate.lat -= _userLatOffset;
            rxCoordinate.lon -= _userLonOffset;
            
            // Sifarkan semula memori offset
            _userLatOffset = 0.0;
            _userLonOffset = 0.0;
            
            // Tutup flag
            isOffsetDone = false;
            Serial.println("[RX] Manual offset removed. Reverted to raw GPS coordinates.");
        }
    }

    void run(float currentRxAlt, double currentRxLat, double currentRxLon) {
      rxAlt = currentRxAlt; 

      // --- MENGGUNAKAN DATA GPS + MANUAL OFFSET ---
      if (currentRxLat != 0.0 && currentRxLon != 0.0) {
          rxCoordinate.lat = currentRxLat + _userLatOffset;
          rxCoordinate.lon = currentRxLon + _userLonOffset;
      }

      taredAlt = (rxAlt - txAlt) - altOffset;

      // --- PENGIRAAN JARAK LIVE (Rover GPS vs Memori RAM) ---
      if (rxCoordinate.lat != 0.0 && savedBaseLat != 0.0) {
          double rawDistanceInMeters = calculateFlatEarthPythagoras(rxCoordinate.lat, rxCoordinate.lon, savedBaseLat, savedBaseLon);
          
          if (rawDistanceInMeters <= 50000.0) {
              getDistance.m = rawDistanceInMeters;
              getDistance.cm = rawDistanceInMeters * 100.0;
              getDistance.km = rawDistanceInMeters / 1000.0;
          }
      }

      if (averagingStatus == 1) {
          unsigned long elapsed = millis() - _roverCalStartTime;
          if (elapsed < 300000) {
              averagingCountdown = (300000 - elapsed) / 1000;
          } else {
              averagingCountdown = 0;
          }
      }

      unsigned long timeSinceLastRx = millis() - lastRxTime;
      if (!_hasReceivedFirstPacket || timeSinceLastRx > currentTimeout) {
        isConnected = false; _pendingRestart = false; _pendingCalibration = false;
        lastConnected = (!_hasReceivedFirstPacket) ? millis() / 1000 : timeSinceLastRx / 1000;
        if (_lastIsConnected) { Serial.println("[RX] *** CONNECTION LOST! ***"); _lastIsConnected = false; }
      } else {
        isConnected = true; lastConnected = 0; 
        if (!hasBeenConnected) hasBeenConnected = true;
        if (!_lastIsConnected) { Serial.println("[RX] *** CONNECTED TO TX! ***"); _lastIsConnected = true; }
      }

      if (status && (millis() - _statusTimer >= 100)) status = false;

      int packetSize = LoRa.parsePacket();
      if (packetSize) {
        if (packetSize == sizeof(TelemetryPacket)) {
            TelemetryPacket packet;
            LoRa.readBytes((uint8_t*)&packet, sizeof(packet));

            if (packet.type == 'T') {
                _hasReceivedFirstPacket = true;
                status = true; _statusTimer = millis(); lastRxTime = millis(); 

                isGpsBaseFix = (packet.flags & 0x01);
                bool baseIsCalibrating = (packet.flags & 0x02) != 0;
                bool baseIsCalDone = (packet.flags & 0x04) != 0;

                baseBatteryLevel = packet.battery;
                RSSI = LoRa.packetRssi();
                SNR = LoRa.packetSnr();

                double tempTxLat = packet.lat / 1000000.0;
                double tempTxLon = packet.lon / 1000000.0;

                if (isGpsBaseFix && tempTxLat != 0.0 && tempTxLon != 0.0) {
                    baseHdop = packet.hdop / 10.0;
                    txAlt = packet.alt;
                    txCoordinate.lat = tempTxLat;
                    txCoordinate.lon = tempTxLon;
                }

                if (baseIsCalibrating) {
                    if (averagingStatus != 1) {
                        averagingStatus = 1;
                        _roverCalStartTime = millis(); 
                        Serial.println("[RX] Base confirms 5-Min Averaging started!");
                    }
                } 
                else if (baseIsCalDone) {
                    if (averagingStatus == 1) { 
                        averagingStatus = 2;
                        averagingCountdown = 0;

                        altOffset = rxAlt - txAlt;
                        taredAlt = 0.0;
                        preferences.begin("lora-rx", false); 
                        preferences.putFloat("altOff", altOffset);
                        preferences.end();
                        
                        saveBasePosition();

                        Serial.println("\n[RX] <<< Base Sync Complete! Base Coordinates Locked.");
                    } else if (averagingStatus == 0) {
                        averagingStatus = 2;
                    }
                } else {
                    averagingStatus = 0;
                    averagingCountdown = 0;
                }

                sendAck();
            }
        } 
        else {
          String incoming = "";
          while (LoRa.available()) incoming += (char)LoRa.read();
          if (incoming.startsWith("R,CONFIRM")) {
              _pendingRestart = false; isConnected = false; status = false;
              lastRxTime = millis() - currentTimeout - 1000; 
          }
        }
      }
    }

    void restartBase() { _pendingRestart = true; Serial.println("[RX] CMD: RESTART BASE"); }

    void saveBasePosition() {
        if (txCoordinate.lat != 0.0 && txCoordinate.lon != 0.0) {
            savedBaseLat = txCoordinate.lat;
            savedBaseLon = txCoordinate.lon;
            
            preferences.begin("lora-rx", false); 
            preferences.putDouble("baseLat", savedBaseLat);
            preferences.putDouble("baseLon", savedBaseLon);
            preferences.end();
            
            Serial.println("[RX] Base coordinates saved successfully to Preferences.");
        } else {
            Serial.println("[RX] Error: Cannot save Base coordinates. No TX GPS Fix.");
        }
    }
};

#endif