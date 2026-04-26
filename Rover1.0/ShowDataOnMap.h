#ifndef SHOW_DATA_ON_MAP_H
#define SHOW_DATA_ON_MAP_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <LittleFS.h>
#include <vector>
#include <functional> // Diperlukan untuk fungsi Callback peratusan Loading

extern TFT_eSPI tft; 

// Struktur untuk menyimpan data bentuk yang diekstrak dari KML
struct MapShape {
  String name;
  uint8_t type;     // 0 = Point, 1 = LineString, 2 = Polygon
  uint16_t color;   // Warna RGB565
  String coords;    // "lon,lat|lon,lat..."
};

class MapViewer {
  public:
    // --- PUBLIC VARIABLES ---
    int currentPoint = 0;      
    int totalPoint = 0;        
    int actualPointCount = 0;  // Jumlah khusus untuk Point sahaja
    String pointName = "";     
    float pointLat = 0.0;      
    float pointLon = 0.0;      

    // --- VARIABLE PROGRESS BAHARU ---
    float percentage = 0.0;
    std::function<void(float)> onLoadingProgress = nullptr;

  private:
    std::vector<MapShape> shapes;
    bool _kmlLoaded = false;
    
    // Pembolehubah untuk Auto-Refresh Fail KML
    unsigned long _lastFileCheckTime = 0;
    size_t _lastFileSize = 0;
    time_t _lastFileTime = 0;

    int _lastSelectedPoint = -1;
    bool _forceRedrawNext = false; 

    // --- VARIABLES SKALA (ABSOLUTE DATA) ---
    float _dataMinLon, _dataMaxLon, _dataMinLat, _dataMaxLat;
    bool _dataBoundsCalculated = false;

    // --- VARIABLES SKALA (VIEWPORT / ZOOM) ---
    float _vMinLon, _vMaxLon, _vMinLat, _vMaxLat;
    float _finalScale, _mapScreenH;
    int _startX, _startY;

    // Variable User Location
    float _userLat = 0.0;
    float _userLon = 0.0;
    int _lastUserPx = -1;
    int _lastUserPy = -1;
    bool _hasUserLocation = false;
    bool _userLocMoved = false; 

    // --- VARIABLES TOUCH HANDLING ---
    bool _isTouching = false;
    uint16_t _touchStartX = 0, _touchStartY = 0;
    uint16_t _touchLastX = 0, _touchLastY = 0;
    unsigned long _touchStartTime = 0;
    unsigned long _lastContinuousZoomTime = 0;
    bool _isLongPressing = false;

    // --- FUNGSI BANTUAN PARSE KML ---
    
    uint16_t kmlColorToRGB565(String kmlColor) {
      if (kmlColor.length() < 8) return 0xFFFF; 
      long b = strtol(kmlColor.substring(2, 4).c_str(), NULL, 16);
      long g = strtol(kmlColor.substring(4, 6).c_str(), NULL, 16);
      long r = strtol(kmlColor.substring(6, 8).c_str(), NULL, 16);
      return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }

    void saveShape(String name, int type, uint16_t color, String rawCoords) {
        if (type == -1) return;
        rawCoords.trim();
        String finalCoords = "";
        finalCoords.reserve(rawCoords.length()); 
        
        int len = rawCoords.length();
        bool inSpace = false;
        
        for(int i = 0; i < len; i++) {
            char c = rawCoords[i];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                if (!inSpace && finalCoords.length() > 0) {
                    finalCoords += "|";
                    inSpace = true;
                }
            } else {
                finalCoords += c;
                inSpace = false;
            }
        }
        
        if (finalCoords.endsWith("|")) {
            finalCoords.remove(finalCoords.length() - 1);
        }

