#ifndef TOUCHBUTTON_H
#define TOUCHBUTTON_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "SmartTextBox.h"
#include <functional>
#include <Preferences.h> // <--- 1. TAMBAH LIBRARY INI

#define BUZZER_PIN 1

enum LoraState { IDLE, WAITING_FOR_REPLY };
extern LoraState currentLoraState;
extern TFT_eSPI tft;

typedef std::function<void()> CallbackFunction;

class TouchButton {
  // ==========================================================
  // 1. NESTED STRUCTS FOR CHAINED SYNTAX
  // ==========================================================
  public:
    
    struct IconTextHandler {
        TouchButton* p;
        IconTextHandler(TouchButton* _p) : p(_p) {}

        void isPressed(String icon, String label, CallbackFunction cb) {
            p->_mode = 2; 
            p->runLogic(icon, label, cb);
        }

        void isLongPressed(String icon, String label, CallbackFunction cb) {
            p->_mode = 5; 
            p->runLogic(icon, label, cb);
        }
    };

    struct IconHandler {
        TouchButton* p;
        IconTextHandler text; 

        IconHandler(TouchButton* _p) : p(_p), text(_p) {}

        void isPressed(String icon, CallbackFunction cb) {
            p->_mode = 1; 
            p->runLogic(icon, "", cb);
        }

        void isLongPressed(String icon, CallbackFunction cb) {
            p->_mode = 4; 
            p->runLogic(icon, "", cb);
        }
    };

    struct TextHandler {
        TouchButton* p;
        TextHandler(TouchButton* _p) : p(_p) {}

        void isPressed(String label, CallbackFunction cb) {
            p->_mode = 0; 
            p->runLogic("", label, cb);
        }

        void isLongPressed(String label, CallbackFunction cb) {
            p->_mode = 3; 
            p->runLogic("", label, cb);
        }
    };

  // ==========================================================
  // 2. PUBLIC MEMBERS, CONSTRUCTOR & STATIC SETTINGS
  // ==========================================================
  public:
    int x, y, w, h; 
    
    TextHandler text;
    IconHandler icon;

    // --- FUNGSI BUZZER & PREFERENCES ---
    static bool isBeep; 

    // Fungsi 1: Load setting masa ESP32 mula hidup
    static void loadSettings() {
        Preferences prefs;
        // Buka "ruang" bernama 'ui_set' (max 15 huruf). true = Read Only
        prefs.begin("ui_set", true); 
        // Ambil nilai 'beep'. Kalau tak pernah save, jadikan true sbg default
        isBeep = prefs.getBool("beep", true); 
        prefs.end(); // Tutup ruang
    }

    // Fungsi 2: Tukar status & Save terus ke memori Flash
    static void toggleBeep() {
        isBeep = !isBeep; // Tukar status (true ke false, false ke true)
        
        Preferences prefs;
        // Buka 'ui_set'. false = Read/Write mode
        prefs.begin("ui_set", false); 
        prefs.putBool("beep", isBeep); // Save nilai baru
        prefs.end(); 
    }

    static void beep() {
        if (isBeep) {
            pinMode(BUZZER_PIN, OUTPUT);
            digitalWrite(BUZZER_PIN, HIGH); 
            delay(50);                      
            digitalWrite(BUZZER_PIN, LOW);  
        }
    }

    TouchButton(int x, int y, uint16_t color, const uint8_t* iconFont, const uint8_t* textFont, uint16_t bgColor = TFT_WHITE) 
      : x(x), y(y), _color(color), _iconFont(iconFont), _textFont(textFont), _bgColor(bgColor),
        text(this), icon(this) 
      {
        w = 0; h = 0; _r = 0;
      }

    static void resetAllButtons() { _masterGen++; }

  // ==========================================================
  // 3. PRIVATE LOGIC
  // ==========================================================
  private:
    int _r; 
    
    String _lastLabel = ""; 
    String _lastIcon = "";
    uint16_t _color, _bgColor; 
    const uint8_t *_iconFont, *_textFont;
    
    bool _hasBeenDrawn = false;
    int _mode = 0; 
    
    int _localGen = -1; 
    static int _masterGen; 

    bool _isHolding = false;           
    unsigned long _lastRepeatTime = 0; 
    bool _isFirstRepeat = true;          
    bool _longPressTriggered = false;
    unsigned long _pressStartTime = 0;
    
    const int INITIAL_DELAY = 1000;      
    const int REPEAT_INTERVAL = 200;     

