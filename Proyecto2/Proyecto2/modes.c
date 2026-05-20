#include "modes.h"
#include "servo.h"
#include "adc.h"
#include "uart.h"
#include "eeprom.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>   

//para tick de 1ms
static volatile uint32_t ms_ticks = 0;

ISR(TIMER0_COMPA_vect)
{
    ms_ticks++;
}

static uint32_t get_ms(void)
{
    uint32_t t;
    uint8_t sreg = SREG;
    cli();
    t = ms_ticks;
    SREG = sreg;
    return t;
}

//pines a utilizar
#define LED_PORT   PORTD
#define LED_BIT    PD5

#define BTN_MODE_PIN  PIND
#define BTN_MODE_BIT  PD4

#define BTN_SAVE_PIN  PIND
#define BTN_SAVE_BIT  PD6

#define BTN_SLOT_PIN  PIND
#define BTN_SLOT_BIT  PD7

//estado inicial
static system_mode_t current_mode = MODE_MANUAL;
static uint8_t  active_slot = 0;      //slot activo y empieza en 0

#define DEBOUNCE_MS  50U

static uint32_t btn_mode_last = 0;
static uint32_t btn_save_last = 0;
static uint32_t btn_slot_last = 0;

static uint8_t btn_mode_prev = 1;
static uint8_t btn_save_prev = 1;
static uint8_t btn_slot_prev = 1;


//parpadeo para la led de estados
static uint32_t led_last_toggle = 0;
static uint8_t  led_state = 0;


//eeprom 
#define EEPROM_PLAY_STEP_MS  1500U //espacio entre slots
static uint32_t eeprom_last_step = 0;
static uint8_t  eeprom_play_slot = 0;



//conveserion a ascii 
static uint16_t parse_uint(const char *s)
{
    uint16_t val = 0;
    while (*s >= '0' && *s <= '9')
    {
        val = val * 10 + (uint16_t)(*s - '0');
        s++;
    }
    return val;
}

static void led_update(void)
{
    uint32_t now = get_ms();

    switch (current_mode)
    {
        case MODE_MANUAL:
            LED_PORT |= (1 << LED_BIT);  //se enciende continua
            break;

        case MODE_EEPROM:
            if (now - led_last_toggle >= 500U) //se enciende cada 500 ms
            {
                led_state ^= 1;
                if (led_state) LED_PORT |= (1 << LED_BIT);
                else           LED_PORT &= ~(1 << LED_BIT);
                led_last_toggle = now;
            }
            break;

        case MODE_UART:
            if (now - led_last_toggle >= 100U) //se enciende cada 100ms
            {
                led_state ^= 1;
                if (led_state) LED_PORT |= (1 << LED_BIT);
                else           LED_PORT &= ~(1 << LED_BIT);
                led_last_toggle = now;
            }
            break;

        default: break;
    }
}

//leer los botones con antirrebote, pull-up
static uint8_t btn_pressed(volatile uint8_t *pin_reg, uint8_t bit,
                            uint8_t *prev, uint32_t *last_time)
{
    uint32_t now = get_ms();
    uint8_t  cur = (*pin_reg >> bit) & 1;  //verifica que no esta presionado

    if (cur != *prev && (now - *last_time) >= DEBOUNCE_MS)
    {
        *prev = cur;
        *last_time = now;
        if (cur == 0) return 1;  //se presiono el boton
    }
    return 0;
}



static void run_manual(void)
{
    //lee y actualiza el estado de los servos
    if (adc_ready())
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            uint16_t raw = adc_get(i);
            //rango leido de los ticks de 0 a 1023
            uint16_t us = 1000U + (uint16_t)((uint32_t)raw * 1000U / 1023U);
            servo_set_us(i, us);
        }
    }

    //boton guardar con el slot deseado
    if (btn_pressed(&BTN_SAVE_PIN, BTN_SAVE_BIT, &btn_save_prev, &btn_save_last))
    {
        uint16_t pos[4];
        for (uint8_t i = 0; i < 4; i++)
            pos[i] = servo_get_us(i);
        eeprom_save_slot(active_slot, pos);

        uart_print_str("Guardado en slot ");
        uart_print_uint(active_slot);
        uart_print_str("\r\n");
    }

    //boton de slot para cambiar a otro 
    if (btn_pressed(&BTN_SLOT_PIN, BTN_SLOT_BIT, &btn_slot_prev, &btn_slot_last))
    {
        active_slot = (active_slot + 1) % EEPROM_SLOTS;
        uart_print_str("Slot activo: ");
        uart_print_uint(active_slot);
        uart_print_str("\r\n");
    }
}

static void run_eeprom(void)
{
    uint32_t now = get_ms();

    //EEPROM slots secuencia
    if (now - eeprom_last_step >= EEPROM_PLAY_STEP_MS)
    {
        uint16_t pos[4];
        eeprom_load_slot(eeprom_play_slot, pos);
        for (uint8_t i = 0; i < 4; i++)
            servo_set_us(i, pos[i]);

        uart_print_str("Reproduciendo slot ");
        uart_print_uint(eeprom_play_slot);
        uart_print_str("\r\n");

        eeprom_play_slot = (eeprom_play_slot + 1) % EEPROM_SLOTS;
        eeprom_last_step = now;
    }

    //reinicia la a slot 0
    if (btn_pressed(&BTN_SLOT_PIN, BTN_SLOT_BIT, &btn_slot_prev, &btn_slot_last))
    {
        eeprom_play_slot = 0;
        eeprom_last_step = get_ms();
        uart_print_str("Reiniciando reproduccion\r\n");
    }
}

