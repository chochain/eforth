///
/// @file
/// @brief - MQTT agent class
/// @note  - Pipeline architecture, 1-input, filter, 1-output
///
#include "MQTTAsync.h"

typedef MQTTAsync                    mqtt_t;
typedef MQTTAsync_message            mqtt_msg_t;
typedef MQTTAsync_connectOptions     mqtt_conn_opts_t;
typedef MQTTAsync_responseOptions    mqtt_res_opts_t;
typedef MQTTAsync_disconnectOptions  mqtt_disconn_opts_t;
typedef MQTTAsync_successData        mqtt_ok_t;
typedef MQTTAsync_failureData        mqtt_err_t;

#define MQTT_CONN_INIT            MQTTAsync_connectOptions_initializer
#define MQTT_DISCONN_INIT         MQTTAsync_disconnectOptions_initializer
#define MQTT_RES_INIT             MQTTAsync_responseOptions_initializer
#define MQTT_MSG_INIT             MQTTAsync_message_initializer

typedef int (*mqtt_hndl)(void*, char*, int, mqtt_msg_t*); ///< message handler

class MQTT {
    static int  _conn_status;
    static int  _sub_status;
    
    const char  *_topic_put;
    const char  *_topic_get;
    
public:
    mqtt_t      sndr;
    mqtt_t      rcvr;

    static void _conn_ok    (void *ctx, mqtt_ok_t  *res) {
        _conn_status = 1;
        printf("_conn_ok token=%d\n",  res->token);
    }
    static void _conn_err   (void *ctx, mqtt_err_t *res) {
        _conn_status = -1;
        printf("_conn_err token=%d\n", res->token);
    }
    static void _sub_ok     (void *ctx, mqtt_ok_t  *res) {
        _sub_status = 1;
        printf("_sub_ok token=%d\n",   res->token);
    }
    static void _sub_err    (void *ctx, mqtt_err_t *res) {
        _sub_status = -1;
        printf("_sub_err token=%d\n",  res->token);
    }
    static void _send_ok    (void *ctx, mqtt_ok_t  *res) { printf("_send_ok token=%d\n",  res->token); }
    static void _send_err   (void *ctx, mqtt_err_t *res) { printf("_send_err token=%d\n", res->token); }
    static void _disconn_ok (void *ctx, mqtt_ok_t  *res) {
        _conn_status = 0;
        printf("_disconn_ok token=%d\n",  res->token);
    }
    static void _disconn_err(void *ctx, mqtt_err_t *res) {
        _conn_status = -2;
        printf("_disconn_err token=%d\n", res->token);
    }
    static void _conn_lost(void *ctx, char *cause);       /// connection lost handler
    static int  _msg_arrived(void *ctx, char *topic, int len, mqtt_msg_t *msg);
    
    MQTT(const char *uri,
         const char *topic_get,
         mqtt_hndl  get_hndl,
         const char *topic_put,
         mqtt_hndl  put_hndl);
    ~MQTT();
    
    int publish(const char *id, mqtt_t *node, const char *payload);

private:
    int _connect(const char *id, const char *rui, mqtt_t *node, mqtt_hndl hndl);
    int _subscribe();
    int _disconnect();
};
