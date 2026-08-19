#include "irrigation_manager.h"

IrrigationManager::IrrigationManager(RelayManager &relayManager, uint8_t relayIndex)
    : _relayMgr(relayManager),
      _relayIndex(relayIndex),
      _active(false),
      _startTime(0),
      _durationMs(0)
{
}

void IrrigationManager::begin()
{
    _relayMgr.setRelay(_relayIndex, false);
    Serial.println("[IrrigationManager] Initialised");
}

void IrrigationManager::startIrrigation(unsigned long durationMs)
{
    if (_active)
    {
        Serial.println("[IrrigationManager] Already running, resetting timer");
    }

    _active     = true;
    _startTime  = millis();
    _durationMs = durationMs;

    _relayMgr.setRelay(_relayIndex, true);

    if (durationMs > 0)
        Serial.printf("[IrrigationManager] Started for %lu ms\n", durationMs);
    else
        Serial.println("[IrrigationManager] Started (indefinite)");
}

void IrrigationManager::stopIrrigation()
{
    if (!_active)
        return;

    _active = false;
    _relayMgr.setRelay(_relayIndex, false);
    Serial.println("[IrrigationManager] Stopped");
}

void IrrigationManager::loop()
{
    if (_active && _durationMs > 0)
    {
        if (millis() - _startTime >= _durationMs)
        {
            Serial.println("[IrrigationManager] Timeout - stopping");
            stopIrrigation();
        }
    }
}
