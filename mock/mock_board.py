"""
Mock to test the bridge and AWS setup without the board
"""

import paho.mqtt.client as mqtt
from argparse import ArgumentParser
import time, json, random

current_state = {
    "active": 0,
    "triggered": 0
}

def on_connect(client: mqtt.Client, userdata, flags, rc):
    print(f"Connected to MQTT-S broker, rc={rc}")
    client.subscribe("$aws/things/iscream/test_down")
    client.subscribe("$aws/things/iscream/shadow/update/accepted")
    client.subscribe("$aws/things/iscream/shadow/get/accepted")

def on_message(client: mqtt.Client, userdata, msg):
    global current_state

    print("received", msg.topic, msg.payload)

    if msg.topic == "$aws/things/iscream/shadow/update/accepted" or msg.topic == "$aws/things/iscream/shadow/get/accepted":
        data = json.loads(msg.payload)["state"]

        if "delta" in data:
            current_state["active"] = data["delta"]["active"]
            current_state["triggered"] = 0

            resp = {
                "state": {
                    "reported": current_state
                }
            }
            print(resp)
            client.publish("$aws/things/iscream/shadow/update", json.dumps(resp), 1)


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

    client.publish("$aws/things/iscream/shadow/get", "", 1)

    while True:

        if random.choice([0,1]) == 0:
            msg = {
                "avg": 35.2,
                "max": 40.8
            }

            # client.publish("$aws/things/iscream/sound_level", json.dumps(msg), 1)
        else:

            active = random.choice([1,0])
            triggered = 0
            if active:
                triggered = random.choice([1,0])

            msg = {
                "state": {
                    "reported": current_state
                }
            }

            # client.publish("$aws/things/iscream/shadow/update", json.dumps(msg), 1)

        time.sleep(10)


if __name__ == '__main__':
    main()
