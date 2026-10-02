// RBG.h
#ifndef RBG_H // 这是“头文件保护”，防止头文件被重复包含，是好习惯
#define RBG_H

#include <Adafruit_NeoPixel.h> // 包含必要的库

// ============================ NeoPixel硬件配置 ============================
#define LED_PIN    21          // NeoPixel数据引脚连接至ESP32的GPIO21
#define LED_COUNT  1           // 连接的NeoPixel灯珠数量（当前配置为1个灯珠）
#define BRIGHTNESS 30          // 亮度设置（0-255范围）

// // 关键点：使用 extern 关键字
// // 这行代码的意思是：“请注意，在其他地方有一个叫 strip 的全局变量，你可以用它”
// // 这是一种“声明”，而不是“定义”
// extern Adafruit_NeoPixel strip;

// 初始化函数
void RBG_init();

// 基础灯光效果
void colorWipe(uint32_t color, uint32_t wait);
void Rainbowcolor();

// 三原色基础颜色
void showRed(int wait = 100);   //纯红色
void showGreen(int wait = 100); //纯绿色
void showBlue(int wait = 100);  //纯蓝色
void RBG_OFF(int wait) ; //熄灭

// 混合颜色
void showYellow(int wait = 100); //显示暗黄色（红色+绿色混合）
void showCyan(int wait = 100);   //显示蓝绿色/天蓝色（绿色+蓝色混合）
void showMagenta(int wait = 100);//显示洋红/粉红色（红色+蓝色混合）
void showWhite(int wait = 100);//显示纯白色（红+绿+蓝三原色混合）

// 常用中间色调
void showOrange(int wait = 100);//显示橙色（红色+少量绿色混合）
void showPurple(int wait = 100);//显示蓝紫色（蓝色+少量红色混合）
void showRose(int wait = 100);//显示淡桃粉色（红色+少量绿色+少量蓝色混合）
void showlavender(int wait = 100);//显示淡紫色（红色+蓝色+少量绿色混合）

// 柔和色调
void showSoftGreen(int wait = 100); //翠绿色
void showSoftBlue(int wait = 100);// 淡水蓝色


// 特殊效果颜色
void showGold(int wait = 100);  //金色
void showSilver(int wait = 100);//银色
void showRainbowColors(int wait = 500);

//调色函数
void debugColorFromSerial();


#endif