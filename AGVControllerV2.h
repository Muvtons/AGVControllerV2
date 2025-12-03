#ifndef AGV_CONTROLLER_V2_H
#define AGV_CONTROLLER_V2_H

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

class AGVController {
public:
    AGVController();
    void sendToWeb(const String& message);
    void begin();
    
private:
    static AGVController* _instance;
    
    // Web components
    WebServer* _server;
    WebSocketsServer* _webSocket;
    DNSServer* _dnsServer;
    
    // RTOS components
    QueueHandle_t _serialQueue;
    SemaphoreHandle_t _printMutex;
    TaskHandle_t _taskWebHandle;
    TaskHandle_t _taskSerialHandle;
    
    // Configuration
    bool _isAPMode;
    String _storedSSID;
    String _storedPassword;
    String _sessionToken;
    
    // Static HTML pages
    static const char _loginPage[];
    static const char _wifiSetupPage[];
    static const char _mainPage[];
    
    // Task functions
    static void _serialTask(void* pvParameters);
    static void _webTask(void* pvParameters);
    
    // Web handlers
    void _handleLogin();
    void _handleScan();
    void _handleSaveWiFi();
    void _handleCaptivePortal();
    
    // WebSocket handler
    void _webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // WiFi functions
    void _startAPMode();
    void _startStationMode();
    
    // Utility functions
    void _loadCredentials();
    void _saveCredentials(const String& ssid, const String& pass);
    String _generateSessionToken();
    void _safePrintln(const String& msg);
    void _safePrint(const String& msg);
};

extern AGVController AGV;

#endif
