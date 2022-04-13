#ifndef _IR_REMOTE_H
#define _IR_REMOTE_H

#include "periph/gpio.h"
#include "isrpipe.h"

#define IR_REMOTE_START_US 4250
#define IR_REMOTE_ZERO_US 450
#define IR_REMOTE_ONE_US 1550
#define IR_REMOTE_EPS_US 200

typedef struct {
    gpio_t pin;
    isrpipe_t isrpipe;

    uint32_t last_rising;
    uint32_t data;
    int read_bits;
    bool started;

    uint8_t isrpipe_buf[64];
} ir_remote_t;

typedef struct {
    uint8_t addr;
    uint8_t cmd;
} ir_remote_cmd_t;

int ir_remote_read(ir_remote_t *ir, ir_remote_cmd_t *command);
void ir_remote_init(ir_remote_t *ir, gpio_t pin);

#endif
