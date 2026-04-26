#ifndef SMART_TEXT_BOX_H
#define SMART_TEXT_BOX_H

#include <Arduino.h>
#include <TFT_eSPI.h>

extern TFT_eSprite sprite;

class SmartTextBox {
  private:
    int16_t _fontSize = 1;         
    const uint8_t* _smoothFont = nullptr; 
    const uint8_t* _iconFont = nullptr; 
    
    uint16_t _defaultColor; 
    uint16_t _bg;
    bool _leftAligned;

    String _lastText = "";
    uint16_t _lastColor = 0; 
    
    // --- STATE TRACKING ---
    int _myLastRenderID = -1; 
    unsigned long _busyUntil = 0; 

    // --- VARIABEL SCROLL ---
    bool _isScrollMode = false;      
    int _scrollX = 0;                
    unsigned long _lastScrollUpdate = 0; 
    unsigned long _scrollPauseStart = 0; 
    bool _isPausedAtEnd = false;     
    bool _isPausedAtStart = false;   

    // --- GLOBAL RENDER COUNTER ---
    static int& _getGlobalRenderID() {
      static int id = 0;
      return id;
    }

    // Cache untuk ingat font apa yang sedang aktif
    static const uint8_t*& _getCurrentLoadedSmoothFont() {
      static const uint8_t* ptr = nullptr;
      return ptr;
    }

    void checkPageChange() {
      int currentID = _getGlobalRenderID();
      if (currentID != _myLastRenderID) {
        _busyUntil = 0; 
      }
    }

    // --- OPTIMIZED FONT LOADER ---
    void loadSelectedFont() {
      if (_smoothFont != nullptr) {
        if (_getCurrentLoadedSmoothFont() != _smoothFont) {
          sprite.loadFont(_smoothFont);
          _getCurrentLoadedSmoothFont() = _smoothFont; 
        }
      } 
      else {
        if (_getCurrentLoadedSmoothFont() != nullptr) {
          sprite.unloadFont(); 
          _getCurrentLoadedSmoothFont() = nullptr; 
        }
        sprite.setTextFont(_fontSize); 
        sprite.setTextSize(1);
      }
    }

    // --- SISTEM FALLBACK RAM PINTAR ---
    // Mencegah masalah glitch di kiri atas skrin apabila RAM tidak mencukupi
    bool allocateSprite(int16_t reqW, int16_t reqH) {
      void* ptr = sprite.createSprite(reqW, reqH);
      if (ptr != nullptr) return true; // Berjaya 16-bit

      // Jika gagal, turunkan kualiti warna ke 8-bit (Jimat 50% RAM) untuk memastikan teks tetap keluar
      sprite.setColorDepth(8);
      ptr = sprite.createSprite(reqW, reqH);
      if (ptr != nullptr) return true; // Berjaya 8-bit

      // Jika gagal juga (RAM kritikal)
      sprite.setColorDepth(16); // Reset
      Serial.println("SmartTextBox Error: RAM sangat rendah! Tidak dapat papar teks.");
      return false;
    }

    void cleanupSprite() {
      sprite.deleteSprite();
      sprite.setColorDepth(16); // Sentiasa kembalikan ke standard 16-bit
    }

