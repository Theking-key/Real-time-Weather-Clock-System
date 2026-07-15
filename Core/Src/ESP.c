#include "ESP.h"
#include "cmsis_os.h"

ESP_t esp_storage;

ESP_HandleTypeDef esp_t = &esp_storage;
void HAL_UART_RxIdleCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        
        // 处理接收到的数据
        esp_t->length = ESP_RESPONSE_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);
        esp_t->response[esp_t->length] = '\0'; // 添加字符串结束符        
        HAL_UART_DMAStop(&huart2); // 停止DMA接收
		esp_t->idlestatus = 1; // 设置idle状态为1

    }
}

void HAL_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART2) {
        // 处理接收到的数据
        esp_t->length = ESP_RESPONSE_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart->hdmarx);

        esp_t->response[esp_t->length] = '\0'; // 添加字符串结束符
        HAL_UART_DMAStop(&huart2); // 停止DMA接收

    }
}



void ESP_Init(void) {
    // 初始化ESP模块
	
	
	__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
	while ( ESP_AT() == false ){
	HAL_Delay(2000);
    ESP_Reset();
	}
    ESP_AT();
    
}

bool ESP_AT(void)
{
    memset(esp_t->response, 0, ESP_RESPONSE_BUFFER_SIZE);
	HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 100);
    HAL_UART_Receive_DMA(&huart2, (uint8_t*)esp_t->response, ESP_RESPONSE_BUFFER_SIZE); 
    while(esp_t->idlestatus == 0) {
        HAL_Delay(100);
        // 等待接收完成
    }
    esp_t->idlestatus = 0; // 重置idle状态为0
    if (strstr(esp_t->response, "OK") != NULL) {
        // ESP模块响应正常
        return true;
    } else {
        // ESP模块响应异常，进行错误处理
        return false;
    }
}
bool ESP_CHECK_WIFI(void)
{
    memset(esp_t->response, 0, ESP_RESPONSE_BUFFER_SIZE);
    HAL_UART_Transmit(&huart2, (uint8_t*)"AT+CWJAP?\r\n", 12, 100);
    HAL_UART_Receive_DMA(&huart2, (uint8_t*)esp_t->response, ESP_RESPONSE_BUFFER_SIZE);
    
    while(esp_t->idlestatus == 0) {
        HAL_Delay(100);
        // 等待接收完成
    }
    esp_t->idlestatus = 0; // 重置idle状态为0
    char* p_head = strstr(esp_t->response, "\"");
    if(p_head == NULL) return false;
    p_head++; 
   
    uint16_t i = 0;

    while(*p_head != '"' && *p_head != '\0' && i < sizeof(esp_t->ssid)-1)
    {
       esp_t->ssid[i++] = *p_head++;
    }
   esp_t->ssid[i] = '\0'; // 字符串结束符

	HAL_UART_Transmit(&huart1, (uint8_t*)esp_t->ssid, strlen(esp_t->ssid), 100);
	return true;
    
}

bool ESP_DISCONNECT(void)
{
    memset(esp_t->response, 0, ESP_RESPONSE_BUFFER_SIZE);
    HAL_UART_Transmit(&huart2, (uint8_t*)"AT+CWQAP\r\n", 10, 100);
    HAL_UART_Receive_DMA(&huart2, (uint8_t*)esp_t->response, ESP_RESPONSE_BUFFER_SIZE);
    while(esp_t->idlestatus == 0) {
        HAL_Delay(100);
        // 等待接收完成
    }
    esp_t->idlestatus = 0; // 重置idle状态为0
    if (strstr(esp_t->response, "OK") != NULL) {
        return true;
    } else {
        return false;
    }
}

void ESP_Reset(void)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)"AT+RST\r\n", 8, 100);
	HAL_Delay(2000);
    esp_t->idlestatus = 0;
}

bool ESP_CWJAP(const char* ssid, const char* password)
{

    memset(esp_t->response, 0, ESP_RESPONSE_BUFFER_SIZE);



    char command[100];
    snprintf(command, sizeof(command), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
	
    HAL_UART_Transmit(&huart2, (uint8_t*)command, strlen(command), 100);
	
    HAL_UART_Receive_DMA(&huart2, (uint8_t*)esp_t->response,ESP_RESPONSE_BUFFER_SIZE);
    while(esp_t->idlestatus == 0) {
        HAL_Delay(100);
        // 等待接收完成
    }
    esp_t->idlestatus = 0; // 重置idle状态为0
    if (strstr(esp_t->response, "CONNECT_FAILED") != NULL) {
        // 连接失败
        return false;
    } else {
        // 连接成功，进行错误处理
        return true;
    }
}

bool ESP_HTTP(const char* url)
{
    memset(esp_t->response, 0, ESP_RESPONSE_BUFFER_SIZE);
	char command[200];
    snprintf(command , sizeof(command), "AT+HTTPGET=%s\r\n", url);
    HAL_UART_Transmit(&huart2, (uint8_t*)command, strlen(command), 10*strlen(command));
    HAL_UART_Receive_DMA(&huart2, (uint8_t*)esp_t->response, ESP_RESPONSE_BUFFER_SIZE);
    while(esp_t->idlestatus == 0) {
        HAL_Delay(100);
        // 等待HTTP请求完成
    }
    esp_t->idlestatus = 0; // 重置idle状态为0
    if (strstr(esp_t->response, "OK") != NULL) {
        // HTTP请求成功 
        
        return true;
    } else {
        // HTTP请求失败，进行错误处理
        return false;
    }
}
