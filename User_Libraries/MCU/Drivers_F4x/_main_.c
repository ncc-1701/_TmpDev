/* This is main project file for init func and loops(main and for time-critical
   loops)
*/

/* Includes ----------------------------------------------------------------- */
#include <stdint.h>
#include <string.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Hardware includes */
#include "stm32f4xx_hal.h"

/* Application includes */
#include "settings.h"
#include "main_app.h"

/* Post-includes ------------------------------------------------------------ */
/* Drivers includes */
#ifdef DEBUG_MODULES
#	include "printf_uart_driver.h"
#endif /*DEBUG_MODULES*/

/* Private constants and macroses ------------------------------------------- */
#ifndef WATCH_DOG_RELOAD_PERIOD
#	define WATCH_DOG_RELOAD_PERIOD 		4000
#endif /*WATCH_DOG_RELOAD_PERIOD*/

#define MMIO16(addr) 				(*(volatile uint16_t *)(addr))
#define MMIO32(addr) 				(*(volatile uint32_t *)(addr))
#define U_ID 						0x1FFF7A10

/* Debug options ------------------------------------------------------------ */
//#define DEBUG_HAL_TICK_PERIOD
//#define DEBUG_APP_IDLE_HOOK
//#define DEBUG_MULTITIMER_PERIOD
//#define DEBUG_FREE_RTOS_MEMORY
//#define DEBUG_IDLE_HOOK
//#define DEBUG_HEAP_CCRAM_KB_SIZE 	63

/* Private variables -------------------------------------------------------- */
static TIM_HandleTypeDef hHAL_TickTim;

#if !defined(DEBUG_MODULES) && !defined(DEBUG)
#	ifndef DO_NOT_INIT_WATCHDOG
static IWDG_HandleTypeDef hiwdg;
#	endif /*DO_NOT_INIT_WATCHDOG*/
#endif /*!defined(DEBUG_MODULES) && !defined(DEBUG)*/

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Private function prototypes ---------------------------------------------- */
static void MainDriverInit();
static void Error_Handler();

/* Low-level private function prototypes ------------------------------------ */
static void SystemClock_Config();
static void HeapInit();
static void WatchDog_Init();
static void ResetWDG();

/* Private functions -------------------------------------------------------- */
int main(void)
{
	/* Initialize all hardware and modules */
	/* Init main driver */
	MainDriverInit();
	
	/* Init main application */
	MainAppInit();
	
	/* Start the RTOS scheduler. */
	FreeRTOS_printf(("vTaskStartScheduler\n"));
	vTaskStartScheduler();
	
	/* We should never get here as control is now taken by the scheduler */
	while(1) ;
}

void vApplicationIdleHook(void)
{
	const TickType_t xToggleRate = pdMS_TO_TICKS( 50UL );
	static TickType_t xLastToggle = 0, xTimeNow;

	xTimeNow = xTaskGetTickCount();

	/* As there is not Timer task, toggle the LED 'manually'.  Doing this from
	   the Idle task will also provide visual feedback of the processor load. */
	if( ( xTimeNow - xLastToggle ) >= xToggleRate )
	{
#if defined(DEBUG_MODULES) && defined(DEBUG_IDLE_HOOK)
		HAL_GPIO_TogglePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN);
#endif /*defined(DEBUG_MODULES) && defined(DEBUG_IDLE_HOOK)*/
		xLastToggle += xToggleRate;
	}
	
	/* Restart WatchDog */
	ResetWDG();
	
	MainAppProcess();
		
#ifdef DEBUG_APP_IDLE_HOOK
#	ifdef TEST_0_GPIO_PORT
	HAL_GPIO_TogglePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*DEBUG_APP_IDLE_HOOK*/

#ifdef DEBUG_FREE_RTOS_MEMORY
	static size_t currRTOS_FreeHeap = 0;
	static size_t lastRTOS_FreeHeap = 0; 

	currRTOS_FreeHeap = xPortGetFreeHeapSize();
	if(lastRTOS_FreeHeap != currRTOS_FreeHeap)
		lastRTOS_FreeHeap = currRTOS_FreeHeap;
#endif /*DEBUG_FREE_RTOS_MEMORY*/
}

void vApplicationTickHook(void)
{
#ifdef DEBUG_MULTITIMER_PERIOD
#	ifdef TEST_0_GPIO_PORT
	HAL_GPIO_TogglePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN);
#	endif /*TEST_0_GPIO_PORT*/
#endif /*DEBUG_MULTITIMER_PERIOD*/
	
	/* Time-critical loop */
	MainAppTimeCriticalProcess();
}

