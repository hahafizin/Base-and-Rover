void statusBar() {
  //jam
  char timeBuffer[6]; // Sediakan ruang memori untuk 5 aksara (HH:MM) + 1 null terminator
  sprintf(timeBuffer, "%02d:%02d", sharedHour, sharedMinute);
  boxClock.text(String(timeBuffer));

  // Baca tekanan dan tukar ke hPa
  float tekananSekarang = bmp.readPressure() / 100.0;

  // Dapatkan trend (-1, 0, atau 1)
  int trend = loraSys.pressureTrend(tekananSekarang);

  // Paparkan di LCD atau Serial (Contoh sahaja)
  if (trend == 1) pressureIcon.text(ICON_INC, (uint16_t)TFT_BLACK);
  else if (trend == -1) pressureIcon.text(ICON_DEC, (uint16_t)TFT_BLACK);
  else pressureIcon.text(ICON_FLAT, (uint16_t)TFT_BLACK);

  // 3. Logic Switch Case
  RoverMission::WifiStatus currentStatus = mission.getStatus();
  switch (currentStatus) {
    case RoverMission::WifiStatus::OFF:
      // WifiIcon.text(ICON_WIFI_3, (uint16_t)TFT_LIGHTGREY);
      break;
    case RoverMission::WifiStatus::READY:
      // WifiIcon.text(ICON_WIFI_3, (uint16_t)TFT_DARKGREEN);
      break;
    case RoverMission::WifiStatus::CONNECTED:
      // WifiIcon.text(ICON_WIFI_3, (uint16_t)TFT_DARKGREEN);
      break;
    case RoverMission::WifiStatus::UPLOADED:
      // WifiIcon.text(ICON_WIFI_3, (uint16_t)TFT_VIOLET);
      break;
    default:
      break;
  }

  if (loraSys.isConnected) {
    int rssi = LoRa.packetRssi();
    if (loraSys.status) {
      statusLora.drawSignal(rssi, -120, -60, TFT_ORANGE, TFT_WHITE, true);
      pixels.setPixelColor(0, pixels.Color(0, 0, 255)); 
    } else {
      statusLora.drawSignal(rssi, -120, -60, TFT_BLACK, TFT_WHITE, true);

      if (rssi > -20) rssi = -20;
      if (rssi < -120) rssi = -120;
      int redVal = map(rssi, -20, -120, 0, 255);
      int greenVal = map(rssi, -20, -120, 255, 0);
      pixels.setPixelColor(0, pixels.Color(redVal, greenVal, 0));
    } 
  } else {
    statusLora.drawSignal(-1000, -120, -60, TFT_RED, TFT_WHITE, false);
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); 
  }
  pixels.show();

  // Update Hanya Bila Ada Data Baru (Setiap 10 saat)
  statusRover.draw(currentVDOP, 0.8, 2.0, batMon.percentage, "RVER", TFT_BLACK, batMon.isCharging? TFT_DARKGREEN : TFT_BLACK, TFT_WHITE, true, false, true);

  int baseBatteryLevel = loraSys.baseBatteryLevel;
  float baseHdop = loraSys.baseHdop;
  
  //simbol base
  if (baseHdop > 2.0)
    statusBase.draw (baseHdop, 0.8, 2.0, baseBatteryLevel,  "BASE",  TFT_BROWN, TFT_BROWN, TFT_WHITE, false, false, true);
  else
    statusBase.draw (baseHdop, 0.8, 2.0, baseBatteryLevel,  "BASE",  TFT_BROWN, TFT_BROWN, TFT_WHITE, true, false, true);
  
  //garisan pemisah
  tft.drawLine(0, 30, 480, 30, TFT_BLACK);
}

