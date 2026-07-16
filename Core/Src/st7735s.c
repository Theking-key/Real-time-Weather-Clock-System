/**
 * @file    st7735s.c
 * @brief   ST7735S TFT LCD Driver for STM32 (HAL Library)
 * @details 1.8" 128x160 TFT, 4线SPI（半双工只发送）, RGB565
 *          支持 DMA 加速批量传输（可选）
 *
 * @ref     ST7735S Datasheet v0.1 (Sitronix)
 *          WH-LCD-180-8pin-SPI Product Specification (WangHong)
 *
 * 8线 SPI LCD 引脚：
 *   Pin 1  GND   — 电源地
 *   Pin 2  VCC   — 2.8~3.3V 电源
 *   Pin 3  SCL   — SPI 时钟
 *   Pin 4  SDA   — SPI MOSI（只发送数据给 LCD，无需 MISO）
 *   Pin 5  RES   — 复位（低电平复位）
 *   Pin 6  DC    — 数据/命令（1=数据, 0=命令）
 *   Pin 7  CS    — 片选（低电平有效）
 *   Pin 8  BLK   — 背光控制（高电平亮）
 */

#include "st7735s.h"


/* ==========================================================================
 *  私有辅助  ——  GPIO 控制
 * ========================================================================== */

static inline void CS_Select(void)  { HAL_GPIO_WritePin(ST7735S_CS_PORT, ST7735S_CS_PIN, GPIO_PIN_RESET); }
static inline void CS_Release(void) { HAL_GPIO_WritePin(ST7735S_CS_PORT, ST7735S_CS_PIN, GPIO_PIN_SET); }
static inline void DC_Cmd(void)     { HAL_GPIO_WritePin(ST7735S_DC_PORT, ST7735S_DC_PIN, GPIO_PIN_RESET); }
static inline void DC_Data(void)    { HAL_GPIO_WritePin(ST7735S_DC_PORT, ST7735S_DC_PIN, GPIO_PIN_SET); }
static inline void Rst_Low(void)    { HAL_GPIO_WritePin(ST7735S_RST_PORT, ST7735S_RST_PIN, GPIO_PIN_RESET); }
static inline void Rst_High(void)   { HAL_GPIO_WritePin(ST7735S_RST_PORT, ST7735S_RST_PIN, GPIO_PIN_SET); }
static inline void Blk_On(void)     { HAL_GPIO_WritePin(ST7735S_BLK_PORT, ST7735S_BLK_PIN, GPIO_PIN_SET); }
static inline void Blk_Off(void)    { HAL_GPIO_WritePin(ST7735S_BLK_PORT, ST7735S_BLK_PIN, GPIO_PIN_RESET); }

/* ==========================================================================
 *  SPI 传输层
 *  支持：全双工 / 半双工只发送 / 单线只发送
 *  半双工模式下，HAL_SPI_Transmit 仍然正常工作，代码无需改动
 * ========================================================================== */

static void SPI_TxByte(uint8_t data)
{
    HAL_SPI_Transmit(&ST7735S_SPI_HANDLE, &data, 1, HAL_MAX_DELAY);
}

static void SPI_TxBuf(const uint8_t *data, uint32_t len)
{
    HAL_SPI_Transmit(&ST7735S_SPI_HANDLE, (uint8_t *)data, len, HAL_MAX_DELAY);
}

/* ==========================================================================
 *  DMA 相关
 * ========================================================================== */

#ifdef ST7735S_USE_DMA

/* DMA 传输完成回调指针 */
static void (*s_dma_tx_callback)(void) = NULL;

/* DMA 传输用缓冲区（静态分配） */
static uint8_t s_dma_buf[ST7735S_DMA_BUF_SIZE];

