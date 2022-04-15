#ifndef _AUDIO_H
#define _AUDIO_H

/*
* Audio processing functions
*
* For this project the onboard MEMS microphones are used, in mono configuration.
* The PDM audio signal goes into channel 2 of the DFSDM peripheral and then into
* filter 0 for PDM->PCM conversion and filtering. For simplicity, audio data is
* received via interrupts instead of DMA.
* Once data is in the ISR, it's fed through a single pole low pass filter before
* the PDM -> dB conversion. For the conversion a reference offset of -26dB
* is subtracted as specified in te datasheet. This value has been verified by
* comparing the resulting sound level measurements with a mobile phone.
* Periodically, when enough data samples have been collected, the specified
* callback is invoked with the sound level of the last period in dB.
*/

#include "periph/gpio.h"

typedef void (*audio_cb_t)(double dB);

void audio_init(gpio_t ckout, gpio_t datin, audio_cb_t cb);
void audio_start(void);

#endif
