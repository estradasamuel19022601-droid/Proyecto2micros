#include "servo.h"
#include <avr/io.h>
#include <avr/interrupt.h>

//tabla de ticks
static volatile uint16_t servo_ticks[SERVO_COUNT] = {3000, 3000, 3000, 3000};


//variables para soft-pwm
//acumula ticks
static volatile uint16_t soft_tick_acc = 0;      //cuenta de 0 a 39999
static volatile uint8_t  soft_high[2]  = {1, 1};  //estado actual

//pines timer2
#define S2_PORT  PORTB
#define S2_BIT   PB3
#define S3_PORT  PORTD
#define S3_BIT   PD3


void servo_init(void)
{
    //pines de timer 1
    DDRB |= (1 << PB1) | (1 << PB2);

	//modo fast PWM, prescaler de 8 
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM11);
    TCCR1B = (1 << WGM13)  | (1 << WGM12)  | (1 << CS11);
    ICR1   = 39999;  //top de timer 40000 contando 0
    OCR1A  = 3000;   
    OCR1B  = 3000;   

    //pines timer 2
    DDRB |= (1 << S2_BIT);
    DDRD |= (1 << S3_BIT);

    // desactivar los pines para usar soft-pwm
    //modo normal, con overflow, 256 ticks
    TCCR2A = 0x00;
    TCCR2B = (1 << CS21);  //prescaler 8
    TCNT2  = 0;
    TIMSK2 = (1 << TOIE2); //habilitar overflow 

    //pines en alto
    S2_PORT |= (1 << S2_BIT);
    S3_PORT |= (1 << S3_BIT);
    soft_high[0] = 1;
    soft_high[1] = 1;
}


ISR(TIMER2_OVF_vect)
{
    soft_tick_acc += 256;   //contar 256 ticks 

    //logica servo 2
    if (soft_high[0])
    {
        if (soft_tick_acc >= servo_ticks[2])
        {
            S2_PORT &= ~(1 << S2_BIT);  //bajar pin cuando se llega al puslo
            soft_high[0] = 0;
        }
    }

    //logica servo 3
    if (soft_high[1])
    {
        if (soft_tick_acc >= servo_ticks[3])
        {
            S3_PORT &= ~(1 << S3_BIT);
            soft_high[1] = 0;
        }
    }

    //rreinicio de periodo
    if (soft_tick_acc >= 40000)
    {
        soft_tick_acc = 0;
        S2_PORT |= (1 << S2_BIT);
        S3_PORT |= (1 << S3_BIT);
        soft_high[0] = 1;
        soft_high[1] = 1;
    }
}



//limitar valores de minimo y maximo
static uint16_t clamp16(uint16_t val, uint16_t lo, uint16_t hi)
{
    if (val < lo) return lo;
    if (val > hi) return hi;
    return val;
}

void servo_set_us(uint8_t idx, uint16_t us)
{
    if (idx >= SERVO_COUNT) return;

    us = clamp16(us, SERVO_US_MIN, SERVO_US_MAX);
    uint16_t ticks = (uint16_t)(us * 2U);  

    servo_ticks[idx] = ticks;

    switch (idx)
    {
        case 0: OCR1A = ticks; break;
        case 1: OCR1B = ticks; break;
        //s2 y s3 se manejan con ISR
        default: break;
    }
}

void servo_set_angle(uint8_t idx, uint8_t angle)
{
    if (angle > 180) angle = 180;
    //mapeo de angulos con microsegundo 
    uint16_t us = SERVO_US_MIN + (uint16_t)((uint32_t)angle * 1000U / 180U);
    servo_set_us(idx, us);
}

uint16_t servo_get_us(uint8_t idx)
{
    if (idx >= SERVO_COUNT) return SERVO_US_MIN;
    return (servo_ticks[idx] / 2U);  //ticks a microsegundos
}