/* 等待 DMA 完成（带超时） */
static void WaitDMA(void)
{
    if (s_dma_tx_callback == NULL) return;

    volatile uint32_t timeout = 5000000;  /* 约 5 秒 @ 72MHz */
    while (HAL_SPI_GetState(&ST7735S_SPI_HANDLE) != HAL_SPI_STATE_READY)
    {
        if (--timeout == 0) break;  /* 超时保护 */
    }
}

void ST7735S_SetTXDoneCallback(void (*cb)(void))
{
    s_dma_tx_callback = cb;
}

/**
 * SPI TX 完成中断入口 —— 用户在 main.c 的 HAL_SPI_TxCpltCallback 里调用它：
 *
 *   void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
 *       if (hspi->Instance == SPI1) {
 *           ST7735S_DMA_IRQHandler();
 *       }
 *   }
 */
void ST7735S_DMA_IRQHandler(void)
{
    if (s_dma_tx_callback)
        s_dma_tx_callback();
}

/* 用 DMA 发送缓冲区（保持 CS 低） */
static void DMA_TxBuf(const uint8_t *data, uint32_t len)
{
    HAL_SPI_Transmit_DMA(&ST7735S_SPI_HANDLE, (uint8_t *)data, len);
    WaitDMA();
}

/*
 * 在 CubeMX 中：
 *   1. SPI 配置为 "Transmit Only Master" 或 "Half-Duplex Master"
 *   2. 给 SPI TX 添加 DMA 通道（Memory → Peripheral）
 *   3. DMA 参数：Normal, Byte, Medium Priority
 *   4. 在 NVIC 中开启 SPI DMA TX 中断
 */

#endif /* ST7735S_USE_DMA */

/* ==========================================================================
 *  延时封装
 * ========================================================================== */

static void Delay(uint32_t ms) { HAL_Delay(ms); }

/* ==========================================================================
 *  底层 LCD 接口
 * ========================================================================== */

void ST7735S_WriteCmd(uint8_t cmd)
{
    CS_Select();
    DC_Cmd();
    SPI_TxByte(cmd);
    CS_Release();
}

void ST7735S_WriteData(uint8_t data)
{
    CS_Select();
    DC_Data();
    SPI_TxByte(data);
    CS_Release();
}

void ST7735S_WriteDataBuf(const uint8_t *data, uint32_t len)
{
    CS_Select();
    DC_Data();
    SPI_TxBuf(data, len);
    CS_Release();
}

void ST7735S_WriteWord(uint16_t word)
{
    uint8_t buf[2] = { (uint8_t)(word >> 8), (uint8_t)(word & 0xFF) };
    CS_Select();
    DC_Data();
    SPI_TxBuf(buf, 2);
    CS_Release();
}

/* ==========================================================================
 *  硬件复位（§9.13 Power ON/OFF Sequence）
 * ========================================================================== */

void ST7735S_HardReset(void)
{
    Rst_Low();
    Delay(10);
    Rst_High();
    Delay(150);
}

/* ==========================================================================
 *  显示控制命令
 * ========================================================================== */

void ST7735S_SetWindow(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    ST7735S_WriteCmd(ST7735S_CMD_CASET);
    ST7735S_WriteData(0x00); ST7735S_WriteData(x1);
    ST7735S_WriteData(0x00); ST7735S_WriteData(x2);

    ST7735S_WriteCmd(ST7735S_CMD_RASET);
    ST7735S_WriteData(0x00); ST7735S_WriteData(y1);
    ST7735S_WriteData(0x00); ST7735S_WriteData(y2);
}

void ST7735S_SetColorMode(uint8_t format)
{
    ST7735S_WriteCmd(ST7735S_CMD_COLMOD);
    ST7735S_WriteData(format);
}

void ST7735S_SetMADCTL(uint8_t ctrl)
{
    ST7735S_WriteCmd(ST7735S_CMD_MADCTL);
    ST7735S_WriteData(ctrl);
}

void ST7735S_SleepIn(void)
{
    ST7735S_WriteCmd(ST7735S_CMD_SLPIN);
    Delay(5);
}

