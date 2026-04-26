#ifndef DRAW_SYMBOL_H
#define DRAW_SYMBOL_H

#include <TFT_eSPI.h>

extern TFT_eSPI tft;

// Definisi Unicode (Pastikan font anda support character ini)
#define ICON_SIG_1 "\uf691" 
#define ICON_SIG_2 "\uf692" 
#define ICON_SIG_3 "\uf693" 
#define ICON_SIG_4 "\uf690" 
#define ICON_SIG_X "\uf694" 

#define ICON_BAT_100  "\uf240" 
#define ICON_BAT_75   "\uf241" 
#define ICON_BAT_50   "\uf242" 
#define ICON_BAT_25   "\uf243" 
#define ICON_BAT_0    "\uf244" 
#define ICON_BAT_CHG  "\uf376" 

class DrawSymbol {
  private:
    int _x, _y;
    
    // UBAH 1: Pointer untuk Raw Bitmap Font (Array)
    const uint8_t* _iconFontData; 

    float _lastSignalValue;
    int _lastBatteryValue;
    String _lastSymbolName;
    uint16_t _lastSymbolColor, _lastBatteryColor, _lastBgColor;
    bool _lastShowSignalText;
    bool _lastIsCharging;
    bool _lastIsConnected;

  public:
    // UBAH 2: Constructor terima raw array (const uint8_t*)
    DrawSymbol(int x, int y, const uint8_t* iconFontData) {
      _x = x; _y = y;
      _iconFontData = iconFontData;

      _lastSignalValue = -999.0;
      _lastBatteryValue = -1;
      _lastSymbolName = "";
      _lastSymbolColor = 0xFFFF;
      _lastBatteryColor = 0xFFFF;
      _lastBgColor = 0xFFFF;
      _lastShowSignalText = false;
      _lastIsCharging = false;
      _lastIsConnected = true;
    }

