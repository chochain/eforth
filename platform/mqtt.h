///
/// @file
/// @brief - MQTT agent class
/// @note  - Pipeline architecture, 1-input, filter, 1-output
///
#include "MQTTClient.h"

typedef MQTTClient                mqtt_t;
typedef MQTTClient_message        mqtt_msg_t;
typedef MQTTClient_connectOptions mqtt_opts_t;
typedef MQTTClient_deliveryToken  mqtt_token_t;
typedef int (*mqtt_hndl)(void*, char*, int, mqtt_msg_t*); ///< message handler

#define MQTT_CONN_INIT            MQTTClient_connectOptions_initializer
#define MQTT_MSG_INIT             MQTTClient_message_initializer        

class MQTT {
    const char  *_topic_put;
    const char  *_topic_get;
    
    mqtt_t      _sndr;
    mqtt_t      _rcvr;
    int         _status;
    int         _token;

public:
    static void _delivered(void *ctx, mqtt_token_t dt);   /// message delivery notifier
    static void _conn_lost(void *ctx, char *cause);       /// connection lost handler
    
    MQTT(const char *uri,
         const char *topic_get,
         mqtt_hndl  get_hndl,
         const char *topic_put,
         mqtt_hndl  put_hndl);
    ~MQTT();
    
    int publish(char *payload);

private:
    int _setup(const char *id, const char *rui, mqtt_t *node, mqtt_hndl hndl);
    int _subscribe();
    int _connect();
    int _disconnect();
};
