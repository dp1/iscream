import paho.mqtt.client as mqtt
from argparse import ArgumentParser

def on_connect(client, userdata, flags, rc):
    print(f"Connected, result {rc}")
    client.subscribe("iscream_uplink")

def on_message(client, userdata, msg):
    print("received", msg.topic, msg.payload)
    client.publish("iscream_downlink", "This is a packet from the bridge")

def main():

    parser = ArgumentParser(description="Transparent MQTT bridge")
    parser.add_argument('host', type=str, nargs='?', default='127.0.0.1')
    parser.add_argument('port', type=int, nargs='?', default=1886)
    args = parser.parse_args()

    client = mqtt.Client("bridge-client", clean_session=True)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(args.host, args.port)
    client.loop_forever()

if __name__ == '__main__':
    main()
