#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "clk.h"
#include "board.h"
#include "periph_conf.h"
#include "timex.h"
#include "xtimer.h"
#include "periph/gpio.h"

#include "ir_remote.h"

#include "net/emcute.h"

#include "network.h"

#ifdef IN_VSCODE
// Just a hack to make autocompletion work
#include "/home/dario/Desktop/uni/iot/RIOT/build/stm32/cmsis/l4/Include/stm32l475xx.h"
#endif

ir_remote_t remote;
ir_remote_cmd_t cmd;

static inline DFSDM_Channel_TypeDef *ch(int id)
{
    return (DFSDM_Channel_TypeDef*)(DFSDM1_Channel0_BASE + 0x20 * id);
}

static inline DFSDM_Filter_TypeDef *flt(int id)
{
    return (DFSDM_Filter_TypeDef*)(DFSDM1_Filter0_BASE + 0x100 * id);
}

void set_ckout_freq(uint32_t target_hz)
{
    uint32_t bus_clock = periph_apb_clk(APB2);
    uint32_t div = bus_clock / target_hz - 1;
    div &= 0xFF; // 8 bit divisor

    ch(0)->CHCFGR1 |= div << DFSDM_CHCFGR1_CKOUTDIV_Pos;
}

gpio_t ckout = GPIO_PIN(PORT_E, 9);
gpio_t datin2 = GPIO_PIN(PORT_E, 7);

uint32_t rms_sum = 0;
uint32_t rms_cnt = 0;
uint32_t rms_lim = 2500;

void isr_dfsdm1_flt0(void)
{
    uint32_t data = flt(0)->FLTRDATAR >> DFSDM_FLTRDATAR_RDATA_Pos;

    data /= 23; // integrator

    rms_sum += data * data;
    rms_cnt++;

    if(rms_cnt == rms_lim)
    {
        // double rms = sqrt(rms_sum / rms_cnt);
        // int db = (int)(20.0 * log10(rms));
        // printf("%3d ", db);

        // for(int i = 30; i < db; i++) putchar('#');
        // putchar('\n');

        rms_sum = 0;
        rms_cnt = 0;
    }
}

void audio_init(void) {

    gpio_init(ckout, GPIO_OUT);
    gpio_init_af(ckout, GPIO_AF6);

    gpio_init(datin2, GPIO_IN);
    gpio_init_af(datin2, GPIO_AF6);

    periph_clk_en(APB2, RCC_APB2ENR_DFSDM1EN);

    ch(0)->CHCFGR1 = 0;

    /*
     1 MHz audio clock
     23x filter oversampling
     1x integrator oversampling
     -> ~43.4kHz audio signal
    */

    set_ckout_freq(1000000);

    ch(2)->CHCFGR1 |= DFSDM_CHCFGR1_SPICKSEL_0; // clock from CKOUT
    ch(2)->CHCFGR1 |= DFSDM_CHCFGR1_CHEN; // enable ch2

    flt(0)->FLTCR1 |= 2 << DFSDM_FLTCR1_RCH_Pos; // filter 0 reads channel 2
    flt(0)->FLTCR1 |= DFSDM_FLTCR1_RCONT; // continuous conversion
    flt(0)->FLTCR2 |= DFSDM_FLTCR2_REOCIE; // end of regular conversion interrupt enable
    flt(0)->FLTFCR |= 4 << DFSDM_FLTFCR_FORD_Pos; // Filter order
    flt(0)->FLTFCR |= 22 << DFSDM_FLTFCR_FOSR_Pos; // Filter oversampling ratio
    flt(0)->FLTFCR |= 0 << DFSDM_FLTFCR_IOSR_Pos; // Integrator oversampling ratio
    flt(0)->FLTCR1 |= DFSDM_FLTCR1_DFEN; // enable flt0

    ch(0)->CHCFGR1 |= DFSDM_CHCFGR1_DFSDMEN; // Enable DFSDM

    NVIC_EnableIRQ(DFSDM1_FLT0_IRQn);
}

// Start audio conversion
void audio_start(void) {
    flt(0)->FLTCR1 |= DFSDM_FLTCR1_RSWSTART; // start conversion
}

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

int main(void) {

    ir_remote_init(&remote, GPIO_PIN(PORT_A, 4));
    audio_init();
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
