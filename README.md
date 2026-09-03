# SmartGarden - Hệ thống quản lý vườn thông minh đa loại cây

## 📋 Giới thiệu

SmartGarden là một hệ thống IoT hoàn chỉnh cho phép quản lý **13 loại cây trồng** khác nhau với các thông số tối ưu riêng cho từng loại. Hệ thống tự động điều khiển các relay (bơm nước, quạt, đèn, v.v.) dựa trên dữ liệu cảm biến và yêu cầu của từng loại cây.

## 🌿 Các Loại Cây Hỗ Trợ (13 loại)

### 🌱 Loại Cây Quý Hiếm
| Loại Cây | Tên Khoa Học | Nhiệt độ | Ẩm đất | pH | EC |
|---------|---------|---------|---------|-----|--------|
| 🌿 **Sâm Ngọc Linh** | Panax Notoginseng | 15-22°C | 70-80% | 5.5-6.5 | 1.2-2.0 |
| 🌿 **Tam Thất** | Salvia Miltiorrhiza | 18-24°C | 60-75% | 6.0-7.0 | 1.5-2.5 |
| 🌿 **Ba Kích** | Morinda Citrifolia | 22-28°C | 65-78% | 6.0-7.0 | 2.0-3.0 |

### 🥬 Loại Rau Ăn Lá
| Loại Cây | Tên Khoa Học | Nhiệt độ | Ẩm đất | pH | EC |
|---------|---------|---------|---------|-----|--------|
| 🥬 **Rau Cải Xoăn** | Lactuca sativa | 18-24°C | 45-65% | 5.8-6.5 | 1.0-1.8 |
| 🌾 **Rau Mầm** | Microgreens | 15-22°C | 70-85% | 6.5-7.0 | 1.0-2.0 |

### 🍅 Loại Cây Ăn Quả
| Loại Cây | Tên Khoa Học | Nhiệt độ | Ẩm đất | pH | EC |
|---------|---------|---------|---------|-----|--------|
| 🍅 **Cà Chua** | Solanum lycopersicum | 20-28°C | 50-70% | 5.5-6.8 | 2.0-3.5 |
| 🍓 **Dâu Tây** | Fragaria vesca | 15-25°C | 60-75% | 5.5-6.8 | 1.2-2.0 |
| 🥒 **Dưa Chuột** | Cucumis sativus | 22-30°C | 50-65% | 6.0-7.0 | 2.5-4.0 |
| 🌶️ **Ớt** | Capsicum annuum | 24-28°C | 60-70% | 6.0-6.8 | 2.0-3.5 |
| 🍆 **Cà Tím** | Solanum melongena | 20-28°C | 50-70% | 5.5-6.5 | 1.8-3.0 |

### 🥕 Loại Rau Quả Khác
| Loại Cây | Tên Khoa Học | Nhiệt độ | Ẩm đất | pH | EC |
|---------|---------|---------|---------|-----|--------|
| 🥕 **Cà Rốt** | Daucus carota | 15-20°C | 65-75% | 6.0-6.8 | 1.5-2.5 |
| 🧅 **Hành Tây** | Allium cepa | 13-18°C | 60-70% | 6.0-7.5 | 1.2-2.0 |
| 🥦 **Súp Lơ** | Brassica oleracea | 15-22°C | 65-75% | 6.0-7.5 | 1.5-2.5 |

## 🚀 Các Tính Năng Chính

### ✨ **1. Quản lý Đa Loại Cây**
- Lưu trữ profile cho 16 loại cây (hiện tại 13 loại)
- Mỗi profile chứa thông số tối ưu riêng
- Dễ dàng thêm loại cây mới
- Lưu trữ persistent trong EEPROM

### 🤖 **2. Điều Khiển Tự Động**
- Tự động bật/tắt relay dựa trên:
  - Thông số cảm biến hiện tại
  - Loại cây được chọn
  - Thời gian trong ngày
- Điều khiển ưu tiên: Soil humidity > Time-based rules

### 🚨 **3. Hệ Thống Cảnh Báo**
- Báo động real-time khi thông số ngoài phạm vi
- Thông báo qua MQTT
- Lưu lịch sử cảnh báo

### 📊 **4. Giám Sát & Logging**
- Ghi lại lịch sử dữ liệu từ cảm biến
- Theo dõi xu hướng theo thời gian
- Xuất dữ liệu qua MQTT

### 🌐 **5. MQTT Integration**
- Quản lý cây qua MQTT topics
- Tích hợp Home Assistant
- Điều khiển từ điện thoại/web

### 💾 **6. Lưu Trữ An Toàn**
- Cấu hình lưu trong EEPROM
- Không mất dữ liệu khi mất điện
- Tự động load khi khởi động

## 📡 MQTT Topics

```
# Quản lý Cây
smartgarden/crop/list              → Danh sách tất cả loại cây
smartgarden/crop/set               → Chọn loại cây (Payload: "lettuce")
smartgarden/crop/current           → Loại cây hiện tại
smartgarden/crop/config            → Cấu hình loại cây hiện tại (JSON)

# Dữ liệu Cảm Biến
smartgarden/sensors/air_temp       → Nhiệt độ không khí (°C)
smartgarden/sensors/air_humidity   → Độ ẩm không khí (%)
smartgarden/sensors/soil_moisture  → Độ ẩm đất (%)
smartgarden/sensors/soil_temp      → Nhiệt độ đất (°C)
smartgarden/sensors/ph             → Giá trị pH
smartgarden/sensors/ec             → Độ dẫn điện (uS/cm)
smartgarden/sensors/nitrogen       → Nitrogen (mg/kg)
smartgarden/sensors/phosphorus     → Phosphorus (mg/kg)
smartgarden/sensors/potassium      → Potassium (mg/kg)

# Relay & Điều Khiển
smartgarden/relay/1/state          → Trạng thái relay 1
smartgarden/relay/1/set            → Điều khiển relay 1
smartgarden/autocontrol/state      → Trạng thái auto control

# Cảnh Báo
smartgarden/alerts                 → Các cảnh báo thời gian thực
```

## 🔧 Cấu Hình

### WiFi & MQTT
Firmware hiện publish Home Assistant MQTT Discovery trong `src/smartgarden.cpp` với prefix mặc định `homeassistant`.

Thông số WiFi/MQTT đang cấu hình trong `src/smartgarden.cpp` (biến `ssid`, `password`, `mqtt_server`, `mqtt_user`, `mqtt_password`) và cũng có macro trong `include/app_config.h`:
```cpp
#define SMARTGARDEN_WIFI_SSID "YOUR_WIFI_SSID"
#define SMARTGARDEN_WIFI_PASSWORD "YOUR_PASSWORD"
#define SMARTGARDEN_MQTT_HOST "192.168.1.100"
#define SMARTGARDEN_MQTT_PORT 1883
#define SMARTGARDEN_MQTT_USERNAME "username"
#define SMARTGARDEN_MQTT_PASSWORD "password"
```

### Chọn Loại Cây
```
Topic: smartgarden/crop/set
Payload: "lettuce"  (hoặc: tomato, ginseng, salvia, morinda, strawberry, v.v.)
```

## 🏗️ Cấu Trúc Code

```
SmartGarden/
├── src/
│   └── smartgarden.cpp          # Main firmware (setup/loop + MQTT discovery)
├── include/
│   ├── app_config.h              # Cấu hình tập trung
│   ├── crop_profiles.h           # Định nghĩa loại cây
│   ├── auto_control.h            # Logic điều khiển
│   └── mqtt_handler.h            # MQTT topics
├── platformio.ini                # Build config
└── README.md                      # Tài liệu này
```

## 📦 Thư Viện Cần Thiết

```
- WiFi (built-in ESP32)
- PubSubClient (MQTT)
- DHT (Cảm biến nhiệt độ/độ ẩm)
- ModbusMaster (Đọc cảm biến đất)
- Preferences (EEPROM storage)
```

## ⚡ Lưu Ý Quan Trọng

1. **Thông số mặc định**: Nếu không đặt WiFi/MQTT, hệ thống sẽ dùng placeholder
2. **Bơm nước mặc định**: Relay 0 được dùng cho bơm tưới
3. **Thời gian cập nhật**: Mỗi 5 giây
4. **Lưu trữ**: Tối đa 16 loại cây

## 🎯 Ví Dụ Sử Dụng

### Chọn Sâm Ngọc Linh
```bash
mosquitto_pub -h 192.168.1.100 -t "smartgarden/crop/set" -m "ginseng"
```

### Liệt kê tất cả loại cây
```bash
mosquitto_sub -h 192.168.1.100 -t "smartgarden/crop/list"
# Response: lettuce,tomato,ginseng,salvia,morinda,strawberry,cucumber,chili,carrot,onion,eggplant,microgreens,broccoli
```

### Xem cấu hình hiện tại
```bash
mosquitto_sub -h 192.168.1.100 -t "smartgarden/crop/config"
```

## 🔌 Kế Nối Phần Cứng

### Cảm Biến
- DHT22: Pin 15
- RS485 (Modbus): RX=16, TX=17, DE/RE=4

### Relay
- Relay 0: Pin 5   (Bơm nước)
- Relay 1: Pin 18  (Quạt)
- Relay 2: Pin 19  (Đèn)
- Relay 3: Pin 27  (Phân bón)
- Relay 4: Pin 32
- Relay 5: Pin 33
- Relay 6: Pin 25
- Relay 7: Pin 26

## 📝 Cách Thêm Loại Cây Mới

Thêm vào `loadDefaults()` trong `include/crop_profiles.h`:
```cpp
CropProfile newCrop = makeProfile(
    "crop_name",
    {tempMin, tempMax},
    {humidityMin, humidityMax},
    {soilMin, soilMax},
    {phMin, phMax},
    {ecMin, ecMax},
    {nMin, nMax},
    {pMin, pMax},
    {kMin, kMax},
    irrigationDurationMs);
newCrop.relayRules[0] = {0, onTimeMs, offTimeMs};
newCrop.relayRuleCount = 1;
create(newCrop);
```

## 📄 License

Mở rộng dự án SmartGarden - tomnyle

## 🤝 Đóng Góp

Chào mừng các đóng góp! Vui lòng tạo Pull Request để thêm loại cây mới hoặc cải thiện tính năng.

---

**Phiên bản**: 1.0.0  
**Cập nhật**: 2026-08-19  
**Trạng thái**: ✅ Hoàn thành 13 loại cây