        if (finalCoords.length() > 0) {
            MapShape s;
            s.name = name;
            s.type = type;
            s.color = color;
            s.coords = finalCoords;
            shapes.push_back(s);
        }
    }

    // Fungsi pintar mencari Point seterusnya dengan perlindungan Infinite Loop
    int findNextPointIndex(int startIndex, int direction) {
        if (shapes.empty() || actualPointCount == 0) return startIndex;

        int len = shapes.size();
        int idx = startIndex;
        
        for (int i = 0; i < len; i++) {
            idx += direction;
            if (idx >= len) idx = 0;
            if (idx < 0) idx = len - 1;
            
            if (shapes[idx].type == 0) {
                return idx;
            }
        }
        return startIndex; 
    }

    void loadKML() {
      if (!LittleFS.exists("/mission.kml")) {
        Serial.println("[MAP] Fail /mission.kml tidak wujud.");
        shapes.clear();
        totalPoint = 0;
        actualPointCount = 0;
        _kmlLoaded = true;
        _lastFileSize = 0;
        percentage = 0.0;
        return;
      }

      File file = LittleFS.open("/mission.kml", "r");
      if (!file) return;

      _lastFileSize = file.size();
      _lastFileTime = file.getLastWrite();

      shapes.clear();
      shapes.reserve(400); 
      Serial.println("[MAP] Membaca /mission.kml baris demi baris...");

      String currentName = "";
      uint16_t currentColor = 0xFFFF;
      int currentType = -1; 
      String currentCoords = "";
      bool insidePlacemark = false;
      bool insideCoordinates = false;

      std::vector<String> styleIDs;
      std::vector<uint16_t> styleColors;
      bool insideGlobalStyle = false;
      String currentStyleID = "";
      uint16_t currentGlobalColor = 0xFFFF;

      float lastUpdatePct = -1.0; 
      percentage = 0.0;

      while (file.available()) {

        // --- UPDATE PROGRESS LOADING MAP ---
        if (_lastFileSize > 0) {
            percentage = ((float)file.position() / (float)_lastFileSize) * 100.0;
            // Hanya kemas kini UI skrin setiap kenaikan 2% untuk elak lagging teruk
            if (percentage - lastUpdatePct >= 2.0) {
                if (onLoadingProgress) {
                    onLoadingProgress(percentage); 
                }
                lastUpdatePct = percentage;
            }
        }

        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        if (!insidePlacemark) {
            if (line.indexOf("<Style id=") != -1) {
                int idStart = line.indexOf("\"") + 1;
                int idEnd = line.indexOf("\"", idStart);
                if (idStart > 0 && idEnd > idStart) {
                    currentStyleID = "#" + line.substring(idStart, idEnd);
                    insideGlobalStyle = true;
                    currentGlobalColor = 0xFFFF;
                }
            }
            if (insideGlobalStyle) {
                if (line.indexOf("<color>") != -1) {
                    int cStart = line.indexOf("<color>") + 7;
                    if (cStart + 8 <= line.length()) {
                        String colStr = line.substring(cStart, cStart + 8);
                        currentGlobalColor = kmlColorToRGB565(colStr);
                    }
                }
                if (line.indexOf("</Style>") != -1) {
                    styleIDs.push_back(currentStyleID);
                    styleColors.push_back(currentGlobalColor);
                    insideGlobalStyle = false;
                }
            }
        }

        if (line.indexOf("<Placemark") != -1) {
            insidePlacemark = true;
            currentName = "P" + String(shapes.size() + 1);
            currentColor = 0xFFFF; 
            currentType = -1;
        }

        if (insidePlacemark) {
            if (line.indexOf("<name>") != -1) {
                int nStart = line.indexOf("<name>") + 6;
                int nEnd = line.indexOf("</name>");
                if (nEnd != -1) currentName = line.substring(nStart, nEnd);
            }

            if (line.indexOf("<styleUrl>") != -1) {
                int sStart = line.indexOf("<styleUrl>") + 10;
                int sEnd = line.indexOf("</styleUrl>");
                if (sEnd != -1) {
                    String url = line.substring(sStart, sEnd);
                    for (size_t i = 0; i < styleIDs.size(); i++) {
                        if (styleIDs[i] == url) {
                            currentColor = styleColors[i];
                            break;
                        }
                    }
                }
            }

            if (line.indexOf("<color>") != -1) {
                int cStart = line.indexOf("<color>") + 7;
                if (cStart + 8 <= line.length()) {
                    String colStr = line.substring(cStart, cStart + 8);
                    currentColor = kmlColorToRGB565(colStr);
                }
            }

            if (line.indexOf("<Point") != -1) currentType = 0;
            if (line.indexOf("<LineString") != -1) currentType = 1;
            if (line.indexOf("<Polygon") != -1) currentType = 2;

            if (line.indexOf("<coordinates>") != -1) {
                insideCoordinates = true;
                currentCoords = "";
                int start = line.indexOf("<coordinates>") + 13;
                int end = line.indexOf("</coordinates>");

                if (end != -1) {
                    currentCoords = line.substring(start, end);
                    insideCoordinates = false;
                    saveShape(currentName, currentType, currentColor, currentCoords);
                } else {
                    currentCoords = line.substring(start) + " ";
                }
            } 
            else if (insideCoordinates) {
                int end = line.indexOf("</coordinates>");
                if (end != -1) {
                    currentCoords += line.substring(0, end);
                    insideCoordinates = false;
                    saveShape(currentName, currentType, currentColor, currentCoords);
                } else {
                    currentCoords += line + " ";
                }
            }

            if (line.indexOf("</Placemark>") != -1) {
                insidePlacemark = false;
            }
        }
      }
      file.close();

      totalPoint = shapes.size();
      
      actualPointCount = 0;
      for (int i = 0; i < totalPoint; i++) {
          if (shapes[i].type == 0) actualPointCount++;
      }

      currentPoint = 0;
      if (totalPoint > 0 && shapes[0].type != 0) {
          currentPoint = findNextPointIndex(0, 1);
      }

      _kmlLoaded = true;
      _dataBoundsCalculated = false; 
      _forceRedrawNext = true;
      
      // Panggil fungsi sekali lagi untuk paksa ia cecah 100% apabila siap
      if (onLoadingProgress) {
          onLoadingProgress(100.0);
      }

      Serial.printf("[MAP] Selesai. Total Bentuk: %d, Jumlah Titik (Point): %d\n", totalPoint, actualPointCount);
    }

    void calculateDataBounds() {
        bool firstVal = true;
        for (int i = 0; i < totalPoint; i++) {
          const char* str = shapes[i].coords.c_str();
          while (*str) {
              float lon = atof(str);
              while (*str && *str != ',') str++;
              if (*str == ',') str++;
              float lat = atof(str);
              while (*str && *str != '|') str++;
              if (*str == '|') str++;

              if (lon != 0.0 && lat != 0.0) {
                  if (firstVal) {
                    _dataMinLon = lon; _dataMaxLon = lon; _dataMinLat = lat; _dataMaxLat = lat;
                    firstVal = false;
                  } else {
                    if (lon < _dataMinLon) _dataMinLon = lon; if (lon > _dataMaxLon) _dataMaxLon = lon;
                    if (lat < _dataMinLat) _dataMinLat = lat; if (lat > _dataMaxLat) _dataMaxLat = lat;
                  }
              }
          }
        }
        if (_dataMinLon == _dataMaxLon) { _dataMinLon -= 0.0001; _dataMaxLon += 0.0001; }
        if (_dataMinLat == _dataMaxLat) { _dataMinLat -= 0.0001; _dataMaxLat += 0.0001; }
        _dataBoundsCalculated = true;
    }

    void updatePublicVariables() {
        if (totalPoint == 0 || actualPointCount == 0) {
            pointName = "No Data";
            pointLat = 0; pointLon = 0;
            return;
        }

        if (currentPoint >= totalPoint) currentPoint = totalPoint - 1;
        if (currentPoint < 0) currentPoint = 0;

        pointName = shapes[currentPoint].name;
        
        String coordsStr = shapes[currentPoint].coords;
        int c1 = coordsStr.indexOf(',');
        if (c1 != -1) {
            pointLon = coordsStr.substring(0, c1).toFloat();
            int c2 = coordsStr.indexOf(',', c1 + 1);
            int end = coordsStr.indexOf('|');
            if (end == -1) end = coordsStr.length();
            
            if (c2 != -1 && c2 < end) {
                pointLat = coordsStr.substring(c1 + 1, c2).toFloat();
            } else {
                pointLat = coordsStr.substring(c1 + 1, end).toFloat();
            }
        } else {
            pointLat = 0; pointLon = 0;
        }
    }

    // --- FUNGSI AUTO-CENTER ---
    void centerOnCurrentPoint() {
        if (!_dataBoundsCalculated || pointLon == 0.0 || pointLat == 0.0) return;

        float currentW = _vMaxLon - _vMinLon;
        float currentH = _vMaxLat - _vMinLat;

        _vMinLon = pointLon - (currentW / 2.0);
        _vMaxLon = pointLon + (currentW / 2.0);
        _vMinLat = pointLat - (currentH / 2.0);
        _vMaxLat = pointLat + (currentH / 2.0);
    }

  public:

    // --- FUNGSI PENGURUSAN MEMORI ---
    void freeMapMemory() {
        shapes.clear();
        shapes.shrink_to_fit(); 
        
        totalPoint = 0;
        actualPointCount = 0;
        _kmlLoaded = false;
        _dataBoundsCalculated = false;
        percentage = 0.0;
        
        currentPoint = 0;
        _lastSelectedPoint = -1;
        _forceRedrawNext = true;
        
        Serial.println("[MAP] Data KML dipadam dari RAM. Memori berjaya dibebaskan!");
    }

    MapViewer() {}

    void forceRefresh() {
      _forceRedrawNext = true;
    }

    void resetState() {
      _lastSelectedPoint = -1;
      _hasUserLocation = false;
      _dataBoundsCalculated = false;
      _forceRedrawNext = true;
      _kmlLoaded = false; 
      percentage = 0.0;
    }

    // --- FUNGSI NAVIGASI DITAPIS KHUSUS UNTUK POINT SAHAJA ---

    void up() {
      if (actualPointCount > 0) {
        currentPoint = findNextPointIndex(currentPoint, 1);
        updatePublicVariables();
        centerOnCurrentPoint(); 
        _forceRedrawNext = true;
      }
    }

    void down() {
      if (actualPointCount > 0) {
        currentPoint = findNextPointIndex(currentPoint, -1);
        updatePublicVariables();
        centerOnCurrentPoint(); 
        _forceRedrawNext = true;
      }
    }

    void jumpUp() {
      if (actualPointCount > 0) {
        int jumpStep = (int)ceil((float)actualPointCount / 20.0);
        if (jumpStep < 1) jumpStep = 1;
        
        for (int i = 0; i < jumpStep; i++) {
            currentPoint = findNextPointIndex(currentPoint, 1);
        }
        updatePublicVariables();
        centerOnCurrentPoint(); 
        _forceRedrawNext = true;
      }
    }

    void jumpDown() {
      if (actualPointCount > 0) {
        int jumpStep = (int)ceil((float)actualPointCount / 20.0);
        if (jumpStep < 1) jumpStep = 1;
        
        for (int i = 0; i < jumpStep; i++) {
            currentPoint = findNextPointIndex(currentPoint, -1);
        }
        updatePublicVariables();
        centerOnCurrentPoint(); 
        _forceRedrawNext = true;
      }
    }

    // ---------------------------------------------------------

    void resetView() {
        if (!_dataBoundsCalculated) return;
        _vMinLon = _dataMinLon; _vMaxLon = _dataMaxLon;
        _vMinLat = _dataMinLat; _vMaxLat = _dataMaxLat;
        _forceRedrawNext = true;
    }

    void zoomIn() {
        float w = _vMaxLon - _vMinLon; float h = _vMaxLat - _vMinLat;
        float cLon = _vMinLon + (w / 2.0); float cLat = _vMinLat + (h / 2.0);
        w *= 0.7; h *= 0.7; 
        _vMinLon = cLon - w/2; _vMaxLon = cLon + w/2;
        _vMinLat = cLat - h/2; _vMaxLat = cLat + h/2;
        _forceRedrawNext = true;
    }

    void zoomOut() {
        float currentW = _vMaxLon - _vMinLon; 
        float currentH = _vMaxLat - _vMinLat;
        
        float maxW = _dataMaxLon - _dataMinLon;
        float maxH = _dataMaxLat - _dataMinLat;

        if (currentW >= maxW || currentH >= maxH) {
            resetView();
            return;
        }

        float cLon = _vMinLon + (currentW / 2.0); 
        float cLat = _vMinLat + (currentH / 2.0);
        
        float newW = currentW * 1.4; 
        float newH = currentH * 1.4; 
        
        if (newW >= maxW || newH >= maxH) {
            resetView();
            return;
        }

        _vMinLon = cLon - newW/2; 
        _vMaxLon = cLon + newW/2;
        _vMinLat = cLat - newH/2; 
        _vMaxLat = cLat + newH/2;
        _forceRedrawNext = true;
    }

    void panUp()    { float s = (_vMaxLat-_vMinLat)*0.2; _vMinLat+=s; _vMaxLat+=s; _forceRedrawNext=true; }
    void panDown()  { float s = (_vMaxLat-_vMinLat)*0.2; _vMinLat-=s; _vMaxLat-=s; _forceRedrawNext=true; }
    void panRight() { float s = (_vMaxLon-_vMinLon)*0.2; _vMinLon+=s; _vMaxLon+=s; _forceRedrawNext=true; }
    void panLeft()  { float s = (_vMaxLon-_vMinLon)*0.2; _vMinLon-=s; _vMaxLon-=s; _forceRedrawNext=true; }

    void zoomInAtTouch(int touchX, int touchY, int mapX, int mapY, int mapW, int mapH) {
        int rx = touchX - mapX; 
        int ry = touchY - mapY; 
        if (rx < 0 || rx > mapW || ry < 0 || ry > mapH) return; 
        
        float targetLon = _vMinLon + ((float)(rx - _startX) / _finalScale);
        float targetLat = _vMinLat + ((float)(_mapScreenH - (ry - _startY)) / _finalScale);

        float w = _vMaxLon - _vMinLon; float h = _vMaxLat - _vMinLat;
        w *= 0.7; h *= 0.7;
        _vMinLon = targetLon - w/2.0; _vMaxLon = targetLon + w/2.0;
        _vMinLat = targetLat - h/2.0; _vMaxLat = targetLat + h/2.0;
        _forceRedrawNext = true;
    }

    void panByPixels(int dx, int dy) {
        float lonOffset = (float)dx / _finalScale;
        float latOffset = (float)dy / _finalScale;
        _vMinLon -= lonOffset; _vMaxLon -= lonOffset;
        _vMinLat += latOffset; _vMaxLat += latOffset;
        _forceRedrawNext = true;
    }

    // --- FUNGSI MENGURUSKAN SENTUHAN (TOUCH HANDLER) ---
    void handleTouch(bool touched, uint16_t t_x, uint16_t t_y, int mapX, int mapY, int mapW, int mapH) {
        unsigned long currentMillis = millis();

        if (touched) {
            if (!_isTouching) {
                if (t_x >= mapX && t_x <= (mapX + mapW) && t_y >= mapY && t_y <= (mapY + mapH)) {
                    _isTouching = true;
                    _touchStartX = t_x;
                    _touchStartY = t_y;
                    _touchLastX = t_x;
                    _touchLastY = t_y;
                    _touchStartTime = currentMillis;
                    _isLongPressing = false;
                }
            } else {
                _touchLastX = t_x;
                _touchLastY = t_y;

                int dx = _touchLastX - _touchStartX;
                int dy = _touchLastY - _touchStartY;
                int distance = sqrt((dx * dx) + (dy * dy));

                if (distance < 25) {
                    if (!_isLongPressing && (currentMillis - _touchStartTime > 600)) {
                        _isLongPressing = true;
                        _lastContinuousZoomTime = currentMillis;
                        zoomOut();
                        Serial.println("[MAP] Aksi: LONG PRESS (Zoom Out Mula)");
                    } 
                    else if (_isLongPressing && (currentMillis - _lastContinuousZoomTime > 300)) {
                        _lastContinuousZoomTime = currentMillis;
                        zoomOut();
                        Serial.println("[MAP] Aksi: LONG PRESS (Zoom Out Berterusan)");
                    }
                } else {
                    _isLongPressing = false;
                }
            }
        } 
        else {
            if (_isTouching) {
                _isTouching = false;

                if (!_isLongPressing) {
                    int dx = _touchLastX - _touchStartX;
                    int dy = _touchLastY - _touchStartY;
                    int distance = sqrt((dx * dx) + (dy * dy));

                    if (distance < 25) {
                        zoomInAtTouch(_touchStartX, _touchStartY, mapX, mapY, mapW, mapH);
                        Serial.println("[MAP] Aksi: TAP (Zoom In)");
                    } else {
                        panByPixels(dx, dy);
                        Serial.println("[MAP] Aksi: SWIPE (Pan Map)");
                    }
                }
            }
        }
    }

    void updateLocation(float userLat, float userLon) {
      if (_hasUserLocation && _userLat == userLat && _userLon == userLon) return; 
      _userLat = userLat;
      _userLon = userLon;
      _hasUserLocation = true;
      _userLocMoved = true; 
    }

    void drawMissionMap(int x, int y, int w, int h, int padding, 
                        uint16_t boxColor, uint16_t pointColor, uint16_t selectedColor, 
                        uint16_t userColor, uint16_t bgColor) {
      
      unsigned long currentMillis = millis();
      if (currentMillis - _lastFileCheckTime >= 1000) {
          _lastFileCheckTime = currentMillis;
          if (LittleFS.exists("/mission.kml")) {
              File f = LittleFS.open("/mission.kml", "r");
              if (f) {
                  size_t currentSize = f.size();
                  time_t currentWrite = f.getLastWrite();
                  f.close();
                  if (currentSize != _lastFileSize || currentWrite != _lastFileTime) {
                      _kmlLoaded = false; 
                  }
              }
          } else if (_lastFileSize != 0) {
              _kmlLoaded = false;
          }
      }

      if (!_kmlLoaded) {
          loadKML();
          updatePublicVariables();
      }

      if (totalPoint == 0) {
          if (_forceRedrawNext) {
              tft.fillRect(x, y, w, h, bgColor);   
              tft.drawRect(x, y, w, h, boxColor);
              _forceRedrawNext = false;
          }
          return;
      }

      if (!_dataBoundsCalculated) {
          calculateDataBounds();
          resetView(); 
      }

      bool selectionChanged = (currentPoint != _lastSelectedPoint);
      if (selectionChanged) {
          _forceRedrawNext = true; 
      }
      
      bool mustRedraw = _forceRedrawNext;

      if (!mustRedraw && !selectionChanged && !_userLocMoved) {
        return; 
      }

      if (mustRedraw) {
         tft.fillRect(x, y, w, h, bgColor);   
         tft.drawRect(x, y, w, h, boxColor);  
         _forceRedrawNext = false; 
      }

      _lastSelectedPoint = currentPoint;

      tft.setViewport(x, y, w, h);

      float viewW = _vMaxLon - _vMinLon;
      float viewH = _vMaxLat - _vMinLat;

      int availW = w - (2 * padding);
      int availH = h - (2 * padding);
      
      float scaleX = (float)availW / viewW;
      float scaleY = (float)availH / viewH;
      _finalScale = (scaleX < scaleY) ? scaleX : scaleY; 

      float screenDataW = viewW * _finalScale;
      _mapScreenH = viewH * _finalScale;

      _startX = padding + (availW - screenDataW) / 2;
      _startY = padding + (availH - _mapScreenH) / 2;

      if (!mustRedraw && _userLocMoved && _lastUserPx != -1) {
          tft.fillCircle(_lastUserPx, _lastUserPy, 3, bgColor);
      }

      bool partialRestore = (!mustRedraw && !selectionChanged && _userLocMoved);

      for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < totalPoint; i++) {
          uint8_t type = shapes[i].type; 
          
          if (pass == 0 && type == 0) continue; 
          if (pass == 1 && type != 0) continue; 

          uint16_t shapeColor = shapes[i].color;
          if (type == 0 && shapeColor == 0xFFFF) shapeColor = pointColor; 
          if (i == currentPoint) shapeColor = selectedColor;

          int prevPx = -1, prevPy = -1;
          int firstPx = -1, firstPy = -1;

          const char* str = shapes[i].coords.c_str();
          while (*str) {
              float lon = atof(str);
              while (*str && *str != ',') str++;
              if (*str == ',') str++;
              
              float lat = atof(str);
              while (*str && *str != '|') str++;
              if (*str == '|') str++;

              if (lon == 0.0 && lat == 0.0) continue;

              int px = _startX + (int)((lon - _vMinLon) * _finalScale);
              int py = _startY + (int)(_mapScreenH - ((lat - _vMinLat) * _finalScale));

              if (type == 0) { 
                  if (partialRestore) {
                      if (abs(px - _lastUserPx) <= 4 && abs(py - _lastUserPy) <= 4) tft.fillCircle(px, py, 2, shapeColor);
                  } else {
                      tft.fillCircle(px, py, 2, shapeColor);
                  }
              } else { 
                  if (prevPx != -1) {
                      if (partialRestore) {
                          int minX = min(prevPx, px) - 4; int maxX = max(prevPx, px) + 4;
                          int minY = min(prevPy, py) - 4; int maxY = max(prevPy, py) + 4;
                          if (_lastUserPx >= minX && _lastUserPx <= maxX && _lastUserPy >= minY && _lastUserPy <= maxY) {
                              tft.drawLine(prevPx, prevPy, px, py, shapeColor);
                          }
                      } else {
                          tft.drawLine(prevPx, prevPy, px, py, shapeColor);
                      }
                  } else {
                      firstPx = px; firstPy = py;
                  }
                  prevPx = px; prevPy = py;
              }
          }

          if (type == 2 && firstPx != -1 && prevPx != -1) { 
              if (partialRestore) {
                  int minX = min(prevPx, firstPx) - 4; int maxX = max(prevPx, firstPx) + 4;
                  int minY = min(prevPy, firstPy) - 4; int maxY = max(prevPy, firstPy) + 4;
                  if (_lastUserPx >= minX && _lastUserPx <= maxX && _lastUserPy >= minY && _lastUserPy <= maxY) {
                      tft.drawLine(prevPx, prevPy, firstPx, firstPy, shapeColor);
                  }
              } else {
                  tft.drawLine(prevPx, prevPy, firstPx, firstPy, shapeColor);
              }
          }
        }
      }

      if (_hasUserLocation) {
         int px = _startX + (int)((_userLon - _vMinLon) * _finalScale);
         int py = _startY + (int)(_mapScreenH - ((_userLat - _vMinLat) * _finalScale));
         tft.fillCircle(px, py, 3, userColor); 
         _lastUserPx = px;
         _lastUserPy = py;
         _userLocMoved = false; 
      }

      tft.resetViewport();
    }
};

#endif