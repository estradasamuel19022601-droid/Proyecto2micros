 


#ifndef MODES_H_
#define MODES_H_

#include <stdint.h>

typedef enum {
	MODE_MANUAL = 0,
	MODE_EEPROM,
	MODE_UART,
	MODE_COUNT
} system_mode_t;

void modes_init(void);

//llamar al loop principal
void modes_run(void);

//modo actual
system_mode_t modes_get(void);


#endif /* MODES_H_ */