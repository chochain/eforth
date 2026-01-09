///
/// @file
/// @brief - MQTT agent class
///
#include <unistd.h>
#include "mqtt.h"

#define CLINETID   "gnii_mqtt"
#define QOS        1
#define TIMEOUT    10000L

void MQTT::_conn_lost(void *ctx, char *cause) {
    printf("\nConnection lost\n");
    if (cause) printf("     cause: %s\n", cause);
}

int MQTT::_conn_status = 0;
int MQTT::_sub_status  = 0;

MQTT::MQTT(
    const char *id,
    const char *uri,            ///< URL of MQTT broker
    const char *topic_get,      ///< input topic,  device: gnii/mqtt/cmd, console: gnii/mqtt/rst
    const char *topic_put,      ///< output topic, device: gnii/mqtt/rst, console: gnii/mqtt/cmd
    mqtt_hndl  get_hndl         ///< message handler
    ) {
    init(id, uri, topic_get, topic_put, get_hndl);
}

MQTT::~MQTT() {
    _disconnect();
    ///
    /// wait for disconnected
    ///
    MQTTAsync_destroy(&_mqtt);
}

int MQTT::init(
    const char *id,
    const char *uri,            ///< URL of MQTT broker
    const char *topic_get,      ///< input topic,  device: gnii/mqtt/cmd, console: gnii/mqtt/rst
    const char *topic_put,      ///< output topic, device: gnii/mqtt/rst, console: gnii/mqtt/cmd
    mqtt_hndl  get_hndl         ///< message handler
    ) {
    printf("client_id=%s using broker at %s\n", id, uri);

    _id        = id;
    _uri       = uri;
    _topic_get = topic_get;
    _topic_put = topic_put;
    
    return 
        _connect(get_hndl) ||
        _subscribe();            ///< 0: success
}

#include <cstring>
int MQTT::publish(const char *payload) {
    mqtt_res_opts_t opts = MQTT_RES_INIT;
    opts.context   = _mqtt;
    opts.onSuccess = _send_ok;
    opts.onFailure = _send_err;
    
    mqtt_msg_t msg = MQTT_MSG_INIT;
    
    msg.payload    = (void*)payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;

    int rc;
    if ((rc = MQTTAsync_sendMessage(_mqtt, _topic_put, &msg, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to publish to %s, return code %d\n", _topic_put, rc);
        return rc;
    }
    printf("%s[%d] << %s", _topic_put, (int)strlen(payload), payload);
    return 0;
}

void MQTT::yield() {
    usleep(10000L);           ///< wait for 10ms (shouldn't affect Async ops)
}

int MQTT::_connect(mqtt_hndl get_hndl) {
    int rc;
    if ((rc = MQTTAsync_create(
             &_mqtt, _uri, _id, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS) {
        printf("Failed to create, return code %d\n", rc);
        return rc;
    }
    if ((rc = MQTTAsync_setCallbacks(
             _mqtt, _mqtt, _conn_lost, get_hndl, NULL)) != MQTTASYNC_SUCCESS) {
        printf("Failed to set callbacks, return code %d\n", rc);
        return rc;
    }
    
    mqtt_conn_opts_t opts  = MQTT_CONN_INIT;
    opts.context           = _mqtt;
    opts.onSuccess         = _conn_ok;
    opts.onFailure         = _conn_err;
    opts.keepAliveInterval = 20;
    opts.cleansession      = 1;                     /// * remove all leftover messages
    
    if ((rc = MQTTAsync_connect(_mqtt, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return rc;
    }

    while (_conn_status != 1) usleep(10000L);
    return 0;
}

int MQTT::_disconnect() {
    mqtt_disconn_opts_t opts = MQTT_DISCONN_INIT;
    opts.onSuccess = _disconn_ok;
    opts.onFailure = _disconn_err;
    
    int rc;
    if ((rc = MQTTAsync_disconnect(_mqtt, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to disconnect, return code %d\n", rc);
        return rc;
    }

    while (_conn_status != 0) usleep(10000L);
    return 0;
}

int MQTT::_subscribe() {
    mqtt_res_opts_t opts = MQTT_RES_INIT;
    opts.context   = _mqtt;
    opts.onSuccess = _sub_ok;
    opts.onFailure = _sub_err;
    
    int rc;
    if ((rc = MQTTAsync_subscribe(_mqtt, _topic_get, QOS, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to subscribe to %s, return code %d\n", _topic_get, rc);
        return rc;
    }
    printf("successfully subscribe to %s\n", _topic_get);

    while (_sub_status != 1) usleep(10000L);
    return 0;
}


