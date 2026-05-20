//Proyecto Microcontroladores, cara animatronica 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include "servo.h"
#include "adc.h"
#include "uart.h"
#include "eeprom.h"
#include "modes.h"

int main(void)
{
	
	servo_init();       //timers para servos  
	adc_init();         //adc para los 4 potenciometros
	uart_init();        //uart para modo serial
	modes_init();       //botones y led

	sei();              //interrupciones

	uart_print_str("Sistema listo. Modo: MANUAL\r\n");

	while (1)
	{
		modes_run();    //maquina de estados
	}
}
