

#include "eeprom.h"
#include <avr/eeprom.h>

//cacluar la direccion del inicio del slot
static uint16_t slot_addr(uint8_t slot)
{
	if (slot >= EEPROM_SLOTS) slot = 0;
	return (uint16_t)(slot * EEPROM_SLOT_SZ);
}

void eeprom_save_slot(uint8_t slot, const uint16_t pos[4])
{
	uint16_t addr = slot_addr(slot);
	for (uint8_t i = 0; i < 4; i++)
	{
		//guardar las lecturas en los slots 
		eeprom_update_byte((uint8_t *)(uintptr_t)(addr + i * 2),
		(uint8_t)(pos[i] & 0xFF));
		eeprom_update_byte((uint8_t *)(uintptr_t)(addr + i * 2 + 1),
		(uint8_t)(pos[i] >> 8));
	}
}

void eeprom_load_slot(uint8_t slot, uint16_t pos[4])
{
	uint16_t addr = slot_addr(slot);
	for (uint8_t i = 0; i < 4; i++)
	{
		uint8_t lo = eeprom_read_byte((const uint8_t *)(uintptr_t)(addr + i * 2));
		uint8_t hi = eeprom_read_byte((const uint8_t *)(uintptr_t)(addr + i * 2 + 1));
		pos[i] = (uint16_t)lo | ((uint16_t)hi << 8);

		//validar el rango, si no es correcto pasa a 1500 (mitad)
		if (pos[i] < 1000 || pos[i] > 2000)
		pos[i] = 1500;
	}
}