    // --- RENDER 1: BIASA (WRAP + NEWLINE SUPPORT) ---
    void render(const String &originalText, uint16_t color) {
      int currentID = _getGlobalRenderID();
      bool screenCleared = (currentID != _myLastRenderID);

      if (originalText == _lastText && color == _lastColor && !_isScrollMode && !screenCleared) return; 
      
      _lastText = originalText;
      _lastColor = color; 
      _myLastRenderID = currentID; 
      _isScrollMode = false; 

      loadSelectedFont();
      if (_smoothFont != nullptr) sprite.setTextColor(color, _bg);

      int fontHeight = sprite.fontHeight();
      int spaceWidth = sprite.textWidth(" ");
      const int LINE_SPACING = 4;   
      const int BOTTOM_PADDING = 8; 

      String text = originalText;
      text.replace("\r", ""); // Buang carriage return yang tersembunyi
      text.replace("\n", " \n ");

      int16_t currentLineWidth = 0;
      int16_t lineCount = 1;
      int16_t cursor = 0;
      
      int effectiveWidth = w - 4; 
      
      while (cursor < text.length()) {
        int spaceIndex = text.indexOf(' ', cursor);
        if (spaceIndex == -1) spaceIndex = text.length();
        String word = text.substring(cursor, spaceIndex);
        
        if (word == "\n") {
            lineCount++;
            currentLineWidth = 0;
            cursor = spaceIndex + 1;
            continue; 
        }

        int wordWidth = sprite.textWidth(word);
        
        if (currentLineWidth + wordWidth > effectiveWidth) {
          if (currentLineWidth > 0) {
            lineCount++;
            currentLineWidth = 0;
          }
        }
        currentLineWidth += wordWidth + spaceWidth;
        cursor = spaceIndex + 1;
      }
      
      int16_t requiredHeight = lineCount * (fontHeight + LINE_SPACING) + BOTTOM_PADDING; 
      int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;

      // Alokasi RAM Selamat
      if (!allocateSprite(w, spriteHeight)) {
        _lastText = ""; // Paksa lukis semula nanti jika RAM ada
        return;
      }

      sprite.fillSprite(_bg);
      
      loadSelectedFont(); 
      sprite.setTextColor(color, _bg);
      sprite.setTextDatum(TL_DATUM); 
      sprite.setTextWrap(false); 

      int16_t drawY = 2; 
      cursor = 0;
      currentLineWidth = 0;
      String currentLineStr = ""; 
      
      while (cursor < text.length()) {
        int spaceIndex = text.indexOf(' ', cursor);
        if (spaceIndex == -1) spaceIndex = text.length();
        String word = text.substring(cursor, spaceIndex);
        
        bool isNewline = (word == "\n");
        int wordWidth = 0;
        if (!isNewline) wordWidth = sprite.textWidth(word);
        
        if (isNewline || (currentLineWidth + wordWidth > effectiveWidth)) {
          // Fix pepijat (bug) yang melukis baris kosong bila ada perkataan panjang
          if (isNewline || currentLineWidth > 0) {
            int16_t drawX = _leftAligned ? 4 : (w - sprite.textWidth(currentLineStr)) / 2;
            sprite.drawString(currentLineStr, drawX, drawY);
            
            currentLineStr = "";
            currentLineWidth = 0;
            drawY += (fontHeight + LINE_SPACING); 
          }
        }
        
        if (!isNewline) {
            if (currentLineStr.length() > 0) {
                currentLineStr += " ";
                currentLineWidth += spaceWidth;
            }
            currentLineStr += word;
            currentLineWidth += wordWidth;
        }
        
        cursor = spaceIndex + 1;
      }
      
      if (currentLineStr.length() > 0) {
        int16_t drawX = _leftAligned ? 4 : (w - sprite.textWidth(currentLineStr)) / 2;
        sprite.drawString(currentLineStr, drawX, drawY);
      }

      int16_t finalX = _leftAligned ? x : (x - (w / 2));
      sprite.pushSprite(finalX, y);

      h = requiredHeight;
      cleanupSprite();
    }

    // --- RENDER 2: CLIP ---
    void renderClipInternal(const String &originalText, uint16_t color) {
      int currentID = _getGlobalRenderID();
      bool screenCleared = (currentID != _myLastRenderID);

      String text = originalText;
      text.replace("\r", "");
      text.replace("\n", " "); // Buang newline untuk fungsi Clip

      if (text == _lastText && color == _lastColor && !_isScrollMode && !screenCleared) return;

      _lastText = text;
      _lastColor = color;
      _myLastRenderID = currentID;
      _isScrollMode = false;

      loadSelectedFont();
      int fontHeight = sprite.fontHeight();

      int16_t requiredHeight = fontHeight + 12;
      int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;

      if (!allocateSprite(w, spriteHeight)) { _lastText = ""; return; }
      sprite.fillSprite(_bg);

      loadSelectedFont();
      sprite.setTextColor(color, _bg);
      sprite.setTextDatum(TL_DATUM);
      sprite.setTextWrap(false);

      int drawX = _leftAligned ? 4 : (w - sprite.textWidth(text)) / 2;
      sprite.drawString(text, drawX, 2);

      int16_t finalX = _leftAligned ? x : (x - (w / 2));
      sprite.pushSprite(finalX, y);

      h = requiredHeight;
      cleanupSprite();
    }

