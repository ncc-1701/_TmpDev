/* HAL configuration file */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32F4xx_HAL_CONF_H
#define __STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "settings.h"
#include "FreeRTOSConfig.h"

/* Exported constants --------------------------------------------------------*/
/* ****************************	Interrupts priority ********************************* */
enum
{
	/* Time-critical interrupt priority */

	/* Interrupt, which use RTOS and
	 * other non-time-critical peripheral interrupt priority */
	RTOS_I_PRIOR = configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY,
	ETH_I_PRIOR = RTOS_I_PRIOR,
	RTC_TIMI_PRIOR = ETH_I_PRIOR,
	TRS_SYNC_PROTO_UART_I_PRIOR = RTC_TIMI_PRIOR,

	/* System timer - the lowest priority */
	TICK_INT_PRIORITY,
};

/* System HAL timer --------------------------------------------------------- */
#define HAL_TIM_MODULE_ENABLED
#define HAL_TICK_TIM 				TIM7
#define HAL_TICK_TIM_CLK_ENABLE() 	__HAL_RCC_TIM7_CLK_ENABLE();
#define HAL_TICK_TIM_IRQn 			TIM7_IRQn
#define HAL_TICK_TIM_IRQHandler 	TIM7_IRQHandler

/* UI (buttons and LEDs) peripheral ------------------------------------------*/
/* Buttons */
#define BUTTON_RESET_GPIO_PIN 		GPIO_PIN_10
#define BUTTON_RESET_GPIO_PORT 		GPIOC
#define BUTTON_RESET_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()

/* LEDs */
#define LED_STATUS_GPIO_PIN 		GPIO_PIN_11
#define LED_STATUS_GPIO_PORT 		GPIOC
#define LED_STATUS_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define LED_STATUS_COMMON_ANODE

/* TRS synchronization protocol peripheral ---------------------------------- */
#define HAL_UART_MODULE_ENABLED

/* Definition for UARTx clock resources */
#define TRS_SYNC_PROTO_UART 		USART6
#define TRS_SYNC_PROTO_UART_CLK_ENABLE() __HAL_RCC_USART6_CLK_ENABLE()
#define TRS_SYNC_PROTO_UART_FORCE_RESET() __HAL_RCC_USART6_FORCE_RESET()
#define TRS_SYNC_PROTO_UART_RELEASE_RESET() __HAL_RCC_USART6_RELEASE_RESET()

/* Definition for UARTx Pins */
#define TRS_SYNC_PROTO_UART_TX_GPIO_PIN GPIO_PIN_6
#define TRS_SYNC_PROTO_UART_TX_GPIO_PORT GPIOC
#define TRS_SYNC_PROTO_UART_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define TRS_SYNC_PROTO_UART_TX_GPIO_MODE GPIO_MODE_AF_PP
#define TRS_SYNC_PROTO_UART_TX_AF 	GPIO_AF8_USART6

#define TRS_SYNC_PROTO_UART_RX_GPIO_PIN GPIO_PIN_7
#define TRS_SYNC_PROTO_UART_RX_GPIO_PORT GPIOC
#define TRS_SYNC_PROTO_UART_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOC_CLK_ENABLE()
#define TRS_SYNC_PROTO_UART_RX_GPIO_PULL GPIO_PULLUP
#define TRS_SYNC_PROTO_UART_RX_AF 	GPIO_AF8_USART6

/* Definition for UARTx NVIC */
#define TRS_SYNC_PROTO_UART_IRQn 	USART6_IRQn
#define TRS_SYNC_PROTO_UART_IRQHandler USART6_IRQHandler

/* RTC peripheral configuration ----------------------------------------------*/
#define HAL_RTC_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED

/* Enable port for calibration */
#define RTC_CLOCK_OUTPUT_ENABLE

/* Ethernet interface (RMII) peripheral configuration ----------------------- */
#define HAL_DMA_MODULE_ENABLED
#define HAL_ETH_MODULE_ENABLED

/* GPIO section */
#define ETH_MDC_GPIO_PIN 			GPIO_PIN_1
#define ETH_MDC_GPIO_PORT 			GPIOC
#define ETH_MDC_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()
	
