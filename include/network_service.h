#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include <Arduino.h>
#include <stdint.h>

class NetworkService {
public:
    NetworkService(const char* ssid, const char* password);
    
    void begin();
    
    // Connection status
    bool isConnected() const;
    
    // Get network information
    int getSignalStrength() const;  // Returns RSSI in dBm
    String getIP() const;
    String getMacAddress() const;
    
    // Periodic check for connection status
    void loop();  // Must be called frequently
    
    // Print network status
    void printStatus() const;
    
private:
    char ssid[64];
    char password[64];
    bool connected;
    unsigned long lastCheckTime;
    unsigned long checkInterval;
};

#endif // NETWORK_SERVICE_H
