#ifndef MSG_PARSE_H
#define MSG_PARSE_H
typedef struct {
    uint8_t msgtype;
    uint8_t length;
    char *data;
} Message;

typedef Message *MessagePtr;

void SendMessage(uint8_t msgtype, uint8_t length, char *data);

void ReceiveMessage(void);

void Message_parse_esp(uint8_t length, char *data);

void Message_parse_lcd(uint8_t length, char *data);

#define ESP_DATA  1
#define LCD_DATA  2
#endif // MSG_PARSE_H
