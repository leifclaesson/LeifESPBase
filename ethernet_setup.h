#pragma once

#define ETH_PHY_ADDR         1
#define ETH_PHY_POWER       -1
#define ETH_PHY_MDC         23
#define ETH_PHY_MDIO        18
#define ETH_PHY_TYPE        ETH_PHY_LAN8720

#ifdef ETH_EXT_CLK
#define ETH_CLK_MODE        ETH_CLOCK_GPIO0_IN
#else
#define ETH_CLK_MODE        ETH_CLOCK_GPIO17_OUT
#endif


/*

The connections needed

*GPIO17 - PHY_POWER   : NC - Osc. Enable - 4k7 Pulldown
*GPIO22 - EMAC_TXD1   : TX1
*GPIO19 - EMAC_TXD0   : TX0
*GPIO21 - EMAC_TX_EN  : TX_EN
*GPIO26 - EMAC_RXD1   : RX1
*GPIO25 - EMAC_RXD0   : RX0
*GPIO27 - EMAC_RX_DV  : CRS
*GPIO00 - EMAC_TX_CLK : nINT/REFCLK (50MHz) - 4k7 Pullup
*GPIO23 - SMI_MDC     : MDC
GPIO18 - SMI_MDIO    : MDIO
GND                  : GND
3V3                  : VCC

PHY_POWER, SMI_MDC and SMI_MDIO can freely be moved to other GPIOs.

EMAC_TXD0, EMAC_TXD1, EMAC_TX_EN, EMAC_RXD0, EMAC_RXD1, EMAC_RX_DV and EMAC_TX_CLK are fixed and can't be rerouted to other GPIOs.













 */
