#ifndef DISPLAY_CTE_H
#define DISPLAY_CTE_H

#include <TFT_eSPI.h>
#include <math.h>

class DisplayCTE {
  private:
    TFT_eSprite* sprite;
    
    // Koordinat Geografi
    double latA, lonA;
    double latB, lonB;
    double totalDistanceMeters = 1.0; 
    bool pointsSet = false;

    // State Memory (Simpan kedudukan terakhir)
    float _lastCTE = 0.0;
    float _lastProgress = 0.0; // 0.0 (A) hingga 1.0 (B)

    // Dimensi & Warna
    int x, y, w, h;
    int paddingY = 20; 
    
    uint16_t c_bg, c_rect, c_ABline, c_locPoint, c_CTEline;

    double toRad(double deg) { return deg * M_PI / 180.0; }

    double getDistance(double lat1, double lon1, double lat2, double lon2) {
      double R = 6371000; 
      double dLat = toRad(lat2 - lat1);
      double dLon = toRad(lon2 - lon1);
      double a = sin(dLat/2) * sin(dLat/2) +
                 cos(toRad(lat1)) * cos(toRad(lat2)) * sin(dLon/2) * sin(dLon/2);
      double c = 2 * atan2(sqrt(a), sqrt(1-a));
      return R * c;
    }

    double getBearing(double lat1, double lon1, double lat2, double lon2) {
      double dLon = toRad(lon2 - lon1);
      double y = sin(dLon) * cos(toRad(lat2));
      double x = cos(toRad(lat1)) * sin(toRad(lat2)) -
                 sin(toRad(lat1)) * cos(toRad(lat2)) * cos(dLon);
      return atan2(y, x);
    }

    // Fungsi Dalaman: Lukis Sprite menggunakan nilai yang DISIMPAN dalam memory (_lastCTE, _lastProgress)
    void drawInternal() {
      
      // 1. Reset Background
      sprite->fillSprite(c_bg);
      sprite->drawRect(0, 0, w, h, c_rect);

      // 2. Koordinat Visual
      int centerX = w / 2;
      int visualLenAB = h - (2 * paddingY); 
      int pointA_Y = h - paddingY;          
      int pointB_Y = paddingY;              

      // 3. Lukis Laluan A-B
      sprite->drawLine(centerX, pointA_Y, centerX, pointB_Y, c_ABline);
      sprite->fillCircle(centerX, pointA_Y, 3, c_ABline); 
      sprite->fillCircle(centerX, pointB_Y, 3, c_ABline); 

      // 4. Kira Posisi Y (Guna _lastProgress)
      float progress = _lastProgress;
      if (progress < 0) progress = 0;
      if (progress > 1) progress = 1;
      
      int currentY = pointA_Y - (int)(visualLenAB * progress);

      // 5. Kira Posisi X (Guna _lastCTE dan Skala Realiti)
      // Skala: pixelsPerMeter = PanjangPixel / JarakSebenar
      float pixelsPerMeter = (float)visualLenAB / (float)totalDistanceMeters;
      int ctePixels = (int)(_lastCTE * pixelsPerMeter);

      // Clamp X 
      int maxOffset = (w / 2) - 4;
      int drawOffset = ctePixels;
      if (drawOffset > maxOffset) drawOffset = maxOffset;
      if (drawOffset < -maxOffset) drawOffset = -maxOffset;

      int currentX = centerX + drawOffset;

      // 6. Lukis Garisan CTE (Merah)
      if (abs(drawOffset) > 1) {
        sprite->drawLine(centerX, currentY, currentX, currentY, c_CTEline);
      }

      // 7. Lukis Icon Lokasi
      sprite->fillCircle(currentX, currentY, 5, c_locPoint);

      // 8. Push ke Skrin
      sprite->pushSprite(x, y);
    }

  public:
    DisplayCTE(TFT_eSPI* tft) {
      sprite = new TFT_eSprite(tft);
    }

    void begin() {} // Placeholder

    void setPointA(double lat, double lon) { latA = lat; lonA = lon; }
    
    void setPointB(double lat, double lon) { 
      latB = lat; lonB = lon; 
      totalDistanceMeters = getDistance(latA, lonA, latB, lonB);
      if(totalDistanceMeters < 0.1) totalDistanceMeters = 1.0; 
      pointsSet = true;
    }

    void drawMap(int _x, int _y, int _w, int _h, 
                 uint16_t ABpointColor, uint16_t pointLocationColor, 
                 uint16_t ABlineColor, uint16_t CTELineColor, 
                 uint16_t bgColor, uint16_t rectangleColor) {
      x = _x; y = _y; w = _w; h = _h;
      c_locPoint = pointLocationColor; c_ABline = ABlineColor;
      c_CTEline = CTELineColor; c_bg = bgColor; c_rect = rectangleColor;

      if (sprite->created()) sprite->deleteSprite();
      sprite->setColorDepth(16);
      
      // Semak jika allocation memori berjaya
      void* spritePtr = sprite->createSprite(w, h);
      
      if (spritePtr == nullptr) {
          Serial.println("RALAT OOM: Tidak cukup RAM untuk cipta paparan CTE!");
          return; // Hentikan fungsi jika gagal
      }

      _lastCTE = 0.0;
      _lastProgress = 0.0;
      drawInternal(); 
    }

    void forceRedraw() {
      drawInternal();
    }

    // UPDATE CARA 1: Guna GPS (Auto update CTE & Progress)
    void updateLocation(double currentLat, double currentLon) {
      if (!pointsSet) return;

      double distAC = getDistance(latA, lonA, currentLat, currentLon);
      double bearingAB = getBearing(latA, lonA, latB, lonB);
      double bearingAC = getBearing(latA, lonA, currentLat, currentLon);

      // Simpan nilai baru ke memory
      _lastCTE = distAC * sin(bearingAC - bearingAB);
      double atd = distAC * cos(bearingAC - bearingAB);
      _lastProgress = (float)(atd / totalDistanceMeters);

      drawInternal();
    }

    // UPDATE CARA 2: Manual CTE sahaja (Kekalkan Progress sedia ada)
    // Berguna jika awak cuma nak update error tapi belum ada fix GPS baru untuk jarak
    void updateCTEOnly(float cte) {
      _lastCTE = cte;
      drawInternal();
    }

    // UPDATE CARA 3: Manual semua
    void updateManual(float cte, float progress) {
        _lastCTE = cte;
        _lastProgress = progress;
        drawInternal();
    }

    void clearMemory() {
      // Semak jika sprite wujud di dalam RAM sebelum buang
      if (sprite->created()) {
        sprite->deleteSprite(); 
      }
    }
};

#endif