# Checklist kiểm thử SmartGarden

## A. Build
- [ ] `pio run -e esp32dev` thành công.
- [ ] Không còn lỗi linker kiểu multiple definition MQTTService.

## B. Wi-Fi và MQTT
- [ ] ESP32 vào Wi-Fi đúng SSID.
- [ ] MQTT connect thành công, publish `smartgarden/availability=online`.
- [ ] Mất kết nối MQTT thì LWT báo `offline`.

## C. Discovery Home Assistant
- [ ] Nhận đủ discovery ở `homeassistant/#`.
- [ ] Tạo đủ entity: 9 sensor + 8 switch + 2 select.

## D. Manual mode
- [ ] Set `smartgarden/mode/set=manual`.
- [ ] Gửi `smartgarden/relay/1/set=ON|OFF` và relay đổi trạng thái đúng.

## E. Auto mode
- [ ] Set `smartgarden/mode/set=auto`.
- [ ] Lệnh relay thủ công bị bỏ qua.
- [ ] Relay climate thay đổi theo DHT22/crop profile.

## F. Monitor mode
- [ ] Set `smartgarden/mode/set=monitor`.
- [ ] Tất cả relay tắt.
- [ ] Mọi lệnh relay thủ công bị bỏ qua.

## G. DHT22
- [ ] `smartgarden/sensors/air_temp` có dữ liệu hợp lệ.
- [ ] `smartgarden/sensors/air_humidity` có dữ liệu hợp lệ.

## H. RS485/soil/NPK (giới hạn hiện tại)
- [ ] Xác minh thực địa phản hồi Modbus đúng sensor đang dùng.
- [ ] Nếu chưa có dữ liệu RS485 hợp lệ: auto irrigation KHÔNG tự bật.
- [ ] Chỉ đánh dấu “hoạt động” sau khi test phần cứng thật (địa chỉ/baud/register/khung).
