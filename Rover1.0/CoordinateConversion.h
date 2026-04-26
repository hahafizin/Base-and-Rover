#ifndef COORDINATE_CONVERSION_H
#define COORDINATE_CONVERSION_H

#include <Arduino.h>
#include <math.h>
#include <Preferences.h> 

// --- ENUM ZON NEGERI (GDM2000 CASSINI) ---
enum Zone {
    ZONE_JOHOR,           // 0
    ZONE_SELANGOR_KL,     // 1
    ZONE_PERAK,           // 2
    ZONE_KEDAH_PERLIS,    // 3
    ZONE_PULAU_PINANG,    // 4
    ZONE_NEGERI_SEMBILAN, // 5
    ZONE_MELAKA,          // 6
    ZONE_PAHANG,          // 7
    ZONE_KELANTAN,        // 8
    ZONE_TERENGGANU       // 9
};

// --- STRUCT DATA OUTPUT ---
struct CoordOutput {
    double lat = 0.0;
    double lon = 0.0;
    double easting = 0.0;
    double northing = 0.0;
    
    // --- [BARU] Variable Origin Zon ---
    double lat0 = 0.0;
    double lon0 = 0.0;

    bool currentmode = true; // true = WGS, false = GDM
    int zoneId = 1;          // Default Selangor (1)
    String zoneName = "Selangor & KL"; 
    
    double x = 0.0; // Dynamic: lat atau easting
    double y = 0.0; // Dynamic: lon atau northing

    // Function: Dapatkan nama zon dari index
    String zoneFromIndex(int index) {
        switch (index) {
            case 0: return "Johor";
            case 1: return "Selangor & KL";
            case 2: return "Perak";
            case 3: return "Kedah & Perlis";
            case 4: return "Pulau Pinang";
            case 5: return "Negeri Sembilan";
            case 6: return "Melaka";
            case 7: return "Pahang";
            case 8: return "Kelantan";
            case 9: return "Terengganu";
            default: return "Unknown";
        }
    }
};

class CoordinateConversion {
private:
    Preferences prefs;
    
    // --- PARAMETER ELLIPSOID GRS80 ---
    const double a = 6378137.0;          
    const double f = 1.0 / 298.257222101; 
    double b, e2;                        

    // --- PARAMETER ZON ---
    double lat0_rad, lon0_rad; 
    double fe, fn;             
    Zone _currentZone; 

    // --- MATH HELPERS ---
    double deg2rad(double deg) { return deg * (PI / 180.0); }
    double rad2deg(double rad) { return rad * (180.0 / PI); }

    double calcM(double lat_rad) {
        double e4 = e2 * e2;
        double e6 = e4 * e2;
        double A0 = 1 - (e2 / 4.0) - (3.0 * e4 / 64.0) - (5.0 * e6 / 256.0);
        double A2 = (3.0 * e2 / 8.0) + (3.0 * e4 / 32.0) + (45.0 * e6 / 1024.0);
        double A4 = (15.0 * e4 / 256.0) + (45.0 * e6 / 1024.0);
        double A6 = (35.0 * e6 / 3072.0);
        return a * (A0 * lat_rad - A2 * sin(2 * lat_rad) + A4 * sin(4 * lat_rad) - A6 * sin(6 * lat_rad));
    }

    // Fungsi internal untuk update X dan Y (Tanpa Save)
    void updateXY() {
        if (get.currentmode == true) { 
            // Mode WGS: x=lat, y=lon
            get.x = get.lat;
            get.y = get.lon;
        } else {
            // Mode GDM: x=easting, y=northing
            get.x = get.easting;
            get.y = get.northing;
        }
    }

    // Fungsi internal set zone (Tanpa Save)
    void setZoneInternal(Zone z) {
        _currentZone = z; 
        get.zoneId = (int)z;
        get.zoneName = get.zoneFromIndex((int)z);

        double lat0_deg, lon0_deg;
        
        switch (z) {
            case ZONE_JOHOR:          lat0_deg = 2.120833333; lon0_deg = 103.470555556; break; // 02° 07' 15", 103° 28' 14"
            case ZONE_SELANGOR_KL:    lat0_deg = 3.070656667; lon0_deg = 101.581623333; break; // 03° 04' 14.364", 101° 34' 53.844"
            case ZONE_PERAK:          lat0_deg = 4.324955278; lon0_deg = 100.934329167; break; // 04° 19' 29.839", 100° 56' 03.585"
            case ZONE_KEDAH_PERLIS:   lat0_deg = 6.132333889; lon0_deg = 100.374955278; break; // 06° 07' 56.402", 100° 22' 29.839"
            case ZONE_PULAU_PINANG:   lat0_deg = 5.413785000; lon0_deg = 100.333422222; break; // 05° 24' 49.626", 100° 20' 00.320"
            case ZONE_NEGERI_SEMBILAN:lat0_deg = 2.684429167; lon0_deg = 101.982071944; break; // 02° 41' 03.945", 101° 58' 55.459"
            case ZONE_MELAKA:         lat0_deg = 2.255639167; lon0_deg = 102.274353889; break; // 02° 15' 20.301", 102° 16' 27.674"
            case ZONE_PAHANG:         lat0_deg = 3.792811111; lon0_deg = 102.398233889; break; // 03° 47' 34.120", 102° 23' 53.642"
            case ZONE_KELANTAN:       lat0_deg = 4.681280556; lon0_deg = 102.147458056; break; // 04° 40' 52.610", 102° 08' 50.849"
            case ZONE_TERENGGANU:     lat0_deg = 4.988748056; lon0_deg = 103.037300833; break; // 04° 59' 19.493", 103° 02' 14.283"
            default:                  lat0_deg = 3.070656667; lon0_deg = 101.581623333; break; // 03° 04' 14.364", 101° 34' 53.844"
        }

        // --- [BARU] Simpan Origin ke Struct Output ---
        get.lat0 = lat0_deg;
        get.lon0 = lon0_deg;

        lat0_rad = deg2rad(lat0_deg);
        lon0_rad = deg2rad(lon0_deg);
        fe = 0.0; fn = 0.0;
    }

