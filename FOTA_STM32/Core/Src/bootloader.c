/*****************************************************************************************************************************
**********************************    Author  : Ehab Magdy Abdullah                      *************************************
**********************************    Linkedin: https://www.linkedin.com/in/ehabmagdyy/  *************************************
**********************************    Youtube : https://www.youtube.com/@EhabMagdyy      *************************************
******************************************************************************************************************************/

/************************************             Includes               ************************************/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "bootloader.h"
#include "usart.h"
#include "crc.h"

/************************************    Static Functions Decelerations  ************************************/
static void Bootloader_Get_Chip_Identification_Number(uint8_t *Host_Buffer);
static void Bootloader_Read_Protection_Level(uint8_t *Host_Buffer);
static void Bootloader_Jump_To_User_App(uint8_t *Host_Buffer);
static void Bootloader_Erase_Flash(uint8_t *Host_Buffer);
static void Bootloader_Memory_Write(uint8_t *Host_Buffer);

static uint8_t Bootloader_CRC_Verify(uint8_t *pData, uint32_t Data_Len, uint32_t Host_CRC);
static void Bootloader_Send_NACK(void);
static void Bootloader_Send_Data_To_Host(uint8_t *Host_Buffer, uint32_t Data_Len);
static uint8_t Host_Address_Verification(uint32_t Jump_Address);
static uint8_t Perform_Flash_Erase(uint8_t Sector_Numebr, uint8_t Number_Of_Sectors);

/************************************    Global Variables Definitions     ************************************/
static uint8_t BL_Host_Buffer[BL_HOST_BUFFER_RX_LENGTH];
static uint8_t appExists = 0;

/************************************ Software Interfaces Implementations ************************************/
BL_Status BL_UART_Fetch_Host_Command(void)
{
	BL_Status Status = BL_NACK;
	HAL_StatusTypeDef HAL_Status = HAL_OK;
	uint8_t Data_Length = 0;

	memset(BL_Host_Buffer, 0, BL_HOST_BUFFER_RX_LENGTH);

	/* Read the length of the command packet received from the HOST */
	HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_UART, BL_Host_Buffer, 1, HAL_MAX_DELAY);

	if(HAL_Status != HAL_OK)
	{
		Status = BL_NACK;
	}
	else
	{
		Data_Length = BL_Host_Buffer[0];
		/* Read the command packet received from the HOST */
		HAL_Status = HAL_UART_Receive(BL_HOST_COMMUNICATION_UART, &BL_Host_Buffer[1], Data_Length, HAL_MAX_DELAY);

		if(HAL_Status != HAL_OK)
		{
			Status = BL_NACK;
		}
		else
		{
			switch(BL_Host_Buffer[1]){
				case CBL_GET_CID_CMD:
					Bootloader_Get_Chip_Identification_Number(BL_Host_Buffer);
					Status = BL_OK;
					break;
				case CBL_GET_RDP_STATUS_CMD:
					Bootloader_Read_Protection_Level(BL_Host_Buffer);
					Status = BL_OK;
					break;
				case CBL_GO_TO_ADDR_CMD:
					Bootloader_Jump_To_User_App(BL_Host_Buffer);
					Status = BL_OK;
					break;
				case CBL_FLASH_ERASE_CMD:
					Bootloader_Erase_Flash(BL_Host_Buffer);
					Status = BL_OK;
					break;
				case CBL_MEM_WRITE_CMD:
					Bootloader_Memory_Write(BL_Host_Buffer);
					Status = BL_OK;
					break;
			}
		}
	}

	return Status;
}

/************************************    Static Functions Implementations  ************************************/
static void Bootloader_Get_Chip_Identification_Number(uint8_t *Host_Buffer)
{
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;
	uint16_t MCU_Identification_Number = 0;
	/* Extract the CRC32 and packet length sent by the HOST */
	Host_CMD_Packet_Len = Host_Buffer[0] + 1;
	Host_CRC32 = *((uint32_t *)((Host_Buffer + Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE));
	/* CRC Verification */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] , Host_CMD_Packet_Len - 4, Host_CRC32)){
		/* Get the MCU chip identification number */
		MCU_Identification_Number = (uint16_t)((DBGMCU->IDCODE) & 0x00000FFF);
		/* Report chip identification number to HOST */
		Bootloader_Send_Data_To_Host((uint8_t *)&MCU_Identification_Number, 2);
	}
	else{
		Bootloader_Send_NACK();
	}
}

