#include <avr/io.h>

#include "nmbs.h"

/* Network settings for the W5500 */
static wiz_NetInfo net_info = {
	.mac = {0xEA, 0x11, 0x22, 0x33, 0x44, 0xEA},
	.ip = {192, 168, 1, 100},
	.sn = {255, 255, 255, 0},
	.gw = {192, 168, 10, 20},
	.dns = {0, 0, 0, 0},
};

/* Interface settings for the W5500 */
static Wiznet_Config_t Wiznet_SPI_Config = {
	.SPI_Instance = &SPIC,
	.SPI_Port_Instance = &PORTC,
	.CS_Port_Instance = &PORTK,
	.CS_Pin_Number = PIN1_bp,
	.Reset_Port_Instance = &PORTF,
	.Reset_Pin_Number = PIN7_bp,
};

static nmbs_t nmbs;
static nmbs_server_t nmbs_server = {
	.id = 0x01,
	.coils =
	{
		0,
	},
	.regs =
	{
		0,
	},
};

int main(void)
{
	// Configure the Clock System for 32 MHz
	CCP = CCP_IOREG_gc;
	OSC.CTRL = OSC_RC32MEN_bm;
	while(!(OSC.STATUS & OSC_RC32MRDY_bm));
	CCP = CCP_IOREG_gc;
	CLK.CTRL = CLK_SCLKSEL_RC32M_gc;

	// Enable the external 32 kHz clock
	OSC.XOSCCTRL = OSC_X32KLPM_bm | OSC_XOSCSEL_32KHz_gc;
	OSC.CTRL |= OSC_XOSCEN_bm;
	while(!(OSC.STATUS & OSC_XOSCRDY_bm));

    wizchip_register_hal(&Wiznet_SPI_Config);
	wizchip_init(NULL, NULL);
	wizchip_setnetinfo(&net_info);

    while(1)
    {
		int status;

		nmbs_server_init(&nmbs, &nmbs_server);

		switch((status = getSn_SR(MB_SOCKET)))
		{
			case SOCK_ESTABLISHED:
			{
				nmbs_server_poll(&nmbs);
				break;
			}
			case SOCK_INIT:
			{
				listen(MB_SOCKET);
				break;
			}
			case SOCK_CLOSED:
			{
				socket(MB_SOCKET, Sn_MR_TCP, 502, 0);
				break;
			}
			case SOCK_CLOSE_WAIT:
			{
				disconnect(MB_SOCKET);
				break;
			}
			default:
			{
				break;
			}
		}
    }
}