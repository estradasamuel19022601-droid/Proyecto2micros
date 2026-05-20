
#ifndef EEPROM_H_
#define EEPROM_H_

#include <stdint.h>

#define EEPROM_SLOTS     10
#define EEPROM_SLOT_SZ    8   //bytes para los slots 2 para cada servo

//guarda las posiciones que tengan los servos
void eeprom_save_slot(uint8_t slot, const uint16_t pos[4]);

//carga las posiciones de los servos en los slots indicados
void eeprom_load_slot(uint8_t slot, uint16_t pos[4]);



#endif /* EEPROM_H_ */