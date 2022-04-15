#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "clk.h"
#include "board.h"
#include "periph_conf.h"
#include "timex.h"
#include "xtimer.h"
#include "periph/gpio.h"
#include "net/emcute.h"

#include "ir_remote.h"
#include "network.h"
#include "audio.h"

ir_remote_t remote;
ir_remote_cmd_t cmd;

gpio_t ir_pin = GPIO_PIN(PORT_A, 4);
gpio_t ckout = GPIO_PIN(PORT_E, 9);
gpio_t datin2 = GPIO_PIN(PORT_E, 7);

void on_pub(const emcute_topic_t *topic, void *data, size_t len) {
    char *in = (char *)data;
    printf("### got publication for topic '%s' [%i] ###\n",
           topic->name, (int)topic->id);

    for(size_t i = 0; i < len; i++) {
        printf("%c", in[i]);
    }
    puts("");

}

void send(int val) {
    printf("VALUE: %d\n",val);
    char str[50];
    sprintf(str,"{\"id\":\"%s\",\"pressed_key\":\"%d\"}",EMCUTE_ID,val);
    mqtt_pub(MQTT_TOPIC_OUT,str,0);
}

void audio_cb(double dB) {
    int db = (int)dB;
    printf("%3d ", db);
    // for(int i = 30; i < db; i++) putchar('#');
    // putchar('\n');
}

int main(void) {

    ir_remote_init(&remote, ir_pin);
    audio_init(ckout, datin2, audio_cb);
    audio_start();
    setup_mqtt(on_pub);

    for(;;)
    {
        if(ir_remote_read(&remote, &cmd)) {
            puts("error");
            return -1;
        }
        printf("Packet addr=0x%X, cmd=0x%X\n", cmd.addr, cmd.cmd);
        send(cmd.cmd);
    }
}
