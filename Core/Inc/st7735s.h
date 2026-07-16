/**
 * @file    st7735s.h
 * @brief   ST7735S TFT LCD Driver for STM32 (HAL Library)
 * @details Driver for 1.8" 128x160 TFT LCD module (WH-N177-1216TCWPG01-A8)
 *          Interface: 4-wire SPI (半双工只发送 + DMA 可选)
 *          Color mode: RGB565 (16-bit/pixel)
 *
 * @ref     ST7735S Datasheet v0.1 (Sitronix)
 *          WH-LCD-180-8pin-SPI Product Specification (WangHong)
 */

#ifndef __ST7735S_H__
#define __ST7735S_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "main.h"
#include <stdint.h>

/* ==========================================================================
 *  Configuration Macros  ——  根据你的板子修改
 * ========================================================================== */

/** LCD 分辨率 */
#define ST7735S_WIDTH           128
#define ST7735S_HEIGHT          160

/**
 * SPI 句柄
 * 使用前在 stm32f4xx_hal_msp.c 或 CubeMX 中确认已正确初始化
 * 支持：全双工 / 半双工只发送 / 单线只发送 模式
 */
#ifndef ST7735S_SPI_HANDLE
#define ST7735S_SPI_HANDLE      hspi1
extern SPI_HandleTypeDef ST7735S_SPI_HANDLE;
#endif

/**
 * 是否使用 DMA
 * 注释掉这行 = 纯阻塞模式（SPI 中断轮询）
 * 取消注释 = 批量传输使用 DMA（需要 CubeMX 配好 SPI TX DMA 通道）
 */
 #define ST7735S_USE_DMA

/**
 * DMA 传输缓冲区大小（字节）
 * 用于 FillRect 等批量刷色，越小越省 RAM，越大传输效率越高
 * 建议值：512 或 1024
 */
#define ST7735S_DMA_BUF_SIZE    512

/* ---- GPIO 引脚定义 ------------------------------------------------ */
/* CS   —— 片选（低电平有效） */
#ifndef ST7735S_CS_PORT
#define ST7735S_CS_PORT         GPIOB
#define ST7735S_CS_PIN          GPIO_PIN_0
#endif

/* DC   —— 数据/命令（1=数据, 0=命令） */
#ifndef ST7735S_DC_PORT
#define ST7735S_DC_PORT         GPIOB
#define ST7735S_DC_PIN          GPIO_PIN_1
#endif

/* RST  —— 硬件复位（低电平有效） */
#ifndef ST7735S_RST_PORT
#define ST7735S_RST_PORT        GPIOB
#define ST7735S_RST_PIN         GPIO_PIN_10
#endif

/* BLK  —— 背光控制（高电平有效） */
#ifndef ST7735S_BLK_PORT
#define ST7735S_BLK_PORT        GPIOB
#define ST7735S_BLK_PIN         GPIO_PIN_11
#endif

/* ==========================================================================
 *  颜色定义（RGB565 — 16 位）
 * ========================================================================== */

#define ST7735S_BLACK           0x0000
#define ST7735S_WHITE           0xFFFF
#define ST7735S_RED             0xF800
#define ST7735S_GREEN           0x07E0
#define ST7735S_BLUE            0x001F
#define ST7735S_YELLOW          0xFFE0
#define ST7735S_CYAN            0x07FF
#define ST7735S_MAGENTA         0xF81F
#define ST7735S_GRAY            0x8410
#define ST7735S_NAVY            0x000F
#define ST7735S_DARKGREEN       0x03E0
#define ST7735S_ORANGE          0xFD20
#define ST7735S_PINK            0xF81F
#define ST7735S_BROWN           0xBC40

/** 将 R(5), G(6), B(5) 拼成 RGB565 */
#define ST7735S_RGB565(r, g, b) ((((r) & 0x1F) << 11) | (((g) & 0x3F) << 5) | ((b) & 0x1F))

/* ==========================================================================
 *  命令集（ST7735S Datasheet §10）
 * ========================================================================== */

/* 系统功能命令 */
#define ST7735S_CMD_NOP             0x00
#define ST7735S_CMD_SWRESET         0x01
#define ST7735S_CMD_SLPIN           0x10
#define ST7735S_CMD_SLPOUT          0x11
#define ST7735S_CMD_PTLON           0x12
#define ST7735S_CMD_NORON           0x13
#define ST7735S_CMD_INVOFF          0x20
#define ST7735S_CMD_INVON           0x21
#define ST7735S_CMD_DISPOFF         0x28
#define ST7735S_CMD_DISPON          0x29
#define ST7735S_CMD_CASET           0x2A
#define ST7735S_CMD_RASET           0x2B
#define ST7735S_CMD_RAMWR           0x2C
#define ST7735S_CMD_RAMRD           0x2E
#define ST7735S_CMD_MADCTL          0x36
#define ST7735S_CMD_COLMOD          0x3A