void HOMEPage() {

  boxTitle.textScroll("HOME");

  if (drawNow) {
    tft.drawSmoothRoundRect(20,  53 , 10, 10, 437, 117, TFT_BLACK); //gps
    tft.drawSmoothRoundRect(20,  187, 10, 10, 217, 117, TFT_BLACK); //lora
    tft.drawSmoothRoundRect(249, 187, 10, 10, 203, 117, TFT_BLACK); //other
    drawNow = false;
  }

  BoxHeader1Page1.text("GPS"); //GPS
  BoxHeader2Page1.text("LoRa"); //LoRa
  BoxHeader3Page1.text("Other"); //Other

  // --- BARIS DATA UTAMA ---
  uint16_t textColor;
  if (loraSys.averagingStatus == 0) {
    textColor = TFT_RED;
  } else if (loraSys.averagingStatus == 1) {
    textColor = TFT_ORANGE;
  } else if (loraSys.averagingStatus == 2) {
    textColor = TFT_DARKGREEN;
  }


  Col1Line1Page1.iconText(ICON_DISTANCE, String(loraSys.getDistance.m, 1) + " m", (uint16_t)textColor); 
  Col1Line2Page1.iconText(ICON_SATELLITE, String(sharedSat)); 
  Col1Line3Page1.iconTextScroll(ICON_LOCATION, coordinate.get.zoneName);
  Col1Line4Page1.iconText(ICON_PRECISION, coordinate.get.currentmode ? "WGS84" : "GDM2000"); 

  // --- DATA BASE ---
  Col2Line1Page1.iconText(ICON_TOWER, "BASE"); 
  Col3Line1Page1.iconText(ICON_PRECISION, String(loraSys.baseHdop, 1)); 
  Col4Line1Page1.iconText(ICON_BAT_100, String(loraSys.baseBatteryLevel) + "%"); 
  Col2Line2Page1.iconText(ICON_LOCATION, String(loraSys.txCoordinate.lat, 6) + ", " + String(loraSys.txCoordinate.lon, 6), (uint16_t)textColor); 

  // --- DATA ROVER ---
  Col2Line3Page1.iconText(ICON_NAV, "ROVER"); 
  Col3Line3Page1.iconText(ICON_PRECISION, String(currentVDOP, 1)); 
  Col4Line3Page1.iconText(ICON_BAT_100, String(batMon.percentage) + "%"); 
  Col2Line4Page1.iconText(ICON_LOCATION, String(loraSys.rxCoordinate.lat, 6) + ", " + String(loraSys.rxCoordinate.lon, 6), (uint16_t)textColor); 

  // --- STATUS SAMBUNGAN ---
  if (loraSys.isConnected) {
    Col1Line5Page1.iconText(ICON_LINK, "Connected", (uint16_t)TFT_DARKGREEN);
  } else {
    if (loraSys.hasBeenConnected)
      Col1Line5Page1.iconText(ICON_LINK_OFF, "Away " + String(loraSys.lastConnected) + "s ago" , (uint16_t)TFT_RED);
    else
      Col1Line5Page1.iconText(ICON_INFO, "Waiting for Base", (uint16_t)TFT_ORANGE);
  }

  if (!loraSys.status) {
    Col1Line6Page1.iconText(ICON_INFO, "Ready");
  } else {
    Col1Line6Page1.iconText(ICON_INFO, "Receiving...", (uint16_t)TFT_DARKGREEN);
  }

  // --- KUALITI ISYARAT LORA ---
  Col1Line7Page1.iconText(ICON_SIG_4, String(LoRa.packetRssi())); 
  Col1Line8Page1.iconText(ICON_SNR, String(LoRa.packetSnr())); 

  // --- PENYIMPANAN & SENSOR ---
  // Col2Line5Page1.iconText(ICON_FILE, String(mission.getFileSize() / 1024.0) + " KB"); 
  Col2Line5Page1.iconText(ICON_TEMP, String(bmp.readTemperature(),1) + "°C"); 
  Col2Line6Page1.iconText(ICON_DATA, String(usedStoragePercent, 1) + "%"); 
  Col3Line5Page1.iconText(ICON_COMPASS, String(compass.northHeading()) + "°"); 
  Col3Line6Page1.iconText(ICON_ALTITUDE, String(loraSys.taredAlt, 0) + "m", 1000);

  Page1BtnOff.icon.isPressed(ICON_POWER, [=]() {  
    turnOffDevice();
  });

  //butang touch tare
  Page1BtnSetup.text.isPressed("SETUP", [=]() {  
    goToPage(SETUP);
  });
}

void SETUPPage() {
  

  // 1. Variable static (ingatan jangka panjang)
  static RoverMission::WifiStatus lastStatus = (RoverMission::WifiStatus)-1;
  
  // 2. Variable sementara (reset setiap kali loop)
  bool forceRedraw = false;

  if (drawNow) {
    boxTitle.textScroll("SETUP");
    Col2Line3Page2.text("Manage data");
    Col2Line4Page2.textScroll("Tap here to manage");
    forceRedraw = true;
    drawNow = false;
  }
  
  Col1Line1Page2.text("Upload and");
  Col1Line2Page2.textScroll("download file here");
  Page2BtnUpload.text.isPressed("WIFI SETUP", [=]() {  
    mission.startNetwork();
    goToPage(WIFISETUP);
  });

  Col1Line3Page2.text("Tap to tare Alt & GPS");
  if (loraSys.averagingStatus == 0)
    Col1Line4Page2.textScroll("GPS not tared yet");
  else if (loraSys.averagingStatus == 1)
    Col1Line4Page2.textScroll("Taring... " + String(loraSys.averagingCountdown));
  else if (loraSys.averagingStatus == 2)
    Col1Line4Page2.textScroll("GPS tared");

  Page2BtnTare.text.isPressed("TARE", [=]() {  
    loraSys.calibrate();
  });

  //cek keadaan magneto 
  compass.checkCalibrationStatus();

  Col2Line1Page2.text(compass.isCalibrated? "Calibrated": "Not calibrated");
  Col2Line2Page2.text(compass.calibrateNow? "CALIBRATE NOW": "No interference");

  Page2BtnCal.text.isPressed("CALIBRATE", [=]() {  
    goToPage(CALIBRATION);
  });

  Page2BtnManage.text.isPressed("MANAGE", [=]() {  
    goToPage(MANAGEDATA);
  });

  Page2BtnBack.icon.text.isPressed(ICON_LEFT, "BACK", [=]() {  
    goToPage(HOME);
  });
  Page2BtnSetup2.icon.text.isPressed(ICON_RIGHT, "NEXT", [=]() {  
    goToPage(SETUP2);
  });
}