void ST7735S_SleepOut(void)
{
    ST7735S_WriteCmd(ST7735S_CMD_SLPOUT);
    Delay(120);
}

void ST7735S_DisplayOn(void)  { ST7735S_WriteCmd(ST7735S_CMD_DISPON); }
void ST7735S_DisplayOff(void) { ST7735S_WriteCmd(ST7735S_CMD_DISPOFF); }
void ST7735S_InvertOn(void)   { ST7735S_WriteCmd(ST7735S_CMD_INVON); }
void ST7735S_InvertOff(void)  { ST7735S_WriteCmd(ST7735S_CMD_INVOFF); }
void ST7735S_BacklightOn(void)  { Blk_On(); }
void ST7735S_BacklightOff(void) { Blk_Off(); }

/* ==========================================================================
 *  初始化序列
 *  基于 WH-N177-1216TCWPG01-A8 模组厂商提供的参数
 * ========================================================================== */

void ST7735S_Init(void)
{
    /* ---- 硬件复位 ---- */
    ST7735S_HardReset();

    /* ---- 软件复位 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_SWRESET);
    Delay(150);

    /* ---- 唤醒 ---- */
    ST7735S_SleepOut();                     /* 包含 120ms 等待 */

    /* ---- 帧率控制 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_FRMCTR1);
    ST7735S_WriteData(0x05); ST7735S_WriteData(0x3C); ST7735S_WriteData(0x3C);

    ST7735S_WriteCmd(ST7735S_CMD_FRMCTR2);
    ST7735S_WriteData(0x05); ST7735S_WriteData(0x3C); ST7735S_WriteData(0x3C);

    ST7735S_WriteCmd(ST7735S_CMD_FRMCTR3);
    ST7735S_WriteData(0x05); ST7735S_WriteData(0x3C); ST7735S_WriteData(0x3C);
    ST7735S_WriteData(0x05); ST7735S_WriteData(0x3C); ST7735S_WriteData(0x3C);

    /* ---- 反转控制（点反转） ---- */
    ST7735S_WriteCmd(ST7735S_CMD_INVCTR);
    ST7735S_WriteData(0x03);

    /* ---- 电源控制 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_PWCTR1);
    ST7735S_WriteData(0xAB); ST7735S_WriteData(0x0B); ST7735S_WriteData(0x04);

    ST7735S_WriteCmd(ST7735S_CMD_PWCTR2);
    ST7735S_WriteData(0xC5);

    ST7735S_WriteCmd(ST7735S_CMD_PWCTR3);
    ST7735S_WriteData(0x0D); ST7735S_WriteData(0x00);

    ST7735S_WriteCmd(ST7735S_CMD_PWCTR4);
    ST7735S_WriteData(0x8D); ST7735S_WriteData(0x6A);

    ST7735S_WriteCmd(ST7735S_CMD_PWCTR5);
    ST7735S_WriteData(0x8D); ST7735S_WriteData(0xEE);

    /* ---- VCOM 控制 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_VMCTR1);
    ST7735S_WriteData(0x0F);

    /* ---- Gamma 校正（正极性） ---- */
    ST7735S_WriteCmd(ST7735S_CMD_GMCTRP1);
    ST7735S_WriteData(0x07); ST7735S_WriteData(0x0E);
    ST7735S_WriteData(0x08); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x10); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x02); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x09); ST7735S_WriteData(0x0F);
    ST7735S_WriteData(0x25); ST7735S_WriteData(0x36);
    ST7735S_WriteData(0x00); ST7735S_WriteData(0x08);
    ST7735S_WriteData(0x04); ST7735S_WriteData(0x10);

    /* ---- Gamma 校正（负极性） ---- */
    ST7735S_WriteCmd(ST7735S_CMD_GMCTRN1);
    ST7735S_WriteData(0x0A); ST7735S_WriteData(0x0D);
    ST7735S_WriteData(0x08); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x0F); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x02); ST7735S_WriteData(0x07);
    ST7735S_WriteData(0x09); ST7735S_WriteData(0x0F);
    ST7735S_WriteData(0x25); ST7735S_WriteData(0x35);
    ST7735S_WriteData(0x00); ST7735S_WriteData(0x09);
    ST7735S_WriteData(0x04); ST7735S_WriteData(0x10);

    /* ---- 开启扩展寄存器 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_EXTCMD);
    ST7735S_WriteData(0x80);

    /* ---- 像素格式 = RGB565 ---- */
    ST7735S_SetColorMode(ST7735S_COLMOD_16BIT);

    /* ---- 内存访问控制（BGR 色彩排列） ---- */
    ST7735S_SetMADCTL(ST7735S_MADCTL_MX | ST7735S_MADCTL_BGR);

    /* ---- 显示反转关闭 ---- */
    ST7735S_WriteCmd(ST7735S_CMD_INVOFF);

    /* ---- 设窗口偏移 ---- */
    ST7735S_SetWindow(2, 1, ST7735S_WIDTH + 1, ST7735S_HEIGHT);

    /* ---- 开显示 ---- */
    ST7735S_DisplayOn();

    /* ---- 清屏为黑色 ---- */
    ST7735S_FillScreen(ST7735S_BLACK);
}