    // --- RENDER 3: SCROLL ---
    void renderScrollInternal(const String &originalText, uint16_t color) {
      String text = originalText;
      text.replace("\r", "");
      text.replace("\n", " "); // Teks bergerak tidak boleh ada newline
      
      loadSelectedFont(); 
      int textPixelWidth = sprite.textWidth(text);
      int fontHeight = sprite.fontHeight();

      if (textPixelWidth <= w - 4) { 
          _scrollX = 0;
          int16_t requiredHeight = fontHeight + 12; 
          int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;
          
          if (!allocateSprite(w, spriteHeight)) { _lastText = ""; return; }
          sprite.fillSprite(_bg);
          
          loadSelectedFont(); 
          sprite.setTextColor(color, _bg);
          sprite.setTextDatum(TL_DATUM);
          sprite.setTextWrap(false); 
          
          int drawX = _leftAligned ? 4 : (w - textPixelWidth) / 2;
          sprite.drawString(text, drawX, 2);
          
          int16_t finalX = _leftAligned ? x : (x - (w / 2));
          sprite.pushSprite(finalX, y);
          h = requiredHeight;
          cleanupSprite();
          return;
      }

      if (_isPausedAtStart) {
          if (millis() - _scrollPauseStart > 500) { _isPausedAtStart = false; _lastScrollUpdate = millis(); }
      }
      else if (_isPausedAtEnd) {
           if (millis() - _scrollPauseStart > 500) { _isPausedAtEnd = false; _isPausedAtStart = true; _scrollPauseStart = millis(); _scrollX = 0; }
      }
      else {
           if (millis() - _lastScrollUpdate > 30) {
             _lastScrollUpdate = millis();
             _scrollX += 2; 
             if (_scrollX > (textPixelWidth - w + 20)) { _isPausedAtEnd = true; _scrollPauseStart = millis(); }
           }
      }

      int16_t requiredHeight = fontHeight + 12; 
      int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;

      if (!allocateSprite(w, spriteHeight)) { _lastText = ""; return; }
      sprite.fillSprite(_bg); 
      
      loadSelectedFont();
      sprite.setTextColor(color, _bg);
      sprite.setTextDatum(TL_DATUM);
      sprite.setTextWrap(false); 

      int startX = _leftAligned ? 4 : 0;
      sprite.drawString(text, startX - _scrollX, 2);

      int16_t finalX = _leftAligned ? x : (x - (w / 2));
      sprite.pushSprite(finalX, y);
      
      h = requiredHeight; 
      cleanupSprite();
    }

