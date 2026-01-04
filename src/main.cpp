#include <Arduino.h>
#include <LovyanGFX.hpp>

// UART pins for external module: TX on GPIO0, RX on GPIO1
constexpr int UART_BAUD = 9600;
constexpr int UART_TX_PIN = 0;
constexpr int UART_RX_PIN = 1;

// 串口日志开关（会比较“吵”，需要时再打开）
#ifndef UART_LOG_EVERY_BYTE
#define UART_LOG_EVERY_BYTE 1
#endif

// LGFX自定义类用于ILI9341配置
class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_SPI _bus_instance;
  lgfx::Light_PWM _light_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();

      // SPI总线配置（ESP32C3优化）
      cfg.spi_host = SPI2_HOST;     // 使用SPI2_HOST
      cfg.spi_mode = 0;             // SPI模式 (0 ~ 3)
      cfg.freq_write = 27000000;    // 降低传输频率以提高稳定性
      cfg.freq_read = 16000000;     // 接收时的SPI时钟
      cfg.spi_3wire = true;         // 设为true时使用MOSI引脚进行接收
      cfg.use_lock = true;          // 设为true时使用事务锁
      cfg.dma_channel = SPI_DMA_CH_AUTO; // 使用DMA通道

      // 根据图片配置SPI引脚
      cfg.pin_sclk = 2;    // SPI SCLK引脚号
      cfg.pin_mosi = 3;    // SPI MOSI引脚号
      cfg.pin_miso = -1;   // 必须为负一否则不显示
      cfg.pin_dc = 10;      // SPI D/C引脚号 (如果不使用设为-1)

      _bus_instance.config(cfg);    // 配置应用到总线
      _panel_instance.setBus(&_bus_instance);      // 设置总线到面板
    }

    {
      auto cfg = _panel_instance.config();    // 获取显示面板配置的结构体

      cfg.pin_cs = 6;     // CS引脚号 (如果不使用设为-1)
      cfg.pin_rst = 7;   // RST引脚号 (如果不使用设为-1)
      cfg.pin_busy = -1;  // BUSY引脚号 (如果不使用设为-1)

      // 以下是显示面板控制设置
      cfg.panel_width = 240;     // 面板实际可显示宽度
      cfg.panel_height = 320;    // 面板实际可显示高度
      cfg.offset_x = 0;          // 面板的X方向偏移量
      cfg.offset_y = 0;          // 面板的Y方向偏移量
      cfg.offset_rotation = 0;   // 旋转方向的偏移 0~7 (4~7是上下颠倒)
      cfg.dummy_read_pixel = 8;  // 读取像素前的伪读取位数
      cfg.dummy_read_bits = 1;   // 读取非像素数据前的伪读取位数
      cfg.readable = false;      // ILI9341 通常不必读；避免触发读流程对 MISO/3-wire 的依赖
      cfg.invert = false;        // 如果面板的明暗颠倒设为true
      cfg.rgb_order = false;     // 如果面板的红蓝颜色顺序颠倒设为true
      cfg.dlen_16bit = false;    // 对于16位数据长度的面板设为true
      cfg.bus_shared = true;     // 如果SD卡等共享总线设为true

      _panel_instance.config(cfg);
    }

    {
      auto cfg = _light_instance.config();    // 获取背光配置的结构体

      cfg.pin_bl = 4;              // 背光连接的引脚号
      cfg.invert = false;          // 是否反转背光的亮度
      cfg.freq = 12000;           // 降低PWM频率以避免通道冲突
      cfg.pwm_channel = 2;        // 使用较低的PWM通道号（ESP32C3限制）

      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);  // 设置背光到面板
    }

    setPanel(&_panel_instance); // 使用的面板设置到LGFX
  }
};

static LGFX lcd;               // LGFX实例

// 缓存最近一次RSSI帧（纯二进制模式，5字节：前4字节数据，第5字节为RSSI）
static uint8_t g_lastRaw = 0;
static int g_lastRssiDbm = 0;
static unsigned long g_lastPacketMs = 0;
static uint8_t g_frame[5] = {0};
static size_t g_framePos = 0;
static unsigned long g_totalBytes = 0;
static unsigned long g_totalFrames = 0;

// 动画相关变量
struct Particle {
  float x, y;
  float vx, vy;
  uint16_t color;
  uint8_t size;
};
static Particle particles[8];
static int rssiHistory[64] = {0};  // RSSI历史记录用于波形图
static int rssiHistoryPos = 0;
static unsigned long lastAnimUpdate = 0;

