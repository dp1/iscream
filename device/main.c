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
gpio_t buzzer = GPIO_PIN(PORT_A, 15);

//=======================================
// State management
//=======================================

typedef enum {
    IDLE = 0,
    ACTIVE = 1,
    TRIGGERED = 2
} state_t;

state_t device_state = IDLE;

// Report the current device state to the device shadow
void report_state(void) {
    bool active = device_state != IDLE;
    bool triggered = device_state == TRIGGERED;

    char state_buf[128];
    sprintf(state_buf, "{\"state\": {\"reported\": {\"active\": %d, \"triggered\": %d}}}", active, triggered);

    mqtt_pub("$aws/things/iscream/shadow/update", state_buf, 1);
}

//========================================
// JSON parsing
//========================================

typedef struct {
    char *data;
    char *end;
} jsondict_t;

// Get a value out of a json dictionary
// Returns a pointer into the original string where the value starts
// Returns {NULL,NULL} if no such child exists
jsondict_t json_get_child(jsondict_t *json, char *key) {
    jsondict_t res = {NULL, NULL};

    if(json == NULL || json->data == NULL || json->data[0] != '{') {
        return res;
    }

    size_t keylen = strlen(key);

    for(char *p = json->data + 1; p < json->end; ++p) {
        if(*p == ' ') continue;

        // start of a key
        if(*p == '\"') {

            p++;
            char *foundkey = p;
            while(p < json->end && *p != '\"') p++;
            if(p == json->end) return res;
            p++;

            if(*p != ':') return res;
            p++;
            char *foundvalue = p;

            // Skip the value
            int depth = 0;
            bool in_string = false;

            while(p < json->end) {
                if(*p == '\"') in_string = !in_string;
                else if(*p == '{' && !in_string) ++depth;
                else if(*p == '}' && !in_string) --depth;

                // End of value found
                if(!in_string && depth == 0 && *p == ',') break;
                if(!in_string && depth == -1 && *p == '}') break;

                ++p;
            }

            if(!strncmp(key, foundkey, keylen)) {
                // Target found
                res.data = foundvalue;
                res.end = p;
                return res;
            }
        }
    }

    // Not found
    return res;
}

//========================================
// IoT core shadow
//========================================

void on_shadow_update(const emcute_topic_t *topic, void *data, size_t len) {
    char *in = (char *)data;
    printf("### got publication for topic '%s' [%i] ###\n",
           topic->name, (int)topic->id);

    for(size_t i = 0; i < len; i++) {
        printf("%c", in[i]);
    }
    puts("");

    jsondict_t json = {.data = data, .end = data + len};
    jsondict_t state = json_get_child(&json, "state");
    jsondict_t delta = json_get_child(&state, "desired");
    jsondict_t active = json_get_child(&delta, "active");
    if(!active.data) return;

    if(active.data[0] == '0' || active.data[0] == 'f') device_state = IDLE;
    else device_state = ACTIVE;

    printf("Received shadow delta, new state %d\n", device_state);
}

//========================================
// Status LEDs and buzzer
//========================================

// Update status LEDs and buzzer
void update_status(void) {
    switch(device_state) {
        case IDLE:
            gpio_clear(LED0_PIN);
            gpio_clear(LED1_PIN);
            gpio_set(buzzer);
            break;
        case ACTIVE:
            gpio_set(LED0_PIN);
            gpio_clear(LED1_PIN);
            gpio_set(buzzer);
            break;
        case TRIGGERED:
            gpio_set(LED0_PIN);
            gpio_set(LED1_PIN);
            gpio_clear(buzzer); // The buzzer is active low
            break;
    }
}

//========================================
// Audio processing
//========================================

typedef struct {
    double avg;
    double max;
} audio_report_t;

audio_report_t cur_audio_report = {.avg = 0, .max = 0};
int cur_report_samples = 0;
uint32_t last_report_ts;

