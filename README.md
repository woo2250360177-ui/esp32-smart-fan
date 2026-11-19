# SmartFan IoT Platform 12.0.0

基于 ESP32-WROOM-32 与 TL-C12C 四线 PWM 风扇的本地边缘监控平台。当前版本仍采用 ESP32 SoftAP，不依赖公网或云端服务。

## 工程文件

- `src/main.cpp`：传感器采集、温控算法、PWM 驱动、红外/按键、HTTP API。
- `src/dashboard_page.h`：嵌入式网页，编译时由 ESP32 直接提供。
- `web/dashboard.html`：同一份独立网页源文件，便于浏览器预览和前端修改。
- `lib/`：项目依赖库。
- `platformio.ini`：PlatformIO 工程配置。

修改网页时，请同步更新 `web/dashboard.html` 与 `src/dashboard_page.h` 中的 HTML 内容。

## 功能

- 设备总览：温度、RPM 估算、PWM 指令、运行模式、实时曲线。
- 设备控制：手动档位、阈值温控、趋势补偿、声光自检、0% 输出。
- 实验数据中心：记录查询、筛选、分页、CSV 导出。
- 算法分析：阈值策略与趋势策略建议对比。
- 设备诊断：DS18B20、数码管 ACK、测速反馈与报警状态。

## 本地访问

1. PlatformIO 打开本工程，先执行 Build。
2. 连接 ESP32，确认串口后执行 Upload。
3. 手机或电脑连接 Wi-Fi：`ESP32-SmartFan`，密码：`Fan123456`。
4. 浏览器访问：`http://192.168.4.1`。

## HTTP API

- `GET /`：设备监控网页。
- `GET /status`：实时设备状态 JSON。
- `POST /control`：设备控制指令，参数为 `value`。
- `GET /records?offset=0&limit=25&filter=all`：分页实验记录。filter 支持 all、alarm、invalid、hot。
- `GET /experiment.csv`：导出实验 CSV（记录结束后可用）。

## 硬件接线（沿用 11.2.0）

IO32 经 1k 电阻接 2N3904 B，E 接共地，C 接风扇蓝色 PWM 线；风扇黄线测速接 IO33，并经 10k 上拉至 3V3。风扇红线接 12V 正，黑线接 12V 负，12V 负极与 ESP32 GND 共地。12V 正极不得接 ESP32。
底板 DS->26，K1->27，IRED->14，D1->21，BEEP->25。

## 运行边界

- 记录最多 600 个样本，存储于 ESP32 RAM，断电丢失；请及时导出。
- RPM 按每转两脉冲估算，尚未校准。
- 0% PWM 指令不保证风扇机械停转；完全断电需切断 12V。
- 超温报警阈值 40°C / 38°C 为项目演示参数，不是工业安全认证值。
- 当前没有云端数据库、永久历史、公网远程控制或事件日志持久化。
- 网页断开不会停止 ESP32 端温控、实体按键或红外控制。

## 验证状态

本次改造完成了网页结构与 API 源码更新，但尚未在用户本地 PlatformIO 环境完成编译、烧录和实机联调。请先 Build，再 Upload；未经实机验证不要覆盖原有稳定版本。
