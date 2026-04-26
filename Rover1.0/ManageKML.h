#ifndef MANAGE_KML_H
#define MANAGE_KML_H

#include <Arduino.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <vector>
#include <math.h>

#define ICON_START      "\uf04b" // Tempat Mula (Play)
#define ICON_MARK       "\uf08d" // Tanda Placemark (Thumbtack)
#define ICON_FINISH     "\uf11e" // Tempat Akhir (Checkered Flag)

// SILA TUKAR NAMA FAIL INI KEPADA NAMA FAIL HEADER FONT ANDA SEBENAR
// Contoh: #include "Icons8.h" 
// Pastikan tatasusunan Icons8 dan ICON_START, ICON_MARK, ICON_FINISH boleh dibaca di sini.

extern const uint8_t Icons8[]; // Beritahu compiler bahawa array ini wujud

struct KMLCoord {
    double lon;
    double lat;
};

struct PathData {
    String name;
    size_t startPos; 
    size_t endPos;   
    bool isDeleted;
};

struct KMLDistanceData {
    double m;
    double km;
};

struct GeometryInfo {
    size_t startPos;
    size_t endPos;
    int count; 
    KMLCoord firstCoord;
    KMLCoord lastCoord;
};

class ManageKML {
private:
    TFT_eSPI* tft;
    String currentFilename;
    std::vector<PathData> paths;
    std::vector<GeometryInfo> geometries; 
    
    int selectedIndex = 0;
    bool fileModified = false;
    
    bool _needMapUpdate = true; 
    bool _borderDrawn = false;

    double mapMinLon, mapMaxLon, mapMinLat, mapMaxLat;
    double mapScale, mapCosLat;
    int mapOffsetX, mapOffsetY, mapW, mapH, mapX, mapY;

    int findInFile(File &f, const char* target) {
        int targetLen = strlen(target);
        int matchIndex = 0;
        while (f.available()) {
            char c = f.read();
            if (c == target[matchIndex]) {
                matchIndex++;
                if (matchIndex == targetLen) return f.position();
            } else {
                if (c == target[0]) matchIndex = 1;
                else matchIndex = 0;
            }
        }
        return -1;
    }

    bool readNextCoord(File &f, size_t endLimit, double &lon, double &lat) {
        char buf[20];
        int idx = 0;
        int step = 0; 
        bool hasData = false;

        while(f.available() && f.position() < endLimit) {
            char c = f.read();

            if (c == '<') break; 

            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                if (hasData && step >= 1) {
                    if (step == 1) { buf[idx] = '\0'; lat = atof(buf); }
                    return true; 
                }
                idx = 0; step = 0; hasData = false;
            } else if (c == ',') {
                buf[idx] = '\0';
                if (step == 0) lon = atof(buf);
                else if (step == 1) lat = atof(buf);
                idx = 0; step++;
            } else {
                if (idx < 19) buf[idx++] = c;
                hasData = true;
            }
        }

        if (hasData && step >= 1) {
            if (step == 1) { buf[idx] = '\0'; lat = atof(buf); }
            return true;
        }
        return false;
    }

    double haversineDistance(double lat1, double lon1, double lat2, double lon2) {
        double dLat = (lat2 - lat1) * PI / 180.0;
        double dLon = (lon2 - lon1) * PI / 180.0;
        lat1 = lat1 * PI / 180.0;
        lat2 = lat2 * PI / 180.0;
        double a = pow(sin(dLat / 2), 2) + pow(sin(dLon / 2), 2) * cos(lat1) * cos(lat2);
        return 6371000.0 * (2 * asin(sqrt(a)));
    }

