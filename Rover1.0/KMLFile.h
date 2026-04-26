#ifndef KML_FILE_H
#define KML_FILE_H

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ==========================================
// KONFIGURASI
// ==========================================
#define MAX_PLACEMARK 1000      
#define NAME_LEN 41         // DINAIKKAN KE 41 untuk memuatkan format "NamaFail: NamaTempat"
#define WIFI_SSID "Rover"
#define WIFI_PASS "password123"

// ==========================================
// HTML & JAVASCRIPT
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Rover Mission Sync</title>

  <style>
    body { font-family: -apple-system, sans-serif; background: #f0f2f5; display: flex; justify-content: center; align-items: center; min-height: 100vh; margin: 0; padding: 20px; color: #333; }
    .card { background: white; padding: 25px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); width: 100%; max-width: 450px; text-align: center; transition: 0.3s; }
    .header-row { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    h2 { margin: 0; color: #2563eb; font-size: 1.5em; }
    
    /* TABS */
    .tabs { display: flex; border-bottom: 1px solid #ddd; margin-bottom: 20px; }
    .tab-btn { flex: 1; padding: 10px 5px; cursor: pointer; border: none; background: none; font-weight: 600; color: #666; border-bottom: 3px solid transparent; transition: 0.2s; font-size: 0.9em; }
    .tab-btn.active { color: #2563eb; border-bottom: 3px solid #2563eb; }
    .tab-content { display: none; }
    .tab-content.active { display: block; animation: fadeIn 0.3s; }

    /* FORMS & UPLOADS */
    .input-group { margin-bottom: 15px; text-align: left; }
    label { display: block; font-size: 0.85em; font-weight: 600; margin-bottom: 5px; color: #555; }
    input[type=text], input[type=number] { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 6px; box-sizing: border-box; }
    .upload-box { border: 2px dashed #cbd5e1; padding: 20px; border-radius: 8px; margin-bottom: 15px; background: #f8fafc; }
    input[type=file] { width: 100%; }

    /* PREVIEW TABLE */
    #preview { margin-top: 15px; text-align: left; max-height: 150px; overflow-y: auto; display: none; border: 1px solid #eee; font-size: 0.8em; }
    table { width: 100%; border-collapse: collapse; }
    th, td { padding: 6px; border-bottom: 1px solid #eee; }
    th { background: #f8fafc; position: sticky; top: 0; }

    /* DOWNLOAD TAB STYLES */
    .file-list { text-align: left; max-height: 300px; overflow-y: auto; border: 1px solid #e2e8f0; border-radius: 8px; margin-bottom: 20px; background: #f8fafc; }
    .file-item { padding: 12px; border-bottom: 1px solid #e2e8f0; display: flex; justify-content: space-between; align-items: center; transition: 0.2s; }
    .file-item:last-child { border-bottom: none; }
    .file-item:hover { background: #e0f2fe; }
    .file-name { font-weight: 500; color: #475569; font-size: 0.95em; }
    .file-size { font-size: 0.8em; color: #94a3b8; margin-right: 10px; }

    /* PROGRESS BAR */
    #progressContainer { display: none; margin-bottom: 20px; text-align: left; }
    .progress-track { background: #e2e8f0; border-radius: 10px; height: 10px; width: 100%; overflow: hidden; }
    .progress-fill { background: #22c55e; height: 100%; width: 0%; transition: width 0.2s ease; }
    .progress-text { font-size: 0.8em; color: #64748b; margin-top: 5px; display: block; text-align: right; }

    /* BUTTONS */
    .btn { border: none; padding: 12px; border-radius: 6px; font-weight: 600; cursor: pointer; width: 100%; font-size: 15px; transition: 0.2s; margin-top: 10px; }
    .btn-primary { background: #2563eb; color: white; }
    .btn-primary:hover { background: #1d4ed8; }
    .btn-danger { background: #ef4444; color: white; margin-top: 30px; }
    .btn-danger:hover { background: #dc2626; }
    .btn:disabled { background: #cbd5e1; cursor: not-allowed; }
    
    .action-btns { display: flex; gap: 8px; }
    .btn-dl { background: #2563eb; color: white; border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-size: 0.85em; transition: 0.2s; }
    .btn-dl:hover { background: #1d4ed8; }
    .btn-dl:disabled { background: #94a3b8; cursor: not-allowed; }
    .btn-del { background: #ef4444; color: white; border: none; padding: 6px 12px; border-radius: 4px; cursor: pointer; font-size: 0.85em; transition: 0.2s; }
    .btn-del:hover { background: #dc2626; }
    .btn-del:disabled { background: #fca5a5; cursor: not-allowed; }

    .status { margin-top: 15px; font-size: 0.9em; font-weight: bold; min-height: 1.2em; }
    .success { color: #16a34a; }
    .error { color: #dc2626; }
    
    /* DISCONNECT SCREEN */
    #disconnect-overlay { position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: #f8fafc; z-index: 1000; display: none; flex-direction: column; justify-content: center; align-items: center; text-align: center; animation: fadeIn 0.5s; }
    .disconnect-icon { font-size: 60px; margin-bottom: 20px; animation: scaleUp 0.5s cubic-bezier(0.175, 0.885, 0.32, 1.275); }
    
    @keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
    @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }
    @keyframes scaleUp { from { transform: scale(0); } to { transform: scale(1); } }
  </style>
</head>
<body>

  <div class="card" id="main-ui">
    <div class="header-row">
      <h2>Rover Sync</h2>
    </div>

    <div class="tabs">
      <button class="tab-btn active" onclick="switchTab('upload')">Upload KML</button>
      <button class="tab-btn" onclick="switchTab('manual')">Add Point</button>
      <button class="tab-btn" onclick="switchTab('download')">Manage Data</button>
    </div>

    <div id="tab-upload" class="tab-content active">
      <div class="upload-box"><input type="file" id="fileInput" accept=".kml" multiple></div>
      <div id="preview"><table id="dataTable"><thead><tr><th>Nama</th><th>Lon</th><th>Lat</th></tr></thead><tbody></tbody></table></div>
      <button class="btn btn-primary" id="btnUpload" onclick="uploadKML()" disabled>Upload to Rover</button>
    </div>

    <div id="tab-manual" class="tab-content">
      <div class="input-group"><label>Nama Tempat (Max 40)</label><input type="text" id="pName" placeholder="Contoh: Checkpoint A" maxlength="40"></div>
      <div class="input-group"><label>Latitude (Y)</label><input type="number" id="pLat" step="any" placeholder="3.123456"></div>
      <div class="input-group"><label>Longitude (X)</label><input type="number" id="pLon" step="any" placeholder="101.123456"></div>
      <div class="input-group"><label>Altitude (Z)</label><input type="number" id="pAlt" step="any" value="0"></div>
      <button class="btn btn-primary" onclick="addManualPoint()">Simpan Point</button>
    </div>

    <div id="tab-download" class="tab-content">
      <div style="text-align: left; margin-bottom: 15px;">
        <div style="display: flex; justify-content: space-between; font-size: 0.85em; color: #64748b; margin-bottom: 5px; font-weight: 600;">
          <span>Storan ESP32</span>
          <span id="storageText">Kira...</span>
        </div>
        <div class="progress-track">
          <div class="progress-fill" id="storageBar" style="background: #3b82f6; width: 0%;"></div>
        </div>
      </div>

      <div id="progressContainer">
        <div class="progress-track"><div class="progress-fill" id="progressBar"></div></div>
        <span class="progress-text" id="progressStatus">Starting...</span>
      </div>

      <div class="file-list" id="fileListContainer">
        <div style="padding:20px; text-align:center; color:#94a3b8;">Loading files...</div>
      </div>
    </div>

    <div class="status" id="statusMsg"></div>
    <hr style="margin-top:25px; border:0; border-top:1px solid #eee;">
    <button class="btn btn-danger" onclick="finishSync()">Finish & Close WiFi</button>
  </div>

  <div id="disconnect-overlay">
    <div class="disconnect-icon">🛜🚫</div>
    <h2 style="color: #475569;">WiFi Dimatikan</h2>
    <p style="color: #64748b; margin-top: 10px;">Sesi sinkronisasi telah tamat.<br>Rover kini dalam mod operasi.</p>
  </div>

  <script>
    function switchTab(tabName) { 
      document.querySelectorAll('.tab-content').forEach(el => el.classList.remove('active')); 
      document.querySelectorAll('.tab-btn').forEach(el => el.classList.remove('active')); 
      document.getElementById('tab-' + tabName).classList.add('active'); 
      document.querySelector(`button[onclick="switchTab('${tabName}')"]`).classList.add('active'); 
      setStatus(""); 
      if(tabName === 'download') loadFiles();
    }
    
    function setStatus(msg, type = "") { const el = document.getElementById('statusMsg'); el.innerText = msg; el.className = "status " + type; }
    
    // --- MULTIPLE KML UPLOAD LOGIC ---
    const fileInput = document.getElementById('fileInput'); 
    let rawMergedKML = ""; 

    function readFileAsync(file) {
      return new Promise((resolve, reject) => {
        let reader = new FileReader();
        reader.onload = () => resolve(reader.result);
        reader.onerror = reject;
        reader.readAsText(file);
      });
    }

    fileInput.addEventListener('change', async function(e) { 
      const files = fileInput.files; 
      if (files.length === 0) return; 
      
      document.getElementById('btnUpload').disabled = true;
      setStatus(`Memproses ${files.length} fail...`, "");
      
      let allPlacemarksCount = 0;
      let mergedKMLContent = "";
      const tbody = document.querySelector('#dataTable tbody'); 
      tbody.innerHTML = ""; 
      
      for (let f = 0; f < files.length; f++) {
        // Dapatkan nama fail (buang .kml di hujung)
        let baseFileName = files[f].name.replace(/\.[^/.]+$/, "");
        
        let text = await readFileAsync(files[f]);
        const parser = new DOMParser(); 
        const xmlDoc = parser.parseFromString(text, "text/xml"); 
        
        // --- 0. UBAHSUAI NAMA PLACEMARK SEBELUM DIGABUNG (TAMBAH NAMA FAIL) ---
        let pMarks = xmlDoc.getElementsByTagNameNS("*", "Placemark");
        if (pMarks.length === 0) pMarks = xmlDoc.getElementsByTagName("Placemark");
        
        for(let p = 0; p < pMarks.length; p++) {
            let nNodes = pMarks[p].getElementsByTagNameNS("*", "name");
            if (nNodes.length === 0) nNodes = pMarks[p].getElementsByTagName("name");
            
            if (nNodes.length > 0) {
                // Tulis ganti nama: "NamaFail: NamaAsal"
                nNodes[0].textContent = baseFileName + ": " + nNodes[0].textContent;
            } else {
                // Jika Placemark asalnya tiada <name>, kita letak nama baru
                let newNameNode = xmlDoc.createElement("name");
                newNameNode.textContent = baseFileName + ": P" + (p + 1);
                pMarks[p].insertBefore(newNameNode, pMarks[p].firstChild);
            }
        }

        // --- 1. GABUNGKAN KANDUNGAN ASAL KML ---
        let docNode = xmlDoc.getElementsByTagNameNS("*", "Document")[0] || xmlDoc.getElementsByTagNameNS("*", "kml")[0];
        if (docNode) {
          let children = docNode.childNodes;
          for(let i = 0; i < children.length; i++) {
             if (children[i].nodeType === 1) { 
                 // Handle tag sama ada dengan namespace prefix atau tanpa prefix
                 let localName = children[i].localName || children[i].nodeName;
                 
                 // Abaikan tag nama dokumen untuk elak konflik
                 if (localName !== "name" && localName !== "LookAt" && localName !== "open") {
                     mergedKMLContent += new XMLSerializer().serializeToString(children[i]) + "\n";
                 }
             }
          }
        }

        // --- 2. BACA UNTUK PREVIEW JADUAL ---
        for (let i = 0; i < pMarks.length; i++) { 
          let nameNodes = pMarks[i].getElementsByTagNameNS("*", "name");
          if(nameNodes.length === 0) nameNodes = pMarks[i].getElementsByTagName("name");
          
          let rawName = (nameNodes.length > 0) ? nameNodes[0].textContent : (baseFileName + ": P" + (allPlacemarksCount+1)); 
          let cleanName = rawName.replace(/,/g, " ").trim().substring(0, 40); 
          
          let coordsNodes = pMarks[i].getElementsByTagNameNS("*", "coordinates");
          if(coordsNodes.length === 0) coordsNodes = pMarks[i].getElementsByTagName("coordinates");
          
          if (coordsNodes.length > 0) { 
            let coords = coordsNodes[0].textContent.trim();
            let firstCoord = coords.split(/\s+/)[0]; 
            let parts = firstCoord.split(','); 
            if (parts.length >= 2) {
              let lon = parseFloat(parts[0]).toFixed(6); 
              let lat = parseFloat(parts[1]).toFixed(6); 
              tbody.innerHTML += `<tr><td>${cleanName}</td><td>${lon}</td><td>${lat}</td></tr>`; 
              allPlacemarksCount++; 
            }
          } 
        } 
      } 

      if (allPlacemarksCount > 0) { 
        rawMergedKML = `<?xml version="1.0" encoding="UTF-8"?>\n<kml xmlns="http://www.opengis.net/kml/2.2" xmlns:gx="http://www.google.com/kml/ext/2.2" xmlns:kml="http://www.opengis.net/kml/2.2" xmlns:atom="http://www.w3.org/2005/Atom">\n  <Document>\n    <name>Merged Mission</name>\n${mergedKMLContent}  </Document>\n</kml>`;
        
        document.getElementById('preview').style.display = "block"; 
        document.getElementById('btnUpload').disabled = false; 
        setStatus(`${allPlacemarksCount} Waypoints dari ${files.length} fail sedia dimuat naik.`, "success"); 
      } else {
        setStatus("Error: Tiada koordinat yang sah dikesan.", "error");
      }
    });
    
    async function uploadKML() { 
      if (!rawMergedKML) return; 
      const btn = document.getElementById('btnUpload'); 
      btn.disabled = true; 
      btn.innerText = "Semak Storan..."; 
      
      try {
        // 1. Semak kapasiti storan ESP32 terlebih dahulu (Amalan Keselamatan Data)
        let res = await fetch('/storage');
        let storageData = await res.json();
        let freeSpace = storageData.total - storageData.used;
        
        // Kira saiz sebenar payload (dalam bait)
        let payloadSize = new Blob([rawMergedKML]).size;
        
        // Semakan: Adakah saiz muat naik terlalu besar? 
        // Nota: Tolak 10240 bytes (10KB) sebagai zon selamat (margin) untuk LittleFS.
        if (payloadSize > (freeSpace - 10240)) {
          setStatus(`Ralat: Saiz fail (${(payloadSize/1024).toFixed(1)}KB) melebihi ruang kosong ESP32. Sila Delete fail lama di tab Manage Data.`, "error");
          btn.innerText = "Upload to Rover"; 
          btn.disabled = false;
          return; // Hentikan proses awal-awal, jangan bebankan ESP32.
        }

        // 2. Jika selamat, teruskan proses muat naik
        btn.innerText = "Sending..."; 
        const formData = new FormData(); 
        const blob = new Blob([rawMergedKML], { type: 'application/vnd.google-earth.kml+xml' });
        formData.append("data", blob, "mission.kml"); 
        
        let uploadRes = await fetch('/upload', { method: 'POST', body: formData });
        if (uploadRes.ok) { 
          setStatus(`KML Berjaya Disimpan!`, "success"); 
        } else { 
          setStatus("Gagal menghantar.", "error"); 
        }
      } catch (err) {
        setStatus("Ralat Network.", "error"); 
      } finally {
        if(btn.innerText !== "Semak Storan...") { 
           btn.innerText = "Upload to Rover"; 
           btn.disabled = false; 
        }
      }
    }
    
    // --- MANUAL ADD LOGIC ---
    function addManualPoint() { const name = document.getElementById('pName').value.trim(); const lat = document.getElementById('pLat').value; const lon = document.getElementById('pLon').value; const alt = document.getElementById('pAlt').value; if(!name || !lat || !lon) { setStatus("Sila isi Nama, Lat, dan Lon.", "error"); return; } const params = new URLSearchParams(); params.append('name', name); params.append('lat', lat); params.append('lon', lon); params.append('alt', alt); fetch('/addpoint', { method: 'POST', body: params }).then(res => res.text()).then(text => { setStatus(`'${name}' berjaya ditambah!`, "success"); document.getElementById('pName').value = ""; document.getElementById('pLat').value = ""; document.getElementById('pLon').value = ""; }).catch(err => setStatus("Gagal menambah point.", "error")); }
    
    // --- DOWNLOAD & DELETE LOGIC ---
    function loadFiles() {
      fetch('/storage').then(r => r.json()).then(data => {
        if(data.total > 0) {
          let pct = Math.round((data.used / data.total) * 100);
          let usedKB = (data.used / 1024).toFixed(1);
          let totalKB = (data.total / 1024).toFixed(1);
          document.getElementById('storageBar').style.width = pct + '%';
          document.getElementById('storageText').innerText = pct + '% (' + usedKB + ' / ' + totalKB + ' KB)';
          if(pct > 85) document.getElementById('storageBar').style.background = '#ef4444';
          else document.getElementById('storageBar').style.background = '#3b82f6';
        }
      }).catch(e => console.log(e));

      fetch('/listfiles').then(response => response.json()).then(files => {
        const container = document.getElementById('fileListContainer'); container.innerHTML = "";
        if (files.length === 0) { container.innerHTML = "<div style='padding:20px; text-align:center; color:#64748b;'>Tiada fail .kml ditemui di dalam storan.</div>"; return; }
        files.forEach(file => {
          const div = document.createElement('div'); div.className = 'file-item';
          div.innerHTML = `
            <div>
              <span class="file-name">${file.name}</span><br>
              <span class="file-size">${file.size}</span>
            </div>
            <div class="action-btns">
              <button class="btn-dl" onclick="processDownload('${file.name}', this)">Download</button>
              <button class="btn-del" onclick="deleteFile('${file.name}', this)">Delete</button>
            </div>
          `;
          container.appendChild(div);
        });
      }).catch(err => { document.getElementById('fileListContainer').innerHTML = "<div style='padding:20px; text-align:center; color:#ef4444;'>Error loading files.</div>"; });
    }

    function processDownload(filename, btnElement) {
      const originalBtnText = btnElement.innerText;
      const pContainer = document.getElementById('progressContainer');
      const pBar = document.getElementById('progressBar');
      const pStatus = document.getElementById('progressStatus');
      btnElement.disabled = true; pContainer.style.display = 'block'; pBar.style.width = '0%'; pStatus.innerText = "Connecting...";

      const xhr = new XMLHttpRequest();
      xhr.open('GET', '/download?file=' + filename, true); 
      xhr.responseType = 'blob'; 
      
      xhr.onprogress = function(e) {
        if (e.lengthComputable) {
          const percent = Math.round((e.loaded / e.total) * 100);
          pBar.style.width = percent + '%'; pStatus.innerText = `Downloading... ${percent}%`; btnElement.innerText = `${percent}%`;
        }
      };
      
      xhr.onload = function() {
        if (xhr.status === 200) {
          pBar.style.width = '100%'; pStatus.innerText = "Saving..."; btnElement.innerText = "Saving...";
          const blob = xhr.response;
          const link = document.createElement('a');
          link.href = URL.createObjectURL(blob);
          link.download = filename; 
          document.body.appendChild(link);
          link.click();
          document.body.removeChild(link);
          setTimeout(() => URL.revokeObjectURL(link.href), 100);
          pStatus.innerText = "Done. Check downloads."; btnElement.innerText = "Done";
          setTimeout(() => { pContainer.style.display = 'none'; btnElement.disabled = false; btnElement.innerText = originalBtnText; }, 1500);
        } else { alert("Gagal download: " + xhr.status); resetUI(btnElement, originalBtnText, pContainer); }
      };
      xhr.onerror = function() { alert("Ralat Network."); resetUI(btnElement, originalBtnText, pContainer); };
      xhr.send();
    }

    function deleteFile(filename, btnElement) {
      if(confirm(`Adakah anda pasti mahu memadam fail '${filename}' secara kekal?\n\nTindakan ini tidak boleh dikembalikan.`)) {
        btnElement.disabled = true; const originalText = btnElement.innerText; btnElement.innerText = "...";
        const params = new URLSearchParams(); params.append('file', filename);
        fetch('/delete', { method: 'POST', body: params }).then(res => {
          if(res.ok) { setStatus(`Fail '${filename}' berjaya dipadam.`, "success"); loadFiles(); } 
          else { setStatus("Gagal memadam fail.", "error"); btnElement.disabled = false; btnElement.innerText = originalText; }
        }).catch(err => { setStatus("Ralat Rangkaian.", "error"); btnElement.disabled = false; btnElement.innerText = originalText; });
      }
    }

    function resetUI(btn, text, container) { btn.disabled = false; btn.innerText = text; container.style.display = 'none'; }
    function finishSync() { fetch('/close', { method: 'POST' }).then(res => { document.getElementById('main-ui').style.display = 'none'; document.getElementById('disconnect-overlay').style.display = 'flex'; }); }
  </script>
</body>
</html>
)rawliteral";


// ==========================================
// CLASS: ROVER MISSION MANAGER
// ==========================================

struct KMLPoint {
  char name[NAME_LEN];
  float lon;
  float lat;
  float alt;
};

class RoverMission {
  public:
    enum class WifiStatus {
      OFF,
      READY,      
      CONNECTED,  
      UPLOADED    
    };
    
    struct DataAccessor;

  private:
    KMLPoint points[MAX_PLACEMARK];
    int totalPoints;
    AsyncWebServer server;
    bool wifiActive;
    
    WifiStatus currentStatus;
    bool pendingShutdown; 
    bool serverStarted; 

    String formatBytes(size_t bytes) {
      if (bytes < 1024) return String(bytes) + " B";
      else if (bytes < (1024 * 1024)) return String(bytes / 1024.0) + " KB";
      else return String(bytes / 1024.0 / 1024.0) + " MB";
    }

    // --- FUNGSI LOAD KML (C++ Parser Ringkas yang dinaiktaraf) ---
    void loadFromStorage() {
      if (!LittleFS.exists("/mission.kml")) {
         Serial.println("[STORAGE] Fail /mission.kml TIDAK WUJUD.");
         totalPoints = 0;
         return;
      }

      File file = LittleFS.open("/mission.kml", "r");
      if (!file) {
        Serial.println("[STORAGE] Gagal membuka fail untuk dibaca.");
        return;
      }

      Serial.print("[STORAGE] Saiz Fail: ");
      Serial.print(file.size());
      Serial.println(" bytes.");

      totalPoints = 0;
      memset(points, 0, sizeof(points)); 
      
      bool inPlacemark = false;
      bool inCoords = false;
      String currentName = "";
      String coordsBuffer = "";

      while (file.available() && totalPoints < MAX_PLACEMARK) {
        String line = file.readStringUntil('\n');

        if (line.indexOf("<Placemark") != -1) {
            inPlacemark = true;
            currentName = "";
        }
        if (line.indexOf("</Placemark>") != -1) {
            inPlacemark = false;
        }

        if (inPlacemark && line.indexOf("<name>") != -1) {
            int start = line.indexOf("<name>") + 6;
            int end = line.indexOf("</name>");
            if (end != -1) {
                currentName = line.substring(start, end);
                currentName.trim();
            }
        }

        if (inPlacemark && line.indexOf("<coordinates>") != -1) {
            inCoords = true;
            coordsBuffer = "";
            int start = line.indexOf("<coordinates>") + 13;
            int end = line.indexOf("</coordinates>");
            if (end != -1) {
                coordsBuffer = line.substring(start, end);
                inCoords = false; 
            } else {
                coordsBuffer = line.substring(start);
            }
        } else if (inCoords) {
            int end = line.indexOf("</coordinates>");
            if (end != -1) {
                coordsBuffer += " " + line.substring(0, end);
                inCoords = false;
            } else {
                coordsBuffer += " " + line;
            }
        }

        if (!inCoords && coordsBuffer.length() > 0) {
            coordsBuffer.trim();
            
            int firstSpace = coordsBuffer.indexOf(' ');
            String firstCoord = (firstSpace != -1) ? coordsBuffer.substring(0, firstSpace) : coordsBuffer;

            int comma1 = firstCoord.indexOf(',');
            int comma2 = firstCoord.indexOf(',', comma1 + 1);

            if (comma1 != -1) {
                 points[totalPoints].lon = firstCoord.substring(0, comma1).toFloat();
                 if (comma2 != -1) {
                    points[totalPoints].lat = firstCoord.substring(comma1 + 1, comma2).toFloat();
                    points[totalPoints].alt = firstCoord.substring(comma2 + 1).toFloat();
                 } else {
                    points[totalPoints].lat = firstCoord.substring(comma1 + 1).toFloat();
                    points[totalPoints].alt = 0.0;
                 }

                 if (currentName == "") currentName = "Point" + String(totalPoints);
                 strncpy(points[totalPoints].name, currentName.c_str(), NAME_LEN - 1);
                 points[totalPoints].name[NAME_LEN - 1] = '\0';

                 totalPoints++;
            }
            coordsBuffer = ""; 
        }
      }
      file.close();
      Serial.printf("[STORAGE] Selesai. Total RAM: %d waypoints.\n", totalPoints);
    }

    void saveToStorage() {
      File file = LittleFS.open("/mission.kml", "w");
      if (!file) {
        Serial.println("[STORAGE] EROR: Gagal buka fail untuk menulis!");
        return;
      }

      file.print("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
      file.print("<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n");
      file.print("  <Document>\n");

      for (int i = 0; i < totalPoints; i++) {
        file.print("    <Placemark>\n");
        file.printf("      <name>%s</name>\n", points[i].name);
        file.print("      <Point>\n");
        file.printf("        <coordinates>%.6f,%.6f,%.2f</coordinates>\n", points[i].lon, points[i].lat, points[i].alt);
        file.print("      </Point>\n");
        file.print("    </Placemark>\n");
      }
      
      file.print("  </Document>\n");
      file.print("</kml>\n");
      
      file.flush(); 
      file.close();
      Serial.printf("[STORAGE] Disimpan KML. Saiz baru: %d points.\n", totalPoints);
    }

    void setupServerRoutes() {
      server.on("/", HTTP_GET, [this](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
      });

      server.on("/upload", HTTP_POST, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "OK");
        this->loadFromStorage(); 
        this->currentStatus = WifiStatus::UPLOADED;
        Serial.println("[WIFI] KML Uploaded & Digabungkan.");
      }, [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        if(!index) request->_tempFile = LittleFS.open("/mission.kml", "w"); 
        if(request->_tempFile) request->_tempFile.write(data, len);
        if(final) if(request->_tempFile) request->_tempFile.close();
      });

      server.on("/addpoint", HTTP_POST, [this](AsyncWebServerRequest *request){
        String name = "Point"; 
        float lat = 0, lon = 0, alt = 0;
        if(request->hasParam("name", true)) name = request->getParam("name", true)->value();
        if(request->hasParam("lat", true)) lat = request->getParam("lat", true)->value().toFloat();
        if(request->hasParam("lon", true)) lon = request->getParam("lon", true)->value().toFloat();
        if(request->hasParam("alt", true)) alt = request->getParam("alt", true)->value().toFloat();
        
        bool success = this->addPlace(name, lat, lon, alt);
        if(success) request->send(200, "text/plain", "Added");
        else request->send(500, "text/plain", "Failed");
      });

      server.on("/listfiles", HTTP_GET, [this](AsyncWebServerRequest *request){
        String json = "[";
        File root = LittleFS.open("/");
        if (root && root.isDirectory()) {
          File file = root.openNextFile();
          bool first = true;
          while (file) {
            String fileName = String(file.name());
            String nameLower = fileName;
            nameLower.toLowerCase();
            if (nameLower.endsWith(".kml")) {
              if (!first) json += ",";
              json += "{\"name\":\"" + fileName + "\",\"size\":\"" + formatBytes(file.size()) + "\"}";
              first = false;
            }
            file = root.openNextFile();
          }
        }
        json += "]";
        request->send(200, "application/json", json);
      });

      server.on("/storage", HTTP_GET, [this](AsyncWebServerRequest *request){
        size_t total = this->getTotalSpace();
        size_t used = this->getUsedSpace();
        String json = "{\"total\":" + String(total) + ",\"used\":" + String(used) + "}";
        request->send(200, "application/json", json);
      });

      server.on("/download", HTTP_GET, [this](AsyncWebServerRequest *request){
        if (request->hasParam("file")) {
          String fileName = request->getParam("file")->value();
          if (LittleFS.exists("/" + fileName)) {
            request->send(LittleFS, "/" + fileName, "application/octet-stream");
          } else {
            request->send(404, "text/plain", "File Not Found");
          }
        } else {
          request->send(400, "text/plain", "Bad Request");
        }
      });

      server.on("/delete", HTTP_POST, [this](AsyncWebServerRequest *request){
        if (request->hasParam("file", true)) {
          String fileName = request->getParam("file", true)->value();
          if (LittleFS.exists("/" + fileName)) {
            LittleFS.remove("/" + fileName);
            Serial.println("[WIFI] Deleted file: " + fileName);
            if (fileName == "mission.kml") {
              this->totalPoints = 0;
              memset(this->points, 0, sizeof(this->points));
            }
            request->send(200, "text/plain", "Deleted");
          } else {
            request->send(404, "text/plain", "File Not Found");
          }
        } else {
          request->send(400, "text/plain", "Bad Request");
        }
      });

      server.on("/close", HTTP_POST, [this](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Closing");
        this->pendingShutdown = true;
      });
      
      Serial.println("[SYSTEM] Laluan Web Server didaftarkan.");
    }

  public:
    struct DataAccessor {
      RoverMission* p;
      DataAccessor(RoverMission* parent) : p(parent) {}
      String name(int i) { return (i >= 0 && i < p->totalPoints) ? String(p->points[i].name) : "N/A"; }
      float lat(int i) { return (i >= 0 && i < p->totalPoints) ? p->points[i].lat : 0.0; }
      float lon(int i) { return (i >= 0 && i < p->totalPoints) ? p->points[i].lon : 0.0; }
      float alt(int i) { return (i >= 0 && i < p->totalPoints) ? p->points[i].alt : 0.0; }
    };

    DataAccessor get;

    RoverMission() : get(this), server(80), totalPoints(0), wifiActive(false), currentStatus(WifiStatus::OFF), pendingShutdown(false), serverStarted(false) {}

    void begin() {
      if (!LittleFS.begin(false)) { 
          if(!LittleFS.begin(true)) { 
            Serial.println("[ERROR] LittleFS Mount Failed Totally");
            return;
          }
      }
      loadFromStorage();
      setupServerRoutes(); 
    }

    // --- FUNGSI MENGHIDUPKAN RANGKAIAN SAHAJA ---
    void startNetwork() {
      if (wifiActive) {
        Serial.println("[WIFI] Rangkaian telah sedia aktif. Abaikan arahan menghidupkan.");
        return; 
      }

      WiFi.mode(WIFI_AP);
      WiFi.softAP(WIFI_SSID, WIFI_PASS);
      
      delay(200); 
      
      if (!serverStarted) {
        server.begin();
        serverStarted = true;
        Serial.println("[SYSTEM] Web Server dihidupkan untuk kali pertama.");
      }
      
      wifiActive = true;
      currentStatus = WifiStatus::READY; 
      pendingShutdown = false;
      Serial.println("[WIFI] AP Dihidupkan (Upload & Download Mode).");
    }

    // --- FUNGSI MEMATIKAN RANGKAIAN SAHAJA ---
    void stopNetwork() {
      if (!wifiActive) {
        Serial.println("[WIFI] Rangkaian telah sedia dimatikan. Abaikan arahan mematikan.");
        return; 
      }
      
      WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_OFF);
      
      wifiActive = false;
      currentStatus = WifiStatus::OFF; 
      Serial.println("[WIFI] Rangkaian Dimatikan (Server kekal standby).");
    }

    void update() {
      if (wifiActive) {
        if (pendingShutdown) {
          delay(500); 
          stopNetwork();
          return;
        }
        if (WiFi.softAPgetStationNum() > 0) {
           if (currentStatus != WifiStatus::UPLOADED) currentStatus = WifiStatus::CONNECTED;
        } else {
           if (currentStatus != WifiStatus::UPLOADED) currentStatus = WifiStatus::READY;
        }
      }
    }

    bool deletePlace(int index) {
      if (index < 0 || index >= totalPoints) return false;
      for (int i = index; i < totalPoints - 1; i++) {
        points[i] = points[i+1]; 
      }
      memset(&points[totalPoints - 1], 0, sizeof(KMLPoint));
      totalPoints--;
      saveToStorage(); 
      return true;
    }

    bool addPlace(String name, float lat, float lon, float alt) {
      if (totalPoints >= MAX_PLACEMARK) return false;
      char cleanName[NAME_LEN];
      strncpy(cleanName, name.c_str(), NAME_LEN - 1);
      cleanName[NAME_LEN - 1] = '\0';
      for (int i = 0; i < totalPoints; i++) {
        if (strcmp(points[i].name, cleanName) == 0) return false;
      }
      strncpy(points[totalPoints].name, cleanName, NAME_LEN);
      points[totalPoints].lat = lat;
      points[totalPoints].lon = lon;
      points[totalPoints].alt = alt;
      totalPoints++;
      saveToStorage(); 
      Serial.printf("[ADD] Added: %s\n", cleanName);
      return true;
    }

    bool deleteFile() {
      if (LittleFS.exists("/mission.kml")) {
        LittleFS.remove("/mission.kml");
        totalPoints = 0;
        memset(&points, 0, sizeof(points));
        Serial.println("[STORAGE] Fail mission.kml telah berjaya dipadam.");
        return true;
      }
      Serial.println("[STORAGE] Tiada fail mission.kml untuk dipadam.");
      return false;
    }

    WifiStatus getStatus() { return currentStatus; }
    int getCount() { return totalPoints; }
    void printAllData() {
      Serial.println("\n--- DATA ---");
      for(int i=0; i<totalPoints; i++) {
        Serial.printf("[%d] %s | %.6f, %.6f, %.2f\n", 
          i, points[i].name, points[i].lon, points[i].lat, points[i].alt);
      }
    }
    bool isWifiOn() { return wifiActive; }

    size_t getFileSize() {
      File file = LittleFS.open("/mission.kml", "r");
      if (!file) return 0;
      size_t s = file.size();
      file.close();
      return s;
    }

    size_t getTotalSpace() { return LittleFS.totalBytes(); }
    size_t getUsedSpace() { return LittleFS.usedBytes(); }
    size_t getFreeSpace() { return LittleFS.totalBytes() - LittleFS.usedBytes(); }
};

#endif