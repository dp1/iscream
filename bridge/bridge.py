import paho.mqtt.client as mqtt
from argparse import ArgumentParser
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTClient
import time

import conf

uplink_subscriptions = [
    # Test link
    "$aws/things/iscream/test_down",

    # Shadow Subscribe topics
    "$aws/things/iscream/shadow/get/accepted",
    "$aws/things/iscream/shadow/get/rejected",
    "$aws/things/iscream/shadow/update/delta",
    "$aws/things/iscream/shadow/update/accepted",
    "$aws/things/iscream/shadow/update/documents",
    "$aws/things/iscream/shadow/update/rejected",
    "$aws/things/iscream/shadow/delete/accepted",
    "$aws/things/iscream/shadow/delete/rejected"
]

downlink_subscriptions = [
    # Test link
    "$aws/things/iscream/test_up",

    # Audio level
    "$aws/things/iscream/sound_level",

    # Shadow Publish topics
    "$aws/things/iscream/shadow/get",
    "$aws/things/iscream/shadow/update",
    "$aws/things/iscream/shadow/delete"
]

paho_client: mqtt.Client = None
aws_client: AWSIoTMQTTClient = None

def on_connect(client, userdata, flags, rc):
    print(f"Connected to downlink MQTT-S broker, rc={rc}")
    # client.subscribe("iscream_uplink")

    for topic in downlink_subscriptions:
        client.subscribe(topic)
        print(f"[Downlink] Subscribed to {topic}")


def on_message(client, userdata, msg):
    global aws_client

    print("Received a new message from device: ")
    print(msg.payload)
    print("from topic: ")
    print(msg.topic)
    print("--------------\n\n")
    # client.publish("iscream_downlink", "This is a packet from the bridge")

    try:
        aws_client.publish(msg.topic, msg.payload.decode(), 1)
    except Exception as e:
        print(e)

def aws_callback(client, userdata, msg):
    global paho_client

    print("[AWS] Received a new message: ")
    print(msg.payload)
    print("from topic: ")
    print(msg.topic)
    print("--------------\n\n")

    # paho_client.publish("iscream_downlink", msg.payload)
    paho_client.publish(msg.topic, msg.payload, 1)

def main():
    global aws_client, paho_client

    parser = ArgumentParser(description="Transparent MQTT bridge")
    parser.add_argument('host', type=str, nargs='?', default='127.0.0.1')
    parser.add_argument('port', type=int, nargs='?', default=1886)
    args = parser.parse_args()

    aws_client = AWSIoTMQTTClient("bridge")
    aws_client.configureEndpoint(conf.host, 8883)
    aws_client.configureCredentials(conf.rootca, conf.privkey, conf.cert)

    # AWSIoTMQTTClient connection configuration
    aws_client.configureAutoReconnectBackoffTime(1, 32, 20)
    aws_client.configureOfflinePublishQueueing(-1)  # Infinite offline Publish queueing
    aws_client.configureDrainingFrequency(2)  # Draining: 2 Hz
    aws_client.configureConnectDisconnectTimeout(10)  # 10 sec
    aws_client.configureMQTTOperationTimeout(5)  # 5 sec

    aws_client.connect()
    print("Connected to AWS MQTT")

    for topic in uplink_subscriptions:
        aws_client.subscribe(topic, 1, aws_callback)
        print(f"[Uplink] Subscribed to {topic}")

    paho_client = mqtt.Client("bridge-client", clean_session=True)
    paho_client.on_connect = on_connect
    paho_client.on_message = on_message

    paho_client.connect(args.host, args.port)
    paho_client.loop_forever()

if __name__ == '__main__':
    main()
