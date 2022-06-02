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

void audio_cb(double dB) {
    int db = (int)dB;
    printf("%3d\n", db);
    // for(int i = 30; i < db; i++) putchar('#');
    // putchar('\n');
}

typedef enum {
    IDLE = 0,
    ACTIVE = 1,
    TRIGGERED = 2
} state_t;

state_t state = IDLE;

// Report the current device state to the device shadow
void report_state(void) {
    bool active = state != IDLE;
    bool triggered = state == TRIGGERED;

    char state_buf[128];
    sprintf(state_buf, "{\"state\": {\"reported\": {\"active\": %d, \"triggered\": %d}}}", active, triggered);

    mqtt_pub("$aws/things/iscream/shadow/update", state_buf, 1);
}

#define IR_BUTTON_ON 0xA2
#define IR_BUTTON_OFF 0x62
#define IR_BUTTON_STOP 0xE2

char ir_thread_stack[THREAD_STACKSIZE_MAIN];

void* ir_remote_thread(void *arg) {
    (void)arg;

    // Report the initial state to keep the shadow in sync after a reboot
    report_state();

    for(;;) {
        if(ir_remote_read(&remote, &cmd)) {
            puts("error reading from remote");
            continue;
        }

        state_t old_state = state;

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

        // Update device shadow on state change
        if(old_state != state) {
            report_state();
        }
    }
    return NULL;
}

const char* mqtt_subs[] = {
    "$aws/things/iscream/shadow/update/accepted",
    "$aws/things/iscream/shadow/get/accepted",
    NULL
};

int main(void) {

    ir_remote_init(&remote, ir_pin);
    audio_init(ckout, datin2, audio_cb);
    audio_start();
    setup_mqtt(mqtt_subs, on_pub);

    thread_create(ir_thread_stack, sizeof(ir_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, ir_remote_thread, NULL, "ir_remote");
}
