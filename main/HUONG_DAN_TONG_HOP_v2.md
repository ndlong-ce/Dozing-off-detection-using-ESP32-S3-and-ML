# ESP32-S3 giám sát ngủ gật: hướng dẫn tổng hợp (v2)

ESP-IDF 5.5.5, build / nạp / xem log bằng **PowerShell**. Firmware gồm: camera GC2145 → tìm mặt (ESP-DL) → CNN mắt mở/nhắm → LED GPIO2 + buzzer GPIO1, kèm WiFi và web xem camera trực tiếp.

> **Quy ước:** dán **từng lệnh một**, chờ xong mới dán lệnh tiếp. (Từng gặp: hai lệnh dính nhau làm `idf.py` báo `No such option: -R`.)
>
> **Thay đổi so với bản v1:** `main.cpp` đã được **tách thành nhiều file**. Mọi cấu hình (WiFi, GPIO, ngưỡng) nằm trong **`main\config.h`**, **không còn nằm trong `main.cpp`**. Các lệnh đổi WiFi cũ nhắm vào `main.cpp` sẽ **không có tác dụng** (xem mục 6 và mục 10).

---

## Mục lục

0. Trạng thái hiện tại
1. Thông tin cố định
2. Cấu trúc thư mục và nội dung file
3. Mỗi lần mở máy: kích hoạt ESP-IDF
4. Kiểm tra file project
5. Build
6. Đổi WiFi, GPIO, ngưỡng (sửa `config.h`)
7. Nạp và xem log
8. Xem camera trên web
9. LED và buzzer: các chân hoạt động thế nào
10. Sự cố WiFi (đọc theo dấu hiệu trong log)
11. Các sự cố khác
12. Chất lượng nhận diện mắt nhắm và hướng cải thiện
13. Việc còn lại
14. Sao lưu và tiếp tục ở chat mới

---

## 0. Trạng thái hiện tại

**Đã kiểm chứng (bằng log hoặc ảnh):**
- Build thành công cả bản tách file (`eye_blink_idf.bin` 0x2ce160 byte, partition 6 MB còn trống 53%) và nạp thành công trên `COM3`.
- Flash 16 MB, PSRAM 8 MB octal 80 MHz chạy tốt.
- Camera GC2145 nhận được (SCCB 0x3C), hình đúng màu, không nhiễu; khoảng 7 FPS khi chưa có mặt, 5,7 - 5,8 FPS khi đang chạy CNN.
- Bộ dò mặt nhận mặt sau khi đổi sang `DL_IMAGE_PIX_TYPE_RGB565BE` (khung vàng quanh mặt, hai ô vuông ở hai mắt).
- Log in giá trị hệ số lượng tử đúng: `input exp=-7  output exp=-5` (lỗi in `exp` rác trước đây chỉ là lỗi log).
- WiFi `P1119` từng kết nối được và trang web mở được; WiFi fallback `EyeCam-ESP32` hoạt động.
- LED GPIO2 sáng khi nhắm mắt (bạn xác nhận).

**Đang gặp:**
- WiFi `Kayungg` hiện **không kết nối được**: log cho thấy board **không thấy mạng này** (xem mục 10, dấu hiệu A). Lệnh đổi WiFi bạn đã chạy nhắm vào `main.cpp` nên không có tác dụng, firmware đang dùng giá trị trong `config.h`.

**Chưa kiểm chứng:**
- Buzzer (code đã có, kêu liên tục một âm khi LED sáng quá 2 giây): chưa có báo cáo thử trên mạch.
- Độ chính xác nhận diện mắt nhắm còn thấp (mục 12).

---

## 1. Thông tin cố định

| Mục | Giá trị |
|---|---|
| Thư mục project | `D:\VisualStudioCode\Embedded_dozing_off\eye_blink_idf` |
| ESP-IDF | `C:\esp\v5.5.5\esp-idf` |
| Python env | `C:\Users\ASUS\.espressif\python_env\idf5.5_py3.13_env` (Python 3.13.2) |
| Script kích hoạt | `D:\VisualStudioCode\Embedded_dozing_off\idf_env.ps1` |
| Chip | ESP32-S3 (QFN56, rev v0.2) |
| Board thật | Nhãn "N8R16" nhưng thực tế **flash 16 MB (Boya) + PSRAM 8 MB octal** (như N16R8) |
| Cổng nạp/log | `COM3` = USB-Enhanced-SERIAL **CH343** (cầu UART) |
| Camera | GC2145 |
| Component (khóa trong `dependencies.lock`) | esp-dl 3.3.12, human_face_detect 0.5.0, esp32-camera 2.1.8, esp_jpeg 1.3.1, dl_fft 0.7.0, esp_new_jpeg 1.0.2 |
| WiFi | đặt trong `main\config.h` (tên và mật khẩu hiện có trong file đó; không chia sẻ file này) |
| WiFi fallback của board | `EyeCam-ESP32` / `12345678` / `http://192.168.4.1` |

**Chân GPIO:**

| Chân | Dùng cho |
|---|---|
| GPIO2 | LED cảnh báo (qua điện trở 220 Ω), sáng khi mức HIGH |
| GPIO1 | chân **I/O** của buzzer (kêu khi mức LOW, vì module kích mức thấp) |
| 3V3 | VCC buzzer |
| GND | GND buzzer |

**Pinout camera (đã chạy đúng trên board này):**

```
XCLK=15  SIOD=4  SIOC=5  VSYNC=6  HREF=7  PCLK=13
Y2=11  Y3=9  Y4=8  Y5=10  Y6=12  Y7=18  Y8=17  Y9=16
```