    // Fungsi internal set mode (Tanpa Save)
    void setModeInternal(bool isWGS) {
        get.currentmode = isWGS;
        updateXY();
    }

public:
    // Variable public untuk akses data (coordinate.get.variable)
    CoordOutput get;

    CoordinateConversion() {
        b = a * (1 - f);
        e2 = 2 * f - f * f;
        setZoneInternal(ZONE_SELANGOR_KL); 
    }

    // Panggil function ini dalam void setup()!
    void begin() {
        prefs.begin("geo_store", false);
        
        // 1. Load Zon
        int savedZone = prefs.getInt("zone_idx", 1);
        if (savedZone < 0 || savedZone > 9) savedZone = 1;
        setZoneInternal((Zone)savedZone);

        // 2. Load Mode
        bool savedMode = prefs.getBool("mode_wgs", true);
        setModeInternal(savedMode);
    }

    // --- FUNGSI SET ZONE (DENGAN AUTO-SAVE) ---
    void setZone(int zoneindex) {
        if (zoneindex < 0 || zoneindex > 9) zoneindex = 1;
        setZoneInternal((Zone)zoneindex); 
        prefs.putInt("zone_idx", zoneindex); 
    }

    // --- FUNGSI SET MODE (DENGAN AUTO-SAVE) ---
    void setToWGS(bool isWGS) {
        setModeInternal(isWGS); 
        prefs.putBool("mode_wgs", isWGS); 
    }

    // --- FUNGSI CONVERSION ---
    
    // 1. Tukar Lat/Lon ke Easting/Northing
    void convToGDM(double lat, double lon) {
        get.lat = lat;
        get.lon = lon;

        double phi = deg2rad(lat);
        double lambda = deg2rad(lon);
        double N = a / sqrt(1 - e2 * sin(phi) * sin(phi)); 
        double T = tan(phi) * tan(phi);
        double A = (lambda - lon0_rad) * cos(phi);
        double M = calcM(phi);
        double M0 = calcM(lat0_rad);

        double term1 = A - (T * pow(A, 3) / 6.0) + ((1 + 3 * T) * pow(A, 5) / 120.0);
        get.easting = fe + N * term1;

        double term2 = (pow(A, 2) / 2.0) - ((1 + 3 * T) * pow(A, 4) / 24.0);
        get.northing = fn + (M - M0) + N * tan(phi) * term2;

        updateXY(); 
    }

    // 2. Tukar Easting/Northing ke Lat/Lon
    void convToWGS(double easting, double northing) {
        get.easting = easting;
        get.northing = northing;

        double M0 = calcM(lat0_rad);
        double M1 = M0 + (northing - fn);
        double mu = M1 / (a * (1 - e2 / 4 - 3 * e2 * e2 / 64));
        double e1 = (1 - sqrt(1 - e2)) / (1 + sqrt(1 - e2));
        
        double phi1 = mu + (3 * e1 / 2 - 27 * pow(e1, 3) / 32) * sin(2 * mu) 
                      + (21 * e1 * e1 / 16 - 55 * pow(e1, 4) / 32) * sin(4 * mu)
                      + (151 * pow(e1, 3) / 96) * sin(6 * mu);

        double N1 = a / sqrt(1 - e2 * sin(phi1) * sin(phi1));
        double R1 = a * (1 - e2) / pow(1 - e2 * sin(phi1) * sin(phi1), 1.5);
        double D = (easting - fe) / N1;
        double T1 = tan(phi1) * tan(phi1);

        double lat_rad = phi1 - (N1 * tan(phi1) / R1) * (pow(D, 2) / 2 - (1 + 3 * T1) * pow(D, 4) / 24);
        double lon_rad = lon0_rad + (D - T1 * pow(D, 3) / 3 + (1 + 3 * T1) * T1 * pow(D, 5) / 15) / cos(phi1);

        get.lat = rad2deg(lat_rad);
        get.lon = rad2deg(lon_rad);

        updateXY(); 
    }
};

CoordinateConversion coordinate;

#endif