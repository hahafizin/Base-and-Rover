#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_HMC5883_U.h>
#include <Adafruit_BMP280.h>
#include <HardwareSerial.h>
#include <TinyGPSPlus.h>
#include <LoRa.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h> // --- UBAH: Tambah Library NeoPixel ---
#include <math.h> // Perlu untuk fungsi pow() dan exp()
#include <Preferences.h> // Library untuk simpan data kekal
#include "SmartTextBox.h"
#include "Magnetometer.h"
#include "KMLFile.h" //manage website dan data management
#include "CoordinateConversion.h"
#include "DrawMap.h"
#include "LoraManager_RX.h"
#include "DrawSymbol.h"
#include "driver/rtc_io.h"
#include "BatteryPercentage.h"
#include "Navigation.h"
#include "PageNavigation.h"
#include "TouchButton.h"
#include "MontserratBold25.h" //tinggi 17
#include "MontserratMedium18.h" //tinggi 12
#include "Icons8.h"
#include "ShowDataOnMap.h"
#include "QRCodeGenerator.h"
#include "DisplayCTE.h"
#include "ManageKML.h"

//pinout
#define RX       17
#define TX       18
#define CS_LORA  8
#define RST_LORA 7
#define DIO_LORA 6
#define LCD_LED  2
#define TOUCH_CS 5
#define TOUCH_IRQ 4
#define BUCK_EN 13
#define SH_SCK   36
#define SH_MOSI  35
#define SH_MISO  37
#define I2C_SDA 15
#define I2C_SCL 16
#define BUZZER 1
#define WAKEUP_PIN 4           // Pin interrupt touchscreen

#define HOLD_TIME_MS 2000      // Masa untuk "Long Press" (2 saat)
#define MONT_B_25 MontserratBold25
#define MONT_M_18 MontserratMedium18
#define CUSTOM_ICONS Icons8

// Senarai Ikon Font Awesome 5 Free (Solid)
#define ICON_DISTANCE   "\uf4d7" // Jarak / Route
#define ICON_PRECISION  "\uf05b" // Crosshair / Ketepatan
#define ICON_SATELLITE  "\uf7c0" // Satellite Dish
#define ICON_MAP        "\uf279" // Map
#define ICON_LOCATION   "\uf124" // Location Arrow
#define ICON_LINK       "\uf0c1" // Link / Sambung
#define ICON_LINK_OFF   "\uf127"
#define ICON_INFO       "\uf05a" // Info Circle
#define ICON_ALTITUDE   "\uf6fc" // Altitude / Mountain
#define ICON_VOLTAGE    "\uf0e7" // Voltage / Bolt
#define ICON_FILE       "\uf15b" // Fail
#define ICON_COMPASS    "\uf14e" // Compass
#define ICON_TEMP       "\uf2cb" // Thermometer (Suhu)
#define ICON_NAV        "\uf5eb" // Directions (Navigation)
#define ICON_WIFI       "\uf1eb" // Wifi
#define ICON_TARE       "\uf24e" // Balance-scale (Tare/Weight)
#define ICON_DATA       "\uf1c0" // Database (Data)
#define ICON_CALIBRATE  "\uf1de" // Sliders-h (Calibrate)
#define ICON_SIG_1      "\uf691"
#define ICON_SIG_2      "\uf692"
#define ICON_SIG_3      "\uf693"
#define ICON_SIG_4      "\uf690"
#define ICON_SIG_X      "\uf694"
#define ICON_WIFI_1     "\uf6aa"
#define ICON_WIFI_2     "\uf6ab"
#define ICON_WIFI_3     "\uf1eb"
#define ICON_WIFI_X     "\uf6ac"
#define ICON_POWER      "\uf011"
#define ICON_TOWER      "\uf519" // Tower Broadcast
#define ICON_SNR        "\uf899" // Sine Wave
#define ICON_ADD        "\uf067"
#define ICON_DELETE     "\uf1f8" // Bentuk tong sampah
#define ICON_CLOSE      "\uf00d" // Bentuk X
#define ICON_BAT_100    "\uf240"
#define ICON_BAT_75     "\uf241"
#define ICON_BAT_50     "\uf242"
#define ICON_BAT_25     "\uf243"
#define ICON_BAT_0      "\uf244"
#define ICON_BAT_CHG    "\uf376" // Pengecasan
#define ICON_SKIPLEFT   "\uf100" //<<
#define ICON_SKIPRIGHT  "\uf101" //>>
#define ICON_SKIPUP     "\uf102" //<<
#define ICON_SKIPDOWN   "\uf103" //>>
#define ICON_LEFT       "\uf104" //<
#define ICON_RIGHT      "\uf105" //>
#define ICON_UP         "\uf106" //^
#define ICON_DOWN       "\uf107" //v
#define ICON_SWAP       "\uf079" //swap
#define ICON_WAYPOINT   "\uf024" // Bendera
#define ICON_XTE        "\uf07e" // Cross Track Error
#define ICON_TARGET     "\uf11e" // Bendera Penamat
#define ICON_BULLSEYE   "\uf648" // Sasaran Tepat
#define ICON_DEST       "\uf3c5" // Lokasi Dituju
#define ICON_DONE        "\uf00c" // Rait biasa
#define ICON_INC        "\uf0d8" // Caret Up
#define ICON_FLAT       "\uf52c" // Equals (=)
#define ICON_DEC        "\uf0d7" // Caret Down
#define ICON_START      "\uf04b" // Tempat Mula (Play)
#define ICON_MARK       "\uf08d" // Tanda Placemark (Thumbtack)
#define ICON_FINISH     "\uf11e" // Tempat Akhir (Checkered Flag)

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite sprite = TFT_eSprite(&tft);
Adafruit_HMC5883_Unified mag = Adafruit_HMC5883_Unified(12345);
Adafruit_BMP280 bmp;
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
Adafruit_INA219 ina219;
Preferences preferences; // Create object
Magnetometer compass;
RoverMission mission;
DrawMap myMap(90, 200, 50, TFT_BLACK, TFT_WHITE);
LoraManager_RX loraSys;
BatteryPercentage batMon;
Navigation nav;
PageNavigation ManagedataControl;
PageNavigation SelectzonePageControl;
PageNavigation SelectKMLPathControl;
Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);
SPIClass sharedSPI(HSPI); // Cipta objek SPI secara manual
MapViewer mapViewer;
DisplayCTE cte(&tft);
ManageKML kml(&tft);
TaskHandle_t TaskGPS; // Handle untuk task