void MANAGEDATAPage() {
  
  // 1. UPDATE DATA NAVIGATION TERLEBIH DAHULU
  // Penting: Beritahu navigation berapa jumlah data terkini & berapa row
  ManagedataControl.setRowInOnePage(6); 
  ManagedataControl.setTotalIndex(mission.getCount());

  if (drawNow) {
    // mission.printAllData();
    boxTitle.textScroll("MANAGE DATA");
    Col1Line1Page3.text("View, add and delete data here");

    // Header table
    Col1Line3Page3.text("NAME", (uint16_t)TFT_DARKGREEN);
    
    // Details header
    Col2Line3Page3.text("DETAILS", (uint16_t)TFT_DARKGREEN);

    // Column details
    Col2Line4Page3.text("LAT", (uint16_t)TFT_DARKGREEN);
    Col2Line5Page3.text("LON", (uint16_t)TFT_DARKGREEN);
    Col2Line6Page3.text("ALT", (uint16_t)TFT_DARKGREEN);
    Col2Line7Page3.text("PAGE", (uint16_t)TFT_DARKGREEN);
    Col2Line8Page3.text("ID", (uint16_t)TFT_DARKGREEN);
    drawNow = false;
  }

  // Senarai Pointer Kotak UI
  SmartTextBox* listBoxes[] = {
    &Col1Line4Page3, 
    &Col1Line5Page3, 
    &Col1Line6Page3, 
    &Col1Line7Page3, 
    &Col1Line8Page3, 
    &Col1Line9Page3 
  };

  // 2. Loop Display Data
  for (int i = 0; i < 6; i++) {
    // Ambil index mula page dari class (firstRowIndex)
    int currentIndex = ManagedataControl.firstRowIndex + i; 

    // Check adakah index wujud dalam database
    if (currentIndex >= 0 && currentIndex < mission.getCount()) {
      
      String namaMisi = String(mission.get.name(currentIndex));

      // LOGIC HIGHLIGHT:
      // Bandingkan terus index loop dengan index yang dipilih dalam class
      if (currentIndex == ManagedataControl.selectedData) {
        listBoxes[i]->text(namaMisi, (uint16_t)TFT_RED);
      } else {
        listBoxes[i]->text(namaMisi);
      }
      
    } else {
      // Jika data tiada/habis, tunjuk kosong
      listBoxes[i]->text(" ");
    }
  }

  // 3. Display Details (Guna ManagedataControl.selectedData)
  Col3Line4Page3.text(String(mission.get.lat(ManagedataControl.selectedData), 6));
  Col3Line5Page3.text(String(mission.get.lon(ManagedataControl.selectedData), 6));
  Col3Line6Page3.text(String(mission.get.alt(ManagedataControl.selectedData), 0));
  
  // Papar Page: Contoh "1/5"
  Col3Line7Page3.text(String(ManagedataControl.tablePage) + "/" + String(ManagedataControl.totalPage));
  Col3Line8Page3.text(String(ManagedataControl.selectedData));

  // 4. Button Controls
  Page3BtnBack.text.isPressed("BACK", [=]() {  
    goBack();
  });

  // Gunakan lambda [=]() untuk memanggil function dalam class
  Page3BtnPrev.icon.isPressed(ICON_LEFT, [=]() {
    ManagedataControl.prevPage();
    drawNow = true; // Redraw jika perlu update header/list
  });

  Page3BtnNext.icon.isPressed(ICON_RIGHT, [=]() {
    ManagedataControl.nextPage();
    drawNow = true;
  });

  Page3BtnUp.icon.isPressed(ICON_UP, [=]() {
    ManagedataControl.goUp();
    // Tiada drawNow = true disini sebab loop di atas automatik update highlight
    // Kecuali jika page bertukar, loop akan handle juga.
  });

  Page3BtnDown.icon.isPressed(ICON_DOWN, [=]() {
    ManagedataControl.goDown();
  });

  Page3BtnAdd.icon.isPressed(ICON_ADD, [=]() {  
    mission.startNetwork();
    goToPage(WIFISETUP);
  });

  Page3BtnDel.icon.isLongPressed(ICON_DELETE, [=]() {
    mission.deletePlace(ManagedataControl.selectedData);
    // Lepas delete, mungkin perlu reset drawNow atau adjust index
    drawNow = true; 
  });
}