Các file Arduino cũ (`app_httpd`, `camera_index`, `cam_esp32.ino`...) **không dùng nữa**. Chỉ dùng một firmware ESP-IDF.

---

## 2. Cấu trúc thư mục và nội dung file

```
eye_blink_idf\
├─ CMakeLists.txt            (project)
├─ sdkconfig.defaults
├─ partitions.csv
├─ dependencies.lock         (tự sinh, GIỮ LẠI)
└─ main\
   ├─ CMakeLists.txt         (main, phải liệt kê đủ file .cpp)
   ├─ idf_component.yml
   ├─ eye_cnn.espdl          (model 15392 byte, PHẢI SAO LƯU)
   ├─ config.h               ← MỌI CẤU HÌNH Ở ĐÂY
   ├─ main.cpp               vòng lặp chính
   ├─ camera.h / .cpp        khởi tạo camera
   ├─ eye_model.h / .cpp     CNN mắt (lớp EyeClassifier)
   ├─ eye_monitor.h / .cpp   tìm mặt + CNN 2 mắt mỗi khung
   ├─ alarm_logic.h / .cpp   đếm khung nhắm, hẹn giờ buzzer
   ├─ led.h / .cpp           LED GPIO2
   ├─ buzzer.h / .cpp        buzzer GPIO1 (task riêng)
   ├─ overlay.h / .cpp       vẽ khung lên ảnh
   ├─ shared.h / .cpp        dữ liệu dùng chung với web
   ├─ wifi_mgr.h / .cpp      WiFi (kết nối, fallback AP)
   ├─ web_server.h / .cpp    web server (cổng 80 và 81)
   └─ web_page.h             HTML + JS của trang web
```

Hai file `CMakeLists.txt` cùng tên nhưng nằm ở hai thư mục khác nhau, **không ghi đè lẫn nhau**.

### `CMakeLists.txt` (thư mục gốc)
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(eye_blink_idf)
```

### `main\CMakeLists.txt`
```cmake
idf_component_register(
    SRCS
        "main.cpp"
        "shared.cpp"
        "wifi_mgr.cpp"
        "web_server.cpp"
        "camera.cpp"
        "eye_model.cpp"
        "eye_monitor.cpp"
        "overlay.cpp"
        "buzzer.cpp"
        "led.cpp"
        "alarm_logic.cpp"
    INCLUDE_DIRS "."
    EMBED_FILES "eye_cnn.espdl"
    PRIV_REQUIRES driver esp_http_server esp_wifi esp_netif esp_event nvs_flash esp_timer
)
```
Thêm file `.cpp` mới thì **phải thêm vào danh sách `SRCS`**, nếu không build báo `undefined reference`.

### `main\idf_component.yml`
```yaml
dependencies:
  idf: ">=5.3"
  espressif/esp-dl: ">=3.2.0"
  espressif/human_face_detect: "*"
  espressif/esp32-camera: "*"
```

### `partitions.csv`
```
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x6000,
phy_init, data, phy,     0xf000,   0x1000,
factory,  app,  factory, 0x10000,  0x600000,
```

### `sdkconfig.defaults` (flash **16MB**)
```
CONFIG_IDF_TARGET="esp32s3"

# Flash 16MB, PSRAM octal 8MB
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y

# CPU + cache
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_ESP32S3_DATA_CACHE_64KB=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y

# Stack cho app_main (face detect + JPEG)
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384

# 2 web server (cong 80 + 81)
CONFIG_LWIP_MAX_SOCKETS=16

# Partition 6MB cho firmware
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
```

> Flash size từng bị đặt nhầm `8MB`. Bản đúng là **16MB**. Log boot có dòng `Detected size(16384k) larger than the size in the binary image header(8192k)` nghĩa là file đang sai.

### Các thông số trong `main\config.h`

| Định nghĩa | Ý nghĩa | Mặc định |
|---|---|---|
| `WIFI_SSID`, `WIFI_PASS` | WiFi để kết nối | (xem file) |
| `AP_SSID`, `AP_PASS` | WiFi board tự phát khi không vào được WiFi | `EyeCam-ESP32` / `12345678` |
| `LED_GPIO`, `LED_ON_LEVEL` | LED, mức HIGH là sáng | GPIO2, 1 |
| `BUZZER_GPIO` | chân I/O buzzer | GPIO1 |
| `BUZZER_ON_LEVEL` | mức kêu: 0 = LOW kêu (module kích mức thấp) | 0 |
| `BUZZER_ACTIVE` | 1 = buzzer chủ động (chỉ bật/tắt); 0 = thụ động (phát đúng tần số bằng PWM) | 1 |
| `BUZZER_DELAY_MS` | LED sáng liên tục bao lâu thì buzzer mới kêu | 2000 |
| `BUZZER_FREQ_HZ` | tần số tiếng kêu (chỉ có tác dụng khi `BUZZER_ACTIVE 0`) | 1000 |
| `BUZZER_SELFTEST` | 1 = buzzer kêu 0,4 giây khi khởi động để kiểm tra phần cứng | 1 |
| `CLOSED_FRAMES` | số khung nhắm liên tiếp mới bật LED | 3 |
| `CLOSED_THRESH` | `P(closed)` lớn hơn giá trị này thì tính là nhắm | 0.15 |
| `EYE_SIDE_RATIO` | cạnh ô crop = tỉ lệ × khoảng cách hai mắt | 0.60 |
| `IMG_SIZE` | cạnh ô mắt đưa vào CNN | 32 |
| `JPEG_QUALITY` | chất lượng ảnh stream (1 - 100) | 60 |

---

## 3. Mỗi lần mở máy: kích hoạt ESP-IDF

Mở **Windows PowerShell**, dán từng lệnh:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
```
```powershell
. "D:\VisualStudioCode\Embedded_dozing_off\idf_env.ps1"
```
```powershell
cd "D:\VisualStudioCode\Embedded_dozing_off\eye_blink_idf"
```

