#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <stdint.h>
#include "pins.h"

#define NUM_RELAYS 8
#define RELAY_ON true
#define RELAY_OFF false

typedef struct {
    uint8_t relayIndex;
    bool state;
} RelayCommand;

class RelayManager {
public:
    RelayManager();
    
    void begin();
    
    // Set relay state
    bool setRelay(uint8_t index, bool state);
    
    // Get relay state
    bool getRelayState(uint8_t index) const;
    
    // Set all relays
    void setAllRelays(bool state);
    
    // Toggle relay
    bool toggleRelay(uint8_t index);
    
    // Get relay pin
    uint8_t getRelayPin(uint8_t index) const;
    
    // Print status
    void printStatus() const;
    
private:
    bool relayStates[NUM_RELAYS];
};

#endif // RELAY_MANAGER_H