void SETUP2Page() {
  boxTitle.textScroll("SETUP 2");

  Col1Line1Page4.text("Coordinate System");
  if (coordinate.get.currentmode) {
    Col1Line2Page4.text("WGS84");
  } else {
    Col1Line2Page4.text("GDM2000");
  }
  if (coordinate.get.currentmode) {
      Page4BtnChooseCoordSystem.text.isPressed("GDM2000", [=]() {
      coordinate.setToWGS(false);
    });
  } else {
      Page4BtnChooseCoordSystem.text.isPressed("WGS84", [=]() {  
      coordinate.setToWGS(true);
    });
  }

  Col1Line3Page4.text("GDM2000 Zone");
  Col1Line4Page4.text(coordinate.get.zoneName);
  Page4BtnSelectZone.text.isPressed("SELECT ZONE", [=]() {  
    goToPage(SELECTZONE);
  });


  Col1Line5Page4.text("Navigation Mode");
  Col1Line6Page4.text("Choose place");

  Page4BtnNavigation.text.isPressed("NAVIGATE PLACE", [=]() {
    goToPage(NAVIGATINGCHOOSEPLACE);
  });

  Page4BtnBack.icon.text.isPressed(ICON_LEFT, "BACK", [=]() {  
    goToPage(SETUP);
  });
  Page4BtnNext.icon.text.isPressed(ICON_RIGHT, "NEXT", [=]() {  
    goToPage(MODESELECTION);
  });
}

void SELECTZONEPage() {
  
  // 1. SETUP NAVIGATION
  // Tetapkan jumlah data (10 zon) dan row per page (6)
  SelectzonePageControl.setRowInOnePage(6);
  SelectzonePageControl.setTotalIndex(10); // Hardcoded 10 mengikut code asal anda

  if (drawNow) {
    boxTitle.textScroll("SELECT ZONE");
    Col1Line1Page5.text("Choose and select zone here");

    //header table
    Col1Line3Page5.text("ZONE", (uint16_t)TFT_DARKGREEN);
    
    //details header
    Col2Line3Page5.text("DETAILS", (uint16_t)TFT_DARKGREEN);

    //column details
    Col2Line4Page5.text("LAT", (uint16_t)TFT_DARKGREEN);
    Col2Line5Page5.text("LON", (uint16_t)TFT_DARKGREEN);
    drawNow = false;
  }
  
  // 2. Masukkan semua kotak ke dalam array (Senarai Pointer)
  SmartTextBox* listBoxes[] = {
    &Col1Line4Page5, 
    &Col1Line5Page5, 
    &Col1Line6Page5, 
    &Col1Line7Page5, 
    &Col1Line8Page5, 
    &Col1Line9Page5 
  };

  // 3. Loop Display Data
  for (int i = 0; i < 6; i++) {
    // Ambil index berdasarkan page semasa
    int currentIndex = SelectzonePageControl.firstRowIndex + i;

    // Check adakah index wujud (0 hingga 9)
    if (currentIndex >= 0 && currentIndex < 10) { 
      
      String namaMisi = coordinate.get.zoneFromIndex(currentIndex);

      // Check adakah baris ini sedang 'selected'?
      if (currentIndex == SelectzonePageControl.selectedData) {
        listBoxes[i]->text(namaMisi, (uint16_t)TFT_RED);
        
        // PENTING: Update backend bila cursor bergerak ke sini
        coordinate.setZone(currentIndex); 
      } else {
        listBoxes[i]->text(namaMisi);
      }
      
    } else {
      // Jika data tiada/habis, tunjuk kosong
      listBoxes[i]->text(" ");
    }
  }

  // Paparkan Lat/Lon untuk zon yang sedang dipilih
  // (Data ini automatik update sebab kita dah call setZone dalam loop di atas)
  Col3Line4Page5.text(String(coordinate.get.lat0, 6));
  Col3Line5Page5.text(String(coordinate.get.lon0, 6));

  // --- BUTTON CONTROLS ---

  // Button Prev Page (<)
  Page5BtnPrev.icon.isPressed(ICON_LEFT, [=]() {
    SelectzonePageControl.prevPage();
  });

  // Button Next Page (>)
  // Logic "Smart Sticky Cursor" dah ada dalam class PageNavigation
  Page5BtnNext.icon.isPressed(ICON_RIGHT,  [=]() {
    SelectzonePageControl.nextPage();
  });

  // Button Up (^)
  Page5BtnUp.icon.isPressed(ICON_UP, [=]() {
    SelectzonePageControl.goUp();
  });

  // Button Down (v)
  // Logic "Seamless Scrolling" & "Infinite Loop" dah ada dalam class PageNavigation
  Page5BtnDown.icon.isPressed(ICON_DOWN, [=]() {
    SelectzonePageControl.goDown();
  });

  // Button Select/Back
  Page5BtnBack.text.isPressed("SELECT", [=]() {  
    goToPage(SETUP2);
  });
}