/* Additional public functions ---------------------------------------------- */
UBaseType_t uxRand( void )
{
const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;

	/* Utility function to generate a pseudo random number. */

	ulNextRand = ( ulMultiplier * ulNextRand ) + ulIncrement;
	return( ( int ) ( ulNextRand >> 16UL ) & 0x7fffUL );
}

/*
 * Callback that provides the inputs necessary to generate a randomized TCP
 * Initial Sequence Number per RFC 6528.  THIS IS ONLY A DUMMY IMPLEMENTATION
 * THAT RETURNS A PSEUDO RANDOM NUMBER SO IS NOT INTENDED FOR USE IN PRODUCTION
 * SYSTEMS.
 */
extern uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
													uint16_t usSourcePort,
													uint32_t ulDestinationAddress,
													uint16_t usDestinationPort )
{
	( void ) ulSourceAddress;
	( void ) usSourcePort;
	( void ) ulDestinationAddress;
	( void ) usDestinationPort;

	UBaseType_t uxRand( void );
	return uxRand();
}

/* Read U_ID register */
void uid_read(struct Unique_ID *id)
{
    id->off0 = MMIO16(U_ID + 0x0);
    id->off2 = MMIO16(U_ID + 0x2);
    id->off4 = MMIO32(U_ID + 0x4);
    id->off8 = MMIO32(U_ID + 0x8);
}

void prvGetRegistersFromStack(unsigned int* hardfault_args)
{
	/* These are volatile to try and prevent the compiler/linker optimizing
	   them away as the variables never actually get used.  If the debugger
	   won't show the values of the variables, make them global by moving their
	   declaration outside of this function. */
	unsigned int stacked_r0;
	unsigned int stacked_r1;
	unsigned int stacked_r2;
	unsigned int stacked_r3;
	unsigned int stacked_r12;
	unsigned int stacked_lr;
	unsigned int stacked_pc;
	unsigned int stacked_psr;

	stacked_r0 = ((unsigned long) hardfault_args[0]);
	stacked_r1 = ((unsigned long) hardfault_args[1]);
	stacked_r2 = ((unsigned long) hardfault_args[2]);
	stacked_r3 = ((unsigned long) hardfault_args[3]);

	stacked_r12 = ((unsigned long) hardfault_args[4]);
	stacked_lr = ((unsigned long) hardfault_args[5]);
	stacked_pc = ((unsigned long) hardfault_args[6]);
	stacked_psr = ((unsigned long) hardfault_args[7]);

	uint8_t* pHF_REG = (uint8_t*)0xE000ED29;
	uint8_t hf_reg = *pHF_REG;
	
	printf("\r\n[Hard fault handler - all numbers in hex]\r\n");
	printf("R0 = %x\r\n", stacked_r0);
	printf("R1 = %x\r\n", stacked_r1);
	printf("R2 = %x\r\n", stacked_r2);
	printf("R3 = %x\r\n", stacked_r3);
	printf("R12 = %x\r\n", stacked_r12);
	printf("LR [R14] = %x  subroutine call return address\r\n", stacked_lr);
	printf("PC [R15] = %x  program counter\r\n", stacked_pc);
	printf("PSR = %x\r\n", stacked_psr);
	printf("BFAR = %x\r\n", (*((volatile unsigned int*)(0xE000ED38))));
	printf("BFSR = %x\r\n", hf_reg);
	printf("CFSR = %x\r\n", (*((volatile unsigned int*)(0xE000ED28))));
	printf("HFSR = %x\r\n", (*((volatile unsigned int*)(0xE000ED2C))));
	printf("DFSR = %x\r\n", (*((volatile unsigned int*)(0xE000ED30))));
	printf("AFSR = %x\r\n", (*((volatile unsigned int*)(0xE000ED3C))));
	printf("SCB_SHCSR = %x\r\n", (unsigned int)(SCB->SHCSR));
}

void MakeHardFault()
{
    __asm volatile
    (
        "MOVS r0, #1 		\n"
        "LDM r0,{r1-r2} 	\n"
        "BX LR 				\n"
    );
}

/* NVIC_GenerateSystemReset */
/* Generates a system reset. */
void SoftResetCPU()
{
	SCB->AIRCR  = ((0x5FA << SCB_AIRCR_VECTKEY_Pos)      |
				 (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
				 SCB_AIRCR_SYSRESETREQ_Msk); //Keep priority group unchanged
	__DSB(); //Ensure completion of memory access
	while(1); //wait until reset
}

void ExternResetWD()
{
	ResetWDG();
}

/* Assert functions --------------------------------------------------------- */
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t* file, uint32_t line)
{
	/* Add his own implementation to report the file name and line number */
	
}
#endif /*USE_FULL_ASSERT*/

