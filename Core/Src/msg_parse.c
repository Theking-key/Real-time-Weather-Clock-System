#include "msg_parse.h"
#include "main.h"
#include "stdbool.h"
#include "string.h"

extern WeatherInfo_t weather_info; // 声明全局变量
extern osMessageQueueId_t Data_Parsing_queueHandle; // 声明消息队列句柄
extern osSemaphoreId_t LCD_refreshHandle; // 声明信号量句柄

static bool SubString(char *src, const char *start_mark, const char *end_mark, char *dst, uint16_t dst_len)
{
    char *start_p = strstr(src, start_mark);
    if(start_p == NULL)
        return false;

    start_p += strlen(start_mark);
    char *end_p = strstr(start_p, end_mark);
    if(end_p == NULL)
        return false;

    uint16_t copy_len = end_p - start_p;
    if(copy_len >= dst_len)
        return false;

    memcpy(dst, start_p, copy_len);
    dst[copy_len] = '\0';
    return true;
}

void SendMessage(uint8_t msgtype, uint8_t length, char *data) {
    Message msg;
    msg.msgtype = msgtype;
    msg.length = length;
    msg.data = data;

    // Send the message to the queue
    osMessageQueuePut(Data_Parsing_queueHandle, &msg, 0, osWaitForever);
}

void ReceiveMessage(void) {
    Message msg;
    // Receive the message from the queue
    if (osMessageQueueGet(Data_Parsing_queueHandle, &msg, NULL, osWaitForever) == osOK) {
        // Process the received message
        Message_parse(msg.msgtype, msg.length, msg.data);
    }
}

void Message_parse(uint8_t msgtype, uint8_t length, char *data) {
    // Implement your message processing logic here
    if (msgtype == ESP_DATA) {

        memset(&weather_info, 0, sizeof(WeatherInfo_t));

    // 1. 提取城市 name:"北京"
    if(!SubString(data, "\"name\":\"", "\"", weather_info.city, sizeof(weather_info.city)));

    // 2. 提取天气文字 text":"多云"
    if(!SubString(data, "\"text\":\"", "\"", weather_info.weather_text, sizeof(weather_info.weather_text)));

    // 3. 提取天气code code":"4"
    if(!SubString(data , "\"code\":\"", "\"", weather_info.weather_code, sizeof(weather_info.weather_code)));

    // 4. 提取温度 temperature":"28"
    if(!SubString(data, "\"temperature\":\"", "\"", weather_info.temp, sizeof(weather_info.temp)));

    osSemaphoreRelease(LCD_refreshHandle);
        // Handle ESP data
    } else if (msgtype == LCD_DATA) {
        // Handle LCD data
    }
}