/*
 * Procesar un comando UART recibido como línea completa
 * Formato: "Sn:xxxx"  ? servo en micros
 *          "An:xxx"   ? servo en grados
 *          "SAVE:n"   ? guardar slot n
 *          "PLAY:n"   ? reproducir slot n
 *          "MODE:M/E/U" ? cambiar modo
 *          "STATUS"   ? imprimir posiciones
 */
static void process_uart_command(const char *cmd)
{
    if (cmd[0] == 'S' && cmd[1] >= '0' && cmd[1] <= '3' && cmd[2] == ':')
    {
        uint8_t idx = (uint8_t)(cmd[1] - '0');
        uint16_t us = parse_uint(cmd + 3);
        servo_set_us(idx, us);
        uart_print_str("OK S");
        uart_send_byte(cmd[1]);
        uart_print_str("=");
        uart_print_uint(us);
        uart_print_str("us\r\n");
    }
    else if (cmd[0] == 'A' && cmd[1] >= '0' && cmd[1] <= '3' && cmd[2] == ':')
    {
        uint8_t idx   = (uint8_t)(cmd[1] - '0');
        uint8_t angle = (uint8_t)parse_uint(cmd + 3);
        servo_set_angle(idx, angle);
        uart_print_str("OK A");
        uart_send_byte(cmd[1]);
        uart_print_str("=");
        uart_print_uint(angle);
        uart_print_str("deg\r\n");
    }
    else if (cmd[0] == 'S' && cmd[1] == 'A' && cmd[2] == 'V' && cmd[3] == 'E' && cmd[4] == ':')
    {
        uint8_t slot = (uint8_t)parse_uint(cmd + 5);
        if (slot >= EEPROM_SLOTS) slot = 0;
        uint16_t pos[4];
        for (uint8_t i = 0; i < 4; i++) pos[i] = servo_get_us(i);
        eeprom_save_slot(slot, pos);
        uart_print_str("Guardado slot ");
        uart_print_uint(slot);
        uart_print_str("\r\n");
    }
    else if (cmd[0] == 'P' && cmd[1] == 'L' && cmd[2] == 'A' && cmd[3] == 'Y' && cmd[4] == ':')
    {
        uint8_t slot = (uint8_t)parse_uint(cmd + 5);
        if (slot >= EEPROM_SLOTS) slot = 0;
        uint16_t pos[4];
        eeprom_load_slot(slot, pos);
        for (uint8_t i = 0; i < 4; i++) servo_set_us(i, pos[i]);
        uart_print_str("Reproduciendo slot ");
        uart_print_uint(slot);
        uart_print_str("\r\n");
    }
    else if (cmd[0] == 'M' && cmd[1] == 'O' && cmd[2] == 'D' && cmd[3] == 'E' && cmd[4] == ':')
    {
        if (cmd[5] == 'M') current_mode = MODE_MANUAL;
        else if (cmd[5] == 'E') { current_mode = MODE_EEPROM; eeprom_play_slot = 0; }
        else if (cmd[5] == 'U') current_mode = MODE_UART;
        uart_print_str("Modo: ");
        uart_print_str(cmd[5] == 'M' ? "MANUAL" : cmd[5] == 'E' ? "EEPROM" : "UART");
        uart_print_str("\r\n");
    }
    else if (cmd[0] == 'S' && cmd[1] == 'T' && cmd[2] == 'A' && cmd[3] == 'T')
    {
        uart_print_str("Posiciones (us):");
        for (uint8_t i = 0; i < 4; i++)
        {
            uart_print_str(" S");
            uart_print_uint(i);
            uart_print_str("=");
            uart_print_uint(servo_get_us(i));
        }
        uart_print_str("\r\n");
    }
    else
    {
        uart_print_str("CMD desconocido: ");  //cuando no se coloca lo correcto
        uart_print_str(cmd);
        uart_print_str("\r\n");
    }
}

static void run_uart(void)
{
    char line[32];
    if (uart_read_line(line, sizeof(line)))
        process_uart_command(line);
}



void modes_init(void)
{
    //salida led
    DDRD  |= (1 << LED_BIT);
    LED_PORT |= (1 << LED_BIT);

    //configurar botones con pull-ups
    DDRD  &= ~((1 << BTN_MODE_BIT) | (1 << BTN_SAVE_BIT) | (1 << BTN_SLOT_BIT));
    PORTD |=  ((1 << BTN_MODE_BIT) | (1 << BTN_SAVE_BIT) | (1 << BTN_SLOT_BIT));

    //timer 0 con 1ms 
    TCCR0A = (1 << WGM01);         //CTC
    TCCR0B = (1 << CS01) | (1 << CS00); //prescaler 64
    OCR0A  = 249;                   
    TIMSK0 = (1 << OCIE0A);        //interrupciones
}

void modes_run(void)
{
    led_update();

    //boton modo
    if (btn_pressed(&BTN_MODE_PIN, BTN_MODE_BIT, &btn_mode_prev, &btn_mode_last))
    {
        current_mode = (system_mode_t)((current_mode + 1) % MODE_COUNT);
        if (current_mode == MODE_EEPROM) eeprom_play_slot = 0;

        uart_print_str("Modo cambiado a: ");
        switch (current_mode)
        {
            case MODE_MANUAL: uart_print_str("MANUAL\r\n");  
			break;
            case MODE_EEPROM: uart_print_str("EEPROM\r\n");  
			break;
            case MODE_UART:   uart_print_str("UART\r\n");    
			break;
            default: 
			break;
        }
    }

    //ejecucion de modos 
    switch (current_mode)
    {
        case MODE_MANUAL: run_manual(); 
		break;
        case MODE_EEPROM: run_eeprom(); 
		break;
        case MODE_UART:   run_uart();   
		break;
        default: 
		break;
    }
}

system_mode_t modes_get(void)
{
    return current_mode;
}
