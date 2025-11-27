#ifndef AGV_CONTROLLERV2_H
#define AGV_CONTROLLERV2_H

#include <Arduino.h>

class AGVController {
public:
    AGVController();
    void begin(); // Initializes everything and starts tasks
    
private:
    static AGVController* _instance;
    
    // RTOS Objects
    TaskHandle_t _taskSerialHandle = nullptr;
    TaskHandle_t _taskWebHandle = nullptr;
    QueueHandle_t _serialQueue = nullptr;
    SemaphoreHandle_t _printMutex = nullptr;
    
    // State
    volatile bool _isAPMode = false;
    String _sessionToken;
    String _storedSSID;
    String _storedPassword;
    
    // Network Objects
    WebServer* _server = nullptr;
    WebSocketsServer* _webSocket = nullptr;
    DNSServer* _dnsServer = nullptr;
    
    // Preferences
    Preferences _prefs;
    
    // Task Wrappers
    static void _serialTask(void* pvParameters);
    static void _webTask(void* pvParameters);
    
    // Event Handlers
    static void _handleRoot();
    static void _handleLogin();
    static void _handleDashboard();
    static void _handleWiFiSetup();
    static void _handleScan();
    static void _handleSaveWiFi();
    static void _handleCaptivePortal();
    static void _webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    
    // Internal Methods
    void _loadCredentials();
    void _saveCredentials(const String& ssid, const String& pass);
    void _startAPMode();
    void _startStationMode();
    String _generateSessionToken();
    void _safePrintln(const String& msg);
    
    // HTML Pages (PROGMEM)
    static const char _loginPage[];
    static const char _wifiSetupPage[];
    static const char _mainPage[];
};

extern AGVController AGV; // Global instance for easy access

#endif
