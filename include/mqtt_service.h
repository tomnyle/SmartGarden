#pragma once

#include "mqtt_handler.h"

class MQTTService
{
public:
    static void publishCropList()
    {
        ::publishCropList();
    }

    static void publishCurrentCropConfig()
    {
        ::publishCurrentCropConfig();
    }
};