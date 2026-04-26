// --- FUNGSI CORE 0: GPS WATCHDOG ---
void codeForCore0(void * pvParameters) {
  for (;;) {
    // 1. Baca data Serial dan kongsi kepada dua parser
    while (gpsSerial.available() > 0) {
      char c = gpsSerial.read();
      
      // Suap TinyGPS++
      gps.encode(c);

      // Suap Parser VDOP Custom
      float vVal = processVDOPChar(c);
      if (vVal != -1.0) {
        currentVDOP = vVal; 
      }
    }

    // 2. Kemaskini Shared Variables jika data valid
    if (gps.location.isValid() && gps.time.isValid() && gps.date.isValid()) {
      sharedLat = gps.location.lat();
      sharedLon = gps.location.lng();
      sharedSat = gps.satellites.value();
      sharedFix = true;

      // KRITIKAN MENTOR: Jangan gunakan formula NEO-6M.
      // Untuk LC76G, formula anggaran ralat (meter) yang lebih jujur:
      if (gps.hdop.isValid()) {
        sharedHDOP = gps.hdop.hdop();
        // LC76G mempunyai ralat horizontal ~1.5m - 2.0m pada HDOP 1.0
        sharedAcc = sharedHDOP * 1.5; 
      }

      // --- PENGIRAAN UNIX EPOCH TIME (UTC) ---
      uint16_t y_utc = gps.date.year();
      uint8_t m_utc = gps.date.month();
      uint8_t d_utc = gps.date.day();
      uint8_t h_utc = gps.time.hour();
      uint8_t min_utc = gps.time.minute();
      uint8_t sec_utc = gps.time.second();

      if (m_utc <= 2) {
          y_utc -= 1;
          m_utc += 12;
      }
      uint32_t t_days = (365UL * y_utc + y_utc / 4 - y_utc / 100 + y_utc / 400 + 153UL * m_utc + 8) / 5 + d_utc - 719543UL;
      sharedUnix = ((t_days * 24 + h_utc) * 60 + min_utc) * 60 + sec_utc;

      // --- PENGIRAAN GMT +8 ---
      // (Logik d, m, y anda dikekalkan kerana sudah tepat)
      int h = h_utc + 8;
      int d = d_utc;
      int m = m_utc;
      int y = y_utc; 
      // ... (Sila masukkan logik overflow d, m, y anda di sini) ...
      
      sharedHour = h;
      sharedMinute = min_utc;
      sharedSecond = sec_utc;
      // Kemaskini shared date di sini

    } else {
      if (gps.charsProcessed() > 10 && !gps.location.isValid()) {
        sharedFix = false;
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS); 
  }
}

// Fungsi ni sekarang terima 'char', bukan lagi 'HardwareSerial'
float processVDOPChar(char c) {
  static String nmeaBuffer = "";
  float foundVDOP = -1.0;

  if (c == '\n') {
    // Cari mesej GSA (Global/GPS)
    if (nmeaBuffer.startsWith("$GNGSA") || nmeaBuffer.startsWith("$GPGSA")) {
      int commaCount = 0;
      int startIndex = -1;
      
      for (int i = 0; i < nmeaBuffer.length(); i++) {
        if (nmeaBuffer[i] == ',') {
          commaCount++;
          // VDOP berada selepas koma ke-16
          if (commaCount == 16) startIndex = i + 1;
          // Tamat pada koma ke-17
          if (commaCount == 17 && startIndex != -1) {
            String vdopStr = nmeaBuffer.substring(startIndex, i);
            if (vdopStr.length() > 0) {
              foundVDOP = vdopStr.toFloat();
            }
            break;
          }
        }
      }
    }
    nmeaBuffer = ""; // Reset setiap baris baru
  } else if (c != '\r') {
    nmeaBuffer += c;
  }
  return foundVDOP;
}

// --- FUNGSI KECERAHAN SKRIN (ESP32 v3.0+) ---
void setBrightness(int level) {
  // Pastikan level dalam julat 0-255
  if (level < 0) level = 0;
  if (level > 255) level = 255;
  
  // Dalam ESP32 v3.0, kita tulis terus ke PIN, bukan Channel
  ledcWrite(LCD_LED, level);
}

void displayBatteryBar(int value, int x, int y, int batColor, String duration) {
  // --- 1. LUKISAN BATERI ---

  // Clamp value 0-100
  if (value > 100) value = 100;
  if (value < 0) value = 0;

  // Bingkai & Cap (Warna Hitam)
  tft.fillRect(x + 3, y - 1, 4, 2, TFT_BLACK); 
  tft.drawRoundRect(x, y, 10, 16, 2, TFT_BLACK);

  // Isi Bateri
  int maxHeight = 14;
  int barHeight = map(value, 0, 100, 0, maxHeight);
  int emptyHeight = maxHeight - barHeight;

  // Lukis isi (Warna bateri)
  tft.fillRect(x + 1, y + 1 + emptyHeight, 8, barHeight, batColor);

  // Padam kosong (Warna Putih)
  if (emptyHeight > 0) {
    tft.fillRect(x + 1, y + 1, 8, emptyHeight, TFT_WHITE); 
  }

  // --- 2. TEKS (PERATUS & DURATION) ---
  
  tft.setTextFont(1);
  tft.setTextColor(TFT_BLACK, TFT_WHITE); // Teks Hitam, BG Putih
  
  // TUKAR: ML_DATUM (Middle Left Alignment)
  // Teks akan bermula dari koordinat X dan memanjang ke kanan
  tft.setTextDatum(ML_DATUM); 

  // TUKAR KOORDINAT X: 
  // x + 10 (lebar bateri) + 5 (jarak kosong) = x + 15
  int textStartX = x + 15;

  // B. Teks Duration (Atas)
  // Padding lebar sikit ("88h 88m") untuk cover teks lama
  tft.setTextPadding(tft.textWidth("ROVER")); 
  tft.drawString(duration, textStartX, y + 3);

  // A. Teks Peratus (Bawah)
  tft.setTextPadding(tft.textWidth("100%")); 
  tft.drawString(String(value) + "%", textStartX, y + 12);

  // Reset padding
  tft.setTextPadding(0);
}

//name viewer variables
int row=0, selectedLine=1, selectedData=0, tablePage=1;

int getTotalPages() {
  int count = mission.getCount();
  if (count == 0) return 1; // Kalau tiada data, anggap page 1
  return (count + 5) / 6;
}

/**
 * Fungsi untuk melukis kompas pada skrin TFT.
 * Parameter 'degree' dibetulkan arah pusingannya (360 - degree).
 */
void drawCompass(int currentHeading, int targetBearing, uint16_t compassColor, uint16_t bgColor, int x, int y, int radius) {
  static TFT_eSprite compSprite(&tft); 
  static int lastRadius = 0;

  // 1. Saiz Sprite (Tanpa text padding, hanya bulatan)
  int spriteW = (radius * 2) + 4; 
  int spriteH = (radius * 2) + 4;

  if (lastRadius != radius) {
    if (lastRadius != 0) compSprite.deleteSprite(); 
    compSprite.setColorDepth(8); 
    compSprite.createSprite(spriteW, spriteH);
    lastRadius = radius;
  }

  // 2. Bersihkan Sprite
  compSprite.fillSprite(bgColor);

  // Pusat bulatan kini berada betul-betul di tengah sprite
  int cx = spriteW / 2;
  int cy = spriteH / 2; 

  // --- LOGIK KIRA SUDUT RELATIF ---
  // Arah sasaran tolak arah Rover menghadap sekarang.
  int relativeDegree = targetBearing - currentHeading;
  if (relativeDegree < 0) {
    relativeDegree += 360;
  }

  // 3. Lukis Dial Bulatan
  compSprite.drawCircle(cx, cy, radius, compassColor);

  // Gunakan sudut relatif untuk memusingkan jarum
  float rad = (relativeDegree * PI) / 180.0;
  
  int len = radius - 4; 
  int baseHalfWidth = radius / 5;

  // 4. Kira Koordinat Jarum Sasaran (Panah Menunjuk Destinasi)
  int xN = cx + len * sin(rad);
  int yN = cy - len * cos(rad);

  // 5. Kira Koordinat Ekor Jarum
  int xS = cx - len * sin(rad); 
  int yS = cy + len * cos(rad);

  // 6. Titik Tapak Jarum (Pusat)
  int xB1 = cx + baseHalfWidth * cos(rad); 
  int yB1 = cy + baseHalfWidth * sin(rad);
  
  int xB2 = cx - baseHalfWidth * cos(rad);
  int yB2 = cy - baseHalfWidth * sin(rad);

  // 7. Lukis Jarum Kompas (Waypoint Arrow)
  // Bahagian menunjuk ke sasaran (Diisi penuh/Filled)
  compSprite.fillTriangle(xN, yN, xB1, yB1, xB2, yB2, compassColor);
  
  // Bahagian ekor (Rangka sahaja/Outline)
  compSprite.drawTriangle(xS, yS, xB1, yB1, xB2, yB2, compassColor);

  // 8. Papar Sprite ke Skrin
  compSprite.pushSprite(x - cx, y - cy);
}

void drawCompassNorth(double degree, uint16_t compassColor, uint16_t bgColor, int x, int y, int radius) {
  static TFT_eSprite compSpriteNorth(&tft); 
  static int lastRadiusNorth = 0;

  // 1. Ruang untuk teks di bahagian atas
  int padNorth = 20; 
  int spWidthNorth = (radius * 2) + 4; 
  int spHeightNorth = (radius * 2) + 4 + padNorth;

  if (lastRadiusNorth != radius) {
    if (lastRadiusNorth != 0) compSpriteNorth.deleteSprite(); 
    compSpriteNorth.setColorDepth(8); 
    compSpriteNorth.createSprite(spWidthNorth, spHeightNorth);
    lastRadiusNorth = radius;
  }

  // 2. Bersihkan Sprite
  compSpriteNorth.fillSprite(bgColor);

  int cxNorth = spWidthNorth / 2;
  int cyNorth = padNorth + radius + 2;

  // 3. Papar Nilai Darjah Semasa (Teks)
  compSpriteNorth.setTextColor(compassColor, bgColor);
  compSpriteNorth.setTextDatum(TC_DATUM);
  compSpriteNorth.drawString(String((int)degree) + " deg", cxNorth, 0, 2); 

  // 4. Lukis Dial Bulatan
  compSpriteNorth.drawCircle(cxNorth, cyNorth, radius, compassColor);

  // --- LOGIK TUNJUK UTARA SAHAJA ---
  // Untuk jarum sentiasa menunjuk ke Utara (0 darjah),
  // kita tolak nilai heading semasa daripada 360.
  float corrDegNorth = 360.0 - degree;
  float radNorth = (corrDegNorth * PI) / 180.0;
  
  int lenNorth = radius - 4; 
  int baseHWNorth = radius / 5;

  // 5. Kira Koordinat Jarum Utara (Panah Menunjuk Utara)
  int xNorth = cxNorth + lenNorth * sin(radNorth);
  int yNorth = cyNorth - lenNorth * cos(radNorth);

  // 6. Kira Koordinat Ekor Jarum (Selatan)
  int xSouth = cxNorth - lenNorth * sin(radNorth); 
  int ySouth = cyNorth + lenNorth * cos(radNorth);

  // 7. Titik Tapak Jarum (Pusat)
  int xB1North = cxNorth + baseHWNorth * cos(radNorth); 
  int yB1North = cyNorth + baseHWNorth * sin(radNorth);
  
  int xB2North = cxNorth - baseHWNorth * cos(radNorth);
  int yB2North = cyNorth - baseHWNorth * sin(radNorth);

  // 8. Lukis Jarum Kompas (North Arrow)
  // Bahagian menunjuk ke Utara (Diisi penuh/Filled)
  compSpriteNorth.fillTriangle(xNorth, yNorth, xB1North, yB1North, xB2North, yB2North, compassColor);
  
  // Bahagian ekor (Rangka sahaja/Outline)
  compSpriteNorth.drawTriangle(xSouth, ySouth, xB1North, yB1North, xB2North, yB2North, compassColor);

  // 9. Papar Sprite ke Skrin
  compSpriteNorth.pushSprite(x - cxNorth, y - cyNorth);
}

// Fungsi untuk menguruskan proses bangun (wake up) dan hidupkan sistem
void turnOnDevice() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  // 1. Semak jika ESP32 bangun kerana sentuhan pada skrin
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    
    pinMode(WAKEUP_PIN, INPUT_PULLUP); 
    
    bool isLongPress = true;
    unsigned long startTime = millis();

    // Loop semakan selama masa yang ditetapkan (2 saat)
    while (millis() - startTime < HOLD_TIME_MS) {
      

      if (digitalRead(WAKEUP_PIN) == HIGH) { // Jika jari diangkat terlalu awal
        isLongPress = false; 
        break;               
      }
      
      delay(10); 
      
    boxSetup.text("Hold for 2 seconds to turn on"); 
    }

    // Jika sentuhan terlalu sekejap, terus tidur semula
    if (!isLongPress) {
      Serial.println("Sentuhan sekejap sahaja. Tidur semula...");
      esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKEUP_PIN, 0); 
      esp_deep_sleep_start(); // Program terhenti di sini dan kembali tidur
    }
    
    Serial.println("Sentuhan lama dikesan! Menghidupkan peranti...");
  }

  // 2. Jika ESP berjaya melepasi semakan Long Press ATAU ia dihidupkan buat kali pertama (Power On Reset)
  // Lepaskan kuncian pin semasa tidur
  gpio_hold_dis(GPIO_NUM_13); // Lepaskan pin Buck Converter
  gpio_hold_dis(GPIO_NUM_48); // Lepaskan pin NeoPixel

  // Hidupkan Buck Converter (Pin 13) sebagai persediaan sistem
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
}
void turnOffDevice() {
  tft.fillScreen(TFT_WHITE);
  boxSetup.text("Shutting down...");
  
  pixels.setBrightness(0);
  pixels.clear();
  pixels.show();

  delay(700); 

  // 1. Matikan Buck Converter (Pin 13 LOW)
  pinMode(BUCK_EN, OUTPUT);
  digitalWrite(BUCK_EN, LOW);

  // 2. URUSAN I2C (JANGAN HOLD PIN INI)
  Wire.end(); 
  // Biarkan SDA/SCL jadi INPUT (Float) supaya tak kacau sensor
  pinMode(I2C_SDA, INPUT); 
  pinMode(I2C_SCL, INPUT);

  // 3. KUNCI HANYA PIN BUCK_EN
  // Kita nak buck converter je mati. Talian I2C biar bebas.
  gpio_hold_en(GPIO_NUM_13); 
  
  // Pastikan SDA/SCL TIDAK dikunci (Disable hold jika pernah aktif)
  gpio_hold_dis((gpio_num_t)I2C_SDA);
  gpio_hold_dis((gpio_num_t)I2C_SCL);

  // 4. Konfigurasi Wakeup
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0);

  // 5. Masuk ke mod Deep Sleep
  esp_deep_sleep_start();
}




