/*****************************************************************************************************************************
**********************************    Author  : Ehab Magdy Abdullah                      *************************************
**********************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *************************************
**********************************    Youtube : https://www.youtube.com/@EhabMagdyy      *************************************
******************************************************************************************************************************/

#ifndef BOOTLOADER_H_
#define BOOTLOADER_H_

/*************************************     Macro Decelerations         *************************************/
#define BL_HOST_COMMUNICATION_UART       &huart1
#define CRC_ENGINE_OBJ                   &hcrc

#define BL_HOST_BUFFER_RX_LENGTH         100

#define BL_ENABLE_UART_DEBUG_MESSAGE     0x00
#define BL_ENABLE_SPI_DEBUG_MESSAGE      0x01
#define BL_ENABLE_CAN_DEBUG_MESSAGE      0x02
#define BL_DEBUG_METHOD 				 (BL_ENABLE_UART_DEBUG_MESSAGE)

#define CBL_GET_CID_CMD                  0x10
#define CBL_GET_RDP_STATUS_CMD           0x11
#define CBL_GO_TO_ADDR_CMD               0x12
#define CBL_FLASH_ERASE_CMD              0x13
#define CBL_MEM_WRITE_CMD                0x14

/* CRC_VERIFICATION */
#define CRC_TYPE_SIZE_BYTE               4

#define CRC_VERIFICATION_FAILED          0x00
#define CRC_VERIFICATION_PASSED          0x01

#define CBL_SEND_NACK                    0xAB

/* Start address of sector 2 */
#define FLASH_SECTOR2_BASE_ADDRESS       0x08008000U

#define ADDRESS_IS_INVALID               0x00
#define ADDRESS_IS_VALID                 0x01

#define STM32F401_SRAM1_SIZE             (64 * 1024)
#define STM32F401_FLASH_SIZE             (1024 * 1024)
#define STM32F401_SRAM1_END              (SRAM1_BASE + STM32F401_SRAM1_SIZE)
#define STM32F401_FLASH_END              (FLASH_BASE + STM32F401_FLASH_SIZE)

/* CBL_FLASH_ERASE_CMD */
#define CBL_FLASH_MAX_SECTOR_NUMBER      6
#define CBL_FLASH_MASS_ERASE             0xFF

#define INVALID_SECTOR_NUMBER            0x00
#define VALID_SECTOR_NUMBER              0x01
#define UNSUCCESSFUL_ERASE               0x02
#define SUCCESSFUL_ERASE                 0x03

#define HAL_SUCCESSFUL_ERASE             0xFFFFFFFFU

/* CBL_MEM_WRITE_CMD */
#define FLASH_PAYLOAD_WRITE_FAILED       0x00
#define FLASH_PAYLOAD_WRITE_PASSED       0x01

#define FLASH_LOCK_WRITE_FAILED          0x00
#define FLASH_LOCK_WRITE_PASSED          0x01

/* CBL_GET_RDP_STATUS_CMD */
#define ROP_LEVEL_READ_INVALID           0x00
#define ROP_LEVE_READL_VALID             0X01

/* CBL_CHANGE_ROP_Level_CMD */
#define ROP_LEVEL_CHANGE_INVALID         0x00
#define ROP_LEVEL_CHANGE_VALID           0X01

#define CBL_ROP_LEVEL_0                  0x00
#define CBL_ROP_LEVEL_1                  0x01
#define CBL_ROP_LEVEL_2                  0x02


/************************************       Data Type Decelerations     ************************************/
typedef enum
{
	BL_NACK = 0,
	BL_OK
}BL_Status;

typedef void (*pMainApp)(void);
typedef void (*Jump_Ptr)(void);

/************************************ Software Interfaces Decelerations ************************************/
BL_Status BL_UART_Fetch_Host_Command(void);
void BL_Print_Message(char *format, ...);

#endif /* BOOTLOADER_H_ */
