# iScream🍦

IoT alarm system with sound level triggering

# Hardware

The device is based around a [B-L475E-IOT01A
Discovery Kit](https://www.st.com/en/evaluation-tools/b-l475e-iot01a.html), an evaluation board with many peripherals. The main peripheral used in this project is the [MP34DT01-M](https://www.st.com/en/audio-ics/mp34dt01-m.html) digital microphone, which is used to monitor the noise level in a room and, when needed, trigger the alarm. There are two such microphones on the board, but only one is used since the project doesn't require the stereo audio and beamforming capabilities of the board.

An [IR receiver module](https://www.sunfounder.com/products/infrared-receiver-module) and its associated remote are used for turning the alarm on and off and to stop the alarm once triggered. Additionally, a [buzzer module](https://www.sunfounder.com/products/active-buzzer-module), together with the 2 LEDs provided on the discovery board, are used to report the state of the system.

# Software

The operating system that runs on the board is [RIOT OS](https://www.riot-os.org/). To support the chosen peripherals, some additional drivers were written:

- Decoding of IR NEC packets from the remote
- Support for the digital microphone via the DFSDM peripheral

## IR Remote interface

> A version of the code has been merged upstream in RIOT. See [PR #17935](https://github.com/RIOT-OS/RIOT/pull/17935)

The remote command emits packets using the NEC format, which are then demodulated and received by the MCU. More information on the protocol can be found [here](https://techdocs.altium.com/display/FPGA/NEC+Infrared+Transmission+Protocol).

The output of the receiver chip is connected to an interrupt-capable GPIO pin, and the packets are deserialized in the ISR code. A packet is defined by a start condition, represented as a ~4250ms long high pulse, followed by 4 bytes, with each bit represented by an individual high pulse. A ~450ms pulse is a zero, a ~1550ms pulse is a one.

|![NEC packet](img/nec_packet.png)|
|:--:|
|A NEC packet, as seen on the output of the receiver chip|

A packet is made up of two 8-bit fields, called `address` and `command` and some error correction fields:

|Offset (bits)|Width (bits)|Name|Description|
|-|-|-|-|
|0|8|Address|`address` field|
|8|8|Inverted address|`~address`, for error correction|
|16|8|Command|`command` field|
|24|8|Inverted command|`~command`, for error correction|

The values are remote-specific, each corresponding to a different key.

The protocol also supports repeat codes, short packets that represent a button being held down, but those are not supported in the code at the moment.

|![NEC repeat codes](img/nec_repeat.png)|
|:--:|
|A NEC packet followed by repeat codes|

## Audio pipeline

The digital microphone, when fed with the correct audio clock, produces a series of samples in PDM format. This signal is fed into the DFSDM (Digital filter for Sigma-Delta Modulators) peripheral in the MCU, which converts the signal to PCM samples, applies a filter and produces samples that the code can use.

In software, a single pole low pass filter is added and the resulting value is periodically converted to dBFS and then dB by adding a constant offset of `-26dB` (which comes from the microphone itself). When this final value goes over a set limit for a set number of samples, the alarm is triggered.

# Network

The alarm communicates via MQTT to AWS IoT core. The packets are sent via[Ethernet-over-serial](https://doc.riot-os.org/group__drivers__ethos.html), then through a transparent MQTT-S/MQTT bridge and finally over the internet to the AWS endpoint. On the user side, a Python/Flask server connects to AWS to retrieve the device shadow data and interact with it as needed.

# User interface

The user interface is simple, and allows the user to see the current state of the alarm (idle, active, triggered) and to remotely turn the alarm on and off.
