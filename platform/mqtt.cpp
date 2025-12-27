///
/// 
///
#include "mqtt.h"

#define CLIENTID   "gnii_mqtt"
#define QOS        1
#define TIMEOUT    10000L

void MQTT::_delivered(void *ctx, mqtt_token_t token) {
    printf("Message with token value %d delivery confirmed\n", token);
}

void MQTT::_conn_lost(void *ctx, char *cause) {
    printf("\nConnection lost\n");
    if (cause) printf("     cause: %s\n", cause);
}

MQTT::MQTT(const char *uri, const char *topic_put, const char *topic_get, int (*callback)(void*, char*, int, mqtt_msg_t*)) {
    _topic_put = topic_put;
    _topic_get = topic_get;
    int rc;
    if ((rc = MQTTClient_create(
             &_rcvr, uri, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create rcvr, return code %d\n", rc);
        goto bail;
    }
    if ((rc = MQTTClient_create(
             &_sndr, uri, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to create sndr, return code %d\n", rc);
        goto bail;
    }
    if ((rc = MQTTClient_setCallbacks(
             _rcvr, NULL,
             MQTT::_conn_lost, callback, MQTT::_delivered)) != MQTTCLIENT_SUCCESS) {
        printf("rcvr: Failed to set callbacks, return code %d\n", rc);
        goto bail;
    }
    rc = _connect();
bail:
    _status = rc;
}

MQTT::~MQTT() {
    _disconnect();
    MQTTClient_destroy(&_rcvr);
    MQTTClient_destroy(&_sndr);
}

#include <cstring>
int MQTT::publish(void *payload) {
    mqtt_msg_t msg = MQTT_MSG_INIT;
    
    msg.payload    = payload;
    msg.payloadlen = (int)strlen((char*)payload);
    msg.qos        = QOS;
    msg.retained   = 0;

    int rc;
    if ((rc = MQTTClient_publishMessage(_sndr, _topic_put, &msg, &_token)) != MQTTCLIENT_SUCCESS) {
         printf("sndr: Failed to publish message, return code %d\n", rc);
         goto bail;
    }
    printf("sndr: Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
           (int)(TIMEOUT/1000), (char*)payload, _topic_put, CLIENTID);
    
    rc = MQTTClient_waitForCompletion(_sndr, _token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", _token);
    
bail:
    return 0;
}

int MQTT::subscribe() {
    int rc;
    if ((rc = MQTTClient_subscribe(_rcvr, _topic_get, QOS)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe, return code %d\n", rc);
        goto bail;
    }
    else {
        int ch;
        do {
            ch = getchar();
        } while (ch!='Q' && ch != 'q');

        if ((rc = MQTTClient_unsubscribe(_rcvr, _topic_get)) != MQTTCLIENT_SUCCESS) {
            printf("rcvr: Failed to unsubscribe, return code %d\n", rc);
            goto bail;
        }
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
