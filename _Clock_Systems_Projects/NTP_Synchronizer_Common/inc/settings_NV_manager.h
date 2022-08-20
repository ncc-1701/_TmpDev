#ifndef _SETTINGS_NV_MANAGER_H_
#define _SETTINGS_NV_MANAGER_H_

// Includes --------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>

// Public constants ------------------------------------------------------------
#if defined(STM32F405xx) || defined(STM32F415xx) || defined(STM32F407xx)|| \
	defined(STM32F417xx) || defined(STM32F427xx) || defined(STM32F437xx) || \
	defined(STM32F429xx)|| defined(STM32F439xx) || defined(STM32F469xx) || \
	defined(STM32F479xx) || defined(STM32F410Tx) || defined(STM32F410Cx) || \
	defined(STM32F410Rx) || defined(STM32F413xx) || defined(STM32F423xx)
#	define STM32F4x_FAMILY
#endif

// Public function prototypes --------------------------------------------------
// Init and loop functions
void SettingsNV_ManagerInit();
void CheckForChangesNV_Settings();

// Driver functions
void SettingsNV_ManagerDriverInit();
void SettingsNV_ManagerDriverProcess();

#if defined (STM32F4x_FAMILY)
bool GetBackUpNV_Memory(uint8_t* mem, uint16_t size);
bool SetBackUpNV_Memory(uint8_t* mem, uint16_t size);
bool GetBackUpNV_Table_1(uint8_t* mem, uint16_t offset, uint16_t size);
bool SetBackUpNV_Table_1(uint8_t* mem, uint16_t offset, uint16_t size);
bool GetBackUpNV_Table_2(uint8_t* mem, uint16_t offset, uint16_t size);
bool SetBackUpNV_Table_2(uint8_t* mem, uint16_t offset, uint16_t size);
#endif

#endif // _SETTINGS_NV_MANAGER_H_