/* ==========================================================================
 *  绘图函数
 * ========================================================================== */

void ST7735S_DrawPixel(uint8_t x, uint8_t y, uint16_t color)
{
    y = ST7735S_HEIGHT - 1 - y;    /* 翻转 Y 轴 */
    if (x >= ST7735S_WIDTH || y >= ST7735S_HEIGHT) return;
    ST7735S_SetWindow(x, y, x, y);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);
    ST7735S_WriteWord(color);
}

void ST7735S_FillScreen(uint16_t color)
{
    ST7735S_FillRect(0, 0, ST7735S_WIDTH - 1, ST7735S_HEIGHT - 1, color);
}

/* ---- 纯阻塞版 FillRect ---- */

void ST7735S_FillRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color)
{
    /* 翻转 Y 轴 */
    y1 = ST7735S_HEIGHT - 1 - y1;
    y2 = ST7735S_HEIGHT - 1 - y2;

    /* 范围裁剪 */
    if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }
    if (x1 >= ST7735S_WIDTH || y1 >= ST7735S_HEIGHT) return;

    uint32_t total = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    ST7735S_SetWindow(x1, y1, x2, y2);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    CS_Select();
    DC_Data();

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t pair[2] = { hi, lo };

    for (uint32_t i = 0; i < total; i++)
        SPI_TxBuf(pair, 2);

    CS_Release();
}

/* ---- DMA 加速版 FillRect ---- */

#ifdef ST7735S_USE_DMA