#define ETH_REF_CLK_GPIO_PIN 		GPIO_PIN_1
#define ETH_REF_CLK_GPIO_PORT 		GPIOA
#define ETH_REF_CLK_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define ETH_MDIO_GPIO_PIN 			GPIO_PIN_2
#define ETH_MDIO_GPIO_PORT 			GPIOA
#define ETH_MDIO_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOA_CLK_ENABLE()

#define ETH_TX_EN_GPIO_PIN 			GPIO_PIN_11
#define ETH_TX_EN_GPIO_PORT 		GPIOB
#define ETH_TX_EN_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()

#define ETH_TXD0_GPIO_PIN 			GPIO_PIN_12
#define ETH_TXD0_GPIO_PORT 			GPIOB
#define ETH_TXD0_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOB_CLK_ENABLE()

#define ETH_TXD1_GPIO_PIN 			GPIO_PIN_13
#define ETH_TXD1_GPIO_PORT 			GPIOB
#define ETH_TXD1_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOB_CLK_ENABLE()

#define ETH_CRS_DV_GPIO_PIN 		GPIO_PIN_7
#define ETH_CRS_DV_GPIO_PORT 		GPIOA
#define ETH_CRS_DV_GPIO_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

#define ETH_RXD0_GPIO_PIN 			GPIO_PIN_4
#define ETH_RXD0_GPIO_PORT 			GPIOC
#define ETH_RXD0_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()

#define ETH_RXD1_GPIO_PIN 			GPIO_PIN_5
#define ETH_RXD1_GPIO_PORT 			GPIOC
#define ETH_RXD1_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOC_CLK_ENABLE()

#define ETH_NRST_GPIO_PIN 			GPIO_PIN_14
#define ETH_NRST_GPIO_PORT 			GPIOB
#define ETH_NRST_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOB_CLK_ENABLE()

/* Section 1 : Ethernet peripheral configuration */
/* Definition of the Ethernet driver buffers size and count */
/* Buffer size for receive */
#define ETH_RX_BUF_SIZE 			ETH_MAX_PACKET_SIZE 
/* Buffer size for transmit */
#define ETH_TX_BUF_SIZE 			ETH_MAX_PACKET_SIZE 
/* Rx buffers of size ETH_RX_BUF_SIZE */
#define ETH_RXBUFNB 				((uint32_t)8)
/* Tx buffers of size ETH_TX_BUF_SIZE */
#define ETH_TXBUFNB 				((uint32_t)4)

/* Section 2: PHY configuration section */
/* PHY Reset delay these values are based on a 1 ms Systick interrupt */
#define PHY_RESET_DELAY 			100
/* PHY Configuration delay */
#define PHY_CONFIG_DELAY 			4000

#define PHY_READ_TO 				4000
#define PHY_WRITE_TO 				4000

/* Section 3: Common PHY Registers */
/* Transceiver Basic Control Register */
#define PHY_BCR 					((uint16_t)0x0000)
/* Transceiver Basic Status Register */
#define PHY_BSR 					((uint16_t)0x0001)
 
#define PHY_RESET 					((uint16_t)0x8000)  // PHY Reset
#define PHY_LOOPBACK 				((uint16_t)0x4000)  // Select loop-back mode
#define PHY_FULLDUPLEX_100M 		((uint16_t)0x2100)  // Set the full-duplex mode at 100 Mb/s
#define PHY_HALFDUPLEX_100M 		((uint16_t)0x2000)  // Set the half-duplex mode at 100 Mb/s
#define PHY_FULLDUPLEX_10M 			((uint16_t)0x0100)  // Set the full-duplex mode at 10 Mb/s
#define PHY_HALFDUPLEX_10M 			((uint16_t)0x0000)  // Set the half-duplex mode at 10 Mb/s
#define PHY_AUTONEGOTIATION 		((uint16_t)0x1000)  // Enable auto-negotiation function
#define PHY_RESTART_AUTONEGOTIATION ((uint16_t)0x0200)  // Restart auto-negotiation function
#define PHY_POWERDOWN 				((uint16_t)0x0800)  // Select the power down mode
#define PHY_ISOLATE 				((uint16_t)0x0400)  // Isolate PHY from MII

#define PHY_AUTONEGO_COMPLETE 		((uint16_t)0x0020)  // Auto-Negotiation process completed
#define PHY_LINKED_STATUS 			((uint16_t)0x0004)  // Valid link established
#define PHY_JABBER_DETECTION 		((uint16_t)0x0002)  // Jabber condition detected
  
