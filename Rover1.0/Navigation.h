#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <Arduino.h>
#include <LittleFS.h>
#include <math.h>
#include <time.h> 

// --- KONFIGURASI ---
#define LOG_FILENAME "/nav_log.dat"
#define KML_FILENAME "/path_log.kml"
#define EARTH_RADIUS 6371000.0
#define MAX_LOG_SIZE 2097152 

struct NavPoint {
    double lat;
    double lon;
};

#define LOG_TYPE_NORMAL    0
#define LOG_TYPE_OBSTACLE  1
#define LOG_TYPE_NEW_PATH  2

struct LogEntry {
    double lat;
    double lon;
    unsigned long timestamp; 
    uint8_t type;            
};

class Navigation {
public:
    NavPoint startPoint = {0.0, 0.0};
    NavPoint endPoint   = {0.0, 0.0};
    
    float distance;
    float bearing;
    float cte;
    
    float totalDistanceTraveled = 0.0;
    float avgSpeed = 0.0;

    // --- PEMBOLEHUBAH BARU UNTUK OFFSET ---
    double dLat = 0.0;
    double dLon = 0.0;
    
private:
    bool _isNavigating = false;
    bool _useTimeForLog = true;
    int _logInterval = 5;       
    
    unsigned long _lastLogTime = 0;
    NavPoint _lastLogPos = {0.0, 0.0};
    unsigned long _startTimeMillis = 0; 
    
    bool _pendingObstacleFlag = false;
    int _totalLogCount = 0;

public:
    bool begin() {
        if (!LittleFS.begin()) return false;
        return true;
    }

    void setEnd(double lat, double lon) {
        endPoint.lat = lat;
        endPoint.lon = lon;
    }

    void setStart(double lat, double lon) {
        startPoint.lat = lat;
        startPoint.lon = lon;
    }

    void recordByTime(bool byTime) {
        _useTimeForLog = byTime;
        _logInterval = byTime ? 2000 : 5;
    }

    // ---------------------------------------------------------
    // FUNGSI BARU: KIRA OFFSET
    // ---------------------------------------------------------
    void calculateOffset(double currentLat, double currentLon, double targetLat, double targetLon) {
        dLat = targetLat - currentLat;
        dLon = targetLon - currentLon;
    }

    // ---------------------------------------------------------
    // 1. START NAVIGATION
    // ---------------------------------------------------------
    void start(double currLat, double currLon, int yyyy, int mm, int dd, int hh, int min) {
        
        startPoint.lat = currLat;
        startPoint.lon = currLon;
        _lastLogPos = {currLat, currLon};

        totalDistanceTraveled = 0.0;
        cte = 0.0;
        _startTimeMillis = millis();
        _lastLogTime = millis();
        _isNavigating = true;
        
        if (LittleFS.exists(LOG_FILENAME)) {
            LittleFS.remove(LOG_FILENAME);
        }

        struct tm timeinfo;
        timeinfo.tm_year = yyyy - 1900;
        timeinfo.tm_mon  = mm - 1;
        timeinfo.tm_mday = dd;
        timeinfo.tm_hour = hh;
        timeinfo.tm_min  = min;
        timeinfo.tm_sec  = 0;
        time_t timeStamp = mktime(&timeinfo);

        File logFile = LittleFS.open(LOG_FILENAME, "w");
        if (logFile) {
            LogEntry headerEntry;
            headerEntry.lat = currLat;
            headerEntry.lon = currLon;
            headerEntry.timestamp = (unsigned long)timeStamp;
            headerEntry.type = LOG_TYPE_NEW_PATH; 
            
            logFile.write((uint8_t*)&headerEntry, sizeof(headerEntry));
            
            LogEntry firstPoint = headerEntry;
            firstPoint.type = LOG_TYPE_NORMAL;
            logFile.write((uint8_t*)&firstPoint, sizeof(firstPoint));

            logFile.close();
        }
    }

    void update(double currLat, double currLon) {
        if (!_isNavigating) return;

        unsigned long currentMillis = millis();

        // 1. Kira jarak antara kedudukan SEKARANG dengan kedudukan LOG TERAKHIR
        float distDelta = calculateDistance(_lastLogPos.lat, _lastLogPos.lon, currLat, currLon);
        
        // ---------------------------------------------------------
        // PENAPIS GPS MELOMPAT (OUTLIER REJECTION)
        // ---------------------------------------------------------
        // Jika jarak melompat lebih dari 30 meter dalam 1 kitaran log, ini adalah ralat (GPS Spike).
        // Nilai 30 meter ini awak boleh ubah bergantung pada kelajuan maksimum rover awak.
        if (distDelta > 30.0 && _totalLogCount > 0) {
            // Abaikan titik ini sepenuhnya, jangan rekod!
            // Kita return awal supaya logik di bawah tidak dijalankan.
            return; 
        }

        // --- Teruskan kiraan normal jika data sah ---
        distance = calculateDistance(currLat, currLon, endPoint.lat, endPoint.lon);
        bearing  = calculateBearing(currLat, currLon, endPoint.lat, endPoint.lon);
        
        if (distDelta > 0.5) totalDistanceTraveled += distDelta;

        float distFromStart = calculateDistance(startPoint.lat, startPoint.lon, currLat, currLon);
        if (distFromStart < 5.0) cte = 0.0; 
        else cte = calculateCrossTrackError(currLat, currLon);

        bool shouldLog = false;
        if (_useTimeForLog) {
            if (currentMillis - _lastLogTime >= _logInterval) shouldLog = true;
        } else {
            if (distDelta >= _logInterval) shouldLog = true;
        }

        if (_pendingObstacleFlag) shouldLog = true;

        if (shouldLog) {
            saveLog(currLat, currLon, currentMillis);
        }
    }