void ST7735S_FillRect_DMA(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color)
{
    /* 翻转 Y 轴 */
    y1 = ST7735S_HEIGHT - 1 - y1;
    y2 = ST7735S_HEIGHT - 1 - y2;

    if (x1 > x2) { uint8_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { uint8_t t = y1; y1 = y2; y2 = t; }
    if (x1 >= ST7735S_WIDTH || y1 >= ST7735S_HEIGHT) return;

    uint32_t total = (uint32_t)(x2 - x1 + 1) * (y2 - y1 + 1);

    ST7735S_SetWindow(x1, y1, x2, y2);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    /* 预填 DMA 缓冲区 */
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    for (uint32_t i = 0; i < ST7735S_DMA_BUF_SIZE; i += 2)
    {
        s_dma_buf[i]     = hi;
        s_dma_buf[i + 1] = lo;
    }

    CS_Select();
    DC_Data();

    uint32_t remaining = total;
    while (remaining > 0)
    {
        uint32_t chunk = remaining * 2;
        if (chunk > ST7735S_DMA_BUF_SIZE)
            chunk = ST7735S_DMA_BUF_SIZE;

        DMA_TxBuf(s_dma_buf, chunk);
        remaining -= chunk / 2;
    }

    CS_Release();
}

void ST7735S_DrawImage_DMA(const uint16_t *image)
{
    uint32_t total = ST7735S_WIDTH * ST7735S_HEIGHT;

    ST7735S_SetWindow(0, 0, ST7735S_WIDTH - 1, ST7735S_HEIGHT - 1);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    CS_Select();
    DC_Data();

    uint32_t remaining = total;
    uint32_t idx = 0;
    while (remaining > 0)
    {
        uint32_t cnt = remaining;
        uint32_t max_words = ST7735S_DMA_BUF_SIZE / 2;
        if (cnt > max_words) cnt = max_words;

        /* 将 RGB565 转为大端字节序 */
        for (uint32_t i = 0; i < cnt; i++)
        {
            s_dma_buf[i * 2]     = (uint8_t)(image[idx + i] >> 8);
            s_dma_buf[i * 2 + 1] = (uint8_t)(image[idx + i] & 0xFF);
        }

        DMA_TxBuf(s_dma_buf, cnt * 2);
        remaining -= cnt;
        idx += cnt;
    }

    CS_Release();
}

#endif /* ST7735S_USE_DMA */

/* ==========================================================================
 *  矩形 / 线
 * ========================================================================== */

void ST7735S_DrawRect(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, uint16_t color)
{
    ST7735S_DrawHLine(x1, y1, x2 - x1 + 1, color);
    ST7735S_DrawHLine(x1, y2, x2 - x1 + 1, color);
    ST7735S_DrawVLine(x1, y1, y2 - y1 + 1, color);
    ST7735S_DrawVLine(x2, y1, y2 - y1 + 1, color);
}

void ST7735S_DrawHLine(uint8_t x, uint8_t y, uint8_t length, uint16_t color)
{
    y = ST7735S_HEIGHT - 1 - y;    /* 翻转 Y 轴 */
    if (y >= ST7735S_HEIGHT || x >= ST7735S_WIDTH) return;
    if (x + length > ST7735S_WIDTH) length = ST7735S_WIDTH - x;
    if (length == 0) return;

    ST7735S_SetWindow(x, y, x + length - 1, y);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    CS_Select();
    DC_Data();

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t pair[2] = { hi, lo };

    for (uint16_t i = 0; i < length; i++)
        SPI_TxBuf(pair, 2);

    CS_Release();
}

void ST7735S_DrawVLine(uint8_t x, uint8_t y, uint8_t length, uint16_t color)
{
    if (x >= ST7735S_WIDTH || y >= ST7735S_HEIGHT) return;
    if (y + length > ST7735S_HEIGHT) length = ST7735S_HEIGHT - y;
    if (length == 0) return;

    ST7735S_SetWindow(x, y, x, y + length - 1);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    CS_Select();
    DC_Data();

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint8_t pair[2] = { hi, lo };

    for (uint16_t i = 0; i < length; i++)
        SPI_TxBuf(pair, 2);

    CS_Release();
}

/* ==========================================================================
 *  5x7 ASCII 字体渲染
 * ========================================================================== */

static const uint8_t s_font5x7[][5] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00 }, /* 0x20 space */
    { 0x00, 0x00, 0x5F, 0x00, 0x00 }, /* ! */
    { 0x00, 0x07, 0x00, 0x07, 0x00 }, /* " */
    { 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* # */
    { 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, /* $ */
    { 0x23, 0x13, 0x08, 0x64, 0x62 }, /* % */
    { 0x36, 0x49, 0x55, 0x22, 0x50 }, /* & */
    { 0x00, 0x05, 0x03, 0x00, 0x00 }, /* ' */
    { 0x00, 0x1C, 0x22, 0x41, 0x00 }, /* ( */
    { 0x00, 0x41, 0x22, 0x1C, 0x00 }, /* ) */
    { 0x08, 0x2A, 0x1C, 0x2A, 0x08 }, /* * */
    { 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* + */
    { 0x00, 0x50, 0x30, 0x00, 0x00 }, /* , */
    { 0x08, 0x08, 0x08, 0x08, 0x08 }, /* - */
    { 0x00, 0x60, 0x60, 0x00, 0x00 }, /* . */
    { 0x20, 0x10, 0x08, 0x04, 0x02 }, /* / */
    { 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* 0 */
    { 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* 1 */
    { 0x42, 0x61, 0x51, 0x49, 0x46 }, /* 2 */
    { 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* 3 */
    { 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* 4 */
    { 0x27, 0x45, 0x45, 0x45, 0x39 }, /* 5 */
    { 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* 6 */
    { 0x01, 0x71, 0x09, 0x05, 0x03 }, /* 7 */
    { 0x36, 0x49, 0x49, 0x49, 0x36 }, /* 8 */
    { 0x06, 0x49, 0x49, 0x29, 0x1E }, /* 9 */
    { 0x00, 0x36, 0x36, 0x00, 0x00 }, /* : */
    { 0x00, 0x56, 0x36, 0x00, 0x00 }, /* ; */
    { 0x00, 0x08, 0x14, 0x22, 0x41 }, /* < */
    { 0x14, 0x14, 0x14, 0x14, 0x14 }, /* = */
    { 0x41, 0x22, 0x14, 0x08, 0x00 }, /* > */
    { 0x02, 0x01, 0x51, 0x09, 0x06 }, /* ? */
    { 0x32, 0x49, 0x79, 0x41, 0x3E }, /* @ */
    { 0x7E, 0x11, 0x11, 0x11, 0x7E }, /* A */
    { 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* B */
    { 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* C */
    { 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* D */
    { 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* E */
    { 0x7F, 0x09, 0x09, 0x01, 0x01 }, /* F */
    { 0x3E, 0x41, 0x41, 0x51, 0x32 }, /* G */
    { 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* H */
    { 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* I */
    { 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* J */
    { 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* K */
    { 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* L */
    { 0x7F, 0x02, 0x04, 0x02, 0x7F }, /* M */
    { 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* N */
    { 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* O */
    { 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* P */
    { 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* Q */
    { 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* R */
    { 0x46, 0x49, 0x49, 0x49, 0x31 }, /* S */
    { 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* T */
    { 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* U */
    { 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* V */
    { 0x7F, 0x20, 0x18, 0x20, 0x7F }, /* W */
    { 0x63, 0x14, 0x08, 0x14, 0x63 }, /* X */
    { 0x03, 0x04, 0x78, 0x04, 0x03 }, /* Y */
    { 0x61, 0x51, 0x49, 0x45, 0x43 }, /* Z */
    { 0x00, 0x00, 0x7F, 0x41, 0x41 }, /* [ */
    { 0x02, 0x04, 0x08, 0x10, 0x20 }, /* \ */
    { 0x41, 0x41, 0x7F, 0x00, 0x00 }, /* ] */
    { 0x04, 0x02, 0x01, 0x02, 0x04 }, /* ^ */
    { 0x40, 0x40, 0x40, 0x40, 0x40 }, /* _ */
    { 0x00, 0x01, 0x02, 0x04, 0x00 }, /* ` */
    { 0x20, 0x54, 0x54, 0x54, 0x78 }, /* a */
    { 0x7F, 0x48, 0x44, 0x44, 0x38 }, /* b */
    { 0x38, 0x44, 0x44, 0x44, 0x20 }, /* c */
    { 0x38, 0x44, 0x44, 0x48, 0x7F }, /* d */
    { 0x38, 0x54, 0x54, 0x54, 0x18 }, /* e */
    { 0x08, 0x7E, 0x09, 0x01, 0x02 }, /* f */
    { 0x08, 0x14, 0x54, 0x54, 0x3C }, /* g */
    { 0x7F, 0x08, 0x04, 0x04, 0x78 }, /* h */
    { 0x00, 0x44, 0x7D, 0x40, 0x00 }, /* i */
    { 0x20, 0x40, 0x44, 0x3D, 0x00 }, /* j */
    { 0x00, 0x7F, 0x10, 0x28, 0x44 }, /* k */
    { 0x00, 0x41, 0x7F, 0x40, 0x00 }, /* l */
    { 0x7C, 0x04, 0x18, 0x04, 0x78 }, /* m */
    { 0x7C, 0x08, 0x04, 0x04, 0x78 }, /* n */
    { 0x38, 0x44, 0x44, 0x44, 0x38 }, /* o */
    { 0x7C, 0x14, 0x14, 0x14, 0x08 }, /* p */
    { 0x08, 0x14, 0x14, 0x18, 0x7C }, /* q */
    { 0x7C, 0x08, 0x04, 0x04, 0x08 }, /* r */
    { 0x48, 0x54, 0x54, 0x54, 0x20 }, /* s */
    { 0x04, 0x3F, 0x44, 0x40, 0x20 }, /* t */
    { 0x3C, 0x40, 0x40, 0x20, 0x7C }, /* u */
    { 0x1C, 0x20, 0x40, 0x20, 0x1C }, /* v */
    { 0x3C, 0x40, 0x30, 0x40, 0x3C }, /* w */
    { 0x44, 0x28, 0x10, 0x28, 0x44 }, /* x */
    { 0x0C, 0x50, 0x50, 0x50, 0x3C }, /* y */
    { 0x44, 0x64, 0x54, 0x4C, 0x44 }, /* z */
    { 0x00, 0x08, 0x36, 0x41, 0x00 }, /* { */
    { 0x00, 0x00, 0x7F, 0x00, 0x00 }, /* | */
    { 0x00, 0x41, 0x36, 0x08, 0x00 }, /* } */
    { 0x08, 0x08, 0x2A, 0x1C, 0x08 }, /* ~ */
};

void ST7735S_DrawChar(uint8_t x, uint8_t y, char ch, uint16_t color, uint16_t bg)
{
    if (ch < 0x20 || ch > 0x7E) ch = ' ';
    uint8_t idx = ch - 0x20;

    for (int8_t col = 0; col < 5; col++)
    {
        uint8_t line = s_font5x7[idx][col];
        for (int8_t row = 0; row < 7; row++)
        {
            if (line & (1 << (6 - row)))
                ST7735S_DrawPixel(x + col, y + (6 - row), color);
            else if (color != bg)
                ST7735S_DrawPixel(x + col, y + (6 - row), bg);
        }
    }
}

void ST7735S_DrawString(uint8_t x, uint8_t y, const char *str, uint16_t color, uint16_t bg)
{
    /* 不透明模式：先画背景矩形 */
    if (color != bg)
    {
        uint16_t len = 0;
        const char *p = str;
        while (*p++) len++;
        ST7735S_FillRect(x, y, x + len * 6 - 1, y + 7, bg);
    }

    while (*str)
    {
        if (x + 6 > ST7735S_WIDTH) break;
        ST7735S_DrawChar(x, y, *str, color, bg);
        x += 6;
        str++;
    }
}

void ST7735S_DrawImage(const uint16_t *image)
{
    uint32_t total = ST7735S_WIDTH * ST7735S_HEIGHT;

    ST7735S_SetWindow(0, 0, ST7735S_WIDTH - 1, ST7735S_HEIGHT - 1);
    ST7735S_WriteCmd(ST7735S_CMD_RAMWR);

    CS_Select();
    DC_Data();

    for (uint32_t i = 0; i < total; i++)
    {
        uint8_t buf[2] = { (uint8_t)(image[i] >> 8), (uint8_t)(image[i] & 0xFF) };
        SPI_TxBuf(buf, 2);
    }
    CS_Release();
}
