#ifndef _NETWORK_H
#define _NETWORK_H

typedef void (*on_pub_handler_t)(const emcute_topic_t *topic, void *data, size_t len);

// on_pub will be called when a message is received on any of the topics
// in sub_topics. sub_topics must end with a NULL pointer
int setup_mqtt(const char **sub_topics, on_pub_handler_t on_pub);
int mqtt_pub(char* topic, const char* data, int qos);

#endif
