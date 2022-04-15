#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "clk.h"
#include "board.h"
#include "periph_conf.h"
#include "timex.h"
#include "xtimer.h"
#include "thread.h"
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
    printf("%3d\n", db);
    // for(int i = 30; i < db; i++) putchar('#');
    // putchar('\n');
}

enum {
    IDLE = 0,
    ACTIVE = 1,
    TRIGGERED = 2
} state = IDLE;

#define IR_BUTTON_ON 0xA2
#define IR_BUTTON_OFF 0x62
#define IR_BUTTON_STOP 0xE2

char ir_thread_stack[THREAD_STACKSIZE_MAIN];

void* ir_remote_thread(void *arg) {
    (void)arg;

    for(;;) {
        if(ir_remote_read(&remote, &cmd)) {
            puts("error reading from remote");
            continue;
        }

        switch(cmd.cmd) {
            case IR_BUTTON_ON:
                puts("Turning on");
                state = ACTIVE;
                break;
            case IR_BUTTON_OFF:
                puts("Turning off");
                state = IDLE;
                break;
            case IR_BUTTON_STOP:
                if(state == TRIGGERED) {
                    puts("Stopping alarm");
                    state = ACTIVE;
                } else {
                    puts("Stop command received, but alarm was't triggered - ignoring");
                }
                break;
        }
    }
    return NULL;
}

int main(void) {

    ir_remote_init(&remote, ir_pin);
    audio_init(ckout, datin2, audio_cb);
    audio_start();
    setup_mqtt(on_pub);

    thread_create(ir_thread_stack, sizeof(ir_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, ir_remote_thread, NULL, "ir_remote");
}