// --- MULA CLASS BACKLIGHT MANAGER ---
class BacklightManager {
  private:
    int ledPin;
    unsigned long lastTouchTime;
    bool isBacklightOn;
    bool isDimmed; 
    
    Preferences pref; // TAMBAH: Objek Preferences

    const int BRIGHTNESS_FULL = 255;
    const int BRIGHTNESS_DIM  = 50;
    const int BRIGHTNESS_OFF  = 0;

    const unsigned long dimDelays[4] = {10000, 30000, 120000, 0}; 
    const unsigned long offDelays[4] = {30000, 120000, 300000, 0}; 

    int dimIndex; 
    int offIndex; 

    String msToString(unsigned long ms) {
      if (ms == 0) return "Never";
      if (ms == 10000) return "10s";
      if (ms == 30000) return "30s";
      if (ms == 120000) return "2m";
      if (ms == 300000) return "5m";
      return String(ms / 1000) + "s"; 
    }

    void updateStrings() {
      currentDelayDim = msToString(dimDelays[dimIndex]);
      currentDelayOff = msToString(offDelays[offIndex]);

      int nextD = (dimIndex + 1) % 4;
      nextCycleDim = msToString(dimDelays[nextD]);

      int nextO = (offIndex + 1) % 4;
      unsigned long currentDimMs = dimDelays[dimIndex];
      
      while (offDelays[nextO] != 0 && currentDimMs != 0 && offDelays[nextO] < currentDimMs) {
        nextO = (nextO + 1) % 4;
      }
      nextCycleOff = msToString(offDelays[nextO]);
    }

    // TAMBAH: Fungsi dalaman untuk simpan tetapan semasa ke memori flash
    void saveSettings() {
      pref.putInt("dimIdx", dimIndex);
      pref.putInt("offIdx", offIndex);
    }

  public:
    String currentDelayDim;
    String currentDelayOff;
    String nextCycleDim;
    String nextCycleOff;

    BacklightManager(int pin) {
      ledPin = pin;
      lastTouchTime = millis();
      isBacklightOn = true;
      isDimmed = false; 
      
      pinMode(ledPin, OUTPUT);
      analogWrite(ledPin, BRIGHTNESS_FULL);
    }

    // TAMBAH: Fungsi begin() untuk dipanggil dalam setup()
    void begin() {
      // Buka ruangan (namespace) bernama "backlight". false = mode Read/Write
      pref.begin("backlight", false); 
      
      // Ambil nilai yang disimpan. Jika tiada (kali pertama run), guna nilai lalai (0)
      dimIndex = pref.getInt("dimIdx", 0);
      offIndex = pref.getInt("offIdx", 0);
      
      updateStrings(); 
    }

    void cycleDim() {
      dimIndex = (dimIndex + 1) % 4;
      unsigned long currentDim = dimDelays[dimIndex];

      if (currentDim == 0) {
        offIndex = 3; 
      } else if (offDelays[offIndex] != 0 && currentDim > offDelays[offIndex]) {
        while (offDelays[offIndex] != 0 && offDelays[offIndex] < currentDim) {
          offIndex = (offIndex + 1) % 4;
        }
      }
      updateStrings();
      saveSettings(); // Simpan setiap kali berubah
    }

    void cycleScreenOff() {
      unsigned long currentDim = dimDelays[dimIndex];
      
      do {
        offIndex = (offIndex + 1) % 4;
      } while (offDelays[offIndex] != 0 && currentDim != 0 && offDelays[offIndex] < currentDim);
      
      updateStrings();
      saveSettings(); // Simpan setiap kali berubah
    }

    void controlBacklight() {
      uint16_t x, y;
      bool touched = tft.getTouch(&x, &y); 
      
      unsigned long dDim = dimDelays[dimIndex];
      unsigned long dOff = offDelays[offIndex];

      if (touched) {
        lastTouchTime = millis();
        if (!isBacklightOn || isDimmed) { 
          analogWrite(ledPin, BRIGHTNESS_FULL);
          isBacklightOn = true;
          isDimmed = false;
        }
      } 
      else {
        unsigned long elapsedTime = millis() - lastTouchTime;

        if (dOff > 0 && elapsedTime > dOff) {
          if (isBacklightOn) { 
            analogWrite(ledPin, BRIGHTNESS_OFF);
            isBacklightOn = false;
            isDimmed = false; 
          }
        } 
        else if (dDim > 0 && elapsedTime > dDim) {
          if (!isDimmed) { 
            analogWrite(ledPin, BRIGHTNESS_DIM);
            isDimmed = true; 
            isBacklightOn = true; 
          }
        } 
      }
    }
};
// --- TAMAT CLASS BACKLIGHT MANAGER ---

