# SmartGarden

Firmware Smart Garden (ESP32 + PlatformIO) hỗ trợ quản lý đa loại cây trồng, auto control theo profile, cảnh báo thông minh và logging dữ liệu.

## Kiến trúc mới

- `src/smartgarden.ino`: luồng firmware chính
- `include/config.h`: cấu hình tập trung (WiFi/MQTT, giới hạn bộ nhớ)
- `include/crop_profiles.h`: định nghĩa `CropProfile` + CRUD + lưu/đọc profile bằng Preferences (NVS)
- `include/auto_control.h`: engine tự động điều khiển relay + phát hiện alert theo range tối ưu
- `include/mqtt_handler.h`: xử lý MQTT topic mở rộng cho crop management
- `include/app_config.h`: compatibility mapping từ cấu hình cũ sang cấu hình mới

## Crop profile

Mỗi loại cây được mô tả bởi:

- Tên cây
- Range tối ưu: nhiệt độ, độ ẩm không khí, độ ẩm đất, pH, EC
- Range N/P/K
- Thời gian tưới
- Rule relay theo chu kỳ bật/tắt

Mặc định có sẵn profile `lettuce` và `tomato`.

## MQTT topics mở rộng

Root topic: `smartgarden`

- `smartgarden/crop/create`
- `smartgarden/crop/update`
- `smartgarden/crop/delete`
- `smartgarden/crop/select`
- `smartgarden/crop/list`

Payload `create/update` dạng CSV:

```text
name,tempMin|tempMax,airMin|airMax,soilMin|soilMax,phMin|phMax,ecMin|ecMax,nMin|nMax,pMin|pMax,kMin|kMax,irrigationMs
```

Ví dụ:

```text
tomato,20|28,55|75,50|70,5.5|6.8,2.0|3.5,150|250,60|90,180|320,45000
```

## Auto control + Alert

- Soil humidity thấp hơn min → bật relay tưới
- Soil humidity cao hơn max → tắt relay tưới
- Relay khác có thể chạy theo cycle (`onDurationMs/offDurationMs`)
- Alert được phát sinh khi bất kỳ chỉ số nào ra ngoài range profile

## Data logging và dashboard

- Lưu lịch sử cảm biến bằng ring buffer (`DATA_LOG_CAPACITY`)
- In dashboard trạng thái định kỳ qua Serial

## Cấu hình an toàn hơn

Trong `platformio.ini`, truyền secrets bằng build flags thay vì hard-code:

```ini
build_flags =
  -DSMARTGARDEN_WIFI_SSID=\"${sysenv.SMARTGARDEN_WIFI_SSID}\"
  -DSMARTGARDEN_WIFI_PASSWORD=\"${sysenv.SMARTGARDEN_WIFI_PASSWORD}\"
  -DSMARTGARDEN_MQTT_HOST=\"${sysenv.SMARTGARDEN_MQTT_HOST}\"
  -DSMARTGARDEN_MQTT_USERNAME=\"${sysenv.SMARTGARDEN_MQTT_USERNAME}\"
  -DSMARTGARDEN_MQTT_PASSWORD=\"${sysenv.SMARTGARDEN_MQTT_PASSWORD}\"
```

## Build

```bash
pio run
```

## Upload

```bash
pio run -t upload
```