//"Soft Reset" Register BMP280
void paksaResetBMP() {
  Serial.println("[BMP] Melancarkan Soft Reset Command...");
  Wire.beginTransmission(0x76); // Alamat BMP280
  Wire.write(0xE0);             // Register Reset
  Wire.write(0xB6);             // Nilai Reset
  Wire.endTransmission();
  delay(200);                   // Tunggu sensor "booting" semula
}

void cuciBusAgresif() {
  Serial.println("[I2C] Melakukan pembersihan bus tegar...");
  
  // 1. Matikan I2C
  Wire.end(); 
  
  // 2. Paksa pin jadi GPIO biasa
  pinMode(I2C_SDA, OUTPUT);
  pinMode(I2C_SCL, OUTPUT);

  // 3. Teknik "Manual Clocking"
  // Kadang-kadang sensor sangkut tengah hantar bit '0'.
  // Kita bagi 9-10 denyutan jam untuk paksa sensor habiskan bit tu.
  for (int i = 0; i < 10; i++) {
    digitalWrite(I2C_SCL, LOW);
    delayMicroseconds(10);
    digitalWrite(I2C_SCL, HIGH);
    delayMicroseconds(10);
  }

  // 4. Isyarat STOP Manual (SDA naik masa SCL tengah tinggi)
  digitalWrite(I2C_SDA, LOW);
  delayMicroseconds(10);
  digitalWrite(I2C_SCL, HIGH);
  delayMicroseconds(10);
  digitalWrite(I2C_SDA, HIGH);
  delay(50);
  
  Serial.println("[I2C] Pembersihan Selesai.");
}