void WIFISETUPPage () {

  boxTitle.text("TRANSFER DATA");
  
  // 4. Dapatkan status terkini
  RoverMission::WifiStatus currentStatus = mission.getStatus();
  static RoverMission::WifiStatus lastStatus = (RoverMission::WifiStatus)-1;

  // 5. Logic Redraw Power
  // Lukis JIKA: (Status Berubah) ATAU (Kena Paksa Lukis/Force Redraw)
  if (currentStatus != lastStatus || drawNow) {
    
    lastStatus = currentStatus; // Simpan status baru
    
    // Pindahkan Switch Case LUKISAN sahaja ke sini
    switch (currentStatus) {
      case RoverMission::WifiStatus::OFF:
        Col1Line1Page6.iconText(ICON_INFO, "Tap 'ON WI-FI' to begin");
        createQR(270, 66, 6, "", false);
        break;
        
      case RoverMission::WifiStatus::READY:
        Col1Line1Page6.iconText(ICON_INFO, "Scan the QR code below to connect to Wi-Fi, or use the following credentials:\n\nName:\nRover\n\nPassword:\npassword123");
        createQR(270, 66, 6, "WIFI:T:WPA;S:Rover;P:password123;;", true);
        break;
        
      case RoverMission::WifiStatus::CONNECTED:
        Col1Line1Page6.iconText(ICON_INFO, "Wi-Fi Connected!\n\nScan the QR code to open the webpage, or visit the link below:\n\nhttp://192.168.4.1");
        createQR(270, 66, 6, "http://192.168.4.1", true);
        break;
        
      case RoverMission::WifiStatus::UPLOADED:
        Col1Line1Page6.iconText(ICON_INFO, "File uploaded successfully!\n\nPlacemark: " + String(mission.getCount()));
        break;
        
      default:
        break;
    }
  }
  drawNow = false;

  Page6BtnBack.text.isPressed("BACK", [=]() {
    mission.stopNetwork();
    goBack();
  });

  // Page6BtnOnWifi.x = Page6BtnBack.x + Page6BtnBack.w + 10;
  // switch (currentStatus) {
  //   case RoverMission::WifiStatus::OFF:
  //     Page6BtnOnWifi.text.isPressed("ON WI-FI", [=]() {
  //       mission.startNetwork();
  //     });
  //     break;
  //   default:
  //     Page6BtnOnWifi.text.isPressed("OFF WI-FI", [=]() {
  //       mission.startNetwork();
  //     });
  //     break;
  // }

  
}


void NAVIGATINGCHOOSEPLACEPage() {

  boxTitle.textScroll("CHOOSE POINT");

  if (drawNow) {
    Col1Line1Page11.text("Loading...");
    drawNow = false;
  }

  tft.drawRect(14, 46, 200, 200, TFT_BLACK);

  mapViewer.onLoadingProgress = [](float pct) {
    // Fungsi ini akan dipanggil secara automatik oleh header semasa proses loading
    Col1Line5Page11.text("Loading Map " + String(pct, 0) + "%");
    
    // (Pilihan) Jika awak nak tengok di Serial Monitor juga:
    // Serial.printf("Loading Map: %.0f%%\n", pct);
  };


  uint16_t t_x = 0, t_y = 0;
  bool touched = tft.getTouch(&t_x, &t_y);
  
  // Masukkan koordinat kotak peta awak yang sebenar (x, y, w, h)
  mapViewer.handleTouch(touched, t_x, t_y, 14, 46, 200, 200);

  mapViewer.drawMissionMap(
    14, 46, 200, 200,  // x, y, w, h
    10,                // Padding
    TFT_BLACK,         // Warna Kotak
    TFT_DARKGREY,      // Warna Titik Biasa
    TFT_RED,           // Warna Titik SELECTED
    TFT_BLUE,          // Warna Lokasi User
    TFT_WHITE          // Warna Background
  );

  Col1Line1Page11.textScroll(String(mapViewer.pointName));

  float distance = calculateDistance(mapViewer.pointLat, mapViewer.pointLon, loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon);
  Col1Line2Page11.iconText(ICON_DISTANCE, String(distance, 1) + "m", (currentVDOP < 1.5 && currentVDOP != 0.0)? (uint16_t)TFT_DARKGREEN : (uint16_t)TFT_RED);

  Col1Line3Page11.iconText(ICON_DATA, String(mapViewer.currentPoint+1) + "/" + String(mapViewer.totalPoint));

  coordinate.convToGDM(mapViewer.pointLat, mapViewer.pointLon);
  Col1Line4Page11.iconText(ICON_TARGET, String(coordinate.get.x, coordinate.get.currentmode? 6 : 2) + 
                                 ", " + String(coordinate.get.y, coordinate.get.currentmode? 6 : 2));
  
  // Update lokasi user pada map preview
  mapViewer.updateLocation(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon);

  // 5. Button Controls
  
  Page11BtnBack.text.isPressed("BACK", [=]() { 
    mapViewer.freeMapMemory(); 
    goBack();
  });

  Page11BtnSkipLeft.x = Page11BtnBack.x + Page11BtnBack.w + 10;
  Page11BtnSkipLeft.icon.isPressed(ICON_SKIPLEFT, [=]() { mapViewer.jumpDown(); });

  Page11BtnLeft.x = Page11BtnSkipLeft.x + Page11BtnSkipLeft.w + 10;
  Page11BtnLeft.icon.isPressed(ICON_LEFT, [=]() { mapViewer.down(); });

  Page11BtnRight.x = Page11BtnLeft.x + Page11BtnLeft.w + 10;
  Page11BtnRight.icon.isPressed(ICON_RIGHT, [=]() { mapViewer.up(); });

  Page11BtnSkipRight.x = Page11BtnRight.x + Page11BtnRight.w + 10;
  Page11BtnSkipRight.icon.isPressed(ICON_SKIPRIGHT, [=]() { mapViewer.jumpUp(); });

  Page11BtnSelect.x = Page11BtnSkipRight.x + Page11BtnSkipRight.w + 10;
  Page11BtnSelect.text.isPressed("CONFIRM", [=]() {
    // 1. Set Destinasi (Point B)
    nav.setEnd(mapViewer.pointLat, mapViewer.pointLon);
    
    // 2. Config Log (Ikut Masa)
    nav.recordByTime(false);

    // 3. Panggil .start() untuk set Point A & Mula Navigasi
    nav.start(
        loraSys.rxCoordinate.lat,
        loraSys.rxCoordinate.lon, 
        sharedYear, sharedMonth, sharedDay, sharedHour, sharedMinute
    );
    
    cte.setPointA(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon);
    cte.setPointB(mapViewer.pointLat, mapViewer.pointLon);
    
    Serial.println("(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon) : " + String(loraSys.rxCoordinate.lat,6) + "," + String(loraSys.rxCoordinate.lon,6));
    Serial.println("(mapViewer.pointLat, mapViewer.pointLon) : " + String(mapViewer.pointLat,6) + "," + String(mapViewer.pointLon,6));

    // 4. CLEAR MAP LAMA (Bebaskan ~92KB RAM)
    mapViewer.freeMapMemory();
    
    // ---------------------------------------------------------
    // 5. TAMBAH INI: ALLOCATE MAP BARU (Gunakan ruang RAM yang dah kosong)
    // ---------------------------------------------------------
    // Masukkan parameter x, y, w, h dan warna mengikut UI awak
    cte.drawMap(14, 46, 140, 202, TFT_BLACK, TFT_RED, TFT_BLACK, TFT_RED, TFT_WHITE, TFT_BLACK);
    
    // 6. Tukar Page
    goToPage(NAVIGATINGPLACE);
  });

  if (loraSys.isOffsetDone) {
    Page11BtnRemoveOffset.x = Page11BtnSelect.x + Page11BtnSelect.w - 170;
    Page11BtnRemoveOffset.y = Page11BtnSelect.y - 60;
    Page11BtnRemoveOffset.text.isPressed("DEL. OFFSET", [=]() { loraSys.removeOffset(); });
  }
}