    // ==========================================
    // FUNGSI 1: DRAW PENUH (SIGNAL + BATERI + TEKS)
    // ==========================================
    void draw(float signalValue, float minSignalValue, float maxSignalValue, int batteryValue, String symbolName, uint16_t symbolColor, uint16_t batteryColor, uint16_t bgColor, bool showSignalText, bool isCharging, bool isConnected) {
      
      bool signalChanged = (abs(signalValue - _lastSignalValue) > 0.01);
      bool batteryChanged = (batteryValue != _lastBatteryValue);
      bool stateChanged = (isCharging != _lastIsCharging || isConnected != _lastIsConnected);
      bool nameChanged = (symbolName != _lastSymbolName);
      bool settingsChanged = (symbolColor != _lastSymbolColor || batteryColor != _lastBatteryColor || bgColor != _lastBgColor || showSignalText != _lastShowSignalText);
      
      bool forceRedraw = settingsChanged || stateChanged;

      int sigIconX = _x;
      
      // KEMASKINI 1: Ubah jarak dari +21 ke +26 supaya background bateri tak tindih signal
      int batIconX = _x + 26; 
      int textStartX = batIconX + 20; 

      // ----------------------------------------
      // 1. LUKIS ICON SIGNAL
      // ----------------------------------------
      if (signalChanged || forceRedraw) {
        tft.fillRect(sigIconX, _y, 22, 18, bgColor);

        // --- PRE-CALCULATE COORDINATES & SIZE (Untuk Wiping) ---
        // Kita kira dulu di sini supaya boleh guna untuk padam sebelum lukis X
        tft.setTextFont(1);
        int textValX = sigIconX - 8; 
        int textValY = _y + 7;        
        int wipeWidth = tft.textWidth("888");
        int wipeHeight = tft.fontHeight(1);

        String signalIconStr = "";
        uint16_t currentSigColor = symbolColor;

        if (!isConnected || signalValue == 0.0) { // TAMBAH: signalValue == 0.0
          // PADAM TEKS DULU SEBELUM LUKIS ICON X
          tft.fillRect(textValX, textValY - wipeHeight + 1, wipeWidth, wipeHeight + 1, bgColor);
          
          signalIconStr = ICON_SIG_X;
          currentSigColor = TFT_RED; 
        } else {
          int activeBars = 0;
          if (minSignalValue < 0 && maxSignalValue < 0) { // RSSI
             float range = maxSignalValue - minSignalValue;
             float step = range / 4.0f;
             if (signalValue >= maxSignalValue - step) activeBars = 4;
             else if (signalValue >= maxSignalValue - (step * 2)) activeBars = 3;
             else if (signalValue >= maxSignalValue - (step * 3)) activeBars = 2;
             else if (signalValue >= minSignalValue) activeBars = 1;
          } else { // HDOP
             float range = maxSignalValue - minSignalValue;
             float step = range / 4.0f;
             if (signalValue <= minSignalValue + step) activeBars = 4;
             else if (signalValue <= minSignalValue + (step * 2)) activeBars = 3;
             else if (signalValue <= minSignalValue + (step * 3)) activeBars = 2;
             else if (signalValue <= maxSignalValue) activeBars = 1;
          }

          switch(activeBars) {
            case 4: signalIconStr = ICON_SIG_4; break;
            case 3: signalIconStr = ICON_SIG_3; break;
            case 2: signalIconStr = ICON_SIG_2; break;
            case 1: signalIconStr = ICON_SIG_1; break;
            default: 
              // PADAM TEKS DULU SEBELUM LUKIS ICON X (Untuk kes default/error)
              tft.fillRect(textValX, textValY - wipeHeight + 1, wipeWidth, wipeHeight + 1, bgColor);
              signalIconStr = ICON_SIG_X; 
              currentSigColor = TFT_RED; 
              break;
          }
        }

        tft.loadFont(_iconFontData); 
        tft.setTextColor(currentSigColor, bgColor);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(signalIconStr, sigIconX, _y);
        tft.unloadFont(); 

        // --- LUKIS TEKS NILAI (JIKA VALID) ---
        // Nota: Kalau tadi dah padam (kes X), if() di bawah akan pastikan ia kekal padam 
        // sebab !isConnected atau teks akan dihiddenkan.
        
        tft.setTextPadding(0);
        tft.setTextDatum(BL_DATUM); 
        String valStr = String(signalValue, 1);
        
        // HANYA lukis jika bukan 0.0 dan connected
        if (showSignalText && isConnected && signalValue != 0.0 && valStr.length() <= 3) {
            tft.setTextColor(symbolColor, bgColor);
            tft.drawString(valStr, textValX, textValY);
        } else if (isConnected) { // Hanya padam jika connected tapi teks terlalu panjang/hidden
             tft.fillRect(textValX, textValY - wipeHeight + 1, wipeWidth, wipeHeight + 1, bgColor);
        }
        
        _lastSignalValue = signalValue;
      }

      // ----------------------------------------
      // 2. LUKIS ICON BATERI
      // ----------------------------------------
      if (batteryChanged || forceRedraw) {
        tft.fillRect(batIconX, _y - 1, 15, 21, bgColor);

        String batIconStr = "";
        if (isCharging) {
          batIconStr = ICON_BAT_CHG;
        } else {
          int val = constrain(batteryValue, 0, 100);
          if (val > 75) batIconStr = ICON_BAT_100;
          else if (val > 50) batIconStr = ICON_BAT_75;
          else if (val > 25) batIconStr = ICON_BAT_50;
          else if (val > 5)  batIconStr = ICON_BAT_25;
          else batIconStr = ICON_BAT_0;
        }

        TFT_eSprite spr = TFT_eSprite(&tft);
        spr.createSprite(24, 24); 
        spr.fillSprite(bgColor); 

        spr.loadFont(_iconFontData);
        spr.setTextColor(batteryColor, bgColor);
        spr.setTextDatum(MC_DATUM); 
        spr.drawString(batIconStr, 12, 12);
        spr.unloadFont(); 

        tft.setPivot(batIconX + 7, _y + 7);
        spr.pushRotated(-90);
        spr.deleteSprite();

        _lastBatteryValue = batteryValue;
      }

      // ----------------------------------------
      // 3. LUKIS LABEL TEKS
      // ----------------------------------------
      if (nameChanged || batteryChanged || forceRedraw) {
        tft.setTextFont(1);
        tft.setTextColor(symbolColor, bgColor);
        tft.setTextDatum(ML_DATUM); 
        
        tft.setTextPadding(tft.textWidth("BASE")); 
        tft.drawString(symbolName, textStartX, _y + 3); 
        
        tft.setTextPadding(tft.textWidth("100% ")); 
        tft.drawString(String(batteryValue) + "%", textStartX, _y + 12); 
        tft.setTextPadding(0);
      }

      _lastSymbolName = symbolName;
      _lastSymbolColor = symbolColor;
      _lastBatteryColor = batteryColor;
      _lastBgColor = bgColor;
      _lastShowSignalText = showSignalText;
      _lastIsCharging = isCharging;
      _lastIsConnected = isConnected;
    }
    
