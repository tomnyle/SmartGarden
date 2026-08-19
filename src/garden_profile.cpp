#include "garden_profile.h"

// ========================================
// CropProfileStore::loadDefaults()
// Populates the store with 13 crop profiles from README.md
// ========================================
// Macro shorthand for RangeValue
#define R(lo, hi) RangeValue{(lo), (hi)}

void CropProfileStore::loadDefaults()
{
    _count       = 0;
    _activeIndex = 0;

    // ── Rare / Medicinal ─────────────────────────────────────────────────────
    // 1. Sâm Ngọc Linh (Vietnamese Ginseng)
    {
        CropProfile p = makeProfile(
            "ginseng",
            R(15, 22), R(70, 90), R(70, 80), R(5.5f, 6.5f),
            R(1.2f, 2.0f), R(80, 200), R(40, 100), R(100, 200), 30000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 20000, 40000};
        p.relayRuleCount = 1;
        create(p);
    }

    // 2. Tam Thất (Salvia / Dan Shen)
    {
        CropProfile p = makeProfile(
            "salvia",
            R(18, 24), R(60, 80), R(60, 75), R(6.0f, 7.0f),
            R(1.5f, 2.5f), R(100, 250), R(50, 120), R(120, 250), 25000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 15000, 45000};
        p.relayRuleCount = 1;
        create(p);
    }

    // 3. Ba Kích (Morinda)
    {
        CropProfile p = makeProfile(
            "morinda",
            R(22, 28), R(60, 80), R(65, 78), R(6.0f, 7.0f),
            R(2.0f, 3.0f), R(120, 280), R(60, 140), R(150, 300), 25000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 20000, 40000};
        p.relayRuleCount = 1;
        create(p);
    }

    // ── Leafy Vegetables ─────────────────────────────────────────────────────
    // 4. Rau Cải Xoăn (Kale)
    {
        CropProfile p = makeProfile(
            "kale",
            R(18, 24), R(50, 75), R(45, 65), R(5.8f, 6.5f),
            R(1.0f, 1.8f), R(100, 200), R(40, 100), R(120, 230), 20000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 57600000UL, 28800000UL}; // 16h on, 8h off
        p.relayRuleCount = 1;
        create(p);
    }

    // 5. Rau Mầm (Microgreens)
    {
        CropProfile p = makeProfile(
            "microgreens",
            R(15, 22), R(60, 85), R(70, 85), R(6.5f, 7.0f),
            R(1.0f, 2.0f), R(80, 180), R(30, 80), R(80, 180), 15000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 57600000UL, 28800000UL};
        p.relayRuleCount = 1;
        create(p);
    }

    // ── Fruiting Vegetables ───────────────────────────────────────────────────
    // 6. Cà Chua (Tomato)
    {
        CropProfile p = makeProfile(
            "tomato",
            R(20, 28), R(50, 75), R(50, 70), R(5.5f, 6.8f),
            R(2.0f, 3.5f), R(150, 300), R(60, 150), R(200, 400), 30000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 57600000UL, 28800000UL};
        p.relayRuleCount = 1;
        create(p);
    }

    // 7. Dâu Tây (Strawberry)
    {
        CropProfile p = makeProfile(
            "strawberry",
            R(15, 25), R(60, 80), R(60, 75), R(5.5f, 6.8f),
            R(1.2f, 2.0f), R(100, 220), R(40, 100), R(150, 280), 25000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 50400000UL, 36000000UL}; // 14h on, 10h off
        p.relayRuleCount = 1;
        create(p);
    }

    // 8. Dưa Chuột (Cucumber)
    {
        CropProfile p = makeProfile(
            "cucumber",
            R(22, 30), R(60, 80), R(50, 65), R(6.0f, 7.0f),
            R(2.5f, 4.0f), R(150, 300), R(60, 150), R(200, 400), 30000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 20000, 40000};
        p.relayRuleCount = 1;
        create(p);
    }

    // 9. Ớt (Chili)
    {
        CropProfile p = makeProfile(
            "chili",
            R(24, 28), R(50, 75), R(60, 70), R(6.0f, 6.8f),
            R(2.0f, 3.5f), R(120, 260), R(50, 130), R(170, 340), 25000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 20000, 40000};
        p.relayRuleCount = 1;
        create(p);
    }

    // 10. Cà Tím (Eggplant)
    {
        CropProfile p = makeProfile(
            "eggplant",
            R(20, 28), R(55, 80), R(50, 70), R(5.5f, 6.5f),
            R(1.8f, 3.0f), R(130, 270), R(55, 135), R(180, 360), 28000);
        p.relayRules[0] = {FAN_RELAY_INDEX, 20000, 40000};
        p.relayRuleCount = 1;
        create(p);
    }

    // ── Root / Other Vegetables ───────────────────────────────────────────────
    // 11. Cà Rốt (Carrot)
    {
        CropProfile p = makeProfile(
            "carrot",
            R(15, 20), R(50, 75), R(65, 75), R(6.0f, 6.8f),
            R(1.5f, 2.5f), R(100, 220), R(45, 110), R(140, 280), 25000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 50400000UL, 36000000UL};
        p.relayRuleCount = 1;
        create(p);
    }

    // 12. Hành Tây (Onion)
    {
        CropProfile p = makeProfile(
            "onion",
            R(13, 18), R(50, 75), R(60, 70), R(6.0f, 7.5f),
            R(1.2f, 2.0f), R(100, 220), R(45, 110), R(130, 260), 20000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 57600000UL, 28800000UL};
        p.relayRuleCount = 1;
        create(p);
    }

    // 13. Súp Lơ (Broccoli)
    {
        CropProfile p = makeProfile(
            "broccoli",
            R(15, 22), R(55, 80), R(65, 75), R(6.0f, 7.5f),
            R(1.5f, 2.5f), R(120, 250), R(55, 135), R(150, 300), 25000);
        p.relayRules[0] = {LIGHT_RELAY_INDEX, 57600000UL, 28800000UL};
        p.relayRuleCount = 1;
        create(p);
    }

    Serial.printf("[CropProfileStore] Loaded %zu default profiles\n", _count);
}
