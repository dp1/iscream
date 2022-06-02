import threading
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTShadowClient
from AWSIoTPythonSDK.core.shadow.deviceShadow import deviceShadow
import time, json
import boto3
from flask import Flask, render_template, request

import conf

app = Flask(__name__)
app.config["TEMPLATES_AUTO_RELOAD"] = True

current_state = {
    "active": 0,
    "triggered": 0
}

shadow: deviceShadow = None

def shadow_getter(shadow):
    """
    Runs in background and periodically gets the reported shadow state
    """

    def shadow_callback(payload, responseStatus, token):
        global current_state

        if responseStatus == "timeout":
            print("Shadow get request " + token + " time out!")
        if responseStatus == "accepted":
            payloadDict = json.loads(payload)
            print("Shadow get request with token: " + token + " accepted!")
            print(payloadDict)

            if "state" not in payloadDict or "reported" not in payloadDict["state"]:
                print("Incomplete, skipping")
                return

            for key in current_state.keys():
                if key in payloadDict["state"]["reported"]:
                    current_state[key] = payloadDict["state"]["reported"][key]
                    print(f"current_state[\"{key}\"] = {current_state[key]}")

        if responseStatus == "rejected":
            print("Shadow get request " + token + " rejected!")

    while True:
        shadow.shadowGet(shadow_callback, 5)
        time.sleep(10)

@app.route('/')
def index():
    return render_template('home.html', current_state=current_state)

@app.route('/state', methods=['GET', 'POST'])
def state():
    global current_state, shadow

    if request.method == 'GET':
        return json.dumps(current_state)
    else:
        enable = 1 if (request.json['action'] == 'enable') else 0

        payload = {
            "state": {
                "desired": {
                    "active": enable,
                    "triggered": 0
                }
            }
        }

        def shadow_update_callback(payload, responseStatus, token):

            if responseStatus == "timeout":
                print("Shadow update request " + token + " time out!")
            if responseStatus == "accepted":
                payloadDict = json.loads(payload)
                print("Shadow update request with token: " + token + " accepted!")
                print(payloadDict)
            if responseStatus == "rejected":
                print("Shadow update request " + token + " rejected!")

        shadow.shadowUpdate(json.dumps(payload), shadow_update_callback, 5)

        return "{}"


def main():
    global awsclient, shadow

    awsclient = AWSIoTMQTTShadowClient("webserver")
    awsclient.configureEndpoint(conf.host, 8883)
    awsclient.configureCredentials(conf.rootca, conf.privkey, conf.cert)

    # AWSIoTMQTTClient connection configuration
    awsclient.configureAutoReconnectBackoffTime(1, 32, 20)
    awsclient.configureConnectDisconnectTimeout(10)  # 10 sec
    awsclient.configureMQTTOperationTimeout(5)  # 5 sec

    awsclient.connect()
    shadow = awsclient.createShadowHandlerWithName("iscream", True)
    print("Connected to AWS MQTT")

    threading.Thread(target=shadow_getter, args=(shadow,)).start()

    # dynamodb = boto3.client("dynamodb")
    # dynamodb_client = boto3.client('dynamodb', region_name="us-east-1")

if __name__ == '__main__':
    main()
    app.run('0.0.0.0', 8000)
