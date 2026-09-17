# Home Assistant + MQTT Discovery cho SmartGarden

## 1. Cài MQTT Integration
1. Cài Mosquitto broker (hoặc broker tương đương).
2. Trong Home Assistant: **Settings → Devices & Services → Add Integration → MQTT**.
3. Cấu hình broker host/port/user/pass trùng `include/app_config.h`.

## 2. Bật Discovery
Trong MQTT integration bật discovery (mặc định HA hỗ trợ `homeassistant/.../config`).

SmartGarden sẽ publish config vào:
- `homeassistant/sensor/.../config`
- `homeassistant/switch/.../config`
- `homeassistant/select/.../config`

Về danh sách crop:
- Firmware publish thêm runtime topic `smartgarden/crop/list` (JSON options).
- MQTT Discovery `select` của Home Assistant lấy options tại thời điểm publish discovery.
- Nếu crop catalog thay đổi, cần publish lại discovery (hoặc reboot/reconnect thiết bị) để HA cập nhật options.

## 3. Entity được tạo
- Sensor: air_temp, air_humidity, soil_moisture, soil_temp, ph, ec, nitrogen, phosphorus, potassium.
- Switch: 8 relay.
- Select: `Crop Profile` và `Operation Mode`.

Lưu ý quan trọng về relay switch:
- Ở `manual`: switch relay nhận lệnh bình thường.
- Ở `auto` và `monitor`: switch vẫn hiển thị để giám sát trạng thái, nhưng lệnh relay thủ công sẽ bị firmware bỏ qua có chủ đích.

Mỗi entity có:
- `availability_topic = smartgarden/availability`
- `payload_available = online`
- `payload_not_available = offline`

## 4. Runtime topics
- Relay command/state: `smartgarden/relay/<1..8>/set|state`
- Crop: `smartgarden/crop/set`, `smartgarden/crop/state`
- Mode: `smartgarden/mode/set`, `smartgarden/mode/state`

## 5. Dashboard mẫu (gợi ý)
- 1 card Select cho `Operation Mode`
- 1 card Select cho `Crop Profile`
- 8 switch relay
- 9 sensor card

## 6. Kiểm tra nhanh topics
```bash
mosquitto_sub -h <broker> -t homeassistant/# -v
mosquitto_sub -h <broker> -t smartgarden/# -v
```

Nếu không có entity mới:
- Xóa retained discovery cũ sai schema rồi reboot ESP32.
- Kiểm tra broker user/pass và log Home Assistant.
