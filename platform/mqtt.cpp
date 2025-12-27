///
/// 
///
#include "mqtt.h"

#define CLIENTID    "gnii_mqtt"
#define TOPIC       "qnii"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

void MQTT::_delivered(void *ctx, MQTTClient_deliveryToken token) {
    printf("Message with token value %d delivery confirmed\n", token);
}

int MQTT::_arrived(void *ctx, char *topic, int len, MQTTClient_message *msg) {
    printf("Message arrived\n");
    printf("     topic: %s\n", topic);
    printf("   message: %.*s\n", msg->payloadlen, (char*)msg->payload);
    
    MQTTClient_freeMessage(&msg);
    MQTTClient_free(topic);

    return 1;
}

void MQTT::_conn_lost(void *ctx, char *cause) {
    printf("\nConnection lost\n");
    if (cause) printf("     cause: %s\n", cause);
}

MQTT::MQTT(const char *uri) {
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
             MQTT::_conn_lost, MQTT::_arrived, MQTT::_delivered)) != MQTTCLIENT_SUCCESS) {
        printf("rcvr: Failed to set callbacks, return code %d\n", rc);
        goto bail;
    }
    rc = 0;
bail:
    _status = rc;
}

MQTT::~MQTT() {
    MQTTClient_destroy(&_rcvr);
    MQTTClient_destroy(&_sndr);
}

int MQTT::connect() {
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
    printf("rcvr: Subscribing to topic %s\nfor %s using QoS%d\n\n"
           "Press Q<Enter> to quit\n\n", TOPIC, CLIENTID, QOS);
    rc = 0;
bail:
    return rc;
}

#include <cstring>
int MQTT::publish(void *payload) {
    MQTTClient_message msg = MQTTClient_message_initializer;
    
    msg.payload    = payload;
    msg.payloadlen = (int)strlen(PAYLOAD);
    msg.qos        = QOS;
    msg.retained   = 0;

    int rc;
    if ((rc = MQTTClient_publishMessage(_sndr, TOPIC, &msg, &_token)) != MQTTCLIENT_SUCCESS) {
         printf("sndr: Failed to publish message, return code %d\n", rc);
         goto bail;
    }
    printf("sndr: Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
           (int)(TIMEOUT/1000), (char*)payload, TOPIC, CLIENTID);
    
    rc = MQTTClient_waitForCompletion(_sndr, _token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", _token);
    
bail:
    return 0;
}

int MQTT::subscribe() {
    int rc;
    if ((rc = MQTTClient_subscribe(_rcvr, TOPIC, QOS)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to subscribe, return code %d\n", rc);
        goto bail;
    }
    else {
        int ch;
        do {
            ch = getchar();
        } while (ch!='Q' && ch != 'q');

        if ((rc = MQTTClient_unsubscribe(_rcvr, TOPIC)) != MQTTCLIENT_SUCCESS) {
            printf("rcvr: Failed to unsubscribe, return code %d\n", rc);
            goto bail;
        }
    }
    rc = 0;
bail:    
    return rc;
}

int MQTT::disconnect() {
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
