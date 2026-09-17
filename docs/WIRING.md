# Wiring SmartGarden (ESP32)

## 1) Pin map
- DHT22 DATA → GPIO4
- RS485 MAX485:
  - RO → GPIO16 (RX2)
  - DI → GPIO17 (TX2)
  - DE/RE → GPIO18
- Relay 1..8 IN:
  - R1 GPIO32
  - R2 GPIO33
  - R3 GPIO25
  - R4 GPIO26
  - R5 GPIO27
  - R6 GPIO14
  - R7 GPIO12
  - R8 GPIO13

## 2) Cảnh báo nguồn
- Relay board nên dùng nguồn riêng đủ dòng.
- Bắt buộc nối GND chung giữa ESP32, relay board, MAX485.
- Tránh cấp tải lớn trực tiếp từ chân ESP32.

## 3) Active-low
Firmware đang dùng relay active-low:
- `LOW` = bật relay
- `HIGH` = tắt relay

Nếu board relay active-high, cần chỉnh logic trong firmware.

## 4) Chân cần tránh/lưu ý
- GPIO12 là chân strapping boot trên ESP32: tránh kéo mức sai khi reset.
- Với tải công suất cao, cần mạch bảo vệ (opto, snubber, cầu chì tùy tải).
