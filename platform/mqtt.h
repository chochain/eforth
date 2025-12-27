///
///
///
#include "MQTTClient.h"

class MQTT {
    MQTTClient_connectOptions _opts = MQTTClient_connectOptions_initializer;
    MQTTClient                _sndr, _rcvr;
    int                       _status;
    int                       _token;

public:    
    static void _delivered(void *ctx, MQTTClient_deliveryToken dt);
    static int  _arrived(void *ctx, char *topic, int len, MQTTClient_message *msg); /// message arrived(topic_name, len)
    static void _conn_lost(void *ctx, char *cause);                                 /// connection lost
    
    MQTT(const char *uri);
    ~MQTT();
    
    int connect();
    int subscribe();
    int publish(void *payload);
    int disconnect();
};