isrpipe_t audio_isrpipe;
uint8_t audio_isrpipe_buf[16 * sizeof(audio_report_t)];
char audio_thread_stack[THREAD_STACKSIZE_MAIN];

#define SOUND_TRIGGER_DB 60
#define SOUND_TRIGGER_SAMPLES 3

int trigger_ctr = 0;

void audio_cb(double dB) {
    printf("%3d\n", (int)dB);

    if(device_state == ACTIVE && dB >= SOUND_TRIGGER_DB) {
        ++trigger_ctr;

        if(trigger_ctr >= SOUND_TRIGGER_SAMPLES) {
            device_state = TRIGGERED;
        }
    }
    else {
        trigger_ctr = 0;
    }

    if(dB > cur_audio_report.max)
        cur_audio_report.max = dB;
    cur_audio_report.avg += dB;
    ++cur_report_samples;

    // Send cumulative report
    if(xtimer_now_usec() - last_report_ts > 10 * US_PER_SEC) {
        cur_audio_report.avg /= cur_report_samples;
        isrpipe_write(&audio_isrpipe, (void*)&cur_audio_report, sizeof(cur_audio_report));

        cur_audio_report.max = dB;
        cur_audio_report.avg = dB;
        cur_report_samples = 1;

        last_report_ts = xtimer_now_usec();
    }
}

void* audio_reader_thread(void* arg) {
    (void)arg;
    char buf[128];

    while(1) {
        audio_report_t report;
        if(isrpipe_read(&audio_isrpipe, (void*)&report, sizeof(report)) != sizeof(report)) {
            puts("audio: isrpipe_read failed");
            return NULL;
        }

        sprintf(buf, "{\"avg\": %lf, \"max\": %lf}", report.avg, report.max);
        mqtt_pub("$aws/things/iscream/sound_level", buf, 1);
    }
}

void audio_reader_init(void) {
    isrpipe_init(&audio_isrpipe, audio_isrpipe_buf, sizeof(audio_isrpipe_buf));
    last_report_ts = xtimer_now_usec();

    thread_create(
        audio_thread_stack, sizeof(audio_thread_stack),
        THREAD_PRIORITY_MAIN - 2, 0, audio_reader_thread,
        NULL, "audio_reder"
    );
}

//========================================
// IR Remote handling
//========================================

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

        switch(cmd.cmd) {
            case IR_BUTTON_ON:
                puts("Turning on");
                device_state = ACTIVE;
                break;
            case IR_BUTTON_OFF:
                puts("Turning off");
                device_state = IDLE;
                break;
            case IR_BUTTON_STOP:
                if(device_state == TRIGGERED) {
                    puts("Stopping alarm");
                    device_state = ACTIVE;
                } else {
                    puts("Stop command received, but alarm was't triggered - ignoring");
                }
                break;
        }

        // If for some reason the main loop has to wait inside mqtt_pub,
        // make sure that the alarm can still be operated with the remote
        update_status();
    }
    return NULL;
}

//========================================
// Main logic
//========================================

const char* mqtt_subs[] = {
    "$aws/things/iscream/shadow/update/accepted",
    NULL
};

int main(void) {
    gpio_init(LED0_PIN, GPIO_OUT);
    gpio_init(LED1_PIN, GPIO_OUT);
    gpio_init(buzzer, GPIO_OUT);
    gpio_set(buzzer); // The buzzer is active low

    ir_remote_init(&remote, ir_pin);
    audio_init(ckout, datin2, audio_cb);
    setup_mqtt(mqtt_subs, on_shadow_update);

    audio_reader_init();
    audio_start();

    thread_create(ir_thread_stack, sizeof(ir_thread_stack), THREAD_PRIORITY_MAIN - 1, 0, ir_remote_thread, NULL, "ir_remote");

    state_t old_state = device_state;
    while(1) {
        update_status();

        if(old_state != device_state) {
            report_state();
            old_state = device_state;
        }

        xtimer_usleep(100000);
    }
}
