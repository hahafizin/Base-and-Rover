#ifndef DRAWMAP_H
#define DRAWMAP_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TinyGPS++.h>

// Pastikan objek global tft dan gps dari main file boleh diakses oleh class ini
extern TFT_eSPI tft;
extern TinyGPSPlus gps;

class DrawMap {
  private:
    int _x, _y, _r;
    uint16_t _mapColor, _bgColor;
    int _lastR;

    // Tetapan memori laluan (history)
    static const int MAX_POINTS = 500;
    double historyLat[MAX_POINTS];
    double historyLon[MAX_POINTS];
    int historyCount;

    // Objek Sprite untuk lukisan peta
    TFT_eSprite mapSprite;

  public:
    // --- CONSTRUCTOR ---
    DrawMap(int x, int y, int r, uint16_t mapColor, uint16_t bgColor) 
      : mapSprite(&tft) {
      _x = x;
      _y = y;
      _r = r;
      _mapColor = mapColor;
      _bgColor = bgColor;
      
      _lastR = 0;
      historyCount = 0;
    }

    // --- DESTRUCTOR ---
    ~DrawMap() {
      mapSprite.deleteSprite();
    }

    // --- FUNCTION: CLEAR HISTORY ---
    void clear() {
      historyCount = 0; 
    }

    // --- FUNCTION: SHOW MAP ---
    void show(double latA, double lonA, double latB, double lonB, double curLat, double curLon, double pointDistance) {
      
      // --------------------------------------------------------
      // BAHAGIAN A: SARINGAN & KEMASKINI DATA LALUAN (HISTORY)
      // --------------------------------------------------------
      if (historyCount == 0) {
        historyLat[0] = curLat;
        historyLon[0] = curLon;
        historyCount++;
      } else {
        double distFromLast = gps.distanceBetween(
                                historyLat[historyCount - 1], historyLon[historyCount - 1], 
                                curLat, curLon);
                                
        if (distFromLast >= pointDistance) {
          if (historyCount < MAX_POINTS) {
            historyLat[historyCount] = curLat;
            historyLon[historyCount] = curLon;
            historyCount++;
          } else {
            for (int i = 1; i < MAX_POINTS; i++) {
              historyLat[i - 1] = historyLat[i];
              historyLon[i - 1] = historyLon[i];
            }
            historyLat[MAX_POINTS - 1] = curLat;
            historyLon[MAX_POINTS - 1] = curLon;
          }
        }
      }

      // --------------------------------------------------------
      // BAHAGIAN B: SETUP SPRITE & MATEMATIK SKALA (ZOOM)
      // --------------------------------------------------------
      int padding = 4; 
      int spriteW = (_r * 2) + (padding * 2);
      
      if (_lastR != _r) {
        if (_lastR != 0) mapSprite.deleteSprite();
        mapSprite.setColorDepth(8); 
        mapSprite.createSprite(spriteW, spriteW);
        _lastR = _r;
      }
      
      mapSprite.fillSprite(_bgColor);
      
      int cx = spriteW / 2;
      int cy = spriteW / 2;

      double distAB = gps.distanceBetween(latA, lonA, latB, lonB);
      double bearingAB = gps.courseTo(latA, lonA, latB, lonB);
      
      double pxPerMeter = (distAB > 0) ? ((double)_r / distAB) : 1.0; 

      // --------------------------------------------------------
      // BAHAGIAN C: LUKIS MAP, TITIK A, DAN TITIK B
      // --------------------------------------------------------
      mapSprite.drawCircle(cx, cy, _r, _mapColor);
      mapSprite.fillCircle(cx, cy, 4, _mapColor);
      
      int bx = cx + _r * sin(radians(bearingAB));
      int by = cy - _r * cos(radians(bearingAB));
      mapSprite.fillCircle(bx, by, 4, _mapColor);

      // --------------------------------------------------------
      // BAHAGIAN D: LUKIS LALUAN GPS (TRAIL)
      // --------------------------------------------------------
      int prevX = -1, prevY = -1;
      
      for (int i = 0; i < historyCount; i++) {
        double pLat = historyLat[i];
        double pLon = historyLon[i];
        
        double d = gps.distanceBetween(latA, lonA, pLat, pLon);
        double b = gps.courseTo(latA, lonA, pLat, pLon);
        
        double pxDist = d * pxPerMeter;
        
        int pX = cx + pxDist * sin(radians(b));
        int pY = cy - pxDist * cos(radians(b));

        if (i > 0) {
          mapSprite.drawLine(prevX, prevY, pX, pY, _mapColor);
        }
        prevX = pX;
        prevY = pY;
      }

      // --------------------------------------------------------
      // BAHAGIAN E: LUKIS TITIK LOKASI SEMASA (CURRENT GPS)
      // --------------------------------------------------------
      // 1. Kira jarak dan bearing lokasi semasa dari Titik A (pusat peta)
      double distCur = gps.distanceBetween(latA, lonA, curLat, curLon);
      double bearingCur = gps.courseTo(latA, lonA, curLat, curLon);
      
      // 2. Tukar kepada jarak piksel
      double pxDistCur = distCur * pxPerMeter;
      
      // 3. Cari kordinat (X,Y) dalam sprite
      int curX = cx + pxDistCur * sin(radians(bearingCur));
      int curY = cy - pxDistCur * cos(radians(bearingCur));

      // 4. Lukis titik lokasi semasa (radius 4px)
      // Nota: Anda boleh tukar '_mapColor' kepada 'TFT_RED' atau warna lain 
      // supaya titik ini lebih menonjol dari garisan laluan.
      mapSprite.fillCircle(curX, curY, 6, _mapColor);

      // --------------------------------------------------------
      // BAHAGIAN F: TOLAK SPRITE KE SKRIN UTAMA
      // --------------------------------------------------------
      mapSprite.pushSprite(_x - cx, _y - cy);
    }
};

#endif