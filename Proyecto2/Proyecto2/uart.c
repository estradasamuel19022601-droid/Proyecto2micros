#include "uart.h"
#include <avr/io.h>
#include <avr/interrupt.h>

#ifndef F_CPU
#define F_CPU 16000000UL
#endif

//registro de baud 9600
#define UBRR_VAL  ((F_CPU / (16UL * UART_BAUD)) - 1UL)

//buffer circular
static volatile uint8_t rx_buf[UART_RX_BUF_SZ];
static volatile uint8_t rx_head = 0;  
static volatile uint8_t rx_tail = 0;  


void uart_init(void)
{
    //rango baud
    UBRR0H = (uint8_t)(UBRR_VAL >> 8);
    UBRR0L = (uint8_t)(UBRR_VAL);

	//habilitar transimision, recepcion y el paso de 8 bits
    UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  //8
}


ISR(USART_RX_vect)
{
    uint8_t data = UDR0;
    uint8_t next = (rx_head + 1) % UART_RX_BUF_SZ;
    if (next != rx_tail)   //descarta si buffer lleno
    {
        rx_buf[rx_head] = data;
        rx_head = next;
    }
}


void uart_send_byte(uint8_t b)
{
    while (!(UCSR0A & (1 << UDRE0)));  //esperar registro de transimision
    UDR0 = b;
}

void uart_print_str(const char *s)
{
    while (*s)
        uart_send_byte((uint8_t)(*s++));
}

void uart_print_uint(uint16_t val)
{
    char buf[6];
    uint8_t i = 0;
    if (val == 0)
    {
        uart_send_byte('0');
        return;
    }
    while (val > 0)
    {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    //invertir
    while (i > 0)
        uart_send_byte((uint8_t)buf[--i]);
}


uint8_t uart_available(void)
{
    return (rx_head != rx_tail) ? 1 : 0;
}

uint8_t uart_read_byte(void)
{
    uint8_t b = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % UART_RX_BUF_SZ;
    return b;
}

uint8_t uart_read_line(char *buf, uint8_t buf_len)
{
    static char line_buf[UART_RX_BUF_SZ];
    static uint8_t line_pos = 0;

    while (uart_available())
    {
        char c = (char)uart_read_byte();

        if (c == '\r') continue;   

        if (c == '\n')
        {
            line_buf[line_pos] = '\0';
            //copiar buffer del usuario
            uint8_t i;
            for (i = 0; i < buf_len - 1 && line_buf[i] != '\0'; i++)
                buf[i] = line_buf[i];
            buf[i] = '\0';
            line_pos = 0;
            return 1;
        }

        if (line_pos < UART_RX_BUF_SZ - 1)
            line_buf[line_pos++] = c;
    }
    return 0;
}