BacklightManager bl(TFT_BL);



// --- SETTING BACKLIGHT ---
// const int pwmFreq = 5000;
// const int pwmResolution = 8; 
int delayToDim = 0, delayToOFF = 0;
float usedStoragePercent = 0.0;


#define MAX_HISTORY 10
int historyCount = 0;
enum ScreenState {
  HOME,
  SETUP,
  SETUP2,
  MANAGEDATA,
  SELECTZONE,
  MODESELECTION,
  NAVIGATINGCHOOSEPLACE,
  NAVIGATINGPLACE,
  WIFISETUP,
  VIEWKML,
  CALIBRATION,
  DONENAVIGATION
};
ScreenState currentState = HOME;
ScreenState pageHistory[MAX_HISTORY];
bool pageChanged = true;

// --- DUAL CORE SETTINGS ---

int currentHeading, targetBearing, error;
float current_mA, busVoltage;
float currentAlt;
// Variable Global (Volatile supaya Core 1 tahu data ini sentiasa berubah)
volatile double sharedLat = 0.0;
volatile double sharedLon = 0.0;
volatile int sharedSat = 0;
volatile bool sharedFix = false; // Status adakah GPS dah dapat lock
volatile int sharedHour = 0;
volatile int sharedMinute = 0;
volatile int sharedSecond = 0;
volatile int sharedDay = 0;
volatile int sharedMonth = 0;
volatile int sharedYear = 0;
volatile unsigned long sharedUnix = 0;
volatile double sharedHDOP = 0.0;
volatile double sharedAcc = 0.0;
volatile float currentVDOP = 0.0;

const char* ssid = "Fizin";
const char* password = "hotspot???";

// --- FPS Counter Variables ---
uint16_t fpsFrames = 0;       // Pembilang frame
uint16_t currentFPS = 0;      // Nilai FPS yang akan dipaparkan
unsigned long lastFpsMillis = 0;

//untuk draw constant teks sekali sahaja
bool drawNow = true;

//small text, offset 24px vertically

//textbox setup
SmartTextBox boxSetup(tft.height()/2, tft.width()/2, 300, MONT_B_25, TFT_BLACK, TFT_WHITE, false);

//status bar
SmartTextBox boxClock(34,  5, 60, MONT_M_18, TFT_BLACK, TFT_WHITE, false);
SmartTextBox WifiIcon(260, 5, 24, CUSTOM_ICONS, TFT_BLACK, TFT_WHITE, false);
SmartTextBox pressureIcon(260, 5, 24, CUSTOM_ICONS, TFT_BLACK, TFT_WHITE, false);
SmartTextBox boxTitle(70, 5, 170, MONT_M_18, TFT_BLACK, TFT_WHITE, true);
DrawSymbol   statusLora(278, 9, CUSTOM_ICONS); 
DrawSymbol   statusBase(316, 9, CUSTOM_ICONS); 
DrawSymbol   statusRover(404, 9, CUSTOM_ICONS);