void NAVIGATINGPLACEPage() {

  if (drawNow) {
    // cte.drawMap(14, 46, 140, 202, TFT_BLACK, TFT_RED, TFT_BLACK, TFT_RED, TFT_WHITE, TFT_BLACK);
    drawNow = false;
  }

  boxTitle.textScroll("NAVIGATING");
  
  // 1. UPDATE NAVIGASI (Wajib panggil setiap loop)
  // Masukkan koordinat terkini (Tared/GPS)
  nav.update(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon);

  //lukis map CTE
  cte.updateLocation(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon);

  // Variable untuk map (nav.distance dalam meter)
  int pointDistance = (int)nav.distance; 

  // Tunjukkan Map Navigasi menggunakan variable dari nav struct
  // myMap.show(
  //     nav.startPoint.lat, nav.startPoint.lon, 
  //     nav.endPoint.lat, nav.endPoint.lon, 
  //     loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon, 
  //     pointDistance
  // );

  Col1Line1Page12.textScroll(String(mapViewer.pointName));

  if(currentVDOP < 1.5 && currentVDOP != 0.0) { //kalau gps dapat signal
    Col1Line2Page12.text("Dist: " + String(nav.distance, 0) + "m, Alt: " + String(loraSys.taredAlt, 0) + "m", (uint16_t)TFT_DARKGREEN);
  } else {
    Col1Line2Page12.text("GNSS NOT FIXED", (uint16_t)TFT_RED);
  }

  if (nav.cte < -5) //kalau user ke kiri
    Col1Line3Page12.iconText(ICON_RIGHT, "Go Right " + String(nav.cte, 1) + "m");
  else if (nav.cte > 5)
    Col1Line3Page12.iconText(ICON_LEFT, "Go Left " + String(nav.cte, 1) + "m");
  else
    Col1Line3Page12.iconText(ICON_UP, "Straight");

  Col1Line4Page12.iconText(ICON_DISTANCE, String(loraSys.getDistance.m,1) + "m");

  // Koordinat Target (GDM/WGS)
  coordinate.convToGDM(nav.endPoint.lat, nav.endPoint.lon);
  Col1Line5Page12.iconText(ICON_TARGET, String(coordinate.get.x, coordinate.get.currentmode? 6 : 2) + 
                                 ", " + String(coordinate.get.y, coordinate.get.currentmode? 6 : 2));

  int currentHeading = compass.northHeading();
  int targetBearing = compass.headingTo(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon, nav.endPoint.lat, nav.endPoint.lon);
  drawCompass(currentHeading, targetBearing, TFT_BLACK, TFT_WHITE, 200, 215, 30);

  // BUTTONS
  Page12BtnBack.icon.isPressed(ICON_LEFT, [=]() {
    nav.stop(); // Stop rekod log
    cte.clearMemory();
    myMap.clear();
    goBack();
  });

  // [FEATURE TAMBAHAN] Butang untuk tanda Tasik/Halangan (Add Point)
  Page12BtnMark.x = Page12BtnBack.x + Page12BtnBack.w + 10;
  Page12BtnMark.icon.isPressed(ICON_WAYPOINT, [=]() {  
     nav.addPoint(); // Tanda bucu halangan dalam log
  });

  Page12BtnTare.x = Page12BtnMark.x + Page12BtnMark.w + 10;
  Page12BtnTare.icon.isPressed(ICON_TARE, [=]() {
    loraSys.saveBasePosition();
  });

  // Kalau nak kekalkan butang tukar GDM/WGS asal, guna code bawah ni:
  Page12BtnChooseCoordSystem.x = Page12BtnTare.x + Page12BtnTare.w + 10;
  if (coordinate.get.currentmode) {
    Page12BtnChooseCoordSystem.icon.text.isPressed(ICON_SWAP, "COORD", [=]() { coordinate.setToWGS(false); });
  } else {
    Page12BtnChooseCoordSystem.icon.text.isPressed(ICON_SWAP, "COORD", [=]() { coordinate.setToWGS(true); });
  }

  Page12BtnConfirm.x = Page12BtnChooseCoordSystem.x + Page12BtnChooseCoordSystem.w + 10;
  Page12BtnConfirm.icon.text.isPressed(ICON_DONE, "DONE", [=]() {
    nav.stop();
    cte.clearMemory();
    myMap.clear();
    goToPage(DONENAVIGATION);
  });

  if (!loraSys.isOffsetDone) {
    Page12BtnDoneCalibration.x = Page12BtnConfirm.x + Page12BtnConfirm.w - 180;
    Page12BtnDoneCalibration.y = Page12BtnConfirm.y - 60;
    Page12BtnDoneCalibration.icon.text.isPressed(ICON_LEFT, "DONE CALIB", [=]() {
      
      nav.stop();
      cte.clearMemory();
      myMap.clear();

      nav.calculateOffset(loraSys.rxCoordinate.lat, loraSys.rxCoordinate.lon, mapViewer.pointLat, mapViewer.pointLon);
      loraSys.addOffset(nav.dLat, nav.dLon);

      // Gunakan nilainya untuk dipapar atau dikira
      Serial.print("Delta Latitud: ");
      Serial.println(nav.dLat, 6);
      Serial.print("Delta Longitud: ");
      Serial.println(nav.dLon, 6);
      goBack();
    });
  }
}


