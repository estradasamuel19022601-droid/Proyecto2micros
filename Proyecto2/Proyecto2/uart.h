

#ifndef UART_H_
#define UART_H_

#include <stdint.h>

#define UART_BAUD       9600UL
#define UART_RX_BUF_SZ  64

void    uart_init(void);

//envio byte
void    uart_send_byte(uint8_t b);

//enviar cadena termianada
void    uart_print_str(const char *s);

//enviar numero entero sin signo
void    uart_print_uint(uint16_t val);

//retorna 1 si hay bytes en el buffer
uint8_t uart_available(void);

/* Lee un byte del buffer (llamar sólo si uart_available() != 0) */
uint8_t uart_read_byte(void);

/*
 * Intenta leer una línea completa terminada en '\n'.
 * Escribe hasta (buf_len-1) caracteres en buf y agrega '\0'.
 * Retorna 1 si se obtuvo línea completa, 0 si todavía no.
 */
uint8_t uart_read_line(char *buf, uint8_t buf_len);



#endif /* UART_H_ */