// 初始化粒子
void initParticles() {
  for (int i = 0; i < 8; i++) {
    particles[i].x = random(0, 320);
    particles[i].y = random(170, 240);
    particles[i].vx = (random(-20, 20)) / 10.0;
    particles[i].vy = (random(-20, 20)) / 10.0;
    particles[i].size = random(2, 5);
    // 彩色粒子
    uint16_t colors[] = {TFT_CYAN, TFT_MAGENTA, TFT_YELLOW, TFT_GREEN, TFT_ORANGE, TFT_PINK};
    particles[i].color = colors[random(0, 6)];
  }
}

// 更新粒子动画
void updateParticles() {
  for (int i = 0; i < 8; i++) {
    // 清除旧位置
    lcd.fillCircle(particles[i].x, particles[i].y, particles[i].size, TFT_BLACK);
    
    // 更新位置
    particles[i].x += particles[i].vx;
    particles[i].y += particles[i].vy;
    
    // 边界检测和反弹
    if (particles[i].x < 0 || particles[i].x > 320) {
      particles[i].vx = -particles[i].vx;
      particles[i].x = constrain(particles[i].x, 0, 320);
    }
    if (particles[i].y < 170 || particles[i].y > 240) {
      particles[i].vy = -particles[i].vy;
      particles[i].y = constrain(particles[i].y, 170, 240);
    }
    
    // 绘制新位置
    lcd.fillCircle(particles[i].x, particles[i].y, particles[i].size, particles[i].color);
  }
}

// 绘制RSSI波形图
void drawRssiWaveform() {
  // 背景区域
  lcd.fillRect(0, 170, 320, 10, TFT_BLACK);
  
  // 绘制波形线
  for (int i = 1; i < 64; i++) {
    int x1 = (i - 1) * 5;
    int x2 = i * 5;
    int prevIdx = (rssiHistoryPos + i - 1) % 64;
    int currIdx = (rssiHistoryPos + i) % 64;
    
    // 将RSSI值映射到0-10的高度范围
    int y1 = 179 - map(constrain(rssiHistory[prevIdx], -120, -40), -120, -40, 0, 9);
    int y2 = 179 - map(constrain(rssiHistory[currIdx], -120, -40), -120, -40, 0, 9);
    
    // 根据信号强度选择颜色
    uint16_t color = TFT_RED;
    if (rssiHistory[currIdx] > -80) color = TFT_GREEN;
    else if (rssiHistory[currIdx] > -100) color = TFT_YELLOW;
    
    lcd.drawLine(x1, y1, x2, y2, color);
  }
}

// 绘制信号强度图标
void drawSignalIcon(int x, int y, bool fullSignal) {
  if (fullSignal) {
    // 满格信号：绘制两根天线在一起
    lcd.setTextColor(TFT_GREEN);
    lcd.setTextSize(3);
    // 使用字符模拟天线图标
    lcd.drawString("))))", x, y);
    lcd.drawString("((((", x + 35, y);
    // 绘制中间的连接点
    lcd.fillCircle(x + 45, y + 12, 4, TFT_GREEN);
    lcd.fillCircle(x + 35, y + 12, 4, TFT_GREEN);
    
    // 绘制信号条
    for (int i = 0; i < 4; i++) {
      int barHeight = (i + 1) * 5;
      lcd.fillRect(x + i * 8, y + 25 - barHeight, 6, barHeight, TFT_GREEN);
      lcd.fillRect(x + 75 - i * 8, y + 25 - barHeight, 6, barHeight, TFT_GREEN);
    }
    
    // 显示"满格"文字
    lcd.setTextSize(1);
    lcd.drawString("FULL SIGNAL", x + 5, y + 30);
  }
}