void MODESELECTIONPage() {

  boxTitle.textScroll("SETTINGS");

  Col1Line1Page10.text("View KML path");
  Col1Line2Page10.text("and delete path");

  Col1Line3Page10.text("Delay to dim");
  Col1Line4Page10.text(String(bl.currentDelayDim));

  Col1Line5Page10.text("Delay to screen off");
  Col1Line6Page10.text(String(bl.currentDelayOff));

  Page10BtnViewKML.text.isPressed("VIEW KML", [=]() {
    goToPage(VIEWKML);
    kml.open("/path_log.kml");
    SelectKMLPathControl.setTotalIndex(kml.getTotalPaths());
    SelectKMLPathControl.reset();
  });
  Page10BtnDelayDim.text.isPressed("SET TO: " + String(bl.nextCycleDim), [=]() {
    bl.cycleDim();
  });
  Page10BtnDelayOff.text.isPressed("SET TO: " + String(bl.nextCycleOff), [=]() {
    bl.cycleScreenOff();
  });

  Page10BtnBack.icon.text.isPressed(ICON_LEFT, "BACK", [=]() {
    goToPage(SETUP2);
  });
}

void VIEWKMLPage() {
  boxTitle.textScroll("VIEW KML");

  kml.drawMap(14, 46, 230, 200, 15, TFT_RED, TFT_BLACK, TFT_WHITE);

  Col1Line1Page9.text("Path " + String(SelectKMLPathControl.selectedData+1));
  Col1Line2Page9.text(String(kml.viewSelectedPath()));
  Col1Line3Page9.text(String(kml.totalDistance.km,1) + "km");
  
  Page9BtnBack.icon.text.isPressed(ICON_LEFT, "BACK", [=]() {
    goBack();
  });

  Page9BtnUp.x = Page9BtnBack.x + Page9BtnBack.w + 10;
  Page9BtnUp.icon.isPressed(ICON_UP, [=]() {
    SelectKMLPathControl.goUp();
    kml.select(SelectKMLPathControl.selectedData);
    kml.updateMap();
  });

  Page9BtnDown.x = Page9BtnUp.x + Page9BtnUp.w + 10;
  Page9BtnDown.icon.isPressed(ICON_DOWN, [=]() {
    SelectKMLPathControl.goDown();
    kml.select(SelectKMLPathControl.selectedData);
    kml.updateMap();
  });

  Page9BtnDel.x = Page9BtnDown.x + Page9BtnDown.w + 10;
  Page9BtnDel.icon.text.isPressed(ICON_DELETE, "DEL", [=]() {
    kml.select(SelectKMLPathControl.selectedData);
    kml.deleteSelectedPath();
    SelectKMLPathControl.goDown();
    kml.select(SelectKMLPathControl.selectedData);
    kml.updateMap();
  });

  Page9BtnDone.x = Page9BtnDel.x + Page9BtnDel.w + 10;
  Page9BtnDone.text.isPressed("DONE", [=]() {
    goToPage(MODESELECTION);
    kml.close();
  });
}

