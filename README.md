# SmartGarden (ESP32 + Home Assistant MQTT)

## 1) Tổng quan kiến trúc
Firmware chạy trên ESP32 (PlatformIO `env:esp32dev`), gồm các phần chính:
- Đọc DHT22 (nhiệt độ/độ ẩm không khí).
- Đọc RS485 (soil/NPK) ở mức tích hợp khung Modbus cơ bản, cần kiểm chứng thực địa.
- Điều khiển 8 relay active-low.
- MQTT runtime qua namespace `smartgarden/...`.
- Home Assistant MQTT Discovery qua namespace `homeassistant/.../config`.

## 2) Chế độ vận hành
- `manual`: cho phép điều khiển relay thủ công từ Home Assistant/MQTT.
- `auto`: firmware tự điều khiển relay climate theo dữ liệu DHT22 + crop profile; bỏ qua lệnh relay thủ công.
- `monitor`: chỉ giám sát, tắt toàn bộ relay và bỏ qua lệnh relay thủ công.

> Lưu ý an toàn: firmware **không tự bật irrigation** khi dữ liệu soil/RS485 chưa đọc thành công và chưa hợp lệ.

## 3) Sơ đồ chân (đang dùng trong code)
- DHT22: GPIO4
- RS485: RX=GPIO16, TX=GPIO17, DE/RE=GPIO18
- Relay 1..8: GPIO32, 33, 25, 26, 27, 14, 12, 13
- Relay active-low: `LOW=ON`, `HIGH=OFF`

Chi tiết wiring: xem `docs/WIRING.md`.

## 4) MQTT topics

### Runtime (`smartgarden/...`)
- Availability: `smartgarden/availability` (`online`/`offline`)
- Sensors:
  - `smartgarden/sensors/air_temp`
  - `smartgarden/sensors/air_humidity`
  - `smartgarden/sensors/soil_moisture`
  - `smartgarden/sensors/soil_temp`
  - `smartgarden/sensors/ph`
  - `smartgarden/sensors/ec`
  - `smartgarden/sensors/nitrogen`
  - `smartgarden/sensors/phosphorus`
  - `smartgarden/sensors/potassium`
- Relay state/command:
  - `smartgarden/relay/<1..8>/state`
  - `smartgarden/relay/<1..8>/set` (`ON`/`OFF`)
- Crop profile:
  - command: `smartgarden/crop/set`
  - state: `smartgarden/crop/state`
- Operation mode:
  - command: `smartgarden/mode/set`
  - state: `smartgarden/mode/state`

### Discovery (`homeassistant/.../config`)
Firmware publish discovery cho:
- 9 sensor
- 8 switch (relay)
- 1 select crop profile
- 1 select operation mode

Chi tiết tích hợp HA: `docs/HOME_ASSISTANT.md`.

## 5) Build & upload (PlatformIO)
```bash
pio run -e esp32dev
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

Nếu máy không có `pio`:
```bash
pip install platformio
```

## 6) Cấu hình
Cấu hình tập trung tại:
- `include/app_config.h` (Wi-Fi, MQTT, topic, interval)
- `include/pins.h` (toàn bộ pin)

Khuyến nghị: chuyển credential thật sang `include/secrets.h` (file local, không commit).

## 7) Kiểm tra nhanh MQTT
```bash
# kiểm tra mode hiện tại
mosquitto_sub -h <broker> -t smartgarden/mode/state -v

# set manual
mosquitto_pub -h <broker> -t smartgarden/mode/set -m manual

# bật relay 1 (chỉ có hiệu lực khi manual)
mosquitto_pub -h <broker> -t smartgarden/relay/1/set -m ON

# xem trạng thái relay
mosquitto_sub -h <broker> -t smartgarden/relay/+/state -v
```

## 8) Giới hạn hiện tại
- DHT22: đã tích hợp đọc runtime.
- RS485/soil/NPK: có khung đọc và publish, nhưng cần kiểm chứng với phần cứng thực tế (địa chỉ slave, baudrate, bản đồ thanh ghi, CRC/khung phản hồi theo sensor thực dùng).
- Không tuyên bố RS485/NPK đã fully production-ready khi chưa xác minh thực địa.

## 9) Troubleshooting
- Không thấy entity HA: kiểm tra MQTT Discovery đã bật và xem `homeassistant/#`.
- Relay không chạy đúng: kiểm tra module có active-low, nguồn relay riêng, mass chung.
- Auto không tưới: kiểm tra dữ liệu RS485 hợp lệ; nếu chưa hợp lệ, firmware chủ động không bật irrigation.

Xem checklist test chi tiết ở `docs/TESTING.md`.