/* 面板功能命令 */
#define ST7735S_CMD_FRMCTR1         0xB1
#define ST7735S_CMD_FRMCTR2         0xB2
#define ST7735S_CMD_FRMCTR3         0xB3
#define ST7735S_CMD_INVCTR          0xB4
#define ST7735S_CMD_PWCTR1          0xC0
#define ST7735S_CMD_PWCTR2          0xC1
#define ST7735S_CMD_PWCTR3          0xC2
#define ST7735S_CMD_PWCTR4          0xC3
#define ST7735S_CMD_PWCTR5          0xC4
#define ST7735S_CMD_VMCTR1          0xC5
#define ST7735S_CMD_GMCTRP1         0xE0
#define ST7735S_CMD_GMCTRN1         0xE1
#define ST7735S_CMD_EXTCMD          0xFC

/* MADCTL bits（§10.1.29） */
#define ST7735S_MADCTL_MY           0x80
#define ST7735S_MADCTL_MX           0x40
#define ST7735S_MADCTL_MV           0x20
#define ST7735S_MADCTL_ML           0x10
#define ST7735S_MADCTL_RGB          0x00
#define ST7735S_MADCTL_BGR          0x08
#define ST7735S_MADCTL_MH           0x04

/* COLMOD（3Ah） */
#define ST7735S_COLMOD_12BIT        0x03
#define ST7735S_COLMOD_16BIT        0x05
#define ST7735S_COLMOD_18BIT        0x06

/* ==========================================================================
 *  公有 API
 * ========================================================================== */

/** @brief LCD 初始化（含硬件复位 + 厂商初始化序列） */
void ST7735S_Init(void);

/** @brief 硬件复位 */
void ST7735S_HardReset(void);

/** @brief 写命令字节 */
void ST7735S_WriteCmd(uint8_t cmd);

/** @brief 写数据字节 */
void ST7735S_WriteData(uint8_t data);

/** @brief 写数据缓冲区（纯阻塞） */
void ST7735S_WriteDataBuf(const uint8_t *data, uint32_t len);

/** @brief 写 16 位字到 LCD（RGB565） */
void ST7735S_WriteWord(uint16_t word);

/** @brief 设窗口（CASET + RASET） */
void ST7735S_SetWindow(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2);

/** @brief 设像素格式（COLMOD） */
void ST7735S_SetColorMode(uint8_t format);

/** @brief 设内存访问控制（MADCTL） */
void ST7735S_SetMADCTL(uint8_t ctrl);

/** @brief 进入睡眠 */
void ST7735S_SleepIn(void);

/** @brief 唤醒 */
void ST7735S_SleepOut(void);

/** @brief 开显示 */
void ST7735S_DisplayOn(void);

/** @brief 关显示 */
void ST7735S_DisplayOff(void);

/** @brief 开反转 */
void ST7735S_InvertOn(void);

/** @brief 关反转 */
void ST7735S_InvertOff(void);

/** @brief 开背光 */
void ST7735S_BacklightOn(void);

/** @brief 关背光 */
void ST7735S_BacklightOff(void);

/* ==========================================================================
 *  绘图函数
 * ========================================================================== */

/** @brief 画一个像素 */
void ST7735S_DrawPixel(uint8_t x, uint8_t y, uint16_t color);

/** @brief 全屏填充 */
void ST7735S_FillScreen(uint16_t color);

/** @brief 填充矩形区域 */
void ST7735S_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color);

/** @brief 画矩形边框 */
void ST7735S_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color);

/** @brief 画水平线 */
void ST7735S_DrawHLine(uint8_t x, uint8_t y, uint8_t length, uint16_t color);

/** @brief 画垂直线 */
void ST7735S_DrawVLine(uint8_t x, uint8_t y, uint8_t length, uint16_t color);

/** @brief 画一个 ASCII 字符（5x7 字体） */
void ST7735S_DrawChar(uint8_t x, uint8_t y, char ch, uint16_t color, uint16_t bg);

/** @brief 画字符串（5x7 字体） */
void ST7735S_DrawString(uint8_t x, uint8_t y, const char *str, uint16_t color, uint16_t bg);

/** @brief 显示全屏图像（RGB565 数组） */
void ST7735S_DrawImage(const uint16_t *image);

/* ==========================================================================
 *  DMA 加速版绘图函数  ——  仅在 ST7735S_USE_DMA 启用时可用
 * ========================================================================== */

#ifdef ST7735S_USE_DMA

/**
 * @brief  用 DMA 加速填充矩形
 * @note   需要 CubeMX 配置了 SPI TX DMA 通道
 *         使用前在 main.c 中实现 HAL_SPI_TxCpltCallback()
 *         并调用 ST7735S_SetTXDoneCallback() 设置传输完成标志
 */
void ST7735S_FillRect_DMA(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color);

/**
 * @brief  用 DMA 加速显示全屏图像
 */
void ST7735S_DrawImage_DMA(const uint16_t *image);

/**
 * @brief  设置 DMA 传输完成回调（由用户提供）
 * @param  cb  回调函数指针，传输完成后被调用
 * @note   在 main.c 的 HAL_SPI_TxCpltCallback 中调用此回调
 *         示例：
 *           volatile uint8_t dma_done = 0;
 *           void my_cb(void) { dma_done = 1; }
 *           ST7735S_SetTXDoneCallback(my_cb);
 */
void ST7735S_SetTXDoneCallback(void (*cb)(void));

#endif /* ST7735S_USE_DMA */

#ifdef __cplusplus
}
#endif

#endif /* __ST7735S_H__ */