void vAssertCalled(const char *pcFile, uint32_t ulLine)
{
	volatile uint32_t ulBlockVariable = 0UL;
	volatile const char *pcAssertedFileName;
	volatile int iAssertedErrno;
	volatile uint32_t ulAssertedLine;

	ulAssertedLine = ulLine;
	pcAssertedFileName = strrchr( pcFile, '/' );

	/* These variables are set so they can be viewed in the debugger, but are
	   not used in the code - the following lines just remove the compiler
	   warning about this. */
	(void) ulAssertedLine;
	(void) iAssertedErrno;

	if(pcAssertedFileName == 0) pcAssertedFileName = strrchr( pcFile, '\\' );
	if(pcAssertedFileName != NULL) pcAssertedFileName++;
	else pcAssertedFileName = pcFile;
	FreeRTOS_printf(("vAssertCalled( %s, %ld\n", pcFile, ulLine));

	/* Setting ulBlockVariable to a non-zero value in the debugger will allow
	   this function to be exited. */
	taskDISABLE_INTERRUPTS();
	{
		while( ulBlockVariable == 0UL )
		{
			__asm volatile( "NOP" );
		}
	}
	taskENABLE_INTERRUPTS();
}	

/* Debug hooks -------------------------------------------------------------- */
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName)
{
	(void) pcTaskName;
	(void) pxTask;

	/* Run time stack overflow checking is performed if
	   configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	   function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
}

void vApplicationMallocFailedHook(void)
{
	volatile uint32_t ulMallocFailures = 0;

	/* Called if a call to pvPortMalloc() fails because there is insufficient
	   free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	   internally by FreeRTOS API functions that create tasks, queues, software
	   timers, and semaphores.  The size of the FreeRTOS heap is set by the
	   configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	ulMallocFailures++;
}

#if defined(DEBUG_MODULES) || defined(DEBUG)
/* The fault handler implementation calls a function called
   prvGetRegistersFromStack(). */
void HardFault_Handler(void)
{
    __asm volatile
    (
        "TST LR, #4 				\n"
        "ITE EQ 					\n"
        "MRSEQ R0, MSP 				\n"
        "MRSNE R0, PSP 				\n"
        "B prvGetRegistersFromStack 		\n"
    );
	
	for(;;) ;
}
#else /*defined(DEBUG_MODULES) || defined(DEBUG)*/
/*void HardFault_Handler(void)
{
	SoftResetCPU();
}*/
#endif /*defined(DEBUG_MODULES) || defined(DEBUG)*/

/* Public low-level functions ----------------------------------------------- */
/* This function handles TIM1 update interrupt.
   Period elapsed callback in non blocking mode
   Initializes the Global MSP */
void HAL_MspInit(void)
{
	/* System interrupt init */
	/* MemoryManagement_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0, 0);
	/* BusFault_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(BusFault_IRQn, 0, 0);
	/* UsageFault_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(UsageFault_IRQn, 0, 0);
	/* SVCall_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SVCall_IRQn, 0, 0);
	/* DebugMonitor_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DebugMonitor_IRQn, 0, 0);
	/* PendSV_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
	/* SysTick_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(SysTick_IRQn, 15, 0);
}

/* HAL-tick timer */
HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
	RCC_ClkInitTypeDef    clkconfig;
	uint32_t              uwTimclock = 0;
	uint32_t              uwPrescalerValue = 0;
	uint32_t              pFLatency;

	/* Configure the IRQ priority */
	HAL_NVIC_SetPriority(HAL_TICK_TIM_IRQn, TickPriority ,0);

	/* Enable the global Interrupt */
	HAL_NVIC_EnableIRQ(HAL_TICK_TIM_IRQn);

	/* Enable clock */
	HAL_TICK_TIM_CLK_ENABLE();

	/* Get clock configuration */
	HAL_RCC_GetClockConfig(&clkconfig, &pFLatency);

	/* Compute clock */
	uwTimclock = 2*HAL_RCC_GetPCLK1Freq();

	/* Compute the prescaler value to have counter clock equal to 1MHz */
	uwPrescalerValue = (uint32_t) ((uwTimclock / 1000000) - 1);

	/* Initialize */
	hHAL_TickTim.Instance = HAL_TICK_TIM;

	/* Initialize TIMx peripheral as follow:
	+ Period = [(TIM7CLK/1000) - 1]. to have a (1/1000) s time base.
	+ Prescaler = (uwTimclock/1000000 - 1) to have a 1MHz counter clock.
	+ ClockDivision = 0
	+ Counter direction = Up
	*/
	hHAL_TickTim.Init.Period = (1000000 / 1000) - 1;
	hHAL_TickTim.Init.Prescaler = uwPrescalerValue;
	hHAL_TickTim.Init.ClockDivision = 0;
	hHAL_TickTim.Init.CounterMode = TIM_COUNTERMODE_UP;
	if(HAL_TIM_Base_Init(&hHAL_TickTim) == HAL_OK)
	{
		/* Start the TIM time Base generation in interrupt mode */
		return HAL_TIM_Base_Start_IT(&hHAL_TickTim);
	}

	/* Return function status */
	return HAL_ERROR;
}

/* Suspend Tick increment. */
/* Disable the tick increment by disabling TIM1 update interrupt. */
void HAL_SuspendTick(void)
{
	/* Disable TIM1 update Interrupt */
	__HAL_TIM_DISABLE_IT(&hHAL_TickTim, TIM_IT_UPDATE);
}

/* Resume Tick increment. */
/* Enable the tick increment by Enabling TIM1 update interrupt. */
void HAL_ResumeTick(void)
{
	/* Enable TIM1 Update interrupt */
	__HAL_TIM_ENABLE_IT(&hHAL_TickTim, TIM_IT_UPDATE);
}

void HAL_TICK_TIM_IRQHandler(void)
{
	HAL_TIM_IRQHandler(&hHAL_TickTim);

#ifdef DEBUG_HAL_TICK_PERIOD
	HAL_GPIO_TogglePin(TEST_0_GPIO_PORT, TEST_0_GPIO_PIN);
#endif /*DEBUG_HAL_TICK_PERIOD*/

	/* HAL Drivers service */
	HAL_IncTick();
}

__attribute__((weak)) void MainAppInit() {}
__attribute__((weak)) void MainAppProcess() {}
__attribute__((weak)) void MainAppTimeCriticalProcess() {}

/* Private low-level functions ---------------------------------------------- */
static void MainDriverInit()
{
	/* Init HAL driver */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/* Heap_5 is used so the maximum heap size available can be calculated and
	configured at run time. */
	HeapInit();

	/* Init Watch Dog */
	WatchDog_Init();
	
	/* Init debug modules, if need */
#ifdef DEBUG_MODULES
	#ifdef DEBUG_UART
	/* Init debug modules */
	PrintfUART_DriverInit();
	/* Output a message on Hyperterminal using printf function */
	PrintfUART_DriverPrint("\n\rDebug module inited\n\r");
	#endif /*DEBUG_UART*/

	/* Init ports for testing */
	GPIO_InitTypeDef  GPIO_InitStruct;
	
#	ifdef TEST_0_GPIO_PORT
	TEST_0_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = TEST_0_GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(TEST_0_GPIO_PORT, &GPIO_InitStruct);
#	endif /*TEST_0_GPIO_PORT*/
	
#	ifdef TEST_1_GPIO_PORT
	TEST_1_GPIO_CLK_ENABLE();
	GPIO_InitStruct.Pin = TEST_1_GPIO_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed  = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(TEST_1_GPIO_PORT, &GPIO_InitStruct);
#	endif /*TEST_1_GPIO_PORT*/
#endif /*DEBUG_MODULES*/
}

/* System Clock Configuration */
/* The system Clock is configured as follow :
   System Clock source            = PLL (HSE)
   SYSCLK(Hz)                     = 144000000
   HCLK(Hz)                       = 144000000
   AHB Prescaler                  = 1
   APB1 Prescaler                 = 4
   APB2 Prescaler                 = 2
   HSE Frequency(Hz)              = 8000000
   PLL_M                          = 8
   PLL_N                          = 288
   PLL_P                          = 2
   PLL_Q                          = 6
   VDD(V)                         = 3.3
   Main regulator output voltage  = Scale2 mode
   Flash Latency(WS)              = 4 */
static void SystemClock_Config()
{
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;

	/* Enable Power Control clock */
	__HAL_RCC_PWR_CLK_ENABLE();

	/* The voltage scaling allows optimizing the power consumption when the
	   device is clocked below the maximum system frequency, to update the
	   voltage scaling value regarding system frequency refer to product
	   datasheet. */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

	RCC_OscInitStruct.OscillatorType =
			RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

#if defined(MCU_MAX_PERFORMANCE)
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 168;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
#elif defined(MCU_HIGH_PERFORMANCE)
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 144;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 6;
#elif defined(MCU_MIDDLE_PERFORMANCE)
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 72;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 3;
#else
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 144;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 6;
#endif /*MCU_PERFORMANCE*/

	if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
		Error_Handler();

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	   clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
			RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;

#if defined(MCU_MAX_PERFORMANCE)
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
#elif defined(MCU_HIGH_PERFORMANCE)
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
#elif defined(MCU_MIDDLE_PERFORMANCE)
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
#else
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
#endif /*MCU_PERFORMANCE*/

	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);

	/* STM32F405x/407x/415x/417x Revision Z devices: prefetch is supported */
	if (HAL_GetREVID() == 0x1001)
	{
		/* Enable the Flash prefetch */
		__HAL_FLASH_PREFETCH_BUFFER_ENABLE();
	}
}

