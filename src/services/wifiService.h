#pragma once

class WifiService {
public:
    void init();
    bool isConnected();
    void reconnectIfNeeded(); // handles WiFi drop mid-session
};