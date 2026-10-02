#include "RBG.h"  

// 创建NeoPixel对象，参数：灯珠数量、数据引脚、像素类型标志
// NEO_RGB表示颜色顺序为红-绿-蓝，NEO_KHZ800表示800KHz数据传输频率
static Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGB + NEO_KHZ800);
//根据不同的板子有不同的调整！！！RBG

void RBG_init()
{
    // ========== NeoPixel初始化 ==========
    strip.begin();           // 初始化NeoPixel硬件
    strip.show();            // 清除显示（将所有灯珠设为关闭状态）
    strip.setBrightness(BRIGHTNESS);  // 设置全局亮度
    // Serial.println("NeoPixel initialized");
}

/******************************************
 * 基础灯光效果：顺序填充颜色
 * 参数:
 *   color - 32位颜色值(可通过strip.Color()创建)
 *   wait - 每个灯珠切换间隔(毫秒)
 *****************************************/
void colorWipe(uint32_t color, uint32_t  duration) 
{
  if (strip.numPixels() == 0 || duration == 0) return;
  strip.fill(color);  // 批量设色
  strip.show();       // 刷新显示
  delay(duration);    // 持续时间
}

/******************************************
 * 三原色基础颜色函数
 *****************************************/
void showRed(int wait) 
{
    colorWipe(strip.Color(255, 0, 0), wait);    // 纯红色
}

void showGreen(int wait) 
{
    colorWipe(strip.Color(0, 255, 0), wait);    // 纯绿色
}

void showBlue(int wait) 
{
    colorWipe(strip.Color(0, 0, 255), wait);    // 纯蓝色
}

void RBG_OFF(int wait) 
{
    colorWipe(strip.Color(0, 0, 0), wait);    //熄灭
}

/******************************************
 * 三原色混合颜色函数
 *****************************************/
void showYellow(int wait) 
{
    colorWipe(strip.Color(255, 100, 0), wait);  // 暗黄色
}

void showCyan(int wait) 
{
    colorWipe(strip.Color(0, 255, 255), wait);  // 绿色+蓝色=蓝绿色(天蓝色)
}

void showMagenta(int wait) 
{
    colorWipe(strip.Color(255, 0, 255), wait);  // 红色+蓝色=洋红(粉红色)
}

void showWhite(int wait) 
{
    colorWipe(strip.Color(255, 255, 255), wait); // 三原色混合=白色
}

/******************************************
 * 常用中间色调函数
 *****************************************/
void showOrange(int wait) 
{
    colorWipe(strip.Color(255, 30, 0), wait);  // 橙色
}

void showRose(int wait) 
{
    colorWipe(strip.Color(255, 100, 100), wait); //淡桃粉色
}

void showSoftGreen(int wait)
{
    colorWipe(strip.Color(155, 255, 0), wait);   //翠绿色
}

void showPurple(int wait)
{
    colorWipe(strip.Color(50,10,255), wait);    //蓝紫色
}

void showSoftBlue(int wait) 
{
    colorWipe(strip.Color(100, 255, 100), wait); // 淡水蓝色
}

void showlavender(int wait) 
{
    colorWipe(strip.Color(100, 100, 255), wait); // 淡紫色
}

/******************************************
 * 特殊效果颜色函数
 *****************************************/
void showGold(int wait) 
{
    colorWipe(strip.Color(255, 255, 0), wait);  // 红色+绿色=黄色(金色)
}

void showSilver(int wait) 
{
    colorWipe(strip.Color(192, 192, 192), wait); // 银色
}

void showRainbowColors(int wait) 
{
    // 快速显示彩虹多色
    showRed(wait);
    showOrange(wait);
    showYellow(wait);
    showGreen(wait);
    showCyan(wait);
    showBlue(wait);
    showMagenta(wait);
    showPurple(wait);
    showSoftGreen(wait);
    showRose(wait);
    showWhite(wait);
}


// 全局变量用于非阻塞彩虹效果
// 在RBG.cpp中作为静态变量
static unsigned long lastRainbowUpdate = 0;
static int rainbowHue = 0;


/******************************************
 * 非阻塞彩虹色效果
 *****************************************/
void Rainbowcolor() 
{
  // 检查是否到了更新时间（每100毫秒更新一次）
  if (millis() - lastRainbowUpdate < 100) 
  {
    return; // 还没到更新时间，直接返回
  }
  
  lastRainbowUpdate = millis(); // 更新最后更新时间
  
  // 计算当前彩虹颜色
  int r = map(sin(rainbowHue/255.0 * 2*PI)*255, -255, 255, 0, 255);
  int g = map(sin((rainbowHue+85)/255.0 * 2*PI)*255, -255, 255, 0, 255);
  int b = map(sin((rainbowHue+170)/255.0 * 2*PI)*255, -255, 255, 0, 255);
  
  // 设置LED颜色
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
  
  // 增加色相值，循环从0到254
  rainbowHue = (rainbowHue + 1) % 255;
}

/******************************************
 * RGB串口调试函数
 * 功能：通过串口输入RGB数值控制LED颜色
 * 输入格式：R,G,B 或 R,G,B,等待时间
 * 示例： 
 *   "255,0,0"     - 显示红色
 *   "0,255,0,500" - 显示绿色，填充时间500ms
 *   "128,128,128" - 显示灰色
 *****************************************/
void debugColorFromSerial() 
{
    if (Serial.available() > 0) 
    {
        String input = Serial.readStringUntil('\n');
        input.trim(); // 去除首尾空格
        
        // 检查输入格式
        if (input.length() > 0) 
        {
            // 解析逗号分隔的数值
            int firstComma = input.indexOf(',');
            int secondComma = input.indexOf(',', firstComma + 1);
            int thirdComma = input.indexOf(',', secondComma + 1);
            
            if (firstComma != -1 && secondComma != -1) 
            {
                // 提取RGB值
                int r = input.substring(0, firstComma).toInt();
                int g = input.substring(firstComma + 1, secondComma).toInt();
                int b = input.substring(secondComma + 1).toInt();
                int waitTime = 100; // 默认等待时间
                
                // 检查是否有第四个参数（等待时间）
                if (thirdComma != -1) 
                {
                    waitTime = input.substring(thirdComma + 1).toInt();
                    b = input.substring(secondComma + 1, thirdComma).toInt();
                }
                
                // 验证数值范围
                r = constrain(r, 0, 255);
                g = constrain(g, 0, 255);
                b = constrain(b, 0, 255);
                waitTime = constrain(waitTime, 0, 5000);
                
                // 显示颜色并反馈信息
                Serial.print("设置颜色: R=");
                Serial.print(r);
                Serial.print(", G=");
                Serial.print(g);
                Serial.print(", B=");
                Serial.print(b);
                Serial.print(", 等待时间=");
                Serial.print(waitTime);
                Serial.println("ms");
                
                colorWipe(strip.Color(r, g, b), waitTime);
                
            } 
            else 
            {
                Serial.println("错误: 请输入正确的格式 R,G,B 或 R,G,B,等待时间");
            }
        }
    }
}