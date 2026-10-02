#include "include.h"
#include "SerialServo.h"
#include <Adafruit_NeoPixel.h>  // NeoPixel控制库

#define UART1_RX_PIN 7  // 定义 UART1 的接收引脚（RX）
#define UART1_TX_PIN 8  // 定义 UART1 的发送引脚（TX）

// 硬件配置
#define LED_PIN    21          // NeoPixel连接的引脚
#define LED_COUNT  1         // 连接的灯珠数量
#define BRIGHTNESS 25         // 亮度设置（0-255）

// 创建NeoPixel对象
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// 定义颜色数组（7种不同颜色）
uint32_t colors[] = {
  strip.Color(255, 0, 0),    // 红色
  strip.Color(0, 255, 0),    // 绿色
  strip.Color(0, 0, 255),    // 蓝色
  strip.Color(255, 255, 0),  // 黄色
  strip.Color(255, 0, 255),  // 洋红色
  strip.Color(0, 255, 255),  // 青色
  strip.Color(255, 255, 255) // 白色
};
int numColors = sizeof(colors) / sizeof(colors[0]); // 计算颜色数量
int startIndex = 0; // 颜色流动的起始索引

void setup() 
{
  Serial.begin(115200);
  delay(1000);
  Serial1.begin(115200, SERIAL_8N1, UART1_RX_PIN, UART1_TX_PIN);
  delay(500);

  // 初始化NeoPixel
  strip.begin();           // 初始化LED对象
  strip.show();            // 将所有灯珠初始化为"关闭"状态
  strip.setBrightness(BRIGHTNESS); // 设置亮度
}

void loop() 
{
  LobotSerialServoMove(Serial1, ID_ALL, 0, 1000);
  delay(500);
  // 调用颜色流动效果，参数100表示每个状态保持100毫秒
  colorFlow(100);
  LobotSerialServoMove(Serial1, ID_ALL, 1000, 1000);
  delay(500);
}

/******************************************
 * 颜色流动效果：每个灯珠显示不同颜色，颜色会流动变化
 * 参数:
 *   wait - 每个状态保持时间(毫秒)
 *****************************************/
void colorFlow(int wait) 
{
  // 为每个灯珠设置颜色
  for(int i = 0; i < strip.numPixels(); i++) 
  {
    // 计算当前灯珠应该显示的颜色索引（循环使用颜色数组）
    int colorIndex = (startIndex + i) % numColors;
    strip.setPixelColor(i, colors[colorIndex]);
  }
  
  strip.show();  // 更新灯带显示
  delay(wait);   // 保持当前状态
  
  // 更新起始索引，实现颜色流动效果
  startIndex = (startIndex + 1) % numColors;
}