/* Section 4: Extended PHY Registers */
#define PHY_SR 						((uint16_t)0x0010)  // PHY status register Offset
#define PHY_MICR 					((uint16_t)0x0011)  // MII Interrupt Control Register
#define PHY_MISR 					((uint16_t)0x0012)  // MII Interrupt Status and Misc. Control Register
 
#define PHY_LINK_STATUS 			((uint16_t)0x0001)  // PHY Link mask
#define PHY_SPEED_STATUS 			((uint16_t)0x0002)  // PHY Speed mask
#define PHY_DUPLEX_STATUS 			((uint16_t)0x0004)  // PHY Duplex mask

#define PHY_MICR_INT_EN 			((uint16_t)0x0002)  // PHY Enable interrupts
#define PHY_MICR_INT_OE 			((uint16_t)0x0001)  // PHY Enable output interrupt events

#define PHY_MISR_LINK_INT_EN 		((uint16_t)0x0020)  // Enable Interrupt on change of link status
#define PHY_LINK_INTERRUPT 			((uint16_t)0x2000)  // PHY link status interrupt mask

/* ************************** Assert Selection ************************************** */
/* Uncomment the line below to expanse the "assert_param" macro in the */
/* HAL drivers code */
//#define USE_FULL_ASSERT 			1

/* Peripheral for debuging ---------------------------------------------------*/
/* UART */
#define HAL_UART_MODULE_ENABLED 
#define DEBUG_UART 					USART2
#define DEBUG_UART_CLK_ENABLE() 	__HAL_RCC_USART2_CLK_ENABLE();
#define DEBUG_UART_FORCE_RESET() 	__HAL_RCC_USART2_FORCE_RESET()
#define DEBUG_UART_RELEASE_RESET() 	__HAL_RCC_USART2_RELEASE_RESET()

#define DEBUG_UART_BAUDRATE 		115200

/* Definition for USART_CLK_SYNC Pins */
#define DEBUG_UART_TX_GPIO_PIN 		GPIO_PIN_5
#define DEBUG_UART_TX_GPIO_PORT 	GPIOD
#define DEBUG_UART_TX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define DEBUG_UART_TX_GPIO_MODE 	GPIO_MODE_AF_PP

#define DEBUG_UART_RX_GPIO_PIN 		GPIO_PIN_6
#define DEBUG_UART_RX_GPIO_PORT 	GPIOD
#define DEBUG_UART_RX_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define DEBUG_UART_RX_GPIO_PULL 	GPIO_PULLUP
#define DEBUG_UART_RX_TX_AF 		GPIO_AF7_USART2

/* Outputs */
#define TEST_0_GPIO_PIN 			GPIO_PIN_6
#define TEST_0_GPIO_PORT 			GPIOD
#define TEST_0_GPIO_CLK_ENABLE() 	__HAL_RCC_GPIOD_CLK_ENABLE()

/* Other HAL modules */
#define HAL_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
#define HAL_IWDG_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED

/* ********************* Oscillator Values adaptation ***************************** */
/* Value of the External oscillator in Hz */
#if !defined  (HSE_VALUE) 
  #define HSE_VALUE    ((uint32_t)25000000U) 
#endif /* HSE_VALUE */

/* Time out for HSE start up, in ms */
#if !defined  (HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT    ((uint32_t)100U)   
#endif /* HSE_STARTUP_TIMEOUT */

/* Value of the Internal oscillator in Hz */
#if !defined  (HSI_VALUE)
  #define HSI_VALUE    ((uint32_t)16000000U) 
#endif /* HSI_VALUE */

/* Value of the Internal Low Speed oscillator in Hz */
#if !defined  (LSI_VALUE) 
 #define LSI_VALUE  ((uint32_t)32000U)       
#endif /* LSI_VALUE */

/* Value of the External Low Speed oscillator in Hz */
#if !defined  (LSE_VALUE)
 #define LSE_VALUE  ((uint32_t)32768U)    
#endif /* LSE_VALUE */

/* Time out for LSE start up, in ms */
#if !defined  (LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT    ((uint32_t)5000U)
#endif /* LSE_STARTUP_TIMEOUT */

/* Value of the Internal oscillator in Hz */
#if !defined  (EXTERNAL_CLOCK_VALUE)
  #define EXTERNAL_CLOCK_VALUE    ((uint32_t)12288000U) 
#endif /* EXTERNAL_CLOCK_VALUE */