void drawRssi()
{
  lcd.fillRect(10, 60, 310, 100, TFT_BLACK);
  lcd.setTextColor(TFT_CYAN);
  lcd.setTextSize(2);
  lcd.drawString("RSSI:", 10, 60);
  lcd.setTextColor(TFT_YELLOW);
  lcd.drawString(String(g_lastRssiDbm) + " dBm", 90, 60);

  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(1);
  lcd.drawString("Raw: 0x" + String(g_lastRaw, HEX), 10, 90);
  String frameStr = "Data: ";
  for (int i = 0; i < 4; ++i) {
    if (i) frameStr += ' ';
    if (g_frame[i] < 16) frameStr += '0';
    frameStr += String(g_frame[i], HEX);
  }
  lcd.drawString(frameStr, 10, 105);
  lcd.drawString("RSSI byte: " + String(g_frame[4], HEX), 10, 120);
  lcd.drawString("Total: " + String(g_totalFrames) + " frames", 10, 135);
  lcd.drawString("Bytes: " + String(g_totalBytes), 10, 150);
  
  // 如果RSSI字节为0，显示满格信号图标
  if (g_frame[4] == 0) {
    drawSignalIcon(200, 80, true);
  }
}

void setup(void)
{
  Serial.begin(115200);           // USB CDC log
  Serial1.begin(UART_BAUD, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN); // 外部模块串口
  const uint32_t serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart) < 3000) {
    delay(10);
  }
  delay(200);
  Serial.println("\n\n=== ILI9341 with LovyanGFX ===");
  Serial.println("ESP32C3 starting...");
  Serial.printf("Serial1: baud=%d RX=GPIO%d TX=GPIO%d (binary 5-byte frames)\n", UART_BAUD, UART_RX_PIN, UART_TX_PIN);
  Serial.println("Frame format: D0 D1 D2 D3 RSSIraw");
  Serial.println("RSSI = -(255 - RSSIraw)");
  Serial.println("================================\n");

  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(220);
  lcd.fillScreen(TFT_BLACK);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.drawString("ESP32C3", 10, 10);
  lcd.drawString("ILI9341 OK!", 10, 30);
  lcd.setTextSize(1);
  lcd.drawString("UART0 TX=GPIO0 RX=GPIO1", 10, 45);
  lcd.drawString("Serial1 9600 bps (bin)", 10, 58);

  // 绘制分隔线
  lcd.drawLine(0, 165, 320, 165, TFT_DARKGREY);
  lcd.setTextSize(1);
  lcd.setTextColor(TFT_DARKGREY);
  lcd.drawString("Animation Zone", 120, 185);
  
  drawRssi();
  initParticles();
  randomSeed(analogRead(0));
}

void loop(void)
{
  // 二进制 5 字节帧：前4字节数据，第5字节为 RSSI raw
  while (Serial1.available()) {
    uint8_t b = static_cast<uint8_t>(Serial1.read());
    g_lastPacketMs = millis();
    g_totalBytes++;

#if UART_LOG_EVERY_BYTE
    Serial.printf("[%lu] RX[%u]=0x%02X (%3d)\n",
                  g_totalBytes,
                  static_cast<unsigned>(g_framePos),
                  b, b);
#endif

    g_frame[g_framePos++] = b;

    if (g_framePos >= 5) {
      g_totalFrames++;
      g_lastRaw = g_frame[4];
      
      // 如果RSSI字节为0，直接输出0 dBm，否则按公式计算
      if (g_lastRaw == 0) {
        g_lastRssiDbm = 0;
      } else {
        g_lastRssiDbm = -static_cast<int>(0xFF - g_lastRaw);
      }

      Serial.println("================================");
      Serial.printf("Frame #%lu: %02X %02X %02X %02X %02X\n",
                    g_totalFrames,
                    g_frame[0], g_frame[1], g_frame[2], g_frame[3], g_frame[4]);
      Serial.printf("RSSI: raw=0x%02X -> %d dBm\n", g_lastRaw, g_lastRssiDbm);
      Serial.printf("Total bytes: %lu\n", g_totalBytes);
      Serial.println("================================\n");

      drawRssi();
      
      // 更新RSSI历史
      rssiHistory[rssiHistoryPos] = g_lastRssiDbm;
      rssiHistoryPos = (rssiHistoryPos + 1) % 64;
      
      g_framePos = 0;
    }
  }

  // 超时清空，避免卡死
  if (g_framePos > 0 && (millis() - g_lastPacketMs) > 500) {
    Serial.printf("[WARN] Frame timeout, discard %u bytes\n", static_cast<unsigned>(g_framePos));
    g_framePos = 0;
  }

  // 更新动画（每50ms更新一次）
  if (millis() - lastAnimUpdate > 50) {
    updateParticles();
    drawRssiWaveform();
    lastAnimUpdate = millis();
  }

  delay(10);
}