void createQR(int x, int y, int w, const char* content, bool showQr) {
    
    // 1. Setup Data QR (Wajib buat walaupun showQr false, sebab kita nak tahu saiz qrcode)
    QRCode qrcode;
    int version = 3; 
    uint8_t qrcodeData[qrcode_getBufferSize(version)];
    
    // Init Text
    qrcode_initText(&qrcode, qrcodeData, version, 0, content);

    // 2. Lukis Background Putih (Quiet Zone)
    // Ini akan sentiasa jalan. Kalau showQr = false, ia jadi kotak putih kosong (cleaner)
    int borderSize = 2; 
    int sizePx = qrcode.size * w;
    
    tft.fillRect(x - (borderSize * w), y - (borderSize * w), 
                 sizePx + (borderSize * 2 * w), 
                 sizePx + (borderSize * 2 * w), 
                 TFT_WHITE);

    // 3. Lukis Modul Hitam (Hanya jika showQr == true)
    if (showQr) {
        for (uint8_t y_idx = 0; y_idx < qrcode.size; y_idx++) {
            for (uint8_t x_idx = 0; x_idx < qrcode.size; x_idx++) {
                
                // Jika modul bernilai 1 (Hitam)
                if (qrcode_getModule(&qrcode, x_idx, y_idx)) {
                    tft.fillRect(x + (x_idx * w), y + (y_idx * w), w, w, TFT_BLACK);
                }
            }
        }
    }
}

