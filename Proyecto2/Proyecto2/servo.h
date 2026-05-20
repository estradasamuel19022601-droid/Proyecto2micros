
#ifndef SERVO_H_
#define SERVO_H_

#include <stdint.h>

//numero de servos
#define SERVO_COUNT 4

//limite de microsegundos
#define SERVO_US_MIN   1000U
#define SERVO_US_MAX   2000U

void     servo_init(void);

//lectura de microsegundos
void     servo_set_us(uint8_t idx, uint16_t us);

//lectura angulos
void     servo_set_angle(uint8_t idx, uint8_t angle);

//lee posicion en micros
uint16_t servo_get_us(uint8_t idx);



#endif /* SERVO_H_ */