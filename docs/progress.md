# 进度日志

## 2026-10-03：建立串口最小工程

### 已完成

- 确认 VS Code、Git、PlatformIO IDE 3.3.4 及 C/C++ 插件已安装。
- 确认 Git 提交身份已配置。
- 创建 esp32dev + Arduino 工程及每秒串口输出程序。
- 配置忽略 .pio/、.vscode/、include/secrets.h。

### 待验证

- PlatformIO Core 正在初始化，编译工具链尚待安装和验证。
- 首次编译、上传和连续串口输出尚未验证。
- 本机枚举到 COM5、COM6，注册表显示均为蓝牙串口；尚未识别到开发板 USB 串口。
- Git 首次提交、GitHub 私有仓库和队友克隆尚未完成。

### 下一步

- 完成 Core 初始化并编译。
- 仅用 USB 数据线连接开发板，识别真实串口，上传并观察约 1 秒一行的输出。
- 验证结果后更新此日志，创建首次 Git 提交。

### 硬件确认

底板引脚、风扇线数/额定电流、驱动模块、温度传感器、屏幕型号均待核对。


## 2026-11-18：SmartFan IoT Platform 12.0.0

### 已完成（源码已推送至 feature/smartfan-iot-platform-v12）

- 将原有单页控制网页升级为五个功能区：设备总览、设备控制、实验数据、算法分析、设备诊断。
- 增加响应式布局、设备状态 KPI、温度/转速/输出曲线。
- 增加实验数据分页查询 API：GET /records，支持 all、alarm、invalid、hot 筛选。
- 增加实验数据表格、分页和 CSV 导出入口。
- 将网页 HTML 独立到 src/dashboard_page.h，同时保留 web/dashboard.html 作为独立源文件。
- 更新固件版本标识为 12.0.0-smartfan-iot，并更新 README。

### 保留的边界

- 原有 /status、/control、/experiment.csv API 与温控、红外、按键、PWM 逻辑保留。
- 仍为 ESP32 SoftAP 本地模式；尚未集成 Blynk 或其他云平台。
- 实验记录仍为最多 600 条 RAM 数据，断电丢失；RPM 仍为估算值。
- 设备诊断只展示固件当前实际提供的状态，不伪造持久化事件日志。

### 待验证

- 用户本地 PlatformIO Build。
- 烧录后测试 /、/status、/records、/control、/experiment.csv。
- 验证五个页面、筛选分页、CSV 导出、手机响应式布局。
- 实机验证控制策略、风扇转速、温度传感器与报警。
