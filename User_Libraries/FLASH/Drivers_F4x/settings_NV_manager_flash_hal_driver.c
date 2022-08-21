/* This is settings manager driver file (store/restore global settings to
internal FLASH memory)
*/

/* Includes ------------------------------------------------------------------*/
/* Standard includes */
#include <stdbool.h>

/* Hardware includes */
/* Hardware configure */
#include "stm32f4xx_hal.h"

/* Application includes */
#include "settings.h"

// Private constants -----------------------------------------------------------
#define FLASH_PAGE_SIZE 			(uint32_t)0x4000  // Page size = 16KByte

// Device voltage range supposed to be [2.7V to 3.6V], the operation will
// be done by word
#define VOLTAGE_RANGE 				(uint8_t)VOLTAGE_RANGE_3

// Pages 0 and 1 base and end addresses
// Base @ of Sector 0, 16 Kbytes
#define ADDR_FLASH_SECTOR_0     ((uint32_t)0x08000000)
// Base @ of Sector 1, 16 Kbytes
#define ADDR_FLASH_SECTOR_1     ((uint32_t)0x08004000)
// Base @ of Sector 2, 16 Kbytes
#define ADDR_FLASH_SECTOR_2     ((uint32_t)0x08008000)
// Base @ of Sector 3, 16 Kbytes
#define ADDR_FLASH_SECTOR_3     ((uint32_t)0x0800C000)

// Variables -------------------------------------------------------------------
static const uint8_t __attribute__((section (".NV_dataSection"),
		aligned(FLASH_PAGE_SIZE)))
NV_SettingsData[FLASH_PAGE_SIZE] = {0};

// Private function prototypes -------------------------------------------------

// Public functions ------------------------------------------------------------
void SettingsNV_ManagerDriverInit() {}

bool GetBackUpNV_Memory(uint8_t* mem, uint16_t size)
{
	// Validate input parameters
	if(size > FLASH_PAGE_SIZE) return false;

	for(uint16_t i = 0; i < size; i++)
	{
		mem[i] = NV_SettingsData[i];
	}
	return true;
}

/*bool RAM_IsMatchToNV_Memory(uint8_t* mem, uint16_t size,
						const uint8_t* NV_mem, uint16_t NV_size)
{
	for(uint16_t i = 0; i < size; i++)
	{
		if(i >= NV_size) return false;
		if(mem[i] != NV_mem[i]) return false;
	}
	return true;
}*/

bool SetBackUpNV_Memory(uint8_t* mem, uint16_t size)
{
	// Validate input parameters
	if(size > FLASH_PAGE_SIZE) return false;

	FLASH_EraseInitTypeDef pEraseInit;
	uint32_t SectorError = 0;
	pEraseInit.TypeErase = TYPEERASE_SECTORS;
	pEraseInit.NbSectors = 1;
	pEraseInit.VoltageRange = VOLTAGE_RANGE;

	// Search corresponding flash sector
	switch((uint32_t)NV_SettingsData)
	{
	case ADDR_FLASH_SECTOR_0:
		//pEraseInit.Sector = FLASH_SECTOR_0;
		// Forbid to use fist FLASH page
		return false;

	case ADDR_FLASH_SECTOR_1:
		pEraseInit.Sector = FLASH_SECTOR_1;
		break;

	case ADDR_FLASH_SECTOR_2:
		pEraseInit.Sector = FLASH_SECTOR_2;
		break;

	case ADDR_FLASH_SECTOR_3:
		pEraseInit.Sector = FLASH_SECTOR_3;
		break;

	default:
		// Wrong allocation NV_Data array
		return false;
	}

	// Unlock the Flash Program Erase controller
	if(HAL_FLASH_Unlock() != HAL_OK) return false;

	// Erase page before writing
	if(HAL_FLASHEx_Erase(&pEraseInit, &SectorError) != HAL_OK)
	{
		// Erase operation was failed, lock flash controller and return
		HAL_FLASH_Lock();
		return false;
	}
	
	// Create register for 16 bit data
	for(uint16_t i = 0; i < size; i++)
	{
		// Program Flash Bank1
		if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE,
							 (uint32_t)(&NV_SettingsData[i]), mem[i]) != HAL_OK)
		{
			// Flash programm failed
			HAL_FLASH_Lock();
			return false;
		}
	}
		
	HAL_FLASH_Lock();
	return true;
}
	
// Private functions -----------------------------------------------------------
