#pragma once

#include <Arduino.h>
#include "relay_manager.h"

// ========================================
// ZoneManager - manages named garden zones, each backed by a relay
// ========================================

#define MAX_ZONES 8

struct GardenZone
{
    char    name[24];
    uint8_t relayIndex;
    bool    active;
};

class ZoneManager
{
public:
    ZoneManager(RelayManager &relayManager);

    void begin();

    // Register a zone (name, relay index).  Returns false if zones are full.
    bool addZone(const char *name, uint8_t relayIndex);

    // Retrieve a zone by index (nullptr if out of range)
    const GardenZone *getZone(uint8_t index) const;

    // Get number of registered zones
    uint8_t count() const { return _count; }

    // Turn a zone on or off by index
    void controlZone(uint8_t index, bool on);

    // Print status of all zones to Serial
    void printStatus() const;

private:
    RelayManager &_relayMgr;
    GardenZone    _zones[MAX_ZONES];
    uint8_t       _count;
};
