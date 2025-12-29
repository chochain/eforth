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
    const char *uri,            ///< URL of MQTT broker
    const char *topic_get,      ///< input topic, i.g. gnii/mqtt/rst
    mqtt_hndl  get_hndl,        ///< message handler
    const char *topic_put,      ///< output topic, i.g. gnii/mqtt/cmd
    mqtt_hndl  put_hndl         ///< message handler
    ) {
    printf("Using server at %s\n", uri);

    _topic_put = topic_put;
    _topic_get = topic_get;
    
    _connect("rcvr", uri, &_rcvr, get_hndl);
//  _connect("sndr", uri, &_sndr, put_hndl);
    _subscribe();
}

MQTT::~MQTT() {
    _disconnect();
    ///
    /// wait for disconnected
    ///
    MQTTAsync_destroy(&_rcvr);
//    MQTTClient_destroy(&_sndr);
}

#include <cstring>
int MQTT::publish(char *payload) {
    mqtt_res_opts_t opts = MQTT_RES_INIT;
    opts.context   = _sndr;
    opts.onSuccess = _send_ok;
    opts.onFailure = _send_err;
    
    mqtt_msg_t msg = MQTT_MSG_INIT;
    
    msg.payload    = (void*)payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;

    int rc;
    if ((rc = MQTTAsync_sendMessage(_sndr, _topic_put, &msg, &opts)) != MQTTASYNC_SUCCESS) {
        printf("sndr: Failed to publish to %s, return code %d\n", _topic_put, rc);
        return rc;
    }
    return 0;
}

int MQTT::_connect(const char *id, const char *uri, mqtt_t *node, mqtt_hndl hndl) {
    int rc;
    if ((rc = MQTTAsync_create(
             node, uri, id, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTASYNC_SUCCESS) {
        printf("%s: Failed to create, return code %d\n", id, rc);
        return rc;
    }
    if ((rc = MQTTAsync_setCallbacks(
             *node, *node, _conn_lost, hndl, NULL)) != MQTTASYNC_SUCCESS) {
        printf("%s: Failed to set callbacks, return code %d\n", id, rc);
        return rc;
    }
    
    mqtt_conn_opts_t opts  = MQTT_CONN_INIT;
    opts.context           = _rcvr;
    opts.onSuccess         = _conn_ok;
    opts.onFailure         = _conn_err;
    opts.keepAliveInterval = 20;
    opts.cleansession      = 1;                     /// * remove all leftover messages
    
    if ((rc = MQTTAsync_connect(*node, &opts)) != MQTTASYNC_SUCCESS) {
        printf("%s: Failed to connect, return code %d\n", id, rc);
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
/*    
    if ((rc = MQTTAsync_disconnect(_sndr, &opts)) != MQTTASYNC_SUCCESS) {
    	printf("Failed to disconnect, return code %d\n", rc);
        return rc;
    }
*/
    if ((rc = MQTTAsync_disconnect(_rcvr, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to disconnect, return code %d\n", rc);
        return rc;
    }

    while (_conn_status != 0) usleep(10000L);
    return 0;
}

int MQTT::_subscribe() {
    mqtt_res_opts_t opts = MQTT_RES_INIT;
    opts.context   = _rcvr;
    opts.onSuccess = _sub_ok;
    opts.onFailure = _sub_err;
    
    int rc;
    if ((rc = MQTTAsync_subscribe(_rcvr, _topic_get, QOS, &opts)) != MQTTASYNC_SUCCESS) {
        printf("Failed to subscribe to %s, return code %d\n", _topic_get, rc);
        return rc;
    }
    printf("successfully subscribe to %s\n", _topic_get);

    while (_sub_status != 1) usleep(10000L);
    return 0;
}


