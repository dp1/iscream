#ifndef _NETWORK_H
#define _NETWORK_H

typedef void (*on_pub_handler_t)(const emcute_topic_t *topic, void *data, size_t len);

int setup_mqtt(on_pub_handler_t on_pub);
int mqtt_pub(char* topic, const char* data, int qos);

#endif
