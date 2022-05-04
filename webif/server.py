from argparse import ArgumentParser
from AWSIoTPythonSDK.MQTTLib import AWSIoTMQTTShadowClient
import time, json
import boto3

import conf

def customShadowCallback_Get(payload, responseStatus, token):
    if responseStatus == "timeout":
        print("Shadow get request " + token + " time out!")
    if responseStatus == "accepted":
        payloadDict = json.loads(payload)
        print("~~~~~~~~~~~~~~~~~~~~~~~")
        print("Shadow get request with token: " + token + " accepted!")
        # print("property: " + str(payloadDict["state"]["desired"]["property"]))
        print(payloadDict)
        print("~~~~~~~~~~~~~~~~~~~~~~~\n\n")
    if responseStatus == "rejected":
        print("Shadow get request " + token + " rejected!")

def main():

    awsclient = AWSIoTMQTTShadowClient("webserver")
    awsclient.configureEndpoint(conf.host, 8883)
    awsclient.configureCredentials(conf.rootca, conf.privkey, conf.cert)

    # AWSIoTMQTTClient connection configuration
    awsclient.configureAutoReconnectBackoffTime(1, 32, 20)
    awsclient.configureConnectDisconnectTimeout(10)  # 10 sec
    awsclient.configureMQTTOperationTimeout(5)  # 5 sec

    awsclient.connect()
    print("Connected to AWS MQTT")

    shadow = awsclient.createShadowHandlerWithName("iscream", True)

    # dynamodb = boto3.client("dynamodb")
    # dynamodb_client = boto3.client('dynamodb', region_name="us-east-1")

    while True:
        shadow.shadowGet(customShadowCallback_Get, 5)
        time.sleep(10)

if __name__ == '__main__':
    main()