//page 1 (home)
SmartTextBox BoxHeader1Page1  (35, 39, 65, MONT_B_25, TFT_BLACK, TFT_WHITE, true);                 //GPS
SmartTextBox Col1Line1Page1(35 , 70 , 140, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //jarak antara 2 point
SmartTextBox Col1Line2Page1(35 , 94 , 140, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //bilangan satelit rover
SmartTextBox Col1Line3Page1(35 , 118, 140, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //location gdm2000
SmartTextBox Col1Line4Page1(35 , 142, 140, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //coordinate system

SmartTextBox Col2Line1Page1(194, 70 , 106, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //text base
SmartTextBox Col3Line1Page1(300, 70 , 73 , CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //hdop base
SmartTextBox Col4Line1Page1(373, 70 , 81 , CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //percent battery base
SmartTextBox Col2Line2Page1(194, 94 , 260, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //koordinat base

SmartTextBox Col2Line3Page1(194, 118, 106, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //text rover
SmartTextBox Col3Line3Page1(300, 118, 73 , CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //hdop rover
SmartTextBox Col4Line3Page1(373, 118, 81 , CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //percent battery rover
SmartTextBox Col2Line4Page1(194, 142, 260, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //koordinat rover

SmartTextBox BoxHeader2Page1  (35, 173, 80, MONT_B_25, TFT_BLACK, TFT_WHITE, true);                 //LoRa
SmartTextBox Col1Line5Page1(35 , 204, 200, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //status connected
SmartTextBox Col1Line6Page1(35 , 228, 200, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //status ready/receiving
SmartTextBox Col1Line7Page1(35 , 252, 100, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //rssi
SmartTextBox Col1Line8Page1(35 , 276, 100, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //snr

SmartTextBox BoxHeader3Page1  (264, 173, 86, MONT_B_25, TFT_BLACK, TFT_WHITE, true);                //Other
SmartTextBox Col2Line5Page1(264, 204, 100, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //storage used
SmartTextBox Col2Line6Page1(264, 228, 100, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //total placemark
SmartTextBox Col3Line5Page1(367, 204, 80,  CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true);  //north heading
SmartTextBox Col3Line6Page1(367, 228, 80,  CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true);  //altitude difference

TouchButton Page1BtnOff(254, 257, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);//off
TouchButton Page1BtnSetup(338, 257, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);//setup

//page 2 (setup)
SmartTextBox Col1Line1Page2(20,  38,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //upload file status
SmartTextBox Col1Line2Page2(20,  62,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //upload file desc
SmartTextBox Col1Line3Page2(20,  142, 220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //tare altitude
SmartTextBox Col1Line4Page2(20,  166, 220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //tare altitude desc
SmartTextBox Col2Line1Page2(249, 38,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //calibrate
SmartTextBox Col2Line2Page2(249, 62,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //calibrate desc
SmartTextBox Col2Line3Page2(249, 142, 220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //calibrate
SmartTextBox Col2Line4Page2(249, 166, 220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //calibrate desc

TouchButton Page2BtnUpload(20, 92, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);
TouchButton Page2BtnTare(20, 196, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);
TouchButton Page2BtnCal(249, 92, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);
TouchButton Page2BtnManage(249, 196, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);
TouchButton Page2BtnBack(20, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);
TouchButton Page2BtnSetup2(350, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //next> (to setup2)

//page 3 (manage data)
SmartTextBox Col1Line1Page3(12,  42 , 400, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //description

SmartTextBox Col1Line3Page3(12,  76,  280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //header row (name)
SmartTextBox Col1Line4Page3(12,  100, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 1, col 1 (name)
SmartTextBox Col1Line5Page3(12,  124, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 2, col 1 (name)
SmartTextBox Col1Line6Page3(12,  148, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 3, col 1 (name)
SmartTextBox Col1Line7Page3(12,  172, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 4, col 1 (name)
SmartTextBox Col1Line8Page3(12,  196, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 5, col 1 (name)
SmartTextBox Col1Line9Page3(12,  220, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 6, col 1 (name)

SmartTextBox Col2Line3Page3(300, 76,  168, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //header row (details)
SmartTextBox Col2Line4Page3(300, 100, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lat
SmartTextBox Col2Line5Page3(300, 124, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lon
SmartTextBox Col2Line6Page3(300, 148, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //alt
SmartTextBox Col2Line7Page3(300, 196, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //page
SmartTextBox Col2Line8Page3(300, 220, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //id

SmartTextBox Col3Line4Page3(350, 100, 118, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lat value
SmartTextBox Col3Line5Page3(350, 124, 118, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lon value
SmartTextBox Col3Line6Page3(350, 148, 118, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //alt value
SmartTextBox Col3Line7Page3(370, 196, 98 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //page value
SmartTextBox Col3Line8Page3(370, 220, 98 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //id value

TouchButton Page3BtnBack(20,  264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //<back
TouchButton Page3BtnPrev(165, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //<
TouchButton Page3BtnNext(215, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //>
TouchButton Page3BtnUp  (265, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //up
TouchButton Page3BtnDown(315, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //down
TouchButton Page3BtnAdd (365, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //+
TouchButton Page3BtnDel (415, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //-

//page 4 (setup 2)
SmartTextBox Col1Line1Page4(20,  38,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Coordinate System (WGS84/GDM2000)
SmartTextBox Col1Line2Page4(20,  62,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //[system coordinate sekarang]

SmartTextBox Col1Line3Page4(20, 110,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //GDM2000 Zone
SmartTextBox Col1Line4Page4(20, 134,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //[zone GDM2000 sekarang]

SmartTextBox Col1Line5Page4(20, 182,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Navigation mode
SmartTextBox Col1Line6Page4(20, 206,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //choose place

TouchButton Page4BtnChooseCoordSystem(249, 38, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //WGS84
TouchButton Page4BtnSelectZone(249, 110, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //Select zone...
TouchButton Page4BtnNavigation(249, 182, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //start

TouchButton Page4BtnBack(12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page4BtnNext(350, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //next

//page 5 (select GDM2000 zone). design sama dengan MANAGE DATA
SmartTextBox Col1Line1Page5(12,  42 , 400, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //description

SmartTextBox Col1Line3Page5(12,  76,  280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //header row (negeri)
SmartTextBox Col1Line4Page5(12,  100, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 1, col 1 (negeri)
SmartTextBox Col1Line5Page5(12,  124, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 2, col 1 (negeri)
SmartTextBox Col1Line6Page5(12,  148, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 3, col 1 (negeri)
SmartTextBox Col1Line7Page5(12,  172, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 4, col 1 (negeri)
SmartTextBox Col1Line8Page5(12,  196, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 5, col 1 (negeri)
SmartTextBox Col1Line9Page5(12,  220, 280, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //row 6, col 1 (negeri)

SmartTextBox Col2Line3Page5(300, 76,  168, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //header row (details)
SmartTextBox Col2Line4Page5(300, 100, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lat
SmartTextBox Col2Line5Page5(300, 124, 60 , MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lon

SmartTextBox Col3Line4Page5(350, 100, 118, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lat value
SmartTextBox Col3Line5Page5(350, 124, 118, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //lon value

TouchButton Page5BtnPrev(15,  264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //<
TouchButton Page5BtnNext(65,  264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //>
TouchButton Page5BtnUp(115, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //up
TouchButton Page5BtnDown(165, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //down
TouchButton Page5BtnBack(320, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //SELECT
TouchButton Page5BtnSetup2(350, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //next> (to setup2)

// //page 6 WIFISETUP Page

SmartTextBox Col1Line1Page6(14,  46,  220, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //upload file status

TouchButton  Page6BtnBack  (12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //SELECT
TouchButton  Page6BtnOnWifi  (0, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //SELECT

//page 7 CALIBRATION Page

SmartTextBox Col1Line1Page7(14,  46,  460, CUSTOM_ICONS, MONT_B_25, TFT_BLACK, TFT_WHITE, true); //Tap calibrate to start calibrate
SmartTextBox Col1Line2Page7(14,  76,  460, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Tap calibrate to start calibrate

TouchButton  Page7BtnBack     (12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //BACK
TouchButton  Page7BtnCalibrate(12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //CALIBRATE

// //page 8 NAVIGATING Page
SmartTextBox Col1Line1Page8(152,  38, 400, MONT_B_25, TFT_BLACK, TFT_WHITE, true); //12312m
SmartTextBox Col1Line2Page8(152,  67, 100, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Bearing
SmartTextBox Col1Line3Page8(152,  91, 100, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //B. Bearing
SmartTextBox Col1Line4Page8(152, 115, 100, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Target

SmartTextBox Col2Line2Page8(262,  67, 130, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //[>>] 124°
SmartTextBox Col2Line3Page8(262,  91, 130, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //180° 
SmartTextBox Col2Line4Page8(262, 115, 200, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //123.123123,\n123.123123

TouchButton Page8BtnBack             ( 12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page8BtnMark             ( 12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //mark
TouchButton Page8BtnChooseCoordSystem(152, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //ChooseCoordSystem
TouchButton Page8BtnConfirm          (300, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //confirm

// //page 9 VIEWKML Page
SmartTextBox Col1Line1Page9(260, 46 ,220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //choose path
SmartTextBox Col1Line2Page9(260, 70 ,220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //[selected path]
SmartTextBox Col1Line3Page9(260, 94 ,220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //[selected path]

TouchButton Page9BtnBack(12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page9BtnUp  (265, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //up
TouchButton Page9BtnDown(315, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //down
TouchButton Page9BtnDel (415, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //delete
TouchButton Page9BtnDone(215, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //Done

//page 10 modeselection page design sama dengan setup 2

SmartTextBox Col1Line1Page10(20,  38,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //view kml file
SmartTextBox Col1Line2Page10(20,  62,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //here

SmartTextBox Col1Line3Page10(20, 110,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Delay to dim
SmartTextBox Col1Line4Page10(20, 134,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //

SmartTextBox Col1Line5Page10(20, 182,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Delay to screen off
SmartTextBox Col1Line6Page10(20, 206,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //

TouchButton Page10BtnViewKML (249, 38, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE);   //view kml
TouchButton Page10BtnDelayDim(249, 110, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //delay dim
TouchButton Page10BtnDelayOff(249, 182, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //delay off

TouchButton Page10BtnBack(12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page10BtnNext(350, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //next

//page 11 Navigating Choose File
SmartTextBox Col1Line1Page11(220,  46 , 250, MONT_B_25, TFT_BLACK, TFT_WHITE, true); //selected name scroll
SmartTextBox Col1Line2Page11(220,  84 , 250, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //distance
SmartTextBox Col1Line3Page11(220, 108 , 250, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //point n/total
SmartTextBox Col1Line4Page11(220, 132 , 250, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //selected point coordinate
SmartTextBox Col1Line5Page11(114, 130, 180, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, false); //loading map...

TouchButton Page11BtnSkipLeft (0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //<
TouchButton Page11BtnLeft     (0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //>
TouchButton Page11BtnRight    (0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //up
TouchButton Page11BtnSkipRight(0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //down

TouchButton Page11BtnBack  (12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //<back (ke modeselection)
TouchButton Page11BtnSelect(0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //NEXT (ke navigatingplace)
TouchButton Page11BtnRemoveOffset(0  , 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //remove offset

//page 12 (navigating to place) ui sama dengan NAVIGATING
SmartTextBox Col1Line1Page12(160,  40, 300, MONT_B_25, TFT_BLACK, TFT_WHITE, true); //12312m
SmartTextBox Col1Line2Page12(160,  70, 300, MONT_B_25, TFT_BLACK, TFT_WHITE, true); //CTE
SmartTextBox Col1Line3Page12(160, 103, 300, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //direction
SmartTextBox Col1Line4Page12(160, 127, 300, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //distance gps
SmartTextBox Col1Line5Page12(160, 151, 300, CUSTOM_ICONS, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //coord

TouchButton Page12BtnBack             ( 12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page12BtnMark             ( 12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //mark
TouchButton Page12BtnTare             ( 12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //mark
TouchButton Page12BtnChooseCoordSystem(152, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //ChooseCoordSystem
TouchButton Page12BtnConfirm          (300, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //confirm
TouchButton Page12BtnDoneCalibration  (250, 234, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //done calibration


//page 13 (done navigation)
SmartTextBox Col1Line1Page13(20,  38,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //View Project
SmartTextBox Col1Line2Page13(20,  62,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //

SmartTextBox Col1Line3Page13(20, 110,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Final Report
SmartTextBox Col1Line4Page13(20, 134,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //

SmartTextBox Col1Line5Page13(20, 182,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //Delete Project
SmartTextBox Col1Line6Page13(20, 206,  220, MONT_M_18, TFT_BLACK, TFT_WHITE, true); //

TouchButton Page13BtnViewProject(249, 38, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //View Project
TouchButton Page13BtnFinalReport(249, 110, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //Final Report
TouchButton Page13BtnDeleteProject(249, 182, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //Delete Project

TouchButton Page13BtnBack(12, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //back
TouchButton Page13BtnNext(350, 264, TFT_LIGHTGREY, CUSTOM_ICONS, MONT_M_18, TFT_WHITE); //next


























void setup() {
  // ---------------------------------------------------------
  // 1. HARDWARE INIT (Paling Asas)
  // ---------------------------------------------------------
  
  // Power up pin penting
  
  tft.init();
  tft.setRotation(3); // Landscape
  tft.fillScreen(TFT_WHITE);
  turnOnDevice();
  boxSetup.text("Rebooting..."); 
  gpio_hold_dis(GPIO_NUM_13);
  gpio_hold_dis((gpio_num_t)I2C_SDA);
  gpio_hold_dis((gpio_num_t)I2C_SCL);
  cuciBusAgresif();
  pinMode(BUCK_EN, OUTPUT);
  digitalWrite(BUCK_EN, HIGH); // Hidupkan power sistem
  pinMode(LCD_LED, OUTPUT);
  // digitalWrite(LCD_LED, HIGH); // Hidupkan backlight
  
  // Init Serial
  Serial.begin(115200);
  // Tunggu sekejap untuk Serial (Max 2 saat) supaya tak terlepas log awal
  unsigned long serialWait = millis();
  while (!Serial && millis() - serialWait < 2000) delay(10);
  
  Serial.println("\n========== ROVER BOOT START ==========");

  // Init LED Pixel (Status Indicator)
  pixels.begin();
  pixels.setBrightness(50);
  pixels.clear();
  pixels.show();

  // ---------------------------------------------------------
  // 2. STORAGE SYSTEM (Kritikal untuk Config)
  // ---------------------------------------------------------
  Serial.println("[INIT] Mounting LittleFS...");
  // Kita guna begin(true) supaya dia auto-format jika partition rosak
  if (LittleFS.begin(true)) { 
      Serial.println("[OK] LittleFS Berjaya Mount!");
      Serial.printf("[INFO] Total Space: %d bytes\n", LittleFS.totalBytes());
      Serial.printf("[INFO] Used Space: %d bytes\n", LittleFS.usedBytes());
  } else {
      Serial.println("[FAIL] LittleFS Gagal Mount! Check Partition.");
      // Kita tak stop sini, teruskan je dulu
  }

  // Check PSRAM (Untuk kepastian)
  Serial.printf("[INFO] Total PSRAM: %d bytes\n", ESP.getPsramSize());

  // ---------------------------------------------------------
  // 3. DISPLAY SYSTEM (Supaya user nampak status)
  // ---------------------------------------------------------
  Serial.println("[INIT] Menghidupkan Skrin TFT...");
  // tft.init();
  // tft.setRotation(3); // Landscape
  // tft.fillScreen(TFT_WHITE);
  
  // Sekarang kita boleh guna boxSetup
  // boxSetup.text("Storage: " + String(LittleFS.totalBytes() / 1024) + " KB");

  // ---------------------------------------------------------
  // 4. LORA COMMUNICATION (SPI)
  // ---------------------------------------------------------
  Serial.println("[INIT] Setup SPI & LoRa...");
  // boxSetup.text("Init LoRa...");
  
  sharedSPI.begin(36, 37, 35); // SCK, MISO, MOSI
  LoRa.setSPI(sharedSPI);
  LoRa.setPins(CS_LORA, RST_LORA, DIO_LORA);

  if (!LoRa.begin(915E6)) {
    Serial.println("[FAIL] LoRa Gagal!");
    // boxSetup.text("LoRa: GAGAL!", TFT_RED);
  } else {
    Serial.println("[OK] LoRa Berjaya");
    // boxSetup.text("LoRa: OK", TFT_GREEN);
  }

  // ---------------------------------------------------------
  // 5. TOUCHSCREEN & PREFERENCES
  // ---------------------------------------------------------
  Serial.println("[INIT] Setup Touch & Config...");
  // boxSetup.text("Load Config...");

  uint16_t calData[5] = { 253, 3666, 209, 3605, 1 };
  tft.setTouch(calData);

  preferences.begin("lora-cfg", true);
  float currentOffset = preferences.getFloat("offset", 0.0);
  preferences.end();
  Serial.print("[INFO] Loaded Offset: "); Serial.println(currentOffset);

  // ---------------------------------------------------------
  // 6. I2C SENSORS (Compass, Baro, Power)
  // ---------------------------------------------------------
  Serial.println("[INIT] Memulakan I2C Bus...");
  // 1. Pastikan pin tak kena "Hold" (Sebab awak guna Sleep)

  // 2. Start Wire sekejap untuk hantar arahan maut
  Wire.begin(I2C_SDA, I2C_SCL);
  paksaResetBMP(); // Tembak reset ke sensor dulu!

  // --- HMC5883L ---
  Serial.println("[INIT] Check Compass (HMC5883L)...");
  // boxSetup.text("Init Compass...");
  
  Wire.beginTransmission(0x1E);
  if (Wire.endTransmission() == 0) {
    Serial.println("[OK] Compass Dikesan");
    mag.begin(); 
    // boxSetup.text("Compass: OK", TFT_GREEN);
  } else {
    Serial.println("[FAIL] Compass Tiada!");
    // boxSetup.text("Compass: FAIL", TFT_RED);
  }

  // --- BMP280 ---
  Serial.println("[INIT] Check Barometer (BMP280)...");
  // boxSetup.text("Init Baro...");
  if (!bmp.begin(0x76)) {
    Serial.println("[FAIL] BMP280 Gagal");
    // boxSetup.text("Baro: FAIL", TFT_RED);
  } else {
    Serial.println("[OK] BMP280 Berjaya");
    // boxSetup.text("Baro: OK", TFT_GREEN);
  }

  // --- INA219 ---
  Serial.println("[INIT] Check Power Monitor (INA219)...");
  if (!ina219.begin()) {
    Serial.println("[FAIL] INA219 Gagal");
    // boxSetup.text("Power: FAIL", TFT_RED);
    // while (1); // Jangan matikan sistem, cuma lapor error
  } else {
    Serial.println("[OK] INA219 Berjaya");
  }

  // ---------------------------------------------------------
  // 7. GPS MODULE (Fast Check Only)
  // ---------------------------------------------------------
  Serial.println("[INIT] Memulakan GPS Serial...");
  // boxSetup.text("Init GPS...");

  // 1. Tetapkan Baudrate 115200
  gpsSerial.begin(115200, SERIAL_8N1, RX, TX); 
  // delay(1000); 

  Serial.println("Konfigurasi ROVER bermula...");

  // 2. Set Update Rate ke 10Hz (Sama macam Base)
  gpsSerial.println("$PQTMGNSSRATE,100*39");
  delay(100);

  // 3. Track Satelit Paling Banyak (Sama macam Base)
  gpsSerial.println("$PQTMGNSS,1,1,1,1,1,0*3E");
  delay(100);

  // 4. Mesej NMEA yang SAMA (Hanya RMC, GGA dan VTG)
  // Memastikan Rover tak hantar mesej merapu yang Base tak hantar
  gpsSerial.println("$PQTMNMEOUT,1,1,0,0,0,1,0,0,0,0,0*3E");
  delay(100);

  // 5. Navigation Mode: Pedestrian (Sesuai untuk Rover yang bergerak perlahan)
  gpsSerial.println("$PQTMNAVMODE,2*39"); 
  delay(100);

  // Simpan ke Flash Memory
  gpsSerial.println("$PQTMSAVEPAR*5A");
  delay(100);

  Serial.println("ROVER sedia digunakan!");

  // Kita cuma check adakah module respon sekejap (1 saat max)
  // Tak perlu tunggu FIX di sini. Biar TaskGPS buat kerja tu.
  unsigned long gpsCheckStart = millis();
  bool gpsDataIncoming = false;
  
  while (millis() - gpsCheckStart < 1000) {
    if (gpsSerial.available()) {
      gpsDataIncoming = true;
      break; // Okay, ada data masuk! Teruskan.
    }
  }

  if(gpsDataIncoming) {
      Serial.println("[OK] GPS Module Detected (Data Stream OK)");
      // boxSetup.text("GPS: Detected", TFT_GREEN);
  } else {
      Serial.println("[WARN] GPS Module Senyap (Mungkin warm-up)");
      // boxSetup.text("GPS: No Data", TFT_YELLOW);
  }

  // ---------------------------------------------------------
  // 8. MULTITASKING (Core 0)
  // ---------------------------------------------------------
  Serial.println("[INIT] Memulakan Background Task...");
  // boxSetup.text("Start Core 0...");

  
  xTaskCreatePinnedToCore(
    codeForCore0,
    "TaskGPS",
    10000,
    NULL,
    1,
    &TaskGPS,
    0
  );

  // ---------------------------------------------------------
  // 9. SOFTWARE MODULES
  // ---------------------------------------------------------
  // Nota: Letak print sebelum function supaya kalau hang, kita tahu modul mana
  
  Serial.println("[INIT] LoRa Manager...");
  // boxSetup.text("Init LoRa Sys...");
  loraSys.begin(); // Hati-hati, ini mungkin ambil masa untuk sync

  Serial.println("[INIT] Mission Manager...");
  // boxSetup.text("Init Mission...");
  mission.begin(); 

  Serial.println("[INIT] Compass Logic...");
  compass.begin();

  Serial.println("[INIT] Navigation Engine...");
  nav.begin();

  // Setup display CTE
  // x=10, y=10, width=100, height=200, MaxError=5.0 meter
  cte.begin();

  bl.begin(); //delay dim/off

  coordinate.begin(); //coordinate conversion

  // ---------------------------------------------------------
  // 10. SELESAI
  // ---------------------------------------------------------
  Serial.println("========== SETUP SELESAI ==========");
  boxSetup.text("", (uint16_t)TFT_GREEN);
  tft.fillScreen(TFT_WHITE); // Atau TFT_BLACK ikut selera awak
}

void loop() {

  // 1. KIRA FPS (Setiap kali loop pusing, kita tambah 1 frame)
  fpsFrames++;
  
  // Update nilai FPS setiap 1 saat (1000ms)
  if (millis() - lastFpsMillis >= 1000) {
    currentFPS = fpsFrames;
    fpsFrames = 0;
    lastFpsMillis = millis();

    uint32_t totalHeap = ESP.getHeapSize(); // Jumlah SRAM yang tersedia untuk aplikasi
    uint32_t freeHeap = ESP.getFreeHeap();  // Baki SRAM yang belum diguna
    uint32_t usedHeap = totalHeap - freeHeap;
    
    float usagePercentage = ((float)usedHeap / (float)totalHeap) * 100.0;

    // Serial.printf("Total SRAM: %u bytes\n", totalHeap);
    // Serial.printf("Used SRAM: %u bytes\n", usedHeap);
    // Serial.printf("Penggunaan SRAM: %.2f%%\n", usagePercentage);
    
    // Paparkan pada skrin ram usage
    // boxClock.text(String(usagePercentage,1));
  }

  // Update bateri
  busVoltage = ina219.getBusVoltage_V();
  current_mA = -ina219.getCurrent_mA();
  batMon.getData(busVoltage, -current_mA);

  // // 4. Debugging Output (Setiap 1 saat)
  // static unsigned long timer = 0;
  // if (millis() - timer > 1000) {
  //   timer = millis();
  //   Serial.print("V: "); Serial.print(busVoltage);
  //   Serial.print(" | State: "); Serial.print(batMon.statusMsg);
    
  //   if (batMon.isCalibrated) {
  //     Serial.print(" | "); Serial.print(batMon.percentage); Serial.print("%");
  //     Serial.print(" | Time: "); Serial.print(batMon.timeStr);
  //   }
  //   Serial.println();
  // }

  //run alt
  currentAlt = bmp.readAltitude(1014);

  //run lora
  loraSys.run(currentAlt, sharedLat, sharedLon);

  String sendToPython =
  "GPS," + String(loraSys.rxCoordinate.lat,6)
  + ","  + String(loraSys.rxCoordinate.lon,6)
  + ","  + String(loraSys.txCoordinate.lat,6)
  + ","  + String(loraSys.txCoordinate.lon,6)
  + ","  + String(loraSys.baseHdop,1)
  + ","  + String(sharedHDOP,1);

  // Serial.println(sendToPython);

  //update kandungan data
  mission.update();
  statusBar(); 
  bl.controlBacklight();

  //test mode long dan short range
  if (Serial.available()) {
    // mode lora
    char c = Serial.read();
    
    if (c == 'r' || c == 'R') {
      while (Serial.available()) Serial.read();
      loraSys.restartBase();
    }
    
    // --- KAWALAN PETA (MAP VIEWER) ---
    else if (c == '+') {
      mapViewer.zoomIn();
      Serial.println("[MAP] Zoom In");
    }
    else if (c == '-') {
      mapViewer.zoomOut();
      Serial.println("[MAP] Zoom Out");
    }
    else if (c == 'w' || c == 'W') {
      mapViewer.panUp();
      Serial.println("[MAP] Pan Atas");
    }
    else if (c == 's' || c == 'S') {
      mapViewer.panDown();
      Serial.println("[MAP] Pan Bawah");
    }
    else if (c == 'a' || c == 'A') {
      mapViewer.panLeft();
      Serial.println("[MAP] Pan Kiri");
    }
    else if (c == 'd' || c == 'D') {
      mapViewer.panRight();
      Serial.println("[MAP] Pan Kanan");
    }
    else if (c == '0') {
      mapViewer.resetView();
      Serial.println("[MAP] Reset View");
    }
  }

  // 1. Semak jika ada perubahan page (Event Trigger)
  if (pageChanged) {
    // a. Clear Skrin
    tft.fillRect(0, 31, tft.width(), tft.height()-31, TFT_WHITE);
    
    // b. draw semula constant text
    drawNow = true;
    // forceRedraw = true; // (Buka komen ini jika perlu)
    
    // c. PENTING: Beritahu semua textbox dan button yang skrin dah bersih!
    SmartTextBox::forceScreenUpdate();
    TouchButton::resetAllButtons(); 
    mapViewer.forceRefresh();

    // d. Dapatkan maklumat storan
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    if (totalBytes > 0) {
      usedStoragePercent = ((float)usedBytes / (float)totalBytes) * 100.0;
    }

    // e. Tutup penanda supaya kod ini tidak diulang-ulang pada setiap loop
    pageChanged = false; 
  }

  //-----------------------------------------------------------------------------------------------------------------

  // Jalankan fungsi untuk skrin semasa
  switch (currentState) {
    case HOME:
      HOMEPage();
      break;
    case SETUP:
      SETUPPage();
      break;
    case MANAGEDATA:
      MANAGEDATAPage();
      break;
    case SETUP2:
      SETUP2Page();
      break;
    case SELECTZONE:
      SELECTZONEPage();
      break;
    case MODESELECTION:
      MODESELECTIONPage();
      break;
    case NAVIGATINGCHOOSEPLACE:
      NAVIGATINGCHOOSEPLACEPage();
      break;
    case NAVIGATINGPLACE:
      NAVIGATINGPLACEPage();
      break;
    case WIFISETUP:
      WIFISETUPPage();
      break;
    case VIEWKML:
      VIEWKMLPage();
      break;
    case CALIBRATION:
      CALIBRATIONPage();
      break;
    case DONENAVIGATION:
      DONENAVIGATIONPage();
      break;
  }

}