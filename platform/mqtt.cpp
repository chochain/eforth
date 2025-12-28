///
/// @file
/// @brief - MQTT agent class
///
#include "mqtt.h"

#define QOS        1
#define TIMEOUT    10000L

void MQTT::_delivered(void *ctx, mqtt_token_t token) {
    printf("Message with token value %d delivery confirmed\n", token);
}

void MQTT::_conn_lost(void *ctx, char *cause) {
    printf("\nConnection lost\n");
    if (cause) printf("     cause: %s\n", cause);
}

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
    
    _status =
        _setup("rcvr", uri, &_rcvr, get_hndl) ||
        _setup("sndr", uri, &_sndr, put_hndl) ||
        _connect()                            ||
        _subscribe();
}

MQTT::~MQTT() {
    _disconnect();
    
    MQTTClient_destroy(&_rcvr);
    MQTTClient_destroy(&_sndr);
}

#include <cstring>
int MQTT::publish(char *payload) {
    mqtt_msg_t msg = MQTT_MSG_INIT;
    
    msg.payload    = (void*)payload;
    msg.payloadlen = (int)strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;

    int rc;
    if ((rc = MQTTClient_publishMessage(_sndr, _topic_put, &msg, &_token)) != MQTTCLIENT_SUCCESS) {
        printf("sndr: Failed to publish to %s, return code %d\n", _topic_put, rc);
        goto bail;
    }
    printf("sndr: sent %s, waiting for up to %d ms on topic %s\n",
           payload, (int)TIMEOUT, _topic_put);
    
    rc = MQTTClient_waitForCompletion(_sndr, _token, TIMEOUT);
    printf("sndr: Message with delivery token %d delivered\n", _token);
    
bail:
    return 0;
}

int MQTT::_setup(const char *id, const char *uri, mqtt_t *node, mqtt_hndl hndl) {
    int rc;
    if ((rc = MQTTClient_create(
             node, uri, id, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("%s: Failed to create, return code %d\n", id, rc);
        goto bail;
    }
    if ((rc = MQTTClient_setCallbacks(
             node, NULL,
             MQTT::_conn_lost, hndl, MQTT::_delivered)) != MQTTCLIENT_SUCCESS) {
        printf("%s: Failed to set callbacks, return code %d\n", id, rc);
        goto bail;
    }
    rc = 0;
bail:
    return rc;
}

int MQTT::_connect() {
    _opts.keepAliveInterval = 20;
    _opts.cleansession = 1;
    
    int rc;
    if ((rc = MQTTClient_connect(_rcvr, &_opts)) != MQTTCLIENT_SUCCESS) {
        printf("rcvr: Failed to connect, return code %d\n", rc);
        goto bail;
    }
    if ((rc = MQTTClient_connect(_sndr, &_opts)) != MQTTCLIENT_SUCCESS) {
        printf("sndr: Failed to connect, return code %d\n", rc);
        goto bail;
    }
    rc = 0;
bail:
    return rc;
}

int MQTT::_disconnect() {
    int rc;
    if ((rc = MQTTClient_unsubscribe(_rcvr, _topic_get)) != MQTTCLIENT_SUCCESS) {
        printf("rcvr: Failed to unsubscribe, return code %d\n", rc);
        goto bail;
    }
    if ((rc = MQTTClient_disconnect(_rcvr, TIMEOUT)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to disconnect, return code %d\n", rc);
        goto bail;
    }
    if ((rc = MQTTClient_disconnect(_sndr, TIMEOUT)) != MQTTCLIENT_SUCCESS) {
    	printf("Failed to disconnect, return code %d\n", rc);
        goto bail;
    }
    rc = 0;
bail:    
    return rc;
}

int MQTT::_subscribe() {
    int rc;
    if ((rc = MQTTClient_subscribe(_rcvr, _topic_get, QOS)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe to %s, return code %d\n", _topic_get, rc);
        goto bail;
    }
    rc = 0;
bail:    
    return rc;
}