    // --- RENDER 4: ICON + TEXT ---
    void renderWithIconInternal(const String &icon, const String &originalText, uint16_t color) {
      int currentID = _getGlobalRenderID();
      bool screenCleared = (currentID != _myLastRenderID);
      String combinedText = icon + originalText; 

      if (combinedText == _lastText && color == _lastColor && !_isScrollMode && !screenCleared) return;

      _lastText = combinedText;
      _lastColor = color;
      _myLastRenderID = currentID;
      _isScrollMode = false;

      if (_iconFont != nullptr) {
        if (_getCurrentLoadedSmoothFont() != _iconFont) {
          sprite.loadFont(_iconFont);
          _getCurrentLoadedSmoothFont() = _iconFont;
        }
      }
      int iconWidth = sprite.textWidth(icon);
      int iconHeight = sprite.fontHeight();
      int gap = 6; 

      loadSelectedFont();
      int fontHeight = sprite.fontHeight();
      int spaceWidth = sprite.textWidth(" ");
      const int LINE_SPACING = 4;   
      const int BOTTOM_PADDING = 8; 

      String text = originalText;
      text.replace("\r", "");
      text.replace("\n", " \n ");

      int effectiveTextWidth = w - iconWidth - gap - 4; 
      if (effectiveTextWidth < 10) effectiveTextWidth = 10; 

      int16_t currentLineWidth = 0;
      int16_t maxTextWidth = 0;
      int16_t lineCount = 1;
      int16_t cursor = 0;
      
      while (cursor < text.length()) {
        int spaceIndex = text.indexOf(' ', cursor);
        if (spaceIndex == -1) spaceIndex = text.length();
        String word = text.substring(cursor, spaceIndex);
        
        if (word == "\n") {
            if (currentLineWidth > maxTextWidth) maxTextWidth = currentLineWidth;
            lineCount++;
            currentLineWidth = 0;
            cursor = spaceIndex + 1;
            continue; 
        }

        int wordWidth = sprite.textWidth(word);
        
        if (currentLineWidth + wordWidth > effectiveTextWidth) {
          if (currentLineWidth > 0) {
            if (currentLineWidth > maxTextWidth) maxTextWidth = currentLineWidth;
            lineCount++;
            currentLineWidth = 0;
          }
        }
        currentLineWidth += wordWidth + spaceWidth;
        cursor = spaceIndex + 1;
      }
      if (currentLineWidth > maxTextWidth) maxTextWidth = currentLineWidth;
      if (maxTextWidth > spaceWidth) maxTextWidth -= spaceWidth;

      int16_t requiredHeight = lineCount * (fontHeight + LINE_SPACING) + BOTTOM_PADDING;
      if (requiredHeight < iconHeight + BOTTOM_PADDING) requiredHeight = iconHeight + BOTTOM_PADDING;
      int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;

      if (!allocateSprite(w, spriteHeight)) { _lastText = ""; return; }
      sprite.fillSprite(_bg);

      int totalWidth = iconWidth + gap + maxTextWidth;
      int blockStartX = _leftAligned ? 4 : (w - totalWidth) / 2;

      if (_iconFont != nullptr) {
        if (_getCurrentLoadedSmoothFont() != _iconFont) {
          sprite.loadFont(_iconFont);
          _getCurrentLoadedSmoothFont() = _iconFont;
        }
      }
      sprite.setTextColor(color, _bg);
      sprite.setTextDatum(TL_DATUM);
      sprite.drawString(icon, blockStartX, 2);

      loadSelectedFont();
      sprite.setTextColor(color, _bg);
      sprite.setTextWrap(false);

      int16_t drawY = 2; 
      cursor = 0;
      currentLineWidth = 0;
      String currentLineStr = ""; 
      int textStartX = blockStartX + iconWidth + gap;
      
      while (cursor < text.length()) {
        int spaceIndex = text.indexOf(' ', cursor);
        if (spaceIndex == -1) spaceIndex = text.length();
        String word = text.substring(cursor, spaceIndex);
        
        bool isNewline = (word == "\n");
        int wordWidth = 0;
        if (!isNewline) wordWidth = sprite.textWidth(word);
        
        if (isNewline || (currentLineWidth + wordWidth > effectiveTextWidth)) {
          if (isNewline || currentLineWidth > 0) {
            int16_t lineDrawX = _leftAligned ? textStartX : textStartX + ((maxTextWidth - sprite.textWidth(currentLineStr)) / 2);
            sprite.drawString(currentLineStr, lineDrawX, drawY);
            
            currentLineStr = "";
            currentLineWidth = 0;
            drawY += (fontHeight + LINE_SPACING); 
          }
        }
        
        if (!isNewline) {
            if (currentLineStr.length() > 0) {
                currentLineStr += " ";
                currentLineWidth += spaceWidth;
            }
            currentLineStr += word;
            currentLineWidth += wordWidth;
        }
        
        cursor = spaceIndex + 1;
      }
      
      if (currentLineStr.length() > 0) {
        int16_t lineDrawX = _leftAligned ? textStartX : textStartX + ((maxTextWidth - sprite.textWidth(currentLineStr)) / 2);
        sprite.drawString(currentLineStr, lineDrawX, drawY);
      }

      int16_t finalX = _leftAligned ? x : (x - (w / 2));
      sprite.pushSprite(finalX, y);

      h = requiredHeight;
      cleanupSprite();
    }

