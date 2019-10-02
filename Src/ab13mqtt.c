/* Includes ------------------------------------------------------------------*/
#include "user_comm.h"

static char subscribeTopic[36], publishTopic[36];
static void MQTTMsgHandler(uint8_t* dat, uint16_t len);
static void logicConsole(int argc, char* argv[]);

void AB13Logic_Init(void) {
    // 要订阅的主题是: D/MHC000000000005
    strcpy(subscribeTopic, "D/");
    strcat(subscribeTopic, WorkParam.mqtt.MQTT_ClientID);
    // 用于发布的主题是: U/report
    strcpy(publishTopic, "U/report");

    CMD_ENT_DEF(ab13logic, logicConsole);
    Cmd_AddEntrance(CMD_ENT(ab13logic));
    // 开启订阅
    Subscribe_MQTT(subscribeTopic, QOS2, MQTTMsgHandler);
    DBG_LOG("Process Start.");

}



// mqtt接收到的消息在这里处理
static void MQTTMsgHandler(uint8_t* dat, uint16_t len) {

}

// CMD console
static void logicConsole(int argc, char* argv[]) {

}

void MainLogicPolling(void){


}