    void addPoint() { _pendingObstacleFlag = true; }

    // ---------------------------------------------------------
    // 2. STOP NAVIGATION
    // ---------------------------------------------------------
    void stop() { 
        if (!_isNavigating) return;
        _isNavigating = false; 
        
        appendPathToKML();
    }

    // ---------------------------------------------------------
    // 3. FUNGSI MAP VIEWER
    // ---------------------------------------------------------
    int getTotalLog() {
        File f = LittleFS.open(LOG_FILENAME, "r");
        if (!f) return 0;
        int count = f.size() / sizeof(LogEntry);
        f.close();
        return count;
    }

    double getLogLat(int index) {
        LogEntry entry;
        if (readLogAtIndex(index, &entry)) return entry.lat;
        return 0.0;
    }

    double getLogLon(int index) {
        LogEntry entry;
        if (readLogAtIndex(index, &entry)) return entry.lon;
        return 0.0;
    }

    bool isLogObstacle(int index) {
        LogEntry entry;
        if (readLogAtIndex(index, &entry)) return (entry.type == LOG_TYPE_OBSTACLE);
        return false;
    }

    void clearAllLogs() {
        if (LittleFS.exists(LOG_FILENAME)) LittleFS.remove(LOG_FILENAME);
        if (LittleFS.exists(KML_FILENAME)) LittleFS.remove(KML_FILENAME);
        _totalLogCount = 0;
        cte = 0.0;
        _isNavigating = false;
    }

private:
    void saveLog(double lat, double lon, unsigned long time) {
        File logFile = LittleFS.open(LOG_FILENAME, "a"); 
        if (logFile) {
            LogEntry entry;
            entry.lat = lat;
            entry.lon = lon;
            entry.timestamp = time; 
            
            if (_pendingObstacleFlag) entry.type = LOG_TYPE_OBSTACLE;
            else entry.type = LOG_TYPE_NORMAL;

            logFile.write((uint8_t*)&entry, sizeof(entry));
            
            _lastLogTime = millis();
            _lastLogPos = {lat, lon};
            _pendingObstacleFlag = false;
            _totalLogCount++;
        }
        logFile.close();
    }

    bool readLogAtIndex(int index, LogEntry* entry) {
        File f = LittleFS.open(LOG_FILENAME, "r");
        if (!f) return false;
        if (f.seek(index * sizeof(LogEntry))) {
            if (f.read((uint8_t*)entry, sizeof(LogEntry)) == sizeof(LogEntry)) {
                f.close();
                return true;
            }
        }
        f.close();
        return false;
    }