**Kết quả mong đợi (cuối):** `Done! You can now compile ESP-IDF projects.`

Nếu file `idf_env.ps1` bị mất, tạo lại:

```powershell
Set-Content -Path "D:\VisualStudioCode\Embedded_dozing_off\idf_env.ps1" -Value @'
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
$env:IDF_PATH = "C:\esp\v5.5.5\esp-idf"
$env:IDF_PYTHON_ENV_PATH = "C:\Users\ASUS\.espressif\python_env\idf5.5_py3.13_env"
& "$env:IDF_PATH\export.ps1"
'@
```

| Triệu chứng | Nguyên nhân | Cách xử lý |
|---|---|---|
| `running scripts is disabled on this system` | Chính sách PowerShell | Chạy lệnh `Set-ExecutionPolicy -Scope Process ...` ở trên (chỉ có hiệu lực trong cửa sổ này) |
| `Cannot find path 'C:\Espressif\frameworks'` | ESP-IDF **không** nằm ở đó | Dùng `C:\esp\v5.5.5\esp-idf` |
| Báo thiếu tool hoặc thiếu Python env | Chưa cài đủ | `& "$env:IDF_PATH\install.ps1" esp32s3` (cần Internet) rồi chạy lại `idf_env.ps1` |
| `idf.py --version` in `v1.0.3` | Đó là bản của wrapper `idf.py.exe`, không phải phiên bản IDF | Kiểm tra: `python "$env:IDF_PATH\tools\idf.py" --version` (phải ra `ESP-IDF v5.5.5`) |

---

## 4. Kiểm tra file project

```powershell
"CMakeLists.txt","sdkconfig.defaults","partitions.csv","main\CMakeLists.txt","main\idf_component.yml","main\eye_cnn.espdl","main\config.h","main\main.cpp" | ForEach-Object { "{0,-26} {1}" -f $_, (Test-Path $_) }
```

**Kết quả mong đợi:** 8 dòng `True`. `main\eye_cnn.espdl` là `False` thì phải khôi phục từ bản sao lưu (model đã huấn luyện không tạo lại được từ tài liệu này).

Kiểm tra mọi file `.cpp` đã nằm trong `main\CMakeLists.txt`:

```powershell
Get-ChildItem main\*.cpp | ForEach-Object { "{0,-20} {1}" -f $_.Name, ((Get-Content main\CMakeLists.txt -Raw) -match [regex]::Escape('"' + $_.Name + '"')) }
```

**Kết quả mong đợi:** 11 dòng `True`. Có `False` thì thêm tên file đó vào `SRCS`.

Kiểm tra cấu hình quan trọng:

```powershell
Select-String -Path sdkconfig.defaults -Pattern "FLASHSIZE"
```
Mong đợi: `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`.

```powershell
Select-String -Path main\config.h -Pattern '#define\s+(WIFI_SSID|CLOSED_THRESH|BUZZER_ACTIVE|BUZZER_FREQ_HZ)'
```
Mong đợi: 4 dòng.

---

## 5. Build

