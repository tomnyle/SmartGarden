#pragma once

#include <Arduino.h>

#include "config.h"

class ZoneManager
{
public:
    ZoneManager()
    {
        for (size_t i = 0; i < SMARTGARDEN_RELAY_COUNT; ++i)
        {
            _enabled[i] = true;
        }
    }

    bool isEnabled(uint8_t index) const
    {
        return index < SMARTGARDEN_RELAY_COUNT ? _enabled[index] : false;
    }

    void setEnabled(uint8_t index, bool enabled)
    {
        if (index < SMARTGARDEN_RELAY_COUNT)
        {
            _enabled[index] = enabled;
        }
    }

private:
    bool _enabled[SMARTGARDEN_RELAY_COUNT];
};