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

## 📡 MQTT Topics (chuẩn runtime + discovery)

```
# Discovery config (Home Assistant subscribe)
homeassistant/sensor/.../config
homeassistant/switch/.../config
homeassistant/select/.../config

# Runtime state/command (ESP32 publish/subscribe)
smartgarden/status                  → online/offline (availability)
smartgarden/diag/rssi               → RSSI Wi‑Fi (dBm)
smartgarden/crop/list               → Danh sách crop
smartgarden/crop/current            → Crop hiện tại
smartgarden/crop/set                → Chọn crop từ HA

smartgarden/sensors/air_temp
smartgarden/sensors/air_humidity
smartgarden/sensors/soil_moisture
smartgarden/sensors/soil_temp
smartgarden/sensors/ph
smartgarden/sensors/ec
smartgarden/sensors/nitrogen
smartgarden/sensors/phosphorus
smartgarden/sensors/potassium

smartgarden/relay/1/state ... smartgarden/relay/8/state
smartgarden/relay/1/set   ... smartgarden/relay/8/set
```

## 🔧 Cấu Hình

### WiFi & MQTT
Chỉnh sửa trong `include/app_config.h`:
```cpp
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER "192.168.100.168"
#define MQTT_PORT 1883
#define MQTT_USERNAME "..."
#define MQTT_PASSWORD "..."
```
> Khuyến nghị: chuyển thông tin nhạy cảm sang `include/secrets.h` và thêm vào `.gitignore`.

### Chọn Loại Cây
```
Topic: smartgarden/crop/set
Payload: "lettuce"  (hoặc: tomato, ginseng, salvia, morinda, strawberry, v.v.)
```

## 🏗️ Cấu Trúc Code

```
SmartGarden/
├── src/
│   └── smartgarden.ino          # Main firmware
├── include/
│   ├── config.h                  # Cấu hình tập trung
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

Giá trị mặc định lấy trực tiếp từ `include/pins.h`:

```cpp
#define DHT_PIN 4
#define RS485_RX 16
#define RS485_TX 17
#define RS485_DE 18
const uint8_t RELAY_PINS[8] = {32, 33, 25, 26, 27, 14, 12, 13};
```

Firmware luôn dùng `DHT_PIN`, `RS485_RX/TX/DE` và `RELAY_PINS[]` từ file này.
Nếu có khác biệt thực tế, ưu tiên cập nhật `include/pins.h` rồi build lại.

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

## 🛠️ Build / Upload / Verify nhanh

```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Kiểm tra MQTT sau khi boot:
```bash
mosquitto_sub -h 192.168.100.168 -t 'homeassistant/+/+/config' -v
mosquitto_sub -h 192.168.100.168 -t 'smartgarden/#' -v
```

Log mong đợi:
- `[MQTT] Connected`
- `[MQTT Discovery] ... published`
- Sensor state xuất hiện dưới `smartgarden/sensors/...`
- Relay command từ HA gửi về `smartgarden/relay/N/set`