**Lần đầu, hoặc khi đổi `sdkconfig.defaults`** (xóa `build\` và `sdkconfig` cũ, tạo lại từ `sdkconfig.defaults`):

```powershell
idf.py set-target esp32s3
```
Mong đợi: `Build files have been written to: .../build`.

**Build:**

```powershell
idf.py build
```
Mong đợi (cuối): `Project build complete. To flash, run:` kèm dòng `eye_blink_idf.bin binary size ... (53%) free`.

Khi chỉ sửa `config.h` hoặc file `.cpp`, chỉ cần `idf.py build` (không cần `set-target`).

**Lưu log để gửi khi có lỗi:**

```powershell
idf.py build 2>&1 | Tee-Object -FilePath build_log.txt
```
```powershell
Select-String -Path build_log.txt -Pattern "error|CMake Error|FAILED" | Select-Object -First 10 | ForEach-Object { $_.Line }
```

**Nếu build lỗi:** dừng ở **lỗi đầu tiên có ý nghĩa** (dòng `error:` hoặc `CMake Error` đầu tiên, không phải dòng cuối) và sửa từng lỗi một.

| Lỗi | Nguyên nhân thường gặp |
|---|---|
| `undefined reference to ...` | Một file `.cpp` chưa nằm trong `SRCS` của `main\CMakeLists.txt` |
| `fatal error: xxx.h: No such file or directory` | Thiếu file đó trong `main\`, hoặc thiếu `idf_component.yml` / chưa tải component (cần Internet) |
| `'BUZZER_FREQ_HZ' was not declared` | `config.h` là bản cũ, chưa có dòng này |
| Khác tên trường API của esp-dl | Gửi phiên bản từ `dependencies.lock` và đoạn lỗi |

---

## 6. Đổi WiFi, GPIO, ngưỡng (sửa `main\config.h`)

> **Quan trọng:** các định nghĩa này nằm trong **`main\config.h`**. Lệnh nhắm vào `main\main.cpp` sẽ không tìm thấy gì (và `Select-String` in ra rỗng), firmware vẫn dùng giá trị cũ trong `config.h`.

Dùng .NET để đọc/ghi (giữ UTF-8). Thay `TEN_WIFI` và `MAT_KHAU`; **không dùng ký tự `"` trong hai giá trị này**:

```powershell
$p = (Resolve-Path main\config.h).Path
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)
$t = $t -replace '(#define\s+WIFI_SSID\s+)"[^"]*"', '$1"TEN_WIFI"'
$t = $t -replace '(#define\s+WIFI_PASS\s+)"[^"]*"', '$1"MAT_KHAU"'
[System.IO.File]::WriteAllText($p, $t, (New-Object System.Text.UTF8Encoding($false)))
Select-String -Path main\config.h -Pattern '#define\s+WIFI_(SSID|PASS)'
```

**Kết quả mong đợi:** đúng 2 dòng `WIFI_SSID` và `WIFI_PASS` hiện tên và mật khẩu bạn vừa đặt.
- Kết quả **rỗng**: file không có các định nghĩa này (đang chạy trên sai file hoặc sai thư mục). Kiểm tra `Get-Location` phải là thư mục project.
- Mật khẩu có ký tự `$`: sửa tay bằng Notepad (`notepad main\config.h`), tránh dùng lệnh trên.

Sau khi sửa phải **build và nạp lại** (mục 5 và 7).

Đổi ngưỡng nhận diện tương tự, ví dụ đặt `CLOSED_THRESH` thành 0.20:

```powershell
$p = (Resolve-Path main\config.h).Path
$t = [System.IO.File]::ReadAllText($p, [System.Text.Encoding]::UTF8)
$t = $t -replace '(#define\s+CLOSED_THRESH\s+)[0-9.]+f', '${1}0.20f'
[System.IO.File]::WriteAllText($p, $t, (New-Object System.Text.UTF8Encoding($false)))
Select-String -Path main\config.h -Pattern '#define\s+CLOSED_THRESH'
```

> Lưu ý: chữ "Mắt đang nhắm..." trên trang web dùng ngưỡng **cố định 0.15** viết trong `main\web_page.h` (tìm `j.p_closed>0.15`), không đọc từ `config.h`. LED và buzzer thì luôn theo `CLOSED_THRESH` trong `config.h`.

---

## 7. Nạp và xem log

### 7.1. Tìm cổng COM
Cắm cáp vào **cổng UART** của board (cổng có chip CH343), rồi:

```powershell
Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM\d+\)' } | Select-Object Name, DeviceID
```

| Trong DeviceID | Cổng là | Dùng để |
|---|---|---|
| `VID_1A86` (CH340/CH343) hoặc `VID_10C4` (CP210x) | cầu UART | **nạp và xem log** (khuyên dùng) |
| `VID_303A&PID_1001` | USB gốc của ESP32-S3 | nạp được, nhưng log mặc định **không hiện** (console đặt ở UART0) |

Lần trước cổng là `COM3`; số cổng có thể đổi khi cắm lại.

### 7.2. Kiểm tra flash thật của board (tùy chọn)
```powershell
python -m esptool --chip esp32s3 -p COM3 flash_id
```
Mong đợi: `Chip is ESP32-S3`, `Detected flash size: 16MB`.

### 7.3. Nạp và mở monitor
```powershell
idf.py -p COM3 flash monitor
```
Thoát monitor bằng **`Ctrl + ]`**.

**Khi nạp:** `Hash of data verified.` ở cả 3 file (bootloader, partition table, app), rồi `Hard resetting via RTS pin...`.

**Log boot đúng cần có:**
- `SPI Flash Size : 16MB` (không còn cảnh báo flash lớn hơn cấu hình).
- `Found 8MB PSRAM device`, `Adding pool of 8192K of PSRAM memory to heap allocator`.
- `Detected GC2145 camera` và `Detected camera at address=0x3c`.
- `input exp=-7  output exp=-5` (số nhỏ, không phải số lớn kỳ lạ).
- Nếu WiFi nối được: `=== Mo trinh duyet: http://192.168.x.x ===`.
- Sau đó là dòng lặp `face=...  P(closed)=...  cnt=...  LED=...  ALM=...ms  BUZ=...  fps=...` (`ALM` là số ms báo động liên tục, buzzer chỉ kêu khi `ALM` đạt 2000); khi có mặt thì thêm dòng `eye pl=... pr=...  logits L=[...] R=[...]`.

Các dòng này **bình thường**: `cam_hal: EV-EOF-OVF` một lần lúc khởi động; `W wifi:Password length matches WPA2 standards`; `W FbsLoader: ... not aligned with 16 bytes`; `W dl::Model: minimize()...`.

### Sự cố khi nạp

| Triệu chứng | Cách xử lý |
|---|---|
| `Could not open COMx, the port is busy or doesn't exist` | Sai số cổng (xem 7.1), hoặc một monitor khác đang mở cổng (thoát bằng `Ctrl + ]`) |
| `Failed to connect ... Wrong boot mode` | Giữ nút **BOOT**, nhấn và thả **RESET**, rồi thả BOOT, chạy lại lệnh nạp |
| `No such option: -R` hoặc lỗi lạ sau `monitor` | Hai lệnh dính nhau. Dán từng lệnh một |
| `PSRAM ID read error` | Chế độ PSRAM sai: board này là octal 80 MHz như trong `sdkconfig.defaults`; kiểm tra file rồi `idf.py set-target esp32s3` |
| Không thấy log sau khi nạp | Đang dùng cổng USB gốc (VID_303A); đổi sang cổng UART (CH343) |
| `Camera probe failed with error 0x105` | Không thấy camera: cắm lại cáp camera, kiểm tra nguồn và pinout (mục 1) |
| `task_wdt: ... IDLE0` mỗi 5 giây | Thiếu `vTaskDelay(1)` ở cuối vòng lặp trong `main.cpp` |

---

## 8. Xem camera trên web

**WiFi nối được:** tìm dòng `=== Mo trinh duyet: http://192.168.x.x ===` trong log, mở địa chỉ đó bằng máy **cùng WiFi**. Dùng `http://` (không phải `https://`); trình duyệt báo "Không bảo mật" là bình thường.

**WiFi không nối được:** board thử 10 lần rồi tự phát WiFi riêng (chờ khoảng 30 - 60 giây tùy trường hợp ở mục 10). Nối điện thoại hoặc máy tính vào WiFi **`EyeCam-ESP32`** (mật khẩu `12345678`), mở **`http://192.168.4.1`**.

Trang web hiện: hình camera (khung vàng quanh mặt, hai ô vuông ở hai mắt: xanh lá khi mở, đỏ khi nhắm, viền đỏ toàn khung khi báo động), dải trạng thái ("Không thấy khuôn mặt", "Mắt mở", "Mắt đang nhắm...", "NHẮM MẮT - CẢNH BÁO!"), dòng `P(closed)`, `cnt`, `FPS`, và dưới cùng là hai **ô mắt 32x32** mà CNN thực sự nhìn thấy (trái | phải).

| Triệu chứng | Cách xử lý |
|---|---|
| Không mở được trang | Kiểm tra máy cùng WiFi với board và dùng đúng IP trong log (IP có thể đổi mỗi lần khởi động) |
| Trang mở nhưng không có hình | Thử mở `http://IP:81/stream`; kiểm tra tường lửa chặn cổng 81; xem log có dòng `Khong start duoc web server cong 81` không |
| Chỉ một người xem được stream | Stream chiếm một luồng của server cổng 81: **đóng tab cũ trước khi mở tab mới** |
| Hình sai màu | Có thể do thứ tự byte RGB565 khi nén JPEG (chưa gặp trên board này) |
| Không thấy khung vàng dù có mặt | Kiểm tra `DL_IMAGE_PIX_TYPE_RGB565BE` trong `eye_monitor.cpp` (nếu là `RGB565LE` bộ dò mặt đọc sai màu và không nhận mặt) |

---

## 9. LED và buzzer: các chân hoạt động thế nào

### Trạng thái các chân

| Chân | Nối với | Bình thường | Khi báo động |
|---|---|---|---|
| **GPIO2** | LED (+ điện trở 220 Ω) | LOW, LED tắt | HIGH, LED sáng |
| **GPIO1** | chân I/O buzzer | HIGH, buzzer im | LOW liên tục, buzzer kêu |
| 3V3 | VCC buzzer | 3,3 V | không đổi |
| GND | GND buzzer | 0 V | không đổi |

### Diễn biến khi nhắm mắt

Mỗi khung hình (khoảng 6 - 7 khung/giây) code kiểm tra `P(closed) > CLOSED_THRESH` (hiện 0,15).

| Thời điểm | Điều gì xảy ra | LED (GPIO2) | Buzzer (GPIO1) |
|---|---|---|---|
| Mở mắt | `closed_cnt = 0` | LOW | HIGH |
| Bắt đầu nhắm | `closed_cnt` tăng 1 mỗi khung nhắm | LOW | HIGH |
| Sau **3 khung nhắm liên tiếp** (khoảng 0,5 giây) | `alarm = true`, bắt đầu hẹn giờ | **HIGH** | HIGH |
| Alarm liên tục **≥ 2 giây** (`BUZZER_DELAY_MS`) | buzzer được yêu cầu kêu | HIGH | **LOW (kêu liên tục)** |
| Chỉ cần **một khung** không thỏa (mắt mở hoặc mất mặt) | `closed_cnt = 0`, hẹn giờ về 0 | **LOW ngay** | **HIGH trong khoảng 10 ms** |

Hệ quả cần biết: vì CNN còn yếu, `P(closed)` có thể dao động quanh ngưỡng. Khi đó LED chớp và buzzer ít khi kịp kêu đủ 2 giây liên tục.

### Loại buzzer

- **Chủ động** (`BUZZER_ACTIVE 1`, đang đặt mặc định): chỉ bật/tắt, **tần số do phần cứng quyết định, không chỉnh được**. GPIO1 chỉ có hai mức.
- **Thụ động** (`BUZZER_ACTIVE 0`): phát xung vuông 50% đúng `BUZZER_FREQ_HZ` (1000 Hz) bằng LEDC timer 1 / channel 1; lúc im GPIO1 giữ HIGH. Muốn tiếng đúng 1000 Hz cần buzzer thụ động.

### Tách lỗi phần cứng khỏi lỗi logic khi buzzer không kêu

Firmware có hai công cụ kiểm tra **không phụ thuộc nhận diện mắt**:
- **Kêu thử lúc khởi động:** nếu `BUZZER_SELFTEST 1`, buzzer kêu 0,4 giây ngay sau khi cấp nguồn.
- **Trang `/beep`:** mở `http://<IP>/beep` bằng trình duyệt thì buzzer kêu 1,5 giây.

Đọc kết quả:

| Quan sát | Kết luận | Làm gì |
|---|---|---|
| Kêu được khi khởi động hoặc `/beep`, nhưng nhắm mắt không kêu | Phần cứng ổn, lỗi ở **logic** | Xem dòng log `ALM=...ms`: nếu không bao giờ đạt 2000 thì LED chớp (CNN dao động quanh ngưỡng) nên bộ hẹn giờ 2 giây liên tục bị đặt lại; xem mục 12 |
| `ALM` đạt 2000, log có `BUZ=1` nhưng không nghe tiếng | Lỗi phần cứng hoặc mức kích | Xem các dòng bên dưới |
| Không kêu cả khi khởi động lẫn `/beep` | Phần cứng / cấu hình buzzer | Xem các dòng bên dưới |

Khi `/beep` cũng không kêu, thử lần lượt:
1. **Thử tay:** rút dây I/O khỏi GPIO1 và chạm đầu dây I/O vào **GND**. Kêu thì module kích mức thấp (đúng với `BUZZER_ON_LEVEL 0`). Chỉ kêu khi chạm vào **VCC (3V3)** thì đổi `BUZZER_ON_LEVEL` thành 1. Không kêu ở cả hai thì kiểm tra dây VCC/GND hoặc module hỏng.
2. **Buzzer thụ động:** cấp điện một chiều (mức LOW cố định) thì buzzer thụ động **im lặng** (chỉ có tiếng "tạch" lúc đóng/ngắt). Đổi `BUZZER_ACTIVE` thành 0 để phát xung 1000 Hz.
3. **Đo GPIO1** bằng đồng hồ vạn năng: khoảng 3,3 V lúc im, xuống khoảng 0 V khi `/beep`. Không thay đổi thì GPIO1 không tới được module (dây lỏng, hoặc chân bị mạch khác trên board kéo), khi đó đổi chân (xem dưới).

**Đổi chân buzzer sang GPIO42** (chỉ khi kiểm tra cho thấy GPIO1 có vấn đề): sửa `BUZZER_GPIO` trong `main\config.h` thành `GPIO_NUM_42`, build, nạp, và **chuyển dây I/O sang chân 42**. Lưu ý chân 39 - 42 là chân JTAG mặc định và một số board ESP32-S3-CAM dùng 38 - 40 cho khe thẻ nhớ, nên GPIO42 không chắc tốt hơn GPIO1; cần đối chiếu sơ đồ board nếu có.

### Đấu dây và kiểm tra

Giữ nguồn **3,3 V** như đang nối: module kích mức thấp cấp 5 V với tín hiệu 3,3 V có thể bị kêu liên tục (suy luận từ kiểu mạch thường gặp, chưa kiểm chứng với module của bạn).

Cách thử:
1. Mở mắt: buzzer im.
2. Nhắm đủ lâu để LED sáng, giữ thêm 2 giây: buzzer kêu liên tục.
3. Mở mắt: buzzer tắt ngay.

Đo bằng đồng hồ vạn năng: GPIO2 khoảng 3,3 V khi LED sáng; GPIO1 xuống khoảng 0 V khi buzzer kêu và về khoảng 3,3 V khi im.

| Triệu chứng | Cách xử lý |
|---|---|
| Buzzer kêu liên tục lúc bình thường | Đổi `BUZZER_ON_LEVEL` thành 1 trong `config.h` (mục 6), build và nạp lại |
| Log có `BUZ=1` nhưng buzzer không kêu | Kiểm tra dây (I/O → GPIO1, VCC → 3V3, GND → GND), thử đổi `BUZZER_ON_LEVEL`; nếu chỉ nghe tiếng "tạch" nhỏ thì là buzzer thụ động, đổi `BUZZER_ACTIVE` thành 0 |
| Bật nguồn có tiếng "tách" ngắn | Bình thường: trong khoảng 1 giây bootloader chạy, GPIO1 chưa được firmware điều khiển |
| LED không sáng dù web báo CẢNH BÁO | Đổi `LED_ON_LEVEL` thành 0, hoặc kiểm tra mắc LED (đúng chiều, điện trở 220 Ω) |

---

## 10. Sự cố WiFi (đọc theo dấu hiệu trong log)

Hai kiểu lỗi trông giống nhau (đều thử lại 10 lần rồi chuyển sang `EyeCam-ESP32`) nhưng **log khác nhau**, và nguyên nhân khác nhau:

| | **A. Board không thấy mạng** | **B. Thấy mạng nhưng bắt tay thất bại** |
|---|---|---|
| Dòng `wifi:state: init -> auth`, `auth -> assoc`, `assoc -> run` | **Không có** | **Có**, lặp lại mỗi lần thử |
| Dòng `wifi:new:<kênh>,...` | Không có | Có (cho biết kênh) |
| Khoảng cách giữa các lần thử lại | khoảng **2,4 giây** | khoảng **5,5 giây** |
| Thời điểm chuyển sang AP riêng | khoảng **28 giây** | khoảng **55 giây** |
| Nguyên nhân thường gặp | Sai tên WiFi (hoa/thường, khoảng trắng), mạng chỉ phát **5 GHz**, hotspot ẩn/tắt, ngoài tầm | **Sai mật khẩu**; mạng doanh nghiệp / có trang đăng nhập; router chỉ WPA3 hoặc lọc MAC |

Lần gần nhất (WiFi `Kayungg`) là **kiểu A**: log không có dòng `wifi:state`, thử lại cách nhau khoảng 2,4 giây, chuyển AP ở giây thứ 28.

### 10.1. Xử lý kiểu A (board không thấy mạng)

Mở PowerShell (không cần kích hoạt ESP-IDF) và xem máy tính có thấy mạng này không, kèm băng tần:

```powershell
netsh wlan show networks mode=bssid
```

Tìm khối có `SSID n : <tên mạng>` và đọc các dòng dưới nó:

| Dòng | Điều cần có |
|---|---|
| `SSID` | Giống **từng ký tự** với `WIFI_SSID` trong `config.h` (hoa/thường, khoảng trắng, dấu) |
| `Channel` | **1 - 13** (băng 2.4 GHz). Số 36 trở lên là 5 GHz, ESP32-S3 **không thấy** |
| `Radio type` | `802.11n` hoặc `802.11g` / `802.11b` là 2.4 GHz; `802.11ac` / `802.11ax` thường là 5 GHz |
| `Authentication` | `WPA2-Personal` (ổn); `WPA3-Personal` thì ESP cần chỉnh thêm; `WPA2-Enterprise` hoặc `Open` thì không nối trực tiếp được |

- **Không thấy mạng trong danh sách:** mạng chưa phát hoặc bạn gõ sai tên. Nếu là hotspot điện thoại: bật hotspot, để màn hình sáng, và chọn băng tần **2.4 GHz** (iPhone: bật "Tương thích tối đa").
- **Thấy mạng nhưng chỉ ở 5 GHz:** đổi router/hotspot sang 2.4 GHz, hoặc dùng tên mạng 2.4 GHz riêng nếu router có.
- **Thấy mạng ở 2.4 GHz và tên khớp:** sửa `config.h` cho khớp (mục 6), build và nạp lại, rồi xem log.

### 10.2. Xử lý kiểu B (thấy mạng nhưng bắt tay thất bại)

Nguyên nhân phổ biến nhất là **sai mật khẩu** (lần trước thiếu một chữ số). Kiểm tra bằng Windows, nếu máy từng kết nối mạng đó:

```powershell
netsh wlan show profile name="TEN_WIFI" key=clear
```

Đọc ba dòng **Authentication**, **Cipher**, **Key Content** (nếu không hiện Key Content, mở PowerShell bằng quyền Administrator và chạy lại), rồi so Key Content với `WIFI_PASS` trong `config.h`.

| Kết quả | Làm gì |
|---|---|
| `WPA2-Personal`, `CCMP`, Key Content **giống** `WIFI_PASS` | Có thể router lọc MAC hoặc giới hạn thiết bị; thử hotspot điện thoại |
| Key Content **khác** | Sửa mật khẩu trong `config.h` (mục 6) |
| `WPA2-Enterprise` hoặc có trang đăng nhập (WiFi trường, ký túc xá) | ESP32 không nối trực tiếp được → dùng hotspot điện thoại |
| `WPA3-Personal` | Báo lại để chỉnh firmware |

### 10.3. Cách kiểm tra chắc chắn: hotspot điện thoại

1. Bật hotspot **2.4 GHz** (WPA2, mật khẩu từ 8 ký tự).
2. Sửa `WIFI_SSID`, `WIFI_PASS` trong `config.h` (mục 6), build và nạp lại.
3. Log có `wifi:connected with ...` và `=== Mo trinh duyet: http://192.168.x.x ===` thì mở địa chỉ đó bằng máy đang nối chung hotspot.
4. Hotspot cũng không nối được thì lỗi nằm ở firmware: gửi log.

Trong lúc chưa nối được WiFi, vẫn dùng bình thường bằng WiFi riêng `EyeCam-ESP32` (mục 8).

---

## 11. Các sự cố khác (đã gặp)

| Triệu chứng | Nguyên nhân | Cách xử lý |
|---|---|---|
| `task_wdt: IDLE0` mỗi 5 giây và `cam_hal: FB-SIZE` | Vòng lặp chính không nhường CPU | Giữ `vTaskDelay(1)` ở cuối vòng lặp trong `main.cpp` |
| Bộ dò mặt không nhận mặt dù hình rõ, sáng, đủ xa | Khai báo sai thứ tự byte ảnh | Dùng `DL_IMAGE_PIX_TYPE_RGB565BE` (không phải `RGB565LE`) |
| Log `input exp=1070290604 ...` | Lỗi in log (truyền cả lớp `ExponentInfo` vào `%d`) | In bằng `get_exponent()` (đã sửa; log hiện `exp=-7`, `exp=-5`) |
| Cảnh báo flash `Detected size(16384k) larger than...` | `sdkconfig.defaults` đang để 8MB | Đặt `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, rồi `idf.py set-target esp32s3` |
| Lệnh đổi WiFi chạy nhưng không có tác dụng, `Select-String` rỗng | Sau khi tách file, các định nghĩa nằm ở `config.h` chứ không ở `main.cpp` | Sửa `main\config.h` (mục 6) |
| PowerShell 5.1 làm hỏng chữ tiếng Việt trong file | `Get-Content`/`Set-Content` mặc định không dùng UTF-8 | Dùng `[System.IO.File]::ReadAllText/WriteAllText` với UTF-8 như ở mục 6 |

---

## 12. Chất lượng nhận diện mắt nhắm và hướng cải thiện

### Số liệu hiện tại
Mắt mở: `P(closed)` khoảng 0,02 - 0,04. Mắt nhắm: chỉ khoảng 0,04 - 0,36 (có mẫu 0,36; 0,15; 0,04). Hai vùng gần như chồng lấn, nên ngưỡng 0,15 chỉ là mức tạm chấp nhận được; CNN chưa tin cậy với ảnh webcam.

### Giả thuyết nguyên nhân (chưa kiểm chứng)
1. **Lệch miền dữ liệu:** MRL là ảnh hồng ngoại; webcam là ảnh màu chuyển sang xám.
2. **Độ phân giải ô mắt thấp:** crop khoảng 20 - 25 điểm ảnh gốc ở QVGA rồi phóng lên 32x32 nên bị mờ.
3. **Lệch tiền xử lý hoặc lượng tử hóa INT8:** cần kiểm tra file huấn luyện/xuất model.
4. **Khung crop khác dữ liệu huấn luyện.**

### Các hướng thử (từ dễ đến khó)

| # | Hướng | Công sức | Ghi chú |
|---|---|---|---|
| A | Hậu xử lý: dùng chênh lệch logit thay vì xác suất, làm mượt theo thời gian (EMA), hiệu chuẩn nền theo người dùng lúc khởi động, dung sai ngắn trước khi coi là mở mắt | Thấp, không cần huấn luyện lại | Giải quyết LED chớp và buzzer ít kịp kêu |
| B | **Thu ảnh webcam thật** rồi huấn luyện lại (kết hợp MRL + dữ liệu của bạn, có tăng cường dữ liệu, lượng tử hóa bằng dữ liệu webcam) | Trung bình | Đáng tin nhất vì khớp đúng cách firmware cắt ô mắt |
| C | Chuẩn hóa độ tương phản ô mắt, hoặc crop từ ảnh độ phân giải cao hơn (VGA) | Trung bình | Nên đánh giá offline trên dữ liệu thu được trước khi đổi firmware |
| D | Thêm đặc trưng cổ điển (vùng đồng tử tối, đường viền mí) làm tín hiệu phụ | Trung bình | Nhạy với ánh sáng |
| E | EAR / điểm mốc khuôn mặt | Cao | `human_face_detect` chỉ cho 5 điểm, không đủ tính EAR |
| F | Chiếu sáng hồng ngoại | Phần cứng | Camera GC2145 có kính lọc IR |

### Thu dữ liệu bằng `collect_eyes.py` (hướng B)

Script chỉ dùng thư viện có sẵn của Python, chạy trên Windows PowerShell trong khi firmware đang chạy và mặt ở trước camera. Đóng tab web cũ để đỡ tranh tài nguyên server.

```powershell
python collect_eyes.py 192.168.1.10 eye_data
```
(thay `192.168.1.10` bằng IP trong log, hoặc `192.168.4.1` khi dùng WiFi `EyeCam-ESP32`.)

Phím: `o` = đang mở mắt, `c` = đang nhắm mắt, `n` = tạm dừng, `q` = thoát. Mỗi ảnh là BMP 64x32 gồm hai ô mắt [trái | phải], lưu kèm `labels.csv`. Chỉ lưu khi firmware đang thấy mặt và ảnh mới khác ảnh trước. Giữ **cả hai mắt cùng trạng thái**, không nháy một mắt.

Mỗi nhãn thu khoảng 1000 ảnh trở lên, thay đổi: góc đầu, khoảng cách, ánh sáng, nhìn xuống, (có/không kính). Có thêm người khác thì tốt hơn (một người dễ làm model quá khớp).

Để đề xuất thay đổi chính xác, cần gửi thêm: file huấn luyện (`main.py`), cách tiền xử lý ảnh khi huấn luyện, script xuất ONNX và lượng tử hóa bằng esp-ppq (kể cả dữ liệu hiệu chuẩn), cấu trúc model, và độ chính xác trên tập kiểm tra MRL.

---

## 13. Việc còn lại

1. **Sửa kết nối WiFi** (mục 10) hoặc dùng WiFi riêng `EyeCam-ESP32`.
2. **Kiểm tra buzzer** trên mạch (mục 9) và báo lại: có kêu không, kêu liên tục hay không.
3. **Cải thiện nhận diện mắt nhắm** (mục 12): trước mắt thu dữ liệu bằng `collect_eyes.py`; hậu xử lý (hướng A) có thể làm ngay.
4. Cảnh báo theo thời gian thực (khoảng 1 giây nhắm mắt) thay vì đếm khung.
5. Rút ngắn thời gian chờ WiFi (hiện 10 lần thử trước khi chuyển sang AP riêng).
6. Cảnh báo `FbsLoader: ... not aligned with 16 bytes`: xử lý sau khi CNN chạy đúng (có thể cần căn 16 byte cho file model nhúng).

---

## 14. Sao lưu và tiếp tục ở chat mới

Sao lưu project, bỏ qua thư mục sinh ra tự động (đổi đường dẫn đích theo ý bạn):

```powershell
robocopy "D:\VisualStudioCode\Embedded_dozing_off\eye_blink_idf" "D:\Backup\eye_blink_idf" /E /XD build managed_components .git
```

(Mã thoát dưới 8 là thành công.) **Phải giữ:** `main\eye_cnn.espdl`, toàn bộ file trong `main\` (kể cả `config.h` và `CMakeLists.txt`), `sdkconfig.defaults`, `partitions.csv`, `CMakeLists.txt` ở thư mục gốc, `dependencies.lock`, `idf_env.ps1`, và `collect_eyes.py`.

Có thể xóa: `build\`, `managed_components\` (tự tải lại khi build), `sdkconfig`, `sdkconfig.old`, `build_log.txt`.

**Khi sang chat mới**, gửi kèm tài liệu này và nói rõ đang ở bước nào. Khi có lỗi, gửi:
- lệnh vừa chạy và **nguyên văn** kết quả (hoặc 30 dòng quanh lỗi đầu tiên);
- với lỗi trên board: khoảng 60 dòng log boot từ `ESP-ROM:` trở đi;
- với lỗi WiFi: đoạn log từ `wifi:mode : sta` đến dòng chuyển sang `EyeCam-ESP32`, vì dấu hiệu A/B nằm ở đó;
- với lỗi web: ảnh chụp trang và log cùng thời điểm.
