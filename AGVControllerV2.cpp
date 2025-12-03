#include "AGVControllerV2.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Static instance definition
AGVController* AGVController::_instance = nullptr;
AGVController AGV; // Global instance

// HTML Pages (Full content in PROGMEM)
const char AGVController::_loginPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>AGV Controller Login</title>
<style>body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;height:100vh;margin:0}
.login-container{background:white;padding:40px;border-radius:10px;box-shadow:0 10px 25px rgba(0,0,0,0.2);width:100%;max-width:400px}
h1{text-align:center;color:#333;margin-bottom:30px}.form-group{margin-bottom:20px}
label{display:block;margin-bottom:5px;color:#555;font-weight:bold}
input{width:100%;padding:12px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}
button{width:100%;padding:12px;background:#667eea;color:white;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;transition:background 0.3s}
button:hover{background:#5568d3}.error{color:#e74c3c;text-align:center;margin-top:10px;display:none}
.robot-icon{text-align:center;font-size:48px;margin-bottom:20px}</style>
</head><body><div class="login-container"><div class="robot-icon">🚗</div><h1>AGV Controller</h1>
<form id="loginForm"><div class="form-group"><label for="username">Username</label><input type="text" id="username" required></div>
<div class="form-group"><label for="password">Password</label><input type="password" id="password" required></div>
<button type="submit">Login</button><div class="error" id="error">Invalid credentials!</div></form></div>
<script>document.getElementById('loginForm').addEventListener('submit',async function(e){e.preventDefault();const u=document.getElementById('username').value;const p=document.getElementById('password').value;const r=await fetch('/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:u,password:p})});const j=await r.json();if(j.success){localStorage.setItem('token',j.token);window.location.href='/dashboard'}else{document.getElementById('error').style.display='block'}})</script>
</body></html>
)rawliteral";

const char AGVController::_wifiSetupPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>WiFi Setup</title>
<style>body{font-family:Arial,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);display:flex;justify-content:center;align-items:center;min-height:100vh;margin:0;padding:20px}
.setup-container{background:white;padding:40px;border-radius:10px;box-shadow:0 10px 25px rgba(0,0,0,0.2);width:100%;max-width:500px}
h1{text-align:center;color:#333;margin-bottom:30px}.form-group{margin-bottom:20px}
label{display:block;margin-bottom:5px;color:#555;font-weight:bold}
input,select{width:100%;padding:12px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}
button{width:100%;padding:12px;background:#2ecc71;color:white;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:10px;transition:background 0.3s}
button:hover{background:#27ae60}.scan-btn{background:#3498db}.scan-btn:hover{background:#2980b9}
.message{text-align:center;padding:10px;margin-top:10px;border-radius:5px;display:none}
.success{background:#d4edda;color:#155724}.error{background:#f8d7da;color:#721c24}
.loading{text-align:center;margin:10px 0;display:none}</style>
</head><body><div class="setup-container"><h1>📡 WiFi Setup</h1>
<form id="wifiForm"><div class="form-group"><label for="ssid">WiFi Network</label><select id="ssid" required><option value="">-- Select or type below --</option></select></div>
<div class="form-group"><label for="ssid_manual">Or Enter Manually</label><input type="text" id="ssid_manual" placeholder="Enter WiFi name"></div>
<button type="button" class="scan-btn" onclick="scanNetworks()">🔍 Scan Networks</button><div class="loading" id="loading">Scanning...</div>
<div class="form-group"><label for="password">WiFi Password</label><input type="password" id="password" required></div>
<button type="submit">💾 Save & Connect</button><div class="message" id="message"></div></form></div>
<script>async function scanNetworks(){document.getElementById('loading').style.display='block';const r=await fetch('/scan');const n=await r.json();document.getElementById('loading').style.display='none';const s=document.getElementById('ssid');s.innerHTML='<option value="">-- Select WiFi Network --</option>';n.forEach(t=>{const o=document.createElement('option');o.value=t.ssid;o.textContent=`${t.ssid} (${t.rssi} dBm) ${t.secured?'🔒':''}`;s.appendChild(o)})}
document.getElementById('wifiForm').addEventListener('submit',async function(e){e.preventDefault();const s=document.getElementById('ssid_manual').value||document.getElementById('ssid').value;const p=document.getElementById('password').value;const r=await fetch('/savewifi',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid:s,password:p})});const j=await r.json();const m=document.getElementById('message');m.style.display='block';if(j.success){m.className='message success';m.textContent='✅ WiFi saved! Restarting...';setTimeout(()=>location.reload(),3000)}else{m.className='message error';m.textContent='❌ Failed to save WiFi settings'}})
window.onload=scanNetworks</script>
</body></html>
)rawliteral";

const char AGVController::_mainPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>AGV Navigation Controller</title>
<style>body{font-family:Arial,sans-serif;max-width:800px;margin:0 auto;padding:20px;background-color:#f5f5f5}
.container{background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}
h1{color:#2c3e50;text-align:center;margin-bottom:30px}
.header{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}
.logout-btn{background:#e74c3c;color:white;padding:8px 15px;border:none;border-radius:5px;cursor:pointer;font-weight:bold}
.logout-btn:hover{background:#c0392b}.control-section{margin-bottom:25px;padding:15px;border:1px solid #ddd;border-radius:5px}
.section-title{font-weight:bold;margin-bottom:10px;color:#34495e}.input-group{margin-bottom:10px}
label{display:inline-block;width:120px;font-weight:bold}input[type="number"]{width:80px;padding:5px;margin:0 5px}
.button-group{margin:15px 0}button{padding:10px 15px;margin:5px;border:none;border-radius:5px;cursor:pointer;font-weight:bold;transition:background-color 0.3s}
.primary-btn{background-color:#3498db;color:white}.primary-btn:hover{background-color:#2980b9}
.success-btn{background-color:#2ecc71;color:white}.success-btn:hover{background-color:#27ae60}
.warning-btn{background-color:#f39c12;color:white}.warning-btn:hover{background-color:#d35400}
.danger-btn{background-color:#e74c3c;color:white}.danger-btn:hover{background-color:#c0392b}
.status-section{background-color:#ecf0f1;padding:15px;border-radius:5px;margin-top:20px}
.log-entry{font-family:monospace;font-size:12px;margin:2px 0;padding:2px 5px;background-color:#2c3e50;color:white;border-radius:3px}
.connection-status{display:inline-block;width:10px;height:10px;border-radius:50%;margin-right:5px}
.connected{background-color:#2ecc71}.disconnected{background-color:#e74c3c}
.radio-group{margin:10px 0}.radio-group label{width:auto;margin-right:15px;font-weight:normal}
.agv-status{background-color:#34495e;color:white;padding:10px;border-radius:5px;margin:10px 0;font-weight:bold}
.loop-count{margin-left:20px;display:none}.loop-count input{width:60px}</style>
</head><body>
<div class="container"><div class="header"><h1>🚗 AGV Navigation Controller</h1><button class="logout-btn" onclick="logout()">Logout</button></div>
<div class="control-section"><div class="section-title">Connection Status</div><div id="connectionStatus"><span class="connection-status disconnected" id="statusIndicator"></span><span id="statusText">Disconnected from ESP32</span></div></div>
<div class="control-section"><div class="section-title">Path Configuration</div>
<div class="input-group"><label for="sourceX">Source X:</label><input type="number" id="sourceX" value="1" min="1" max="10"><label for="sourceY">Source Y:</label><input type="number" id="sourceY" value="1" min="1" max="10"></div>
<div class="input-group"><label for="destX">Destination X:</label><input type="number" id="destX" value="3" min="1" max="10"><label for="destY">Destination Y:</label><input type="number" id="destY" value="2" min="1" max="10"></div>
<div class="radio-group"><label>Execution Mode:</label><input type="radio" id="once" name="mode" value="once" checked onchange="toggleLoopCount()"><label for="once">Run Once</label><input type="radio" id="loop" name="mode" value="loop" onchange="toggleLoopCount()"><label for="loop">Loop Continuously</label><span class="loop-count" id="loopCountContainer"><label for="loopCount">Times:</label><input type="number" id="loopCount" value="2" min="2" max="1000"></span></div>
<div class="button-group"><button class="success-btn" onclick="sendPath()">Update Path</button><button class="primary-btn" onclick="sendDefault()">Load Default Path</button></div></div>
<div class="control-section"><div class="section-title">AGV Control</div><div class="button-group"><button class="success-btn" onclick="sendStart()">▶️ START</button><button class="warning-btn" onclick="sendStop()">⏹️ STOP</button><button class="danger-btn" onclick="sendAbort()">🛑 ABORT</button></div></div>
<div class="control-section"><div class="section-title">AGV Status</div><div id="agvStatusDisplay" class="agv-status">AGV: Waiting for ESP32 connection...</div></div>
<div class="status-section"><div class="section-title">System Logs</div><div id="logDisplay" style="max-height:200px;overflow-y:auto;margin-top:10px;"></div></div></div>
<script>let ws, isConnected=false;function checkAuth(){if(!localStorage.getItem('token'))window.location.href='/'}function logout(){localStorage.removeItem('token');window.location.href='/'}
function connect(){ws=new WebSocket('ws://'+window.location.hostname+':81');ws.onopen=function(){isConnected=true;updateConnectionStatus(true,'Connected to ESP32');addLog('✅ Connected to ESP32');updateAGVStatus('Connected - Ready for commands')}
ws.onclose=function(){isConnected=false;updateConnectionStatus(false,'Disconnected from ESP32');addLog('🔌 Disconnected. Reconnecting...');updateAGVStatus('Disconnected');setTimeout(connect,2000)}
ws.onerror=function(){addLog('❌ Connection error')};ws.onmessage=function(e){addLog('📥 ESP32: '+e.data);updateAGVStatus(e.data)}}
function updateConnectionStatus(c,m){const i=document.getElementById('statusIndicator'),t=document.getElementById('statusText');i.className=c?'connection-status connected':'connection-status disconnected';t.textContent=m}
function updateAGVStatus(m){const s=document.getElementById('agvStatusDisplay');s.textContent='AGV: '+m;if(m.includes('START')||m.includes('Moving'))s.style.backgroundColor='#27ae60';else if(m.includes('STOP')||m.includes('Stopped'))s.style.backgroundColor='#f39c12';else if(m.includes('ABORT')||m.includes('Emergency'))s.style.backgroundColor='#e74c3c';else if(m.includes('Ready')||m.includes('Connected'))s.style.backgroundColor='#3498db';else s.style.backgroundColor='#34495e'}
function addLog(m){const l=document.getElementById('logDisplay'),e=document.createElement('div');e.className='log-entry';e.textContent='['+new Date().toLocaleTimeString()+'] '+m;l.appendChild(e);l.scrollTop=l.scrollHeight}
function sendCommand(c){if(!isConnected||!ws||ws.readyState!==WebSocket.OPEN){addLog('❌ Not connected to ESP32');return}ws.send(c);addLog('📤 Sent: '+c)}
function generatePath(sx,sy,dx,dy){let p='',cx=sx,cy=sy;while(cx!==dx){const d=(cx<dx)?'E':'W';p+=`(${cx},${cy})${d}`;cx+=(cx<dx)?1:-1}while(cy!==dy){const d=(cy<dy)?'N':'S';p+=`(${cx},${cy})${d}`;cy+=(cy<dy)?1:-1}p+=`(${cx},${cy})E`;return p}
function toggleLoopCount(){const l=document.getElementById('loop'),c=document.getElementById('loopCountContainer');c.style.display=l.checked?'inline-block':'none'}
function sendPath(){const sx=parseInt(document.getElementById('sourceX').value),sy=parseInt(document.getElementById('sourceY').value),dx=parseInt(document.getElementById('destX').value),dy=parseInt(document.getElementById('destY').value),m=document.querySelector('input[name="mode"]:checked').value,lc=document.getElementById('loopCount').value;const p=generatePath(sx,sy,dx,dy);sendCommand(m==='loop'?`PATH:${p}:LOOP:${lc}`:`PATH:${p}:ONCE`)}
function sendDefault(){sendCommand('DEFAULT')}function sendStart(){sendCommand('START')}function sendStop(){sendCommand('STOP')}function sendAbort(){sendCommand('ABORT')}
window.onload=function(){checkAuth();addLog('🔧 AGV Control Interface Initialized');addLog('📍 Connecting to ESP32...');updateAGVStatus('Web interface ready - Connecting...');toggleLoopCount();connect()}</script>
</body></html>
)rawliteral";

// ===== CONSTRUCTOR =====
// Also update the constructor to initialize the new queue:
AGVController::AGVController() :
    _server(nullptr),
    _webSocket(nullptr),
    _dnsServer(nullptr),
    _webQueue(nullptr),
    _outgoingWebQueue(nullptr),  // Initialize new queue pointer
    _printMutex(nullptr),
    _taskWebHandle(nullptr),
    _taskSerialHandle(nullptr),
    _isAPMode(false),
    _storedSSID(""),
    _storedPassword(""),
    _sessionToken("") {
    _instance = this;
}

// ===== PUBLIC METHODS =====
// Update begin() method to create the new queue:
void AGVController::begin() {
    Serial.begin(115200);
    delay(1000);
    _safePrintln("\n\nESP32-S3 AGV Controller Library v2.0\n");
    
    // Initialize RTOS objects
    _webQueue = xQueueCreate(20, sizeof(char[256]));
    _outgoingWebQueue = xQueueCreate(20, sizeof(char[256]));  // Create new queue
    _printMutex = xSemaphoreCreateMutex();
    
    if (_webQueue == NULL || _outgoingWebQueue == NULL) {
        Serial.println("ERROR: Failed to create queues!");
        return;
    }
    
    // Load WiFi credentials
    _loadCredentials();
    
    // Create tasks - BOTH ON CORE 0 as requested
    xTaskCreatePinnedToCore(
        _serialTask,
        "AGV_SerialMgr",
        4096,
        nullptr,
        3,  // High priority
        &_taskSerialHandle,
        0   // Core 0 (both tasks on same core)
    );
    
    xTaskCreatePinnedToCore(
        _webTask,
        "AGV_WebMgr",
        8192,
        nullptr,
        2,  // Medium priority
        &_taskWebHandle,
        0   // Core 0
    );
}

// ===== TASK FUNCTIONS =====
void AGVController::_serialTask(void* pvParameters) {
    AGVController* ctrl = AGVController::_instance;
    char buffer[256];
    
    for(;;) {
        if (Serial.available() > 0) {
            size_t len = Serial.readBytesUntil('\n', buffer, 255);
            if(len > 0) {
                buffer[len] = '\0';
                
                // Remove trailing carriage return if present
                if (buffer[len-1] == '\r') {
                    buffer[len-1] = '\0';
                    len--;
                }
                
                char sendBuffer[256];
                strncpy(sendBuffer, buffer, sizeof(sendBuffer)-1);
                sendBuffer[sizeof(sendBuffer)-1] = '\0';
                
                if (xQueueSend(ctrl->_serialQueue, &sendBuffer, 0) != pdPASS) {
                    if(xSemaphoreTake(ctrl->_printMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                        Serial.println("[ERROR] Serial queue full!");
                        xSemaphoreGive(ctrl->_printMutex);
                    }
                }
                
                if(xSemaphoreTake(ctrl->_printMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    Serial.printf("\n[SERIAL->WEB] %s\n", buffer);
                    xSemaphoreGive(ctrl->_printMutex);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AGVController::_webTask(void* pvParameters) {
    AGVController* ctrl = AGVController::_instance;
    TickType_t lastWake = xTaskGetTickCount();
    
    // Wait for begin() to complete setup
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Initialize servers
    ctrl->_server = new WebServer(80);
    ctrl->_webSocket = new WebSocketsServer(81);
    ctrl->_dnsServer = new DNSServer();
    
    // Setup routes - make sure all handlers are static
    ctrl->_server->on("/", HTTP_GET, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_server->send_P(200, "text/html", AGVController::_instance->_loginPage);
        }
    });
    
    ctrl->_server->on("/login", HTTP_POST, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_handleLogin();
        }
    });
    
    ctrl->_server->on("/dashboard", HTTP_GET, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_server->send_P(200, "text/html", AGVController::_instance->_mainPage);
        }
    });
    
    ctrl->_server->on("/setup", HTTP_GET, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_server->send_P(200, "text/html", AGVController::_instance->_wifiSetupPage);
        }
    });
    
    ctrl->_server->on("/scan", HTTP_GET, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_handleScan();
        }
    });
    
    ctrl->_server->on("/savewifi", HTTP_POST, []() {
        if (AGVController::_instance) {
            AGVController::_instance->_handleSaveWiFi();
        }
    });
    
    ctrl->_server->onNotFound([]() {
        if (AGVController::_instance) {
            AGVController::_instance->_handleCaptivePortal();
        }
    });
    
    ctrl->_webSocket->onEvent([](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        if (AGVController::_instance) {
            AGVController::_instance->_webSocketEvent(num, type, payload, length);
        }
    });
    
    // Start WiFi mode
    if(ctrl->_storedSSID.length() > 0) {
        ctrl->_startStationMode();
    } else {
        ctrl->_startAPMode();
    }
    
    // Main loop
// Main loop
    for(;;) {
        if(ctrl->_isAPMode) {
            ctrl->_dnsServer->processNextRequest();
        }
        
        ctrl->_server->handleClient();
        
        if(!ctrl->_isAPMode && ctrl->_webSocket != nullptr) {
            ctrl->_webSocket->loop();
            
            // Process serial queue (incoming from USB)
            char buffer[256];
            while(xQueueReceive(ctrl->_webQueue, &buffer, 0) == pdTRUE) {
                ctrl->_webSocket->broadcastTXT(buffer);
            }
            
            // Process outgoing web queue (from sendToWeb())
            while(xQueueReceive(ctrl->_outgoingWebQueue, &buffer, 0) == pdTRUE) {
                ctrl->_webSocket->broadcastTXT(buffer);
                ctrl->_safePrintln("[WEB->BROWSER] " + String(buffer));
            }
        }
        
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(1));
    }

}

// ===== WEB HANDLERS =====
void AGVController::_handleLogin() {
    if(_server->method() == HTTP_POST) {
        String body = _server->arg("plain");
        int uStart = body.indexOf("\"username\":\"") + 12;
        int uEnd = body.indexOf("\"", uStart);
        int pStart = body.indexOf("\"password\":\"") + 12;
        int pEnd = body.indexOf("\"", pStart);
        
        if (uStart == -1 || uEnd == -1 || pStart == -1 || pEnd == -1) {
            _server->send(400, "application/json", "{\"success\":false}");
            return;
        }
        
        String username = body.substring(uStart, uEnd);
        String password = body.substring(pStart, pEnd);
        
        if(username == "admin" && password == "admin123") {
            _sessionToken = _generateSessionToken();
            _server->send(200, "application/json", "{\"success\":true,\"token\":\"" + _sessionToken + "\"}");
        } else {
            _server->send(200, "application/json", "{\"success\":false}");
        }
    }
}

void AGVController::_handleScan() {
    int n = WiFi.scanNetworks(false, true);  // async=false, show_hidden=true
    String json = "[";
    for(int i = 0; i < n; i++) {
        if(i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + ",\"secured\":" + String(WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "true" : "false") + "}";
    }
    json += "]";
    _server->send(200, "application/json", json);
    WiFi.scanDelete();  // Clear scan results
}

void AGVController::_handleSaveWiFi() {
    if(_server->method() == HTTP_POST) {
        String body = _server->arg("plain");
        int ssidStart = body.indexOf("\"ssid\":\"") + 8;
        int ssidEnd = body.indexOf("\"", ssidStart);
        int passStart = body.indexOf("\"password\":\"") + 12;
        int passEnd = body.indexOf("\"", passStart);
        
        if (ssidStart == -1 || ssidEnd == -1 || passStart == -1 || passEnd == -1) {
            _server->send(400, "application/json", "{\"success\":false}");
            return;
        }
        
        String ssid = body.substring(ssidStart, ssidEnd);
        String pass = body.substring(passStart, passEnd);
        
        _saveCredentials(ssid, pass);
        _server->send(200, "application/json", "{\"success\":true}");
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP.restart();
    }
}

void AGVController::_handleCaptivePortal() {
    if (_isAPMode) {
        _server->sendHeader("Location", "http://192.168.4.1/setup", true);
        _server->send(302, "text/plain", "");
    } else {
        _server->send(404, "text/plain", "File Not Found");
    }
}

void AGVController::_webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            _safePrintln("[WebSocket] Client #" + String(num) + " disconnected");
            break;
            
        case WStype_CONNECTED:
            _safePrintln("[WebSocket] Client #" + String(num) + " connected from " + _webSocket->remoteIP(num).toString());
            _webSocket->sendTXT(num, "ESP32 Connected - Ready for commands");
            break;
            
        case WStype_TEXT: {
            String message = String((char*)payload);
            _safePrintln("\n[WEB->SERIAL] " + message);
            String response = "Received: " + message;
            _webSocket->sendTXT(num, response);
            
            // Also echo to Serial
            if(xSemaphoreTake(_printMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                Serial.println("\n[WEB->AGV] " + message);
                xSemaphoreGive(_printMutex);
            }
            break;
        }
            
        default:
            break;
    }
}

// ===== WIFI MODE FUNCTIONS =====
void AGVController::_startAPMode() {
    _isAPMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP("AGV_Controller_Setup", "12345678");
    
    IPAddress IP = WiFi.softAPIP();
    _safePrintln("\n========================================\n  STARTING ACCESS POINT MODE\n========================================");
    _safePrintln("📡 AP IP: " + IP.toString());
    _safePrintln("📶 SSID: AGV_Controller_Setup");
    _safePrintln("🔑 Pass: 12345678\n========================================\n");
    
    _dnsServer->start(53, "*", IP);
    _server->begin();
}

void AGVController::_startStationMode() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(_storedSSID.c_str(), _storedPassword.c_str());
    
    _safePrintln("Connecting to WiFi: " + _storedSSID);
    
    int attempts = 0;
    while(WiFi.status() != WL_CONNECTED && attempts < 30) {
        vTaskDelay(pdMS_TO_TICKS(500));
        _safePrint(".");
        attempts++;
    }
    
    if(WiFi.status() == WL_CONNECTED) {
        _isAPMode = false;
        _safePrintln("\n========================================\n  STARTING STATION MODE\n========================================");
        _safePrintln("📡 IP: " + WiFi.localIP().toString());
        
        if(MDNS.begin("agvcontrol")) {
            MDNS.addService("http", "tcp", 80);
            _safePrintln("🔍 mDNS: agvcontrol.local");
        }
        
        _server->begin();
        _webSocket->begin();
        
        _safePrintln("========================================\n  SYSTEM READY\n  Default Login: admin / admin123\n========================================\n");
    } else {
        _safePrintln("\n❌ Failed to connect to WiFi, starting AP mode...");
        _startAPMode();
    }
}

// ===== UTILITY FUNCTIONS =====
void AGVController::_loadCredentials() {
    Preferences prefs;
    prefs.begin("wifi", false);
    _storedSSID = prefs.getString("ssid", "");
    _storedPassword = prefs.getString("password", "");
    prefs.end();
    
    if (_storedSSID.length() > 0) {
        _safePrintln("Loaded WiFi credentials for: " + _storedSSID);
    } else {
        _safePrintln("No WiFi credentials stored");
    }
}

void AGVController::_saveCredentials(const String& ssid, const String& pass) {
    Preferences prefs;
    prefs.begin("wifi", false);
    prefs.putString("ssid", ssid);
    prefs.putString("password", pass);
    prefs.end();
    _safePrintln("Saved WiFi credentials for: " + ssid);
}

String AGVController::_generateSessionToken() {
    String token;
    randomSeed(micros());
    for(int i = 0; i < 32; i++) {
        token += String(random(0, 16), HEX);
    }
    return token;
}

void AGVController::_safePrintln(const String& msg) {
    if(xSemaphoreTake(_printMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.println(msg);
        xSemaphoreGive(_printMutex);
    }
}

void AGVController::_safePrint(const String& msg) {
    if(xSemaphoreTake(_printMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        Serial.print(msg);
        xSemaphoreGive(_printMutex);
    }
}


// ===== NEW PUBLIC METHOD =====
void AGVController::sendToWeb(const String& message) {
    // Safety checks
    if (_outgoingWebQueue == nullptr) {
        _safePrintln("[ERROR] Outgoing web queue not initialized");
        return;
    }
    
    if (message.length() == 0) {
        return;  // Don't send empty messages
    }
    
    // Prepare message for queue (limit to 255 chars for safety)
    char buffer[256];
    size_t len = message.length();
    if (len > 255) len = 255;
    strncpy(buffer, message.c_str(), len);
    buffer[len] = '\0';
    
    // Send to queue (non-blocking, 0 tick timeout)
    BaseType_t result = xQueueSend(_outgoingWebQueue, &buffer, 0);
    
    if (result != pdTRUE) {
        _safePrintln("[WARNING] Outgoing web queue full, message dropped: " + message.substring(0, 30) + "...");
    }
}