static uint8_t CBL_STM32F401_Get_RDP_Level(void)
{
	FLASH_OBProgramInitTypeDef FLASH_OBProgram;
	/* Get the Option byte configuration */
	HAL_FLASHEx_OBGetConfig(&FLASH_OBProgram);

	return (uint8_t)(FLASH_OBProgram.RDPLevel);
}

static void Bootloader_Read_Protection_Level(uint8_t *Host_Buffer)
{
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;
	uint8_t RDP_Level = 0;

	/* Extract the CRC32 and packet length sent by the HOST */
	Host_CMD_Packet_Len = Host_Buffer[0] + 1;
	Host_CRC32 = *((uint32_t *)((Host_Buffer + Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE));
/* CRC Verification */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] , Host_CMD_Packet_Len - 4, Host_CRC32)){
		/* Read Protection Level */
		RDP_Level = CBL_STM32F401_Get_RDP_Level();
		if(0xAA == RDP_Level)		 RDP_Level = 0x00;
		else if(0x55 == RDP_Level)   RDP_Level = 0x01;
		/* Report Valid Protection Level */
		Bootloader_Send_Data_To_Host((uint8_t *)&RDP_Level, 1);
	}
	else{
		Bootloader_Send_NACK();
	}
}

static void Bootloader_Jump_To_User_App(uint8_t *Host_Buffer)
{
	uint16_t Host_CMD_Packet_Len = 0;
	uint32_t Host_CRC32 = 0;

	/* Extract the CRC32 and packet length sent by the HOST */
	Host_CMD_Packet_Len = Host_Buffer[0] + 1;
	Host_CRC32 = *((uint32_t *)((Host_Buffer + Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE));
	/* CRC Verification */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] , Host_CMD_Packet_Len - 4, Host_CRC32))
	{
		if(0xFFFFFFFF != *((volatile uint32_t *)FLASH_SECTOR2_BASE_ADDRESS))
		{
			appExists = 1;
			Bootloader_Send_Data_To_Host((uint8_t *)&appExists, 1);
			/* Value of the main stack pointer of our main application */
			uint32_t MSP_Value = *((volatile uint32_t *)FLASH_SECTOR2_BASE_ADDRESS);

			/* Reset Handler definition function of our main application */
			uint32_t MainAppAddr = *((volatile uint32_t *)(FLASH_SECTOR2_BASE_ADDRESS + 4));

			/* Fetch the reset handler address of the user application */
			pMainApp ResetHandler_Address = (pMainApp)MainAppAddr;

			/* Set Main Stack Pointer */
			__set_MSP(MSP_Value);

			/* DeInitialize / Disable of modules */
			HAL_RCC_DeInit(); /* DeInitialize the RCC clock configuration to the default reset state. */
			                  /* Disable Maskable Interrupt */
			/* Jump to Application Reset Handler */
			ResetHandler_Address();
		}
		else
		{
			appExists = 0;
			Bootloader_Send_Data_To_Host((uint8_t *)&appExists, 1);
		}
	}
	else{
		Bootloader_Send_NACK();
	}
}