    // Helper Overload
    void draw(float signalValue, float minSignalValue, float maxSignalValue, int batteryValue, String symbolName, uint16_t symbolColor, uint16_t batteryColor, uint16_t bgColor) {
      draw(signalValue, minSignalValue, maxSignalValue, batteryValue, symbolName, symbolColor, batteryColor, bgColor, false, false, true);
    }

    // ==========================================
    // FUNGSI 2: DRAW SIGNAL SAHAJA (RSSI)
    // ==========================================
    // UBAH: Buang parameter showSignalText sebab tak perlukan teks lagi
    void drawSignal(float signalValue, float minSignalValue, float maxSignalValue, uint16_t symbolColor, uint16_t bgColor, bool isConnected) {
      
      bool signalChanged = (abs(signalValue - _lastSignalValue) > 0.01);
      bool stateChanged = (isConnected != _lastIsConnected);
      bool settingsChanged = (symbolColor != _lastSymbolColor || bgColor != _lastBgColor);
      bool forceRedraw = settingsChanged || stateChanged;

      int sigIconX = _x;

      if (signalChanged || forceRedraw) {
        tft.fillRect(sigIconX, _y, 22, 18, bgColor);

        String signalIconStr = "";
        uint16_t currentSigColor = symbolColor;

        // UBAH: Jadikan merah jika disconnected ATAU nilai signal == 0.0
        if (!isConnected || signalValue == 0.0) {
            signalIconStr = ICON_SIG_X;
            currentSigColor = TFT_RED;
        } else {
            int activeBars = 0;
            if (minSignalValue < 0 && maxSignalValue < 0) {
                float range = maxSignalValue - minSignalValue;
                float step = range / 4.0f;
                if (signalValue >= maxSignalValue - step) activeBars = 4;
                else if (signalValue >= maxSignalValue - (step * 2)) activeBars = 3;
                else if (signalValue >= maxSignalValue - (step * 3)) activeBars = 2;
                else if (signalValue >= minSignalValue) activeBars = 1;
            } else {
                float range = maxSignalValue - minSignalValue;
                float step = range / 4.0f;
                if (signalValue <= minSignalValue + step) activeBars = 4;
                else if (signalValue <= minSignalValue + (step * 2)) activeBars = 3;
                else if (signalValue <= minSignalValue + (step * 3)) activeBars = 2;
                else if (signalValue <= maxSignalValue) activeBars = 1;
            }

            switch(activeBars) {
              case 4: signalIconStr = ICON_SIG_4; break;
              case 3: signalIconStr = ICON_SIG_3; break;
              case 2: signalIconStr = ICON_SIG_2; break;
              case 1: signalIconStr = ICON_SIG_1; break;
              default: 
                signalIconStr = ICON_SIG_X; 
                currentSigColor = TFT_RED;
                break;
            }
        }

        tft.loadFont(_iconFontData);
        tft.setTextColor(currentSigColor, bgColor);
        tft.setTextDatum(TL_DATUM);
        tft.drawString(signalIconStr, sigIconX, _y);
        tft.unloadFont(); 

        _lastSignalValue = signalValue;
      }

      _lastSymbolColor = symbolColor;
      _lastBgColor = bgColor;
      _lastIsConnected = isConnected;
    }
};

#endif