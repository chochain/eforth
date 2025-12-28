///
/// @file
/// @brief - MQTT agent class
/// @note  - Pipeline architecture, 1-input, filter, 1-output
///
#include "MQTTClient.h"

typedef MQTTClient_message        mqtt_msg_t;
typedef MQTTClient_deliveryToken  mqtt_token_t;
#define MQTT_CONN_INIT            MQTTClient_connectOptions_initializer
#define MQTT_MSG_INIT             MQTTClient_message_initializer        

class MQTT {
    MQTTClient_connectOptions _opts = MQTT_CONN_INIT;
    MQTTClient _sndr;
    MQTTClient _rcvr;
    int        _status;
    int        _token;
    const char *_topic_put;
    const char *_topic_get;

public:
    static void _delivered(void *ctx, mqtt_token_t dt);   /// message delivery notifier
    static void _conn_lost(void *ctx, char *cause);       /// connection lost handler
    
    MQTT(const char *uri,
         const char *topic_get,
         int (*hndl)(void*, char*, int, mqtt_msg_t*),
         const char *topic_put);
    ~MQTT();
    
    int publish(char *payload);

private:    
    int _subscribe();
    int _connect();
    int _disconnect();
};
