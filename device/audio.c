#include <math.h>

#include "audio.h"

#include "clk.h"
#include "board.h"
#include "periph/gpio.h"

static inline DFSDM_Channel_TypeDef *ch(int id) {
    return (DFSDM_Channel_TypeDef*)(DFSDM1_Channel0_BASE + 0x20 * id);
}

static inline DFSDM_Filter_TypeDef *flt(int id) {
    return (DFSDM_Filter_TypeDef*)(DFSDM1_Filter0_BASE + 0x100 * id);
}

static void set_ckout_freq(uint32_t target_hz) {
    uint32_t bus_clock = periph_apb_clk(APB2);
    uint32_t div = bus_clock / target_hz - 1;
    div &= 0xFF; // 8 bit divisor

    ch(0)->CHCFGR1 |= div << DFSDM_CHCFGR1_CKOUTDIV_Pos;
}

static audio_cb_t audio_cb;

static int num_samples = 0;
#define SAMPLES_PER_MEASURE 5000

static double y = 0, y_old = 0;
static double alpha = 0.001;

void isr_dfsdm1_flt0(void)
{
    uint32_t data = flt(0)->FLTRDATAR >> DFSDM_FLTRDATAR_RDATA_Pos;

    // Single pole low pass filter
    // https://stackoverflow.com/a/2168313/7547712
    double x = (double)data;
    y = alpha * x + (1.0 - alpha) * y_old;
    y_old = y;

    if(++num_samples == SAMPLES_PER_MEASURE)
    {
        double db = 20.0 * log10(y) - 26.0;
        audio_cb(db);
        num_samples = 0;
    }
}

void audio_init(gpio_t ckout, gpio_t datin, audio_cb_t cb) {

    audio_cb = cb;

    gpio_init(ckout, GPIO_OUT);
    gpio_init_af(ckout, GPIO_AF6);

    gpio_init(datin, GPIO_IN);
    gpio_init_af(datin, GPIO_AF6);

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