    // --- RENDER 5: ICON + TEXT SCROLL ---
    void renderWithIconScrollInternal(const String &icon, const String &originalText, uint16_t color) {
      String text = originalText;
      text.replace("\r", "");
      text.replace("\n", " ");

      loadSelectedFont();
      int fontHeight = sprite.fontHeight();
      int textWidth = sprite.textWidth(text);

      if (_iconFont != nullptr) {
        if (_getCurrentLoadedSmoothFont() != _iconFont) {
          sprite.loadFont(_iconFont);
          _getCurrentLoadedSmoothFont() = _iconFont;
        }
      }
      int iconWidth = sprite.textWidth(icon);
      int gap = 6;

      int totalWidth = iconWidth + gap + textWidth;
      
      int startX = _leftAligned ? 4 : (w - totalWidth) / 2;
      if (startX < 4) startX = 4;

      int textStartX = startX + iconWidth + gap; 
      int scrollableWidth = w - textStartX;  

      if (textWidth <= scrollableWidth) {
          _scrollX = 0;
      } else {
          if (_isPausedAtStart) {
              if (millis() - _scrollPauseStart > 500) { _isPausedAtStart = false; _lastScrollUpdate = millis(); }
          }
          else if (_isPausedAtEnd) {
               if (millis() - _scrollPauseStart > 500) { _isPausedAtEnd = false; _isPausedAtStart = true; _scrollPauseStart = millis(); _scrollX = 0; }
          }
          else {
               if (millis() - _lastScrollUpdate > 30) {
                 _lastScrollUpdate = millis();
                 _scrollX += 2; 
                 if (_scrollX > (textWidth - scrollableWidth + 20)) {
                   _isPausedAtEnd = true;      
                   _scrollPauseStart = millis(); 
                 }
               }
          }
      }

      int16_t requiredHeight = fontHeight + 12;
      int16_t spriteHeight = (requiredHeight > h) ? requiredHeight : h;

      if (!allocateSprite(w, spriteHeight)) { _lastText = ""; return; }
      sprite.fillSprite(_bg);

      loadSelectedFont();
      sprite.setTextColor(color, _bg);
      sprite.setTextDatum(TL_DATUM);
      sprite.setTextWrap(false);
      sprite.drawString(text, textStartX - _scrollX, 1); 

      sprite.fillRect(0, 0, textStartX, spriteHeight, _bg);

      if (_iconFont != nullptr) {
        if (_getCurrentLoadedSmoothFont() != _iconFont) {
          sprite.loadFont(_iconFont);
          _getCurrentLoadedSmoothFont() = _iconFont;
        }
      }
      sprite.setTextColor(color, _bg);
      sprite.drawString(icon, startX, 2);

      int16_t finalX = _leftAligned ? x : (x - (w / 2));
      sprite.pushSprite(finalX, y);

      h = requiredHeight;
      cleanupSprite();
    }

  public:
    int16_t x, y, w, h; 

    // Constructor 1 (Built in Font)
    SmartTextBox(int16_t xPos, int16_t yPos, int16_t boxW, int16_t fontSize, 
                 uint16_t color, uint16_t bg, bool leftAligned) {
      x = xPos; y = yPos; w = boxW; h = 0;
      _fontSize = fontSize;
      _smoothFont = nullptr;
      _iconFont = nullptr;
      _defaultColor = color;
      _bg = bg;
      _leftAligned = leftAligned;
    }

    // Constructor 2 (Smooth Font)
    SmartTextBox(int16_t xPos, int16_t yPos, int16_t boxW, const uint8_t* smoothFont, 
                 uint16_t color, uint16_t bg, bool leftAligned) {
      x = xPos; y = yPos; w = boxW; h = 0;
      _fontSize = 1;         
      _smoothFont = smoothFont; 
      _iconFont = nullptr;
      _defaultColor = color;
      _bg = bg;
      _leftAligned = leftAligned;
    }

    // Constructor 3 (Smooth Icon Font + Smooth Text Font)
    SmartTextBox(int16_t xPos, int16_t yPos, int16_t boxW, const uint8_t* iconFont, const uint8_t* smoothFont, 
                 uint16_t color, uint16_t bg, bool leftAligned) {
      x = xPos; y = yPos; w = boxW; h = 0;
      _fontSize = 1;         
      _iconFont = iconFont;
      _smoothFont = smoothFont; 
      _defaultColor = color;
      _bg = bg;
      _leftAligned = leftAligned;
    }