    bool getCachedTouch(uint16_t &x_touch, uint16_t &y_touch) {
      static unsigned long globalLastCheck = 0;
      static bool globalIsTouched = false;
      static uint16_t globalX = 0, globalY = 0;
      static unsigned long lastValidTouchTime = 0;
      const int DEBOUNCE_MS = 50;

      if (millis() - globalLastCheck >= 20) {
        globalLastCheck = millis();
        uint16_t tempX, tempY;
        if (tft.getTouch(&tempX, &tempY, 50)) {
            globalIsTouched = true; globalX = tempX; globalY = tempY;
            lastValidTouchTime = millis();
        } else if (millis() - lastValidTouchTime > DEBOUNCE_MS) {
            globalIsTouched = false;
        }
      }
      x_touch = globalX; y_touch = globalY; return globalIsTouched;
    }

    void calculateDimensions(String icon, String label) {
      tft.loadFont(_textFont);
      int textW = tft.textWidth(label);
      int textH = tft.fontHeight();
      tft.unloadFont();

      int standardHeight = textH + 31;

      if (_mode == 1 || _mode == 4) {
         h = standardHeight; 
         w = h;           
         _r = h / 2;     
         return;
      }

      if (_mode == 2 || _mode == 5) {
         tft.loadFont(_iconFont); int iW = tft.textWidth(icon); tft.unloadFont();
         w = iW + 10 + textW + 46;
         h = standardHeight; 
         _r = h / 2;
         return;
      }

      w = textW + 46; 
      h = standardHeight;
      _r = h / 2;
    }

    void runLogic(String icon, String label, CallbackFunction callback) {
        bool visualChanged = (!_hasBeenDrawn || label != _lastLabel || icon != _lastIcon || _localGen != _masterGen);
        
        if (visualChanged) {
            if (_hasBeenDrawn) tft.fillRect(x, y, w + 2, h + 2, _bgColor); 
            _lastLabel = label; _lastIcon = icon; _localGen = _masterGen;
            calculateDimensions(icon, label);
            drawNormal();
            _hasBeenDrawn = true;
        }

        if (digitalRead(8) == LOW) return; // CS_LORA = 8

        uint16_t tx, ty;
        if (getCachedTouch(tx, ty)) {
            if (tx > x && tx < (x + w) && ty > y && ty < (y + h)) {
                
                if (_mode <= 2) { 
                    if (!_isHolding) {
                        _isHolding = true; _isFirstRepeat = true;
                        beep(); 
                        drawPressed();
                        if (callback) callback();
                        _lastRepeatTime = millis();
                    } else {
                        unsigned long waitTime = _isFirstRepeat ? INITIAL_DELAY : REPEAT_INTERVAL;
                        if (millis() - _lastRepeatTime >= waitTime) {
                            beep(); 
                            if (callback) callback();
                            _lastRepeatTime = millis(); _isFirstRepeat = false;
                        }
                    }
                } 
                else { 
                    if (!_isHolding) {
                        _isHolding = true; _longPressTriggered = false; _pressStartTime = millis();
                        beep(); 
                        drawPressed();
                    } else {
                        if (!_longPressTriggered && (millis() - _pressStartTime > 1000)) {
                            _longPressTriggered = true;
                            beep(); 
                            if (callback) callback();
                        }
                    }
                }
            } else { if (_isHolding) { _isHolding = false; drawNormal(); } }
        } else { if (_isHolding) { _isHolding = false; drawNormal(); } }
    }

    void drawNormal() { renderButton(_color, TFT_BLACK); }
    void drawPressed() { renderButton(_color ^ 0xFFFF, TFT_WHITE); }

    void renderButton(uint16_t btnCol, uint16_t txtCol) {
        tft.fillRoundRect(x, y, w, h, _r, btnCol);
        tft.drawRoundRect(x, y, w, h, _r, TFT_BLACK);
        
        int cx = x + (w / 2);
        int cy = y + (h / 2);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(txtCol, btnCol);

        if (_mode == 1 || _mode == 4) {
            tft.loadFont(_iconFont);
            tft.drawString(_lastIcon, cx, cy + 2); 
            tft.unloadFont();
        }
        else if (_mode == 2 || _mode == 5) {
             tft.loadFont(_iconFont); int iW = tft.textWidth(_lastIcon); tft.unloadFont();
             tft.loadFont(_textFont); int tW = tft.textWidth(_lastLabel); tft.unloadFont();
             
             int gap = 10;
             int startX = cx - ((iW + gap + tW) / 2);
             
             tft.loadFont(_iconFont); 
             tft.drawString(_lastIcon, startX + (iW/2), cy + 1); 
             tft.unloadFont();

             tft.loadFont(_textFont); 
             tft.drawString(_lastLabel, startX + iW + gap + (tW/2), cy - 3); 
             tft.unloadFont();
        }
        else {
             tft.loadFont(_textFont);
             tft.drawString(_lastLabel, cx, cy - 3); 
             tft.unloadFont();
        }
    }
};

int TouchButton::_masterGen = 0;
bool TouchButton::isBeep = true; 

#endif