float calculateDistance(float latx, float lonx, float laty, float lony) {
    // Pemalar tempatan untuk pengiraan (tiada pembolehubah luar)
    const float PI_VAL = 3.14159265359;
    const float DEG_TO_METER = 111319.9; // 1 darjah latitud ≈ 111.32 km

    // Dapatkan purata latitud dalam unit radian (fungsi cos() dalam C++ perlukan radian)
    float avgLatRad = ((latx + laty) / 2.0) * (PI_VAL / 180.0);

    // Kira perbezaan latitud dan longitud dalam darjah
    float dLat = laty - latx;
    float dLon = lony - lonx;

    // Tukar perbezaan darjah kepada jarak dalam unit meter
    float dLatMeters = dLat * DEG_TO_METER;
    float dLonMeters = dLon * DEG_TO_METER * cos(avgLatRad);

    // Gunakan Teorem Pythagoras: Jarak = punca kuasa dua (x^2 + y^2)
    return sqrt((dLatMeters * dLatMeters) + (dLonMeters * dLonMeters));
}

void goToPage(ScreenState nextPage) {
  if (currentState == nextPage) return; 

  if (historyCount < MAX_HISTORY) {
    pageHistory[historyCount] = currentState;
    historyCount++;
  } else {
    for (int i = 0; i < MAX_HISTORY - 1; i++) {
      pageHistory[i] = pageHistory[i + 1];
    }
    pageHistory[MAX_HISTORY - 1] = currentState;
  }
  
  currentState = nextPage;
  
  // BERITAHU SISTEM PAGE TELAH BERUBAH
  pageChanged = true; 
}

void goBack() {
  if (historyCount > 0) {
    historyCount--;
    currentState = pageHistory[historyCount];
    
    // BERITAHU SISTEM PAGE TELAH BERUBAH
    pageChanged = true; 
  } else {
    if (currentState != HOME) {
      currentState = HOME;
      pageChanged = true; // Beritahu juga jika kembali ke default HOME
    }
  }
}
