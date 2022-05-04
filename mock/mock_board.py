"""
Mock to test the bridge and AWS setup without the board
"""

import paho.mqtt.client as mqtt
from argparse import ArgumentParser
import time, json, random

def on_connect(client: mqtt.Client, userdata, flags, rc):
    print(f"Connected to MQTT-S broker, rc={rc}")
    client.subscribe("$aws/things/iscream/test_down")
    client.subscribe("$aws/things/iscream/shadow/update/accepted")

def on_message(client: mqtt.Client, userdata, msg):
    print("received", msg.topic, msg.payload)

def main():

    parser = ArgumentParser(description="Mock for MQTT bridge testing")
    parser.add_argument('host', type=str, nargs='?', default='127.0.0.1')
    parser.add_argument('port', type=int, nargs='?', default=1886)
    args = parser.parse_args()

    client = mqtt.Client("mock-board", clean_session=True)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(args.host, args.port)
    client.loop_start()

    while True:

        if random.choice([0,1]) == 0:
            msg = {
                "avg": 35.2,
                "max": 40.8
            }

            client.publish("$aws/things/iscream/sound_level", json.dumps(msg), 1)
        else:

            active = random.choice([True,False])
            triggered = False
            if active:
                triggered = random.choice([True,False])

            msg = {
                "state": {
                    "reported": {
                        "active": active,
                        "triggered": triggered,
                    }
                }
            }

            client.publish("$aws/things/iscream/shadow/update", json.dumps(msg), 1)

        time.sleep(10)


if __name__ == '__main__':
    main()