/* Heap_5 is used so the maximum heap size can be calculated and initialised at
 * run time.  See http://www.freertos.org/a00111.html. */
static void HeapInit()
{
#ifdef DEBUG_HEAP_CCRAM_KB_SIZE
static volatile uint8_t mem_occup[1024 * DEBUG_HEAP_CCRAM_KB_SIZE] = {};
mem_occup[0] = 0xFF;
if(mem_occup[1] == 0x01) mem_occup[0] = 0;
#endif /*DEBUG_HEAP_CCRAM_KB_SIZE*/

#if (configUSE_CCRAM_FOR_HEAP != 0)
extern uint8_t __bss_ccram_end__, _e_ccram_stack;
#define HEAP_CCRAM_START		__bss_ccram_end__
#define HEAP_CCRAM_END		_e_ccram_stack

volatile uint32_t ulHeap_CCRAMSize;
volatile uint8_t *pucHeap_CCRAM_Start;
#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/

extern uint8_t __bss_end__, _estack;
#define HEAP_START		__bss_end__
#define HEAP_END		_estack

volatile uint32_t ulHeapSize;
volatile uint8_t *pucHeapStart;

	/* Heap_5 is used so the maximum heap size can be calculated and initialised
	at run time. */

#if (configUSE_CCRAM_FOR_HEAP != 0)
	/* CCRAM part of heap */
	pucHeap_CCRAM_Start = (uint8_t *) ((((uint32_t) &HEAP_CCRAM_START) + 7) & ~0x07ul);

	ulHeap_CCRAMSize = (uint32_t) (&HEAP_CCRAM_END - &HEAP_CCRAM_START);
	ulHeap_CCRAMSize &= ~0x07ul;
	ulHeap_CCRAMSize -= 1024;
#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/

	/* RAM part of heap */
	pucHeapStart = (uint8_t *) ((((uint32_t) &HEAP_START) + 7) & ~0x07ul);

	ulHeapSize = (uint32_t) (&HEAP_END - &HEAP_START);
	ulHeapSize &= ~0x07ul;
	ulHeapSize -= 1024;

	HeapRegion_t xHeapRegions[] =
	{
#if (configUSE_CCRAM_FOR_HEAP != 0)
		{ (unsigned char *) pucHeap_CCRAM_Start, ulHeap_CCRAMSize },
#endif /*(configUSE_CCRAM_FOR_HEAP != 0)*/

		{ (unsigned char *) pucHeapStart, ulHeapSize },
		{ NULL, 0 }
 	};

	vPortDefineHeapRegions(xHeapRegions);
}

/* IWDG init function */
static void WatchDog_Init()
{
#if !defined(DEBUG_MODULES) && !defined(DEBUG)
#	ifndef DO_NOT_INIT_WATCHDOG
	hiwdg.Instance = IWDG;
	hiwdg.Init.Prescaler = IWDG_PRESCALER_128;
	hiwdg.Init.Reload = (WATCH_DOG_RELOAD_PERIOD*32)/128;
	if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
	{
		Error_Handler();
	}
#	endif /*DO_NOT_INIT_WATCHDOG*/
#endif /*!defined(DEBUG_MODULES) && !defined(DEBUG)*/
}

static void ResetWDG()
{
#if !defined(DEBUG_MODULES) && !defined(DEBUG)
#	ifndef DO_NOT_INIT_WATCHDOG
	HAL_IWDG_Refresh(&hiwdg);
#	endif /*DO_NOT_INIT_WATCHDOG*/
#endif /*!defined(DEBUG_MODULES) && !defined(DEBUG)*/
}

static void Error_Handler(void)
{
	__disable_irq();
	while(1) ;
}