static uint8_t Perform_Flash_Erase(uint8_t Sector_Numebr, uint8_t Number_Of_Sectors)
{
	uint8_t Sector_Validity_Status = INVALID_SECTOR_NUMBER;
	FLASH_EraseInitTypeDef pEraseInit;
	uint8_t Remaining_Sectors = 0;
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint32_t SectorError = 0;

	if(Number_Of_Sectors > CBL_FLASH_MAX_SECTOR_NUMBER){
		/* Number Of sectors is out of range */
		Sector_Validity_Status = INVALID_SECTOR_NUMBER;
	}
	else{
		if((Sector_Numebr <= (CBL_FLASH_MAX_SECTOR_NUMBER - 1)) || (CBL_FLASH_MASS_ERASE == Sector_Numebr))
		{
			/* Check if user needs Mass erase */
			if(CBL_FLASH_MASS_ERASE == Sector_Numebr)
			{
				pEraseInit.TypeErase = FLASH_TYPEERASE_MASSERASE; /* Flash Mass erase activation */
			}
			else
			{
				/* User needs Sector erase */
				Remaining_Sectors = CBL_FLASH_MAX_SECTOR_NUMBER - Sector_Numebr;
				if(Number_Of_Sectors > Remaining_Sectors)
				{
					Number_Of_Sectors = Remaining_Sectors;
				}
				else { /* Nothing */ }

				pEraseInit.TypeErase = FLASH_TYPEERASE_SECTORS; /* Sectors erase only */
				pEraseInit.Sector = Sector_Numebr;        /* Initial FLASH sector to erase when Mass erase is disabled */
				pEraseInit.NbSectors = Number_Of_Sectors; /* Number of sectors to be erased. */
			}

			pEraseInit.Banks = FLASH_BANK_1; /* Bank 1  */
			pEraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3; /* Device operating range: 2.7V to 3.6V */

			/* Unlock the FLASH control register access */
			HAL_Status = HAL_FLASH_Unlock();
			/* Perform a mass erase or erase the specified FLASH memory sectors */
			HAL_Status = HAL_FLASHEx_Erase(&pEraseInit, &SectorError);
			if(HAL_SUCCESSFUL_ERASE == SectorError){
				Sector_Validity_Status = SUCCESSFUL_ERASE;
			}
			else{
				Sector_Validity_Status = UNSUCCESSFUL_ERASE;
			}
			/* Locks the FLASH control register access */
			HAL_Status = HAL_FLASH_Lock();
		}
		else{
			Sector_Validity_Status = UNSUCCESSFUL_ERASE;
		}
	}
	return Sector_Validity_Status;
}

static void Bootloader_Erase_Flash(uint8_t *Host_Buffer)
{
	uint16_t Host_CMD_Packet_Len = 0;
    uint32_t Host_CRC32 = 0;
	uint8_t Erase_Status = 0;

	/* Extract the CRC32 and packet length sent by the HOST */
	Host_CMD_Packet_Len = Host_Buffer[0] + 1;
	Host_CRC32 = *((uint32_t *)((Host_Buffer + Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE));
	/* CRC Verification */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] , Host_CMD_Packet_Len - 4, Host_CRC32))
	{
		/* Perform Mass erase or sector erase of the user flash */
		Erase_Status = Perform_Flash_Erase(2, 4);
		if(SUCCESSFUL_ERASE == Erase_Status){
			/* Report erase Passed */
			Bootloader_Send_Data_To_Host((uint8_t *)&Erase_Status, 1);
		}
		else{
			/* Report erase failed */
			Bootloader_Send_Data_To_Host((uint8_t *)&Erase_Status, 1);
		}
	}
	else{
		/* Send Not acknowledge to the HOST */
		Bootloader_Send_NACK();
	}
}

static uint8_t Flash_Memory_Write_Payload(uint8_t *Host_Payload, uint32_t Payload_Start_Address, uint16_t Payload_Len){
	HAL_StatusTypeDef HAL_Status = HAL_ERROR;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	uint16_t Payload_Counter = 0;

	/* Unlock the FLASH control register access */
  HAL_Status = HAL_FLASH_Unlock();

	if(HAL_Status != HAL_OK){
		Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	}
	else{
		for(Payload_Counter = 0; Payload_Counter < Payload_Len; Payload_Counter++){
			/* Program a byte at a specified address */
			HAL_Status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, Payload_Start_Address + Payload_Counter, Host_Payload[Payload_Counter]);
			if(HAL_Status != HAL_OK){
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
				break;
			}
			else{
				Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;
			}
		}
	}

	if((FLASH_PAYLOAD_WRITE_PASSED == Flash_Payload_Write_Status) && (HAL_OK == HAL_Status)){
		/* Locks the FLASH control register access */
		HAL_Status = HAL_FLASH_Lock();
		if(HAL_Status != HAL_OK){
			Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
		}
		else{
			Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_PASSED;
		}
	}
	else{
		Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;
	}

	return Flash_Payload_Write_Status;
}