    static void forceScreenUpdate() {
      _getGlobalRenderID()++; 
    }

    // ==========================================
    // KATEGORI 1: .text (HANYA TEKS)
    // ==========================================
    
    void text(const String &t) {
      checkPageChange(); 
      if (millis() < _busyUntil) return;
      render(t, _defaultColor);
    }
    
    void text(const String &t, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      render(t, customColor);
    }

    void text(const String &t, int durationMs) {
      checkPageChange(); 
      if (millis() < _busyUntil) return;
      render(t, _defaultColor);
      _busyUntil = millis() + durationMs;
    }

    void text(const String &t, int durationMs, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      render(t, customColor);
      _busyUntil = millis() + durationMs; 
    }

    // ==========================================
    // KATEGORI 2: .textScroll (TEKS BERGERAK)
    // ==========================================

    void textScroll(const String &t) {
      textScroll(t, _defaultColor);
    }

    void textScroll(const String &t, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return; 

      int currentID = _getGlobalRenderID();
      bool screenCleared = (currentID != _myLastRenderID);

      if (t != _lastText || !_isScrollMode || screenCleared) {
        _lastText = t;
        _myLastRenderID = currentID; 
        
        _scrollX = 0;
        _isPausedAtEnd = false;
        _isPausedAtStart = true;      
        _scrollPauseStart = millis(); 
        _lastScrollUpdate = millis();
        _isScrollMode = true; 
      }
      renderScrollInternal(t, customColor);
    }

    // ==========================================
    // KATEGORI 3: .iconText (IKON & TEKS)
    // ==========================================

    void iconText(const String &icon, const String &t) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      renderWithIconInternal(icon, t, _defaultColor);
    }

    void iconText(const String &icon, const String &t, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      renderWithIconInternal(icon, t, customColor);
    }

    void iconText(const String &icon, const String &t, int durationMs) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      renderWithIconInternal(icon, t, _defaultColor);
      _busyUntil = millis() + durationMs;
    }

    void iconText(const String &icon, const String &t, int durationMs, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return;
      renderWithIconInternal(icon, t, customColor);
      _busyUntil = millis() + durationMs;
    }

    // ==========================================
    // KATEGORI 4: .iconTextScroll (IKON STATIK, TEKS GERAK)
    // ==========================================

    void iconTextScroll(const String &icon, const String &t) {
      iconTextScroll(icon, t, _defaultColor);
    }

    void iconTextScroll(const String &icon, const String &t, uint16_t customColor) {
      checkPageChange();
      if (millis() < _busyUntil) return; 

      int currentID = _getGlobalRenderID();
      bool screenCleared = (currentID != _myLastRenderID);
      String combinedText = icon + t;

      if (combinedText != _lastText || !_isScrollMode || screenCleared) {
        _lastText = combinedText;
        _myLastRenderID = currentID; 
        
        _scrollX = 0;
        _isPausedAtEnd = false;
        _isPausedAtStart = true;      
        _scrollPauseStart = millis(); 
        _lastScrollUpdate = millis();
        _isScrollMode = true; 
      }
      renderWithIconScrollInternal(icon, t, customColor);
    }

    // ==========================================
    // KATEGORI TAMBAHAN: .textClip (POTONG TEKS PANJANG)
    // ==========================================
    void textClip(const String &t) { checkPageChange(); if (millis() < _busyUntil) return; renderClipInternal(t, _defaultColor); }
    void textClip(const String &t, uint16_t customColor) { checkPageChange(); if (millis() < _busyUntil) return; renderClipInternal(t, customColor); }
    void textClip(const String &t, int durationMs) { checkPageChange(); if (millis() < _busyUntil) return; renderClipInternal(t, _defaultColor); _busyUntil = millis() + durationMs; }
    void textClip(const String &t, int durationMs, uint16_t customColor) { checkPageChange(); if (millis() < _busyUntil) return; renderClipInternal(t, customColor); _busyUntil = millis() + durationMs; }
    
    int getBoxHeight() { return h; }
};

#endif