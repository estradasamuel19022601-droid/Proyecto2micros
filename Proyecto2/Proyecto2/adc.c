


#include "adc.h"
#include <avr/io.h>
#include <avr/interrupt.h>

static volatile uint16_t adc_result[ADC_CHANNELS] = {512, 512, 512, 512};
static volatile uint8_t  adc_current_ch = 0;
static volatile uint8_t  adc_flag       = 0;  //ciclo completo = 1

void adc_init(void)
{

    ADMUX  = (1 << REFS0);

    ADCSRA = (1 << ADEN) | (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	//activar adc, interrupciones y el prescaler 128

    adc_current_ch = 0;
   
    ADCSRA |= (1 << ADSC); //iniciar primera conversion
}
void adc_start_scan(void)
{
    adc_current_ch = 0;
    ADMUX = (ADMUX & 0xF0);   //seleccion de canal 
    ADCSRA |= (1 << ADSC);
}


ISR(ADC_vect)
{
    
    adc_result[adc_current_ch] = ADC;

    //se mueve al siguiente canal
    adc_current_ch++;
    if (adc_current_ch >= ADC_CHANNELS)
    {
        adc_current_ch = 0;
        adc_flag = 1;   //se completo el ciclo 
    }

    //seleccinar nuevo canal y empezar conversion 
    ADMUX = (ADMUX & 0xF0) | (adc_current_ch & 0x0F);
    ADCSRA |= (1 << ADSC);
}

uint16_t adc_get(uint8_t channel) //devuelve el ultimo valor convertido 
{
    if (channel >= ADC_CHANNELS) return 0;
    uint16_t val;
    uint8_t sreg = SREG;
    cli();
    val = adc_result[channel];
    SREG = sreg;
    return val;
}  //evitar que ISR modifique datos 

uint8_t adc_ready(void)
{
    if (adc_flag)
    {
        adc_flag = 0;
        return 1;
    }
    return 0;  //verificacion y limpieza de ciclo
}