static void Bootloader_Memory_Write(uint8_t *Host_Buffer)
{
	uint16_t Host_CMD_Packet_Len = 0;
    uint32_t Host_CRC32 = 0;
	uint32_t HOST_Address = 0;
	uint8_t Payload_Len = 0;
	uint8_t Address_Verification = ADDRESS_IS_INVALID;
	uint8_t Flash_Payload_Write_Status = FLASH_PAYLOAD_WRITE_FAILED;

	/* Extract the CRC32 and packet length sent by the HOST */
	Host_CMD_Packet_Len = Host_Buffer[0] + 1;
	Host_CRC32 = *((uint32_t *)((Host_Buffer + Host_CMD_Packet_Len) - CRC_TYPE_SIZE_BYTE));
	/* CRC Verification */
	if(CRC_VERIFICATION_PASSED == Bootloader_CRC_Verify((uint8_t *)&Host_Buffer[0] , Host_CMD_Packet_Len - 4, Host_CRC32)){
		/* Extract the start address from the Host packet */
		HOST_Address = *((uint32_t *)(&Host_Buffer[2]));
		/* Extract the payload length from the Host packet */
		Payload_Len = Host_Buffer[6];
		/* Verify the Extracted address to be valid address */
		Address_Verification = Host_Address_Verification(HOST_Address);
		if(ADDRESS_IS_VALID == Address_Verification){
			/* Write the payload to the Flash memory */
			Flash_Payload_Write_Status = Flash_Memory_Write_Payload((uint8_t *)&Host_Buffer[7], HOST_Address, Payload_Len);
			if(FLASH_PAYLOAD_WRITE_PASSED == Flash_Payload_Write_Status){
				/* Report payload write passed */
				Bootloader_Send_Data_To_Host((uint8_t *)&Flash_Payload_Write_Status, 1);
			}
			else{
				/* Report payload write failed */
				Bootloader_Send_Data_To_Host((uint8_t *)&Flash_Payload_Write_Status, 1);
			}
		}
		else{
			/* Report address verification failed */
			Address_Verification = ADDRESS_IS_INVALID;
			Bootloader_Send_Data_To_Host((uint8_t *)&Address_Verification, 1);
		}
	}
	else{
		/* Send Not acknowledge to the HOST */
		Bootloader_Send_NACK();
	}
}

static uint8_t Host_Address_Verification(uint32_t Jump_Address)
{
	uint8_t Address_Verification = ADDRESS_IS_INVALID;

	if((Jump_Address >= SRAM1_BASE) && (Jump_Address <= STM32F401_SRAM1_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else if((Jump_Address >= FLASH_BASE) && (Jump_Address <= STM32F401_FLASH_END))
	{
		Address_Verification = ADDRESS_IS_VALID;
	}
	else
	{
		Address_Verification = ADDRESS_IS_INVALID;
	}
	return Address_Verification;
}

static uint8_t Bootloader_CRC_Verify(uint8_t *pData, uint32_t Data_Len, uint32_t Host_CRC)
{
	uint8_t CRC_Status = CRC_VERIFICATION_FAILED;
	uint32_t MCU_CRC_Calculated = 0;
	uint8_t Data_Counter = 0;
	uint32_t Data_Buffer = 0;
	/* Calculate CRC32 */
	for(Data_Counter = 0; Data_Counter < Data_Len; Data_Counter++){
		Data_Buffer = (uint32_t)pData[Data_Counter];
		MCU_CRC_Calculated = HAL_CRC_Accumulate(CRC_ENGINE_OBJ, &Data_Buffer, 1);
	}
	/* Reset the CRC Calculation Unit */
  __HAL_CRC_DR_RESET(CRC_ENGINE_OBJ);
	/* Compare the Host CRC and Calculated CRC */
	if(MCU_CRC_Calculated == Host_CRC){
		CRC_Status = CRC_VERIFICATION_PASSED;
	}
	else{
		CRC_Status = CRC_VERIFICATION_FAILED;
	}

	return CRC_Status;
}

static void Bootloader_Send_NACK(void)
{
	uint8_t Ack_Value = CBL_SEND_NACK;
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_UART, &Ack_Value, 1, HAL_MAX_DELAY);
}

static void Bootloader_Send_Data_To_Host(uint8_t *Host_Buffer, uint32_t Data_Len)
{
	HAL_UART_Transmit(BL_HOST_COMMUNICATION_UART, Host_Buffer, Data_Len, HAL_MAX_DELAY);
}