    // ---------------------------------------------------------
    // INJECTION LOGIK (DAT -> KML) DENGAN FOLDER & PLACEMARK
    // ---------------------------------------------------------
    void appendPathToKML() {
        File binFile = LittleFS.open(LOG_FILENAME, "r");
        if (!binFile) return;

        File kmlFile;
        bool kmlExists = LittleFS.exists(KML_FILENAME);

        if (!kmlExists) {
            kmlFile = LittleFS.open(KML_FILENAME, "w");
            kmlFile.println("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
            kmlFile.println("<kml xmlns=\"http://www.opengis.net/kml/2.2\">");
            kmlFile.println("<Document>");
            kmlFile.println("<name>Rover Multi-Path Log</name>");
            kmlFile.println("<Style id=\"pathLine\"><LineStyle><color>ff0000ff</color><width>4</width></LineStyle></Style>");
        } else {
            kmlFile = LittleFS.open(KML_FILENAME, "r+");
            if (kmlFile) {
                long searchPos = kmlFile.size() - 128; 
                if (searchPos < 0) searchPos = 0;
                kmlFile.seek(searchPos);
                
                char buffer[129];
                int bytesRead = kmlFile.read((uint8_t*)buffer, 128);
                buffer[bytesRead] = '\0';
                
                char* target = strstr(buffer, "</Document>");
                
                if (target != NULL) {
                    int offset = target - buffer;
                    kmlFile.seek(searchPos + offset);
                } else {
                    kmlFile.seek(kmlFile.size());
                }
            } else {
                kmlFile = LittleFS.open(KML_FILENAME, "a");
            }
        }

        if (!kmlFile) {
            binFile.close();
            return;
        }

        LogEntry entry;
        bool hasHeader = false;
        char dateBuffer[30] = "Rover Session";

        // ==========================================
        // PUSINGAN 1: Tulis Folder & Garisan Laluan
        // ==========================================
        while (binFile.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry)) {
            
            // Tangkap Header & Buka Folder
            if (entry.type == LOG_TYPE_NEW_PATH && !hasHeader) {
                time_t rawTime = (time_t)entry.timestamp;
                struct tm * ti = localtime(&rawTime);
                sprintf(dateBuffer, "%04d-%02d-%02d %02d:%02d", 
                        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday, ti->tm_hour, ti->tm_min);

                // Mula Folder
                kmlFile.println("<Folder>");
                kmlFile.print("<name>"); kmlFile.print(dateBuffer); kmlFile.println("</name>");

                // Mula Garisan
                kmlFile.println("<Placemark>");
                kmlFile.println("<name>Path</name>");
                kmlFile.println("<styleUrl>#pathLine</styleUrl>");
                kmlFile.println("<LineString><coordinates>");
                
                hasHeader = true;
            } 
            
            // Tulis Semua Koordinat (Termasuk obstacle, supaya garisan tak terputus)
            if (hasHeader) {
                kmlFile.print(String(entry.lon, 6));
                kmlFile.print(",");
                kmlFile.print(String(entry.lat, 6));
                kmlFile.print(",0 ");
            }
        }

        // Tutup Garisan
        if (hasHeader) {
            kmlFile.println("\n</coordinates></LineString></Placemark>");
        }

        // ==========================================
        // PUSINGAN 2: Tulis Placemark / Titik Halangan
        // ==========================================
        binFile.seek(0); // Pusing balik fail binari ke 0
        int pointCount = 1;

        while (binFile.read((uint8_t*)&entry, sizeof(entry)) == sizeof(entry)) {
            if (entry.type == LOG_TYPE_OBSTACLE) {
                kmlFile.println("<Placemark>");
                kmlFile.print("<name>Point "); kmlFile.print(pointCount); kmlFile.println("</name>");
                kmlFile.println("<Point><coordinates>");
                kmlFile.print(String(entry.lon, 6));
                kmlFile.print(",");
                kmlFile.print(String(entry.lat, 6));
                kmlFile.println(",0");
                kmlFile.println("</coordinates></Point>");
                kmlFile.println("</Placemark>");
                pointCount++;
            }
        }

        // Tutup Folder
        if (hasHeader) {
            kmlFile.println("</Folder>");
        }

        // Tulis Semula Tag Penutup Utama KML
        kmlFile.println("</Document>\n</kml>");

        kmlFile.close();
        binFile.close();

        // Padam `.dat` kerana proses penukaran telah lengkap
        LittleFS.remove(LOG_FILENAME);
    }
    
    // --- Helper Math (Flat Earth / Pythagoras) ---
    double toRadians(double degree) { return degree * M_PI / 180.0; }
    double toDegrees(double radian) { return radian * 180.0 / M_PI; }

    float calculateDistance(double lat1, double lon1, double lat2, double lon2) {
        double lat1Rad = toRadians(lat1);
        double lat2Rad = toRadians(lat2);
        double lon1Rad = toRadians(lon1);
        double lon2Rad = toRadians(lon2);
        
        double meanLat = (lat1Rad + lat2Rad) / 2.0;
        double x = (lon2Rad - lon1Rad) * cos(meanLat);
        double y = (lat2Rad - lat1Rad);
        
        double distance = sqrt(x * x + y * y) * EARTH_RADIUS;
        return (float)distance;
    }

    float calculateBearing(double lat1, double lon1, double lat2, double lon2) {
        double lat1Rad = toRadians(lat1);
        double lat2Rad = toRadians(lat2);
        double lon1Rad = toRadians(lon1);
        double lon2Rad = toRadians(lon2);
        
        double meanLat = (lat1Rad + lat2Rad) / 2.0;
        double dx = (lon2Rad - lon1Rad) * cos(meanLat);
        double dy = (lat2Rad - lat1Rad);
        
        double brng = atan2(dx, dy); 
        brng = toDegrees(brng);
        return (float)fmod((brng + 360.0), 360.0);
    }

    float calculateCrossTrackError(double currLat, double currLon) {
        float dist_AD = calculateDistance(startPoint.lat, startPoint.lon, currLat, currLon);
        float bearing_AD = calculateBearing(startPoint.lat, startPoint.lon, currLat, currLon);
        float bearing_AB = calculateBearing(startPoint.lat, startPoint.lon, endPoint.lat, endPoint.lon);
        double theta = toRadians(bearing_AD - bearing_AB);
        double XTE = sin(theta) * dist_AD;
        return (float)XTE;
    }
};

#endif