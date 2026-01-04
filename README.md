# Hide-and-seek

基于 ESP32C3 的 LoRa RSSI 信号强度监测系统（收发配套）。该项目包含**接收端**和**发送端**两个配套使用的模块：

- **接收端**（本项目）：通过 UART 接收无线信号数据，在 ILI9341 TFT 屏幕上实时显示 RSSI 值、信号波形和动画
- **发送端**：基于 ESPHome，可定时发送数据包用于测试接收端信号强度

## 项目简介

本项目使用 ESP32C3 微控制器与 ILI9341 TFT 显示屏，通过 UART 接收外部无线模块发送的 RSSI（接收信号强度指示）数据帧，并将信号强度、原始数据、波形图和统计信息实时显示在屏幕上。配合发送端使用，可以实时监测和调试 LoRa 无线通信的信号质量。

## 硬件要求

### 主控芯片
- **ESP32C3** (airm2m_core_esp32c3 开发板)

### 无线模块
- **大夏龙雀 LR32** LoRa 无线通信模块

### 显示屏
- **ILI9341** TFT LCD (240x320 像素)

### 引脚连接

#### SPI 总线配置 (用于 ILI9341)
| 功能 | GPIO |
|------|------|
| SCLK | GPIO2 |
| MOSI | GPIO3 |
| DC   | GPIO10 |
| CS   | GPIO6 |
| RST  | GPIO7 |
| BL   | GPIO4 |

#### UART 配置 (用于外部模块通信)
| 功能 | GPIO |
|------|------|
| TX   | GPIO0 |
| RX   | GPIO1 |

**串口参数**: 9600 baud, 8N1

## 发送端配置（ESPHome）

项目包含基于 **ESPHome** 的发送端配置文件 `lora-esp32c3.yaml`，用于配套测试接收端。

### 发送端特性
- 定时发送数据包（默认每1秒）
- 发送标准数据模式：`0x1F, 0x1F, 0x1F, 0xFF`


### 发送端使用
1. 将 `lora-esp32c3.yaml` 烧录到另一块 ESP32C3 中
2. 配置 WiFi 信息和 API 密钥
3. 通过 ESPHome 的 Web 界面控制发送或自动定时发送
4. 接收端会实时显示接收到的信号强度

## 软件依赖

### 接收端开发环境
- [PlatformIO](https://platformio.org/)
- Platform: espressif32@6.4.0
- Framework: Arduino

### 接收端库依赖
- [LovyanGFX](https://github.com/lovyan03/LovyanGFX) - 高性能图形库

依赖库已在 `platformio.ini` 中配置，PlatformIO 会自动下载。

### 发送端依赖
- [ESPHome](https://esphome.io/) - 开源自动化框架

## 数据格式

### UART 数据帧结构
系统接收 5 字节二进制数据帧：

```
[D0] [D1] [D2] [D3] [RSSI_RAW]
```

- **D0-D3**: 4 字节数据内容
- **RSSI_RAW**: 第 5 字节为 RSSI 原始值

### RSSI 计算公式
当 RSSI_RAW = 0 时，显示 **0 dBm**（满格信号）

当 RSSI_RAW ≠ 0 时：
```
RSSI (dBm) = -(255 - RSSI_RAW)
```

### 无线模块配置（AT+DRSSI）

通过 **AT+DRSSI** 命令配置无线模块是否在接收数据包时附加 RSSI 值：

#### 命令格式
- **查询指令**：`AT+DRSSI`
  - 响应：`+DRSSI=<param>`
  
- **设置指令**：`AT+DRSSI<param>`
  - 响应：`+DRSSI=<param> OK`
  - 参数值：
    - `0`：关闭（默认值）
    - `1`：打开

#### 配置说明
1. **设置完该指令后需重启生效**
2. 启用 DRSSI 后，接收数据包时会自动在末尾附加 RSSI 字节
3. RSSI 字节会随信号强度而变化

#### 示例
**发送端发送**：`31 32 33 34 35`（5个字节）

**接收端接收**（启用 DRSSI 后）：`31 32 33 34 35 AB`（6个字节，最后一位为 RSSI）

其中 `AB` 是实际检测到的 RSSI 值，计算方法：
```
实际信号强度 = -(0xFF - 0xAB) = -(255 - 171) = -84 dBm
```

## 功能特性

**接收端**
- ✅ 实时接收和解析 UART 数据帧
- ✅ 计算并显示 RSSI 值（单位：dBm）
- ✅ 满格信号（0 dBm）时显示双天线图标
- ✅ 显示原始数据帧内容（十六进制）
- ✅ 实时信号强度波形图
- ✅ 移动粒子动画效果
- ✅ 统计接收帧数和字节总数
- ✅ 彩色 TFT 显示界面
- ✅ USB CDC 串口调试输出
- ✅ 帧超时自动重置机制

**发送端**
- ✅ 定时自动发送数据包 (`0x1F, 0x1F, 0x1F, 0xFF`)
- ✅ 手动控制发送
- ✅ Web 界面和 Home Assistant 集成

## 显示界面

屏幕显示内容包括：
- **RSSI**: 信号强度（dBm）
- **Raw**: RSSI 原始十六进制值
- **Data**: 数据帧前 4 字节内容
- **RSSI byte**: RSSI 字节值
- **Total**: 累计接收帧数
- **Bytes**: 累计接收字节数

## 编译和上传

### 接收端编译和上传

#### 1. 安装 PlatformIO
如果还未安装，请先安装 [PlatformIO IDE](https://platformio.org/platformio-ide) 或 [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html)。

#### 2. 克隆项目
```bash
git clone <repository-url>
cd Hide-and-seek
```

#### 3. 编译项目
```bash
platformio run
```

#### 4. 上传到开发板
```bash
platformio run --target upload
```

#### 5. 监控串口输出
```bash
platformio device monitor
```

或使用快捷命令一次完成编译、上传和监控：
```bash
platformio run --target upload && platformio device monitor
```

### 发送端编译和上传（ESPHome）

#### 1. 安装 ESPHome
```bash
pip install esphome
```

#### 2. 准备配置文件
编辑 `lora-esp32c3.yaml`，配置 WiFi 和其他参数

#### 3. 编译和上传
```bash
esphome run lora-esp32c3.yaml
```

#### 4. 通过 Web 界面控制
启动后可访问本地 Web 界面进行控制和配置

## 串口调试输出

通过 USB CDC 串口（115200 baud）可以查看详细的调试信息：

```
=== ILI9341 with LovyanGFX ===
ESP32C3 starting...
Serial1: baud=9600 RX=GPIO1 TX=GPIO0 (binary 5-byte frames)
Frame format: D0 D1 D2 D3 RSSIraw
RSSI = -(255 - RSSIraw)
================================

[1] RX[0]=0x12 (18)
[2] RX[1]=0x34 (52)
...
================================
Frame #1: 12 34 56 78 9A
RSSI: raw=0x9A -> -101 dBm
Total bytes: 5
================================
```

## 配置选项

### 修改日志详细程度
在 `main.cpp` 中可以控制是否输出每个字节的接收日志：

```cpp
#define UART_LOG_EVERY_BYTE 1  // 设为 0 关闭详细日志
```

### 调整显示参数
可在 `setup()` 函数中修改：
```cpp
lcd.setRotation(1);         // 屏幕旋转方向 (0-3)
lcd.setBrightness(220);     // 背光亮度 (0-255)
```

## 使用流程

1. **硬件连接**：按照引脚连接表连接接收端的 ESP32C3、ILI9341 屏幕和无线模块
2. **烧录接收端**：将接收端代码编译上传到接收端 ESP32C3
3. **烧录发送端**：将发送端 ESPHome 配置编译上传到另一块 ESP32C3
4. **开始测试**：
   - 打开接收端串口监视器，观察调试输出
   - 通过发送端的 Web 界面或自动定时模式发送数据
   - 接收端屏幕实时显示接收到的 RSSI 值、波形图和动画
5. **信号调试**：根据显示的信号强度调整天线位置或距离

## 故障排除

### 屏幕不显示
1. 检查 SPI 引脚连接是否正确
2. 确认 CS 和 RST 引脚连接正常
3. 验证供电电压是否稳定

### 无法接收数据
1. 确认 UART 波特率匹配（默认 9600）
2. 检查 TX/RX 引脚是否连接正确
3. 确保外部模块正常发送数据

### 编译错误
1. 更新 PlatformIO 平台：`platformio platform update`
2. 清理项目：`platformio run --target clean`
3. 重新编译：`platformio run`

## 许可证

本项目采用 MIT 许可证。

## 作者

- Repository: [bpm280-aht20_unified-sensor_esp32](https://github.com/14790897/bpm280-aht20_unified-sensor_esp32)

## 参考资料

- [ESP32C3 技术文档](https://www.espressif.com/en/products/socs/esp32-c3)
- [LovyanGFX GitHub](https://github.com/lovyan03/LovyanGFX)
- [ILI9341 数据手册](https://cdn-shop.adafruit.com/datasheets/ILI9341.pdf)
- [PlatformIO 文档](https://docs.platformio.org/)