/* Tip: To avoid modifying this file each time you need to use different HSE,
   ===  you can define the HSE value in your toolchain compiler preprocessor. */

/* *************************** System Configuration ********************************* */
#define  VDD_VALUE                    ((uint32_t)3300U) /* Value of VDD in mv */
#define  USE_RTOS                     0U
#define  PREFETCH_ENABLE              1U
#define  INSTRUCTION_CACHE_ENABLE     1U
#define  DATA_CACHE_ENABLE            1U

/* Includes ------------------------------------------------------------------*/
/* Include module's header file */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32f4xx_hal_rcc.h"
#endif /* HAL_RCC_MODULE_ENABLED */

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32f4xx_hal_gpio.h"
#endif /* HAL_GPIO_MODULE_ENABLED */

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32f4xx_hal_dma.h"
#endif /* HAL_DMA_MODULE_ENABLED */
   
#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32f4xx_hal_cortex.h"
#endif /* HAL_CORTEX_MODULE_ENABLED */

#ifdef HAL_ADC_MODULE_ENABLED
  #include "stm32f4xx_hal_adc.h"
#endif /* HAL_ADC_MODULE_ENABLED */

#ifdef HAL_CAN_MODULE_ENABLED
  #include "stm32f4xx_hal_can.h"
#endif /* HAL_CAN_MODULE_ENABLED */

#ifdef HAL_CRC_MODULE_ENABLED
  #include "stm32f4xx_hal_crc.h"
#endif /* HAL_CRC_MODULE_ENABLED */

#ifdef HAL_CRYP_MODULE_ENABLED
  #include "stm32f4xx_hal_cryp.h" 
#endif /* HAL_CRYP_MODULE_ENABLED */

#ifdef HAL_DMA2D_MODULE_ENABLED
  #include "stm32f4xx_hal_dma2d.h"
#endif /* HAL_DMA2D_MODULE_ENABLED */

#ifdef HAL_DAC_MODULE_ENABLED
  #include "stm32f4xx_hal_dac.h"
#endif /* HAL_DAC_MODULE_ENABLED */

#ifdef HAL_DCMI_MODULE_ENABLED
  #include "stm32f4xx_hal_dcmi.h"
#endif /* HAL_DCMI_MODULE_ENABLED */

#ifdef HAL_ETH_MODULE_ENABLED
  #include "stm32f4xx_hal_eth.h"
#endif /* HAL_ETH_MODULE_ENABLED */

#ifdef HAL_FLASH_MODULE_ENABLED
  #include "stm32f4xx_hal_flash.h"
#endif /* HAL_FLASH_MODULE_ENABLED */
 
#ifdef HAL_SRAM_MODULE_ENABLED
  #include "stm32f4xx_hal_sram.h"
#endif /* HAL_SRAM_MODULE_ENABLED */

#ifdef HAL_NOR_MODULE_ENABLED
  #include "stm32f4xx_hal_nor.h"
#endif /* HAL_NOR_MODULE_ENABLED */

#ifdef HAL_NAND_MODULE_ENABLED
  #include "stm32f4xx_hal_nand.h"
#endif /* HAL_NAND_MODULE_ENABLED */

#ifdef HAL_PCCARD_MODULE_ENABLED
  #include "stm32f4xx_hal_pccard.h"
#endif /* HAL_PCCARD_MODULE_ENABLED */ 
  
#ifdef HAL_SDRAM_MODULE_ENABLED
  #include "stm32f4xx_hal_sdram.h"
#endif /* HAL_SDRAM_MODULE_ENABLED */

#ifdef HAL_HASH_MODULE_ENABLED
 #include "stm32f4xx_hal_hash.h"
#endif /* HAL_HASH_MODULE_ENABLED */

#ifdef HAL_I2C_MODULE_ENABLED
 #include "stm32f4xx_hal_i2c.h"
#endif /* HAL_I2C_MODULE_ENABLED */

#ifdef HAL_I2S_MODULE_ENABLED
 #include "stm32f4xx_hal_i2s.h"
#endif /* HAL_I2S_MODULE_ENABLED */

#ifdef HAL_IWDG_MODULE_ENABLED
 #include "stm32f4xx_hal_iwdg.h"
#endif /* HAL_IWDG_MODULE_ENABLED */

#ifdef HAL_LTDC_MODULE_ENABLED
 #include "stm32f4xx_hal_ltdc.h"
