#include <stdlib.h>
#include "xtimer.h"

#include "ir_remote.h"

static void ir_remote_isr(void *arg)
{
    ir_remote_t *ir = (ir_remote_t *)arg;

    if(gpio_read(ir->pin) != 0) // rising edge
    {
        ir->last_rising = xtimer_now_usec();
    }
    else // falling edge
    {
        uint32_t length = xtimer_now_usec() - ir->last_rising;

        if(abs(length - IR_REMOTE_START_US) <= IR_REMOTE_EPS_US)
        {
            ir->started = true;
            ir->data = 0;
            ir->read_bits = 0;
        }
        else if(abs(length - IR_REMOTE_ZERO_US) <= IR_REMOTE_EPS_US)
        {
            ir->data = (ir->data << 1);
            ir->read_bits++;
        }
        else if(abs(length - IR_REMOTE_ONE_US) <= IR_REMOTE_EPS_US)
        {
            ir->data = (ir->data << 1) | 1;
            ir->read_bits++;
        }
        else // Interpacket delay
        {
            ir->started = false;
        }

        if(ir->started && ir->read_bits == 32)
        {
            ir->started = false;

            uint8_t addr = (ir->data >> 24) & 0xff;
            uint8_t addr_inv = (ir->data >> 16) & 0xff;
            uint8_t cmd = (ir->data >> 8) & 0xff;
            uint8_t cmd_inv = ir->data & 0xff;

            if((addr ^ addr_inv) == 0xff && (cmd ^ cmd_inv) == 0xff)
            {
                ir_remote_cmd_t command = { .addr = addr, .cmd = cmd };
                isrpipe_write(&ir->isrpipe, (void*)&command, sizeof(command));
            }
        }
    }
}

int ir_remote_read(ir_remote_t *ir, ir_remote_cmd_t *command)
{
    int to_read = sizeof(ir_remote_cmd_t);
    if(isrpipe_read(&ir->isrpipe, (void*)command, to_read) != to_read)
    {
        return -1;
    }
    return 0;
}

void ir_remote_init(ir_remote_t *ir, gpio_t pin)
{
    ir->pin = pin;
    ir->data = 0;
    ir->last_rising = 0;
    ir->read_bits = 0;
    ir->started = false;

    isrpipe_init(&ir->isrpipe, ir->isrpipe_buf, sizeof(ir->isrpipe_buf));
    gpio_init_int(ir->pin, GPIO_IN, GPIO_BOTH, ir_remote_isr, ir);
}
