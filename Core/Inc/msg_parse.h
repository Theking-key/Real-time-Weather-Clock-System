#ifndef MSG_PARSE_H
#define MSG_PARSE_H

#include "cmsis_os.h"


typedef struct {
    uint8_t msgtype;
    uint8_t length;
    char *data;
} Message;

typedef Message *MessagePtr;

void SendMessage(uint8_t msgtype, uint8_t length, char *data);

void ReceiveMessage(void);

void Message_parse(uint8_t msgtype, uint8_t length, char *data);

#define ESP_DATA  1
#define LCD_DATA  2
#endif // MSG_PARSE_H