void CALIBRATIONPage() {
  boxTitle.textScroll("MAG CALIBRATION");
  // 1. Sentiasa panggil update
  bool justFinished = compass.update();

  // 2. Pembolehubah statik untuk menyimpan masa supaya mesej tak hilang cepat
  static unsigned long msgTimer = 0;
  static bool showResultMsg = false;

  // Jika kalibrasi baru sahaja tamat, kita mula kira masa 3 saat
  if (justFinished) {
    msgTimer = millis(); 
    showResultMsg = true;
  }

  // Jika sudah lebih 3 saat (3000ms), matikan paparan mesej result
  if (showResultMsg && (millis() - msgTimer > 5000)) {
    showResultMsg = false;
  }


  // --- LOGIK PAPARAN (Disusun ikut keutamaan) ---

  if (compass.isCalibrating) {
    // KEUTAMAAN 1: Sedang Calibrate
    Col1Line1Page7.text("CALIBRATING IN: " + String(compass.countdown) + "s");
    Col1Line2Page7.text("DO FIGURE 8 MOTION");
  } 
  else if (showResultMsg) {
    // KEUTAMAAN 2: Baru tamat (Mesej ini akan ditahan selama 3 saat)
    if (compass.isSuccessful) {
      Col1Line1Page7.text("CALIBRATION SUCCESS!");
      Col1Line2Page7.text("DATA SAVED");
    } else {
      Col1Line1Page7.text("CALIBRATION FAILED!", (uint16_t)TFT_RED);
      Col1Line2Page7.text("PLEASE RETRY");
    }
  }
  else if (compass.calibrateNow) {
    // KEUTAMAAN 3: Perlu Calibrate
    if (!compass.isCalibrated) {
      // Kes 1: Memori kosong / belum pernah calibrate
      Col1Line1Page7.text("STATUS: NEED CALIBRATION");
      Col1Line2Page7.text("TAP CALIBRATE TO START");
    } else if (!compass.isSuccessful) {
      // Kes 2: User cuba calibrate tadi tapi gagal (tak cukup pusing)
      Col1Line1Page7.text("CALIBRATION UNSUCCESSFUL", (uint16_t)TFT_RED);
      Col1Line2Page7.text("PLEASE CALIBRATE AGAIN", (uint16_t)TFT_RED);
    } else {
      // Kes 3: Ada gangguan besi / magnet berhampiran (Drifted)
      Col1Line1Page7.text("MAGNETIC INTERFERENCE!", (uint16_t)TFT_RED);
      Col1Line2Page7.text("PLEASE CALIBRATE AGAIN", (uint16_t)TFT_RED);
    }
  }
  else if (compass.isCalibrated) {
    // KEUTAMAAN 4: Semuanya Normal
    Col1Line1Page7.text("COMPASS READY & NORMAL");
    Col1Line2Page7.text("TAP 'CALIBRATE' TO START");
  } 
  else {
    // Default jika sensor tidak dikesan
    Col1Line1Page7.text("SENSOR ERROR", (uint16_t)TFT_RED);
    Col1Line2Page7.text("PLEASE CHECK WIRING", (uint16_t)TFT_RED);
  }


  drawCompassNorth(compass.northHeading(), TFT_BLACK, TFT_WHITE, tft.width()/2, 180, 50);
  

  
  Page7BtnBack.text.isPressed("BACK", [=]() {
    goBack();
    kml.close();
  });
  
  Page7BtnCalibrate.x = Page7BtnBack.x + Page7BtnBack.w + 10;
  Page7BtnCalibrate.text.isPressed("CALIBRATE", [=]() {
    compass.startCalibration();
  });

}

void DONENAVIGATIONPage() {
  
  Col1Line1Page13.text("View Project"); //View Project
  Col1Line2Page13.text(""); //

  Col1Line3Page13.text("Final Report"); //Final Report
  Col1Line4Page13.text(""); //

  Col1Line5Page13.text("Delete Project"); //Delete Project
  Col1Line6Page13.text(""); //

  Page13BtnViewProject.text.isPressed("View Project", [=]() {
    goToPage(VIEWKML);
    kml.open("/path_log.kml");
    SelectKMLPathControl.setTotalIndex(kml.getTotalPaths());
    SelectKMLPathControl.reset();
  }); //View Project
  Page13BtnFinalReport.text.isPressed("Final Report", [=]() {
    mission.startNetwork();
    goToPage(WIFISETUP);
  }); //Final Report
  Page13BtnDeleteProject.text.isPressed("Delete Project", [=]() {
    mission.deleteFile();
  }); //Delete Project

  Page13BtnBack.text.isPressed("BACK", [=]() {
    goBack();
  });//back
  Page13BtnNext.text.isPressed("NEXT", [=]() {
    goToPage(SETUP2);
  }); //next

}


