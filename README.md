# 基于 ESP32 的多模式智能桌面风扇

两人课程设计，截止日期：2026-10-31。

## 开发环境

- VS Code + PlatformIO IDE + Arduino 框架，C/C++。
- 实物：ESP-WROOM-32 核心板 + 多功能实验底板。
- PlatformIO board：esp32dev（Espressif ESP32 Dev Module）。
- Conda 不参与本工程。

## 当前程序

`src/main.cpp` 以 115200 波特率重复输出 `ESP32 smart fan: serial OK`，间隔约 1 秒。
只使用串口，不配置底板外设引脚。运行结果以 `docs/progress.md` 为准。

## 操作

在 VS Code 中打开本目录。待 PlatformIO 初始化完成，在 PlatformIO 的 Project Tasks → esp32dev → General 中执行 Build、Upload、Monitor。

成功标准：Build 显示 SUCCESS；Upload 显示 SUCCESS；Monitor 每秒显示上述文字。
上传前确认实际 USB 串口，不把蓝牙串口当作开发板。两台电脑 COM 编号可不同，暂不在配置中固定端口。

## Git 协作

源码、platformio.ini 和文档纳入版本管理。.pio/、.vscode/、include/secrets.h 被忽略。
Wi-Fi 密码、令牌等凭据不能写入受版本管理的文件。

GitHub 私有仓库及邀请尚待完成。在远程仓库建立、队友克隆后：

1. 开工前确认 `git status` 干净，执行 `git pull --ff-only` 获取已推送的更新。
2. 完成一小步后用 `git diff` 检查变更，编译或测试，然后 `git add 文件路径`。
3. 用 `git commit -m "说明本次修改"` 在本机保存版本。
4. 用 `git push` 将提交同步到 GitHub。Commit 本身不会同步到队友电脑。

开始前约定各自修改的模块；早期共用 main.cpp 时轮流改。遇到冲突或非快进提示，先停下保留现场，一起处理，不强制推送。

## 硬件与任务边界

底板引脚必须查商家原理图或例程；温度传感器、显示屏、风扇线数与电流、驱动模块型号待确认。
12V 不可直接接 GPIO 或开发板 5V/3.3V 引脚。核对供电和驱动后才能接风扇。

顺序：串口 → LED/按键 → 测温 → 风扇开关/调速 → 手动/自动及显示 → Wi-Fi 网页 → 结构及稳定性测试。
定时关闭和自然风为可选拓展。

交付：至少 8 页报告（含两人设计分工、日志、问题与解决、实物照片），源码和答辩 PPT。
两人均承担设计任务，具体分工待共同确认。
