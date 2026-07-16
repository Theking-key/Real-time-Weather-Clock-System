#include "stm32f1xx_hal.h"
#include "string.h"
#include <stdbool.h>
#include <stdio.h>
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart1;

#define ESP_RESPONSE_BUFFER_SIZE 300

typedef struct {
    char response[ESP_RESPONSE_BUFFER_SIZE]; // 存储响应的缓冲区
    size_t length;      // 响应的长度
    char ssid[32];      // WiFi SSID
    uint8_t idlestatus;      // idle
} ESP_t;

typedef ESP_t* ESP_HandleTypeDef;

void HAL_UART_RxIdleCallback(UART_HandleTypeDef *huart);

void ESP_Init(void);

bool ESP_AT(void);

void ESP_Reset(void);

bool ESP_CWJAP(const char* ssid, const char* password);

bool ESP_CHECK_WIFI(void);

bool ESP_DISCONNECT(void);

bool ESP_HTTP(const char* url);