public:
    KMLDistanceData totalDistance;

    ManageKML(TFT_eSPI* display) {
        tft = display;
    }

    bool open(String filename) {
        paths.clear();
        selectedIndex = 0;
        fileModified = false;
        _needMapUpdate = true;
        _borderDrawn = false;
        currentFilename = filename;

        File f = LittleFS.open(filename, "r");
        if (!f) return false;

        bool useFolder = false;
        if (findInFile(f, "<Folder>") != -1) {
            useFolder = true;
        }
        
        f.seek(0);
        const char* targetTag = useFolder ? "<Folder>" : "<Placemark>";
        const char* endTag = useFolder ? "</Folder>" : "</Placemark>";

        while (f.available()) {
            int posMula = findInFile(f, targetTag);
            if (posMula == -1) break; 

            PathData p;
            p.startPos = posMula - strlen(targetTag); 
            p.isDeleted = false;
            p.name = "Unknown";

            size_t afterMula = f.position();

            int nameMula = findInFile(f, "<name>");
            if (nameMula != -1) {
                String tempName = "";
                while (f.available()) {
                    char c = f.read();
                    if (c == '<') break;
                    tempName += c;
                }
                p.name = tempName;
            }

            f.seek(afterMula); 
            int posTamat = findInFile(f, endTag);
            if (posTamat != -1) {
                p.endPos = posTamat;
                paths.push_back(p);
                f.seek(p.endPos); 
            } else {
                break;
            }
        }
        f.close();

        if (!paths.empty()) {
            select(0);
        }
        return true;
    }

    void resetUI() {
        _borderDrawn = false;
        _needMapUpdate = true;
    }

    String viewPath(int index) {
        if (index < 0 || index >= paths.size()) return "Index Error";
        return paths[index].name;
    }

    String viewSelectedPath() {
        if (paths.empty() || selectedIndex >= paths.size()) return "No Path";
        return paths[selectedIndex].name;
    }

    bool select(int index) {
        if (index >= 0 && index < paths.size()) {
            selectedIndex = index;
            totalDistance.m = 0; totalDistance.km = 0;
            geometries.clear();

            if (paths[selectedIndex].isDeleted) {
                _needMapUpdate = true;
                return true;
            }

            File f = LittleFS.open(currentFilename, "r");
            if (f) {
                f.seek(paths[selectedIndex].startPos);
                
                mapMinLon = 999; mapMaxLon = -999;
                mapMinLat = 999; mapMaxLat = -999;
                
                while (f.position() < paths[selectedIndex].endPos) {
                    int coordMula = findInFile(f, "<coordinates>");
                    if (coordMula == -1 || coordMula > paths[selectedIndex].endPos) break;
                    
                    int coordTamat = findInFile(f, "</coordinates>");
                    if (coordTamat == -1) break;

                    GeometryInfo geo;
                    geo.startPos = coordMula;
                    geo.endPos = coordTamat;
                    geo.count = 0;
                    
                    f.seek(coordMula);
                    double lon, lat;
                    double prevLon = 0, prevLat = 0;
                    
                    while (readNextCoord(f, coordTamat, lon, lat)) {
                        geo.count++;
                        if (geo.count == 1) {
                            geo.firstCoord = {lon, lat};
                        } else {
                            totalDistance.m += haversineDistance(prevLat, prevLon, lat, lon);
                        }
                        prevLon = lon;
                        prevLat = lat;
                        geo.lastCoord = {lon, lat};

                        if (lon < mapMinLon) mapMinLon = lon;
                        if (lon > mapMaxLon) mapMaxLon = lon;
                        if (lat < mapMinLat) mapMinLat = lat;
                        if (lat > mapMaxLat) mapMaxLat = lat;
                    }
                    
                    if (geo.count > 0) {
                        geometries.push_back(geo);
                    }
                    f.seek(coordTamat);
                }
                totalDistance.km = totalDistance.m / 1000.0;
                f.close();
            }

            _needMapUpdate = true;
            return true;
        }
        return false;
    }

    int getTotalPaths() {
        return paths.size();
    }

    bool isDeleted(int index) {
        if (index >= 0 && index < paths.size()) {
            return paths[index].isDeleted;
        }
        return true; 
    }

    void updateMap() {
        _needMapUpdate = true;
    }

    void drawMap(int x, int y, int w, int h, int offset, uint16_t lineColor, uint16_t rectColor, uint16_t bgColor) {
        if (!_borderDrawn) {
            tft->drawRect(x, y, w, h, rectColor);
            _borderDrawn = true;
        }

        if (!_needMapUpdate) return;
        _needMapUpdate = false; 

        tft->fillRect(x + 1, y + 1, w - 2, h - 2, bgColor);

        if (paths.empty() || paths[selectedIndex].isDeleted || geometries.empty()) return;

        double deltaLon = mapMaxLon - mapMinLon;
        double deltaLat = mapMaxLat - mapMinLat;
        if (deltaLon == 0) deltaLon = 0.0001; 
        if (deltaLat == 0) deltaLat = 0.0001;

        double midLat = (mapMinLat + mapMaxLat) / 2.0;
        mapCosLat = cos(midLat * PI / 180.0);

        double scaleX = (w - 2.0 * offset) / (deltaLon * mapCosLat);
        double scaleY = (h - 2.0 * offset) / deltaLat;
        mapScale = std::min(scaleX, scaleY);

        double pixelW = (deltaLon * mapCosLat) * mapScale;
        double pixelH = deltaLat * mapScale;
        mapOffsetX = x + offset + (w - 2 * offset - pixelW) / 2;
        mapOffsetY = y + offset + (h - 2 * offset - pixelH) / 2;

        mapX = x; mapY = y; mapW = w; mapH = h;

        File f = LittleFS.open(currentFilename, "r");
        if (f) {
            for (auto& geo : geometries) {
                // JIKA IA ADALAH PLACEMARK (HANYA 1 TITIK)
                if (geo.count == 1) {
                    int px = mapOffsetX + (geo.firstCoord.lon - mapMinLon) * mapCosLat * mapScale;
                    int py = mapOffsetY + (mapMaxLat - geo.firstCoord.lat) * mapScale;
                    
                    // --- GUNA FONT SMOOTH UNTUK PLACEMARK ---
                    tft->loadFont(Icons8);
                    tft->setTextDatum(MC_DATUM); 
                    
                    // RAHSIA SMOOTH FONT: Letak bgColor sebagai parameter kedua
                    tft->setTextColor(TFT_BLACK, bgColor); 
                    
                    // py ditolak 7 supaya hujung tajam pin tepat pada koordinat
                    tft->drawString(String(ICON_MARK), px, py - 7);
                    
                    tft->unloadFont(); 
                } 
                // JIKA IA ADALAH PATH (>1 TITIK)
                else if (geo.count > 1) {
                    f.seek(geo.startPos);
                    double lon, lat;
                    int prevPx = -1, prevPy = -1;

                    while (readNextCoord(f, geo.endPos, lon, lat)) {
                        int px = mapOffsetX + (lon - mapMinLon) * mapCosLat * mapScale;
                        int py = mapOffsetY + (mapMaxLat - lat) * mapScale; 

                        if (prevPx != -1) {
                            tft->drawLine(prevPx, prevPy, px, py, lineColor);
                        }
                        prevPx = px;
                        prevPy = py;
                    }
                    
                    // --- GUNA FONT SMOOTH UNTUK TITIK MULA DAN AKHIR LALUAN ---
                    int sx = mapOffsetX + (geo.firstCoord.lon - mapMinLon) * mapCosLat * mapScale;
                    int sy = mapOffsetY + (mapMaxLat - geo.firstCoord.lat) * mapScale;
                    
                    int ex = mapOffsetX + (geo.lastCoord.lon - mapMinLon) * mapCosLat * mapScale;
                    int ey = mapOffsetY + (mapMaxLat - geo.lastCoord.lat) * mapScale;

                    tft->loadFont(Icons8);
                    tft->setTextDatum(MC_DATUM); 

                    // Lukis Icon Mula (Tambah bgColor untuk anti-aliasing)
                    tft->setTextColor(TFT_BLACK, bgColor);
                    tft->drawString(String(ICON_START), sx, sy);

                    // Lukis Icon Akhir (Tambah bgColor untuk anti-aliasing)
                    tft->setTextColor(TFT_BLACK, bgColor);
                    tft->drawString(String(ICON_FINISH), ex, ey);

                    tft->unloadFont();
                }
            }
            f.close();
        }
    }

    void updateLocation(double lat, double lon, uint16_t pointColor) {
        if (paths.empty() || paths[selectedIndex].isDeleted) return;
        
        int px = mapOffsetX + (lon - mapMinLon) * mapCosLat * mapScale;
        int py = mapOffsetY + (mapMaxLat - lat) * mapScale;

        if (px >= mapX && px <= (mapX + mapW) && py >= mapY && py <= (mapY + mapH)) {
            tft->fillCircle(px, py, 4, pointColor);
        }
    }

    void deleteSelectedPath() {
        if (!paths.empty() && !paths[selectedIndex].isDeleted) {
            paths[selectedIndex].isDeleted = true;
            fileModified = true;
            totalDistance.m = 0;
            totalDistance.km = 0;
            geometries.clear();
        }
    }

    void close() {
        if (fileModified && paths.size() > 0) {
            File fRead = LittleFS.open(currentFilename, "r");
            File fWrite = LittleFS.open("/temp_kml.tmp", "w");
            
            if (fRead && fWrite) {
                size_t currentPos = 0;
                uint8_t buf[256]; 
                
                for (int i = 0; i < paths.size(); i++) {
                    if (paths[i].isDeleted) {
                        size_t toRead = paths[i].startPos - currentPos;
                        while (toRead > 0 && fRead.available()) {
                            size_t bytesToRead = std::min((size_t)256, toRead);
                            size_t bytesRead = fRead.read(buf, bytesToRead);
                            fWrite.write(buf, bytesRead);
                            toRead -= bytesRead;
                            currentPos += bytesRead;
                        }
                        fRead.seek(paths[i].endPos);
                        currentPos = paths[i].endPos;
                    }
                }
                
                while (fRead.available()) {
                    size_t bytesRead = fRead.read(buf, 256);
                    fWrite.write(buf, bytesRead);
                }
                
                fRead.close();
                fWrite.close();
                
                LittleFS.remove(currentFilename);
                LittleFS.rename("/temp_kml.tmp", currentFilename);
            }
        }
        
        paths.clear();
        selectedIndex = 0;
        fileModified = false;
        _borderDrawn = false;
    }
};

#endif