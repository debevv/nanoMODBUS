/*
 * wizchip_port.c
 *	XMega port for the Wiznet library
 *
 * Created: 20.06.2025 11:37:17
 *  Author: Daniel Kampert
 */

#include <avr/io.h>

#include "wizchip.h"

static volatile SPI_t* SPI;

static volatile PORT_t* CS_Port;
static uint16_t			CS_Num;

static void hal_spi_cs_sel(void)
{
	CS_Port->OUTCLR = (1 << CS_Num);
}

static void hal_spi_cs_desel(void)
{
	CS_Port->OUTSET = (1 << CS_Num);
}

static uint8_t hal_spi_readbyte(void)
{
	SPI->DATA = 0xFF;
	while(!(SPI->STATUS & SPI_IF_bm));

	return SPI->DATA;
}

static void hal_spi_writebyte(uint8_t data)
{
	SPI->DATA = data;
	while(!(SPI->STATUS & SPI_IF_bm));
}

static void hal_spi_readburst(uint8_t *pBuf, uint16_t len)
{
	for (uint8_t i = 0; i < len; i++)
	{
		pBuf[i] = hal_spi_readbyte();
	}
}

static void hal_spi_writeburst(uint8_t *pBuf, uint16_t len)
{
	for (uint8_t i = 0; i < len; i++)
	{
		hal_spi_writebyte(pBuf[i]);
	}
}

Wiznet_Error_t wizchip_register_hal(Wiznet_Config_t* Wiznet_SPI_Config)
{
	if((Wiznet_SPI_Config == NULL) || (Wiznet_SPI_Config->SPI_Instance == NULL) || (Wiznet_SPI_Config->CS_Port_Instance == NULL))
	{
		return __ERROR;		
	}

	SPI = (SPI_t*)Wiznet_SPI_Config->SPI_Instance;
	CS_Port = (PORT_t*)Wiznet_SPI_Config->CS_Port_Instance;

	if(CS_Port == NULL)
	{
		// CS is hardwired
		reg_wizchip_cs_cbfunc(NULL, NULL);
	}
	else
	{
		CS_Num = Wiznet_SPI_Config->CS_Pin_Number;

		// Configure WIZ5500 CS
		CS_Port->DIRSET = PIN1_bm;
		CS_Port->OUTSET = 1 << CS_Num;

		reg_wizchip_cs_cbfunc(hal_spi_cs_sel, hal_spi_cs_desel);
	}

	// Configure the SPI master
	((PORT_t*)Wiznet_SPI_Config->SPI_Port_Instance)->DIR = PIN7_bm | PIN5_bm | PIN4_bm;
	SPI->CTRL = SPI_ENABLE_bm | SPI_MASTER_bm | SPI_MODE_0_gc;

	// Configure WIZ5500 RESET
	((PORT_t*)Wiznet_SPI_Config->Reset_Port_Instance)->DIRSET = (1 << Wiznet_SPI_Config->Reset_Pin_Number);
	((PORT_t*)Wiznet_SPI_Config->Reset_Port_Instance)->OUTSET = (1 << Wiznet_SPI_Config->Reset_Pin_Number);

	// Configure WIZ5500 INT
	//PORTA.DIRCLR = PIN7_bm;

	reg_wizchip_cris_cbfunc(NULL, NULL);
	reg_wizchip_spi_cbfunc(hal_spi_readbyte, hal_spi_writebyte);
	reg_wizchip_spiburst_cbfunc(hal_spi_readburst, hal_spi_writeburst);

	uint8_t version = getVERSIONR();

	// The current silicon version is 0x04 (see the datasheet)
	if(version != 0x04)
	{
		return __ERROR;
	}

	return __OK;
}