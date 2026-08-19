#include "zone_manager.h"

ZoneManager::ZoneManager(RelayManager &relayManager)
    : _relayMgr(relayManager), _count(0)
{
    memset(_zones, 0, sizeof(_zones));
}

void ZoneManager::begin()
{
    Serial.println("[ZoneManager] Initialised");
}

bool ZoneManager::addZone(const char *name, uint8_t relayIndex)
{
    if (_count >= MAX_ZONES)
    {
        Serial.println("[ZoneManager] Zone table full");
        return false;
    }

    strncpy(_zones[_count].name, name, sizeof(_zones[_count].name) - 1);
    _zones[_count].name[sizeof(_zones[_count].name) - 1] = '\0';
    _zones[_count].relayIndex = relayIndex;
    _zones[_count].active     = false;
    _count++;

    Serial.printf("[ZoneManager] Added zone '%s' -> relay %u\n", name, relayIndex);
    return true;
}

const GardenZone *ZoneManager::getZone(uint8_t index) const
{
    if (index >= _count)
        return nullptr;
    return &_zones[index];
}

void ZoneManager::controlZone(uint8_t index, bool on)
{
    if (index >= _count)
    {
        Serial.printf("[ZoneManager] Invalid zone index %u\n", index);
        return;
    }

    _zones[index].active = on;
    _relayMgr.setRelay(_zones[index].relayIndex, on);

    Serial.printf("[ZoneManager] Zone '%s' -> %s\n",
                  _zones[index].name, on ? "ON" : "OFF");
}

void ZoneManager::printStatus() const
{
    Serial.println("\n===== ZONE STATUS =====");
    for (uint8_t i = 0; i < _count; i++)
    {
        Serial.printf("  [%u] %-20s (relay %u): %s\n",
                      i, _zones[i].name, _zones[i].relayIndex,
                      _zones[i].active ? "ON" : "OFF");
    }
    Serial.println("=======================");
}
