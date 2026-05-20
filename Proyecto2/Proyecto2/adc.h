

#ifndef ADC_H_
#define ADC_H_

#include <stdint.h>

#define ADC_CHANNELS 4

void     adc_init(void);

//inicia convesrion del canal 0 y pasa a los demas 
void     adc_start_scan(void);

//Retorna el ultimo valor convertido 
uint16_t adc_get(uint8_t channel);

//pasa a 1 si hay un nuevo dato, limpia al leer
uint8_t  adc_ready(void);


#endif /* ADC_H_ */