#endif /* HAL_LTDC_MODULE_ENABLED */

#ifdef HAL_PWR_MODULE_ENABLED
 #include "stm32f4xx_hal_pwr.h"
#endif /* HAL_PWR_MODULE_ENABLED */

#ifdef HAL_RNG_MODULE_ENABLED
 #include "stm32f4xx_hal_rng.h"
#endif /* HAL_RNG_MODULE_ENABLED */

#ifdef HAL_RTC_MODULE_ENABLED
 #include "stm32f4xx_hal_rtc.h"
#endif /* HAL_RTC_MODULE_ENABLED */

#ifdef HAL_SAI_MODULE_ENABLED
 #include "stm32f4xx_hal_sai.h"
#endif /* HAL_SAI_MODULE_ENABLED */

#ifdef HAL_SD_MODULE_ENABLED
 #include "stm32f4xx_hal_sd.h"
#endif /* HAL_SD_MODULE_ENABLED */

#ifdef HAL_SPI_MODULE_ENABLED
 #include "stm32f4xx_hal_spi.h"
#endif /* HAL_SPI_MODULE_ENABLED */

#ifdef HAL_TIM_MODULE_ENABLED
 #include "stm32f4xx_hal_tim.h"
#endif /* HAL_TIM_MODULE_ENABLED */

#ifdef HAL_UART_MODULE_ENABLED
 #include "stm32f4xx_hal_uart.h"
#endif /* HAL_UART_MODULE_ENABLED */

#ifdef HAL_USART_MODULE_ENABLED
 #include "stm32f4xx_hal_usart.h"
#endif /* HAL_USART_MODULE_ENABLED */

#ifdef HAL_IRDA_MODULE_ENABLED
 #include "stm32f4xx_hal_irda.h"
#endif /* HAL_IRDA_MODULE_ENABLED */

#ifdef HAL_SMARTCARD_MODULE_ENABLED
 #include "stm32f4xx_hal_smartcard.h"
#endif /* HAL_SMARTCARD_MODULE_ENABLED */

#ifdef HAL_WWDG_MODULE_ENABLED
 #include "stm32f4xx_hal_wwdg.h"
#endif /* HAL_WWDG_MODULE_ENABLED */

#ifdef HAL_PCD_MODULE_ENABLED
 #include "stm32f4xx_hal_pcd.h"
#endif /* HAL_PCD_MODULE_ENABLED */

#ifdef HAL_HCD_MODULE_ENABLED
 #include "stm32f4xx_hal_hcd.h"
#endif /* HAL_HCD_MODULE_ENABLED */
   
#ifdef HAL_DSI_MODULE_ENABLED
 #include "stm32f4xx_hal_dsi.h"
#endif /* HAL_DSI_MODULE_ENABLED */

#ifdef HAL_QSPI_MODULE_ENABLED
 #include "stm32f4xx_hal_qspi.h"
#endif /* HAL_QSPI_MODULE_ENABLED */

#ifdef HAL_CEC_MODULE_ENABLED
 #include "stm32f4xx_hal_cec.h"
#endif /* HAL_CEC_MODULE_ENABLED */

#ifdef HAL_FMPI2C_MODULE_ENABLED
 #include "stm32f4xx_hal_fmpi2c.h"
#endif /* HAL_FMPI2C_MODULE_ENABLED */

#ifdef HAL_SPDIFRX_MODULE_ENABLED
 #include "stm32f4xx_hal_spdifrx.h"
#endif /* HAL_SPDIFRX_MODULE_ENABLED */

#ifdef HAL_LPTIM_MODULE_ENABLED
 #include "stm32f4xx_hal_lptim.h"
#endif /* HAL_LPTIM_MODULE_ENABLED */

/* Exported macro ------------------------------------------------------------*/
#ifdef  USE_FULL_ASSERT
/* The assert_param macro is used for function's parameters check */
#	define assert_param(expr) ((expr) ? (void)0 : \
assert_failed((uint8_t *)__FILE__, __LINE__))
/* Exported functions --------------------------------------------------------*/
void assert_failed(uint8_t* file, uint32_t line);
#else
#	define assert_param(expr) ((void)0)
#endif /* USE_FULL_ASSERT */


#ifdef __cplusplus
}
#endif

#endif /* __STM32F4xx_HAL_CONF_H */
