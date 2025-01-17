/**
 * @file emac.c
 *
 */
/* Copyright (C) 2018-2022 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wuseless-cast"

#include <cstdint>
#include <stdbool.h>
#include <cstring>
//#ifndef NDEBUG
# include <cstdio>
//#endif
#include <cassert>

#include "emac.h"

#include "h3.h"
#include "ephy.h"
#include "phy.h"
#include "mii.h"

#include "debug.h"

coherent_region *p_coherent_region = 0;

void _set_syscon_ephy(void) {
	/* H3 based SoC's that has an Internal 100MBit PHY
	 * needs to be configured and powered up before use
	 */
	uint32_t value = H3_SYSTEM->EMAC_CLK;

	value &= ~H3_EPHY_DEFAULT_MASK;
	value |= H3_EPHY_DEFAULT_VALUE;
	value |= PHY_ADDR << H3_EPHY_ADDR_SHIFT;
	value &= ~H3_EPHY_SHUTDOWN;
	value |= H3_EPHY_SELECT;

	H3_SYSTEM->EMAC_CLK = value;
}

void _adjust_link(bool duplex, uint32_t speed) {
	uint32_t value = H3_EMAC->CTL0;

	if (duplex) {
		value |= CTL0_DUPLEX_FULL_DUPLEX;
	} else {
		value &= (uint32_t)~CTL0_DUPLEX_FULL_DUPLEX;
	}

	value &= (uint32_t)~(CTL0_SPEED_MASK << CTL0_SPEED_SHIFT);

	switch (speed) {
	case 1000:
		break;
	case 100:
		value |= CTL0_SPEED_100M;
		break;
	case 10:
		value |= CTL0_SPEED_10M;
		break;
	default:
		break;
	}

	H3_EMAC->CTL0 = value;
}

static void _rx_descs_init(void) {
	struct emac_dma_desc *desc_table_p = &p_coherent_region->rx_chain[0];
	char *rxbuffs = &p_coherent_region->rxbuffer[0];
	struct emac_dma_desc *desc_p;
	uint32_t idx;

	for (idx = 0; idx < CONFIG_RX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->buf_addr = (uintptr_t) &rxbuffs[idx * CONFIG_ETH_BUFSIZE];
		desc_p->next = (uintptr_t) &desc_table_p[idx + 1];
		desc_p->st = CONFIG_ETH_RXSIZE;
		desc_p->status = (1U << 31);
	}

	/* Correcting the last pointer of the chain */
	desc_p->next = (uintptr_t) &desc_table_p[0];

	H3_EMAC->RX_DMA_DESC = (uintptr_t)&desc_table_p[0];
	p_coherent_region->rx_currdescnum = 0;
}

static void _tx_descs_init(void) {
	struct emac_dma_desc *desc_table_p = &p_coherent_region->tx_chain[0];
	char *txbuffs = &p_coherent_region->txbuffer[0];
	struct emac_dma_desc *desc_p;
	uint32_t idx;

	for (idx = 0; idx < CONFIG_TX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->buf_addr = (uintptr_t) &txbuffs[idx * CONFIG_ETH_BUFSIZE];
		desc_p->next = (uintptr_t) &desc_table_p[idx + 1];
		desc_p->status = (1U << 31);
		desc_p->st = 0;
	}

	/* Correcting the last pointer of the chain */
	desc_p->next = (uintptr_t) &desc_table_p[0];

	H3_EMAC->TX_DMA_DESC = (uintptr_t)&desc_table_p[0];
	p_coherent_region->tx_currdescnum = 0;
}

void _autonegotiation(void) {
	uint32_t value;

	DEBUG_PRINTF( "PHY status BMCR: %04X, BMSR: %04X, ADVERTISE: %04X", phy_read(PHY_ADDR, MII_BMCR), phy_read(PHY_ADDR, MII_BMSR), phy_read(PHY_ADDR, MII_ADVERTISE));

	phy_write(PHY_ADDR, MII_ADVERTISE, ADVERTISE_FULL | ADVERTISE_RFAULT);

	value = (uint32_t)phy_read (PHY_ADDR, MII_BMCR);
	value |= BMCR_ANRESTART;
	phy_write(PHY_ADDR, MII_BMCR, (uint16_t) value);

	const uint32_t micros_timeout = H3_TIMER->AVS_CNT1 + (5 * 1000 * 1000);

	while (H3_TIMER->AVS_CNT1 < micros_timeout) {
		if (phy_read (PHY_ADDR, MII_BMSR) & BMSR_ANEGCOMPLETE) {
			break;
		}
	}

	DEBUG_PRINTF("%d", micros_timeout - H3_TIMER->AVS_CNT1);
	DEBUG_PRINTF( "PHY status BMCR: %04x, BMSR: %04x", phy_read (PHY_ADDR, MII_BMCR), phy_read (PHY_ADDR, MII_BMSR) );
}

extern void mac_address_get(uint8_t paddr[]);

void __attribute__((cold)) emac_start(uint8_t paddr[]) {
	mac_address_get(paddr);

	uint32_t value;

	H3_CCU->BUS_SOFT_RESET2 |= BUS_SOFT_RESET2_EPHY_RST;
	udelay(1000); // 1ms
	H3_CCU->BUS_CLK_GATING4 &= (uint32_t)~BUS_CLK_GATING4_EPHY_GATING;
	udelay(1000); // 1ms
	H3_CCU->BUS_CLK_GATING4 |= BUS_CLK_GATING4_EPHY_GATING;

	_set_syscon_ephy();
	_autonegotiation();
	_adjust_link(true, 100);

#ifndef NDEBUG
	printf("sizeof(struct coherent_region)=%u\n", sizeof(struct coherent_region));
	printf("H3_SYSTEM->EMAC_CLK=%p ", H3_SYSTEM->EMAC_CLK);
	debug_print_bits(H3_SYSTEM->EMAC_CLK);
	printf("H3_EMAC->CTL0=%p ", H3_EMAC->CTL0);
	debug_print_bits(H3_EMAC->CTL0);
	printf( "PHY status BMCR: %04x, BMSR: %04x\n", phy_read (PHY_ADDR, MII_BMCR), phy_read (PHY_ADDR, MII_BMSR) );
	debug_print_bits((uint32_t) phy_read (PHY_ADDR, MII_BMCR));
	debug_print_bits((uint32_t) phy_read (PHY_ADDR, MII_BMSR));
#endif

	assert(p_coherent_region == 0);
	assert(sizeof(struct coherent_region) < MEGABYTE/2);

	p_coherent_region = (struct coherent_region *)H3_MEM_COHERENT_REGION;

	value = H3_EMAC->RX_CTL0;
	value &= ~RX_CTL0_RX_EN;
	H3_EMAC->RX_CTL0 = value;

	value = H3_EMAC->TX_CTL0;
	value &= ~TX_CTL0_TX_EN;
	H3_EMAC->TX_CTL0 = value;

	value = H3_EMAC->TX_CTL1;
	value &= (uint32_t) ~TX_CTL1_TX_DMA_EN;
	H3_EMAC->TX_CTL1 = value;

	value = H3_EMAC->RX_CTL1;
	value &= (uint32_t) ~RX_CTL1_RX_DMA_EN;
	H3_EMAC->RX_CTL1 = value;

	_rx_descs_init();
	_tx_descs_init();

	H3_EMAC->RX_FRM_FLT = RX_FRM_FLT_RX_ALL_MULTICAST;

	value = H3_EMAC->RX_CTL1;
	value |= RX_CTL1_RX_DMA_EN;
	H3_EMAC->RX_CTL1 = value;

	value = H3_EMAC->TX_CTL1;
	value |= TX_CTL1_TX_DMA_EN;
	H3_EMAC->TX_CTL1 = value;

	value = H3_EMAC->RX_CTL0;
	value |= RX_CTL0_RX_EN;
	H3_EMAC->RX_CTL0 = value;

	value = H3_EMAC->TX_CTL0;
	value |= TX_CTL0_TX_EN;
	H3_EMAC->TX_CTL0 = value;

//	H3_EMAC->INT_EN = (uint32_t)(~0);

#ifndef NDEBUG
	printf("================\n");
	printf("H3_EMAC->RX_CTL0=%p ", H3_EMAC->RX_CTL0);
	debug_print_bits(H3_EMAC->RX_CTL0);
	printf("H3_EMAC->RX_CTL1=%p ", H3_EMAC->RX_CTL1);
	debug_print_bits(H3_EMAC->RX_CTL1);
	printf("H3_EMAC->RX_FRM_FLT=%p ", H3_EMAC->RX_FRM_FLT);
	debug_print_bits(H3_EMAC->RX_FRM_FLT);
	puts("");
	printf("H3_EMAC->TX_CTL0=%p ", H3_EMAC->TX_CTL0);
	debug_print_bits(H3_EMAC->TX_CTL0);
	printf("H3_EMAC->TX_CTL1=%p ", H3_EMAC->TX_CTL1);
	debug_print_bits(H3_EMAC->TX_CTL1);
	puts("");
	printf("H3_EMAC->ADDR[0].LOW=%08x, H3_EMAC->ADDR[0].HIGH=%08x\n", H3_EMAC->ADDR[0].LOW, H3_EMAC->ADDR[0].HIGH);
	puts("");
	printf("H3_EMAC->TX_DMA_STA=%p\n", H3_EMAC->TX_DMA_STA);
	printf("H3_EMAC->TX_CUR_DESC=%p\n", H3_EMAC->TX_CUR_DESC);
	printf("H3_EMAC->TX_CUR_BUF=%p\n", H3_EMAC->TX_CUR_BUF);
	puts("");
	printf("H3_EMAC->RX_DMA_STA=%p\n", H3_EMAC->RX_DMA_STA);
	printf("H3_EMAC->RX_CUR_DESC=%p\n", H3_EMAC->RX_CUR_DESC);
	printf("H3_EMAC->RX_CUR_BUF=%p\n", H3_EMAC->RX_CUR_BUF);
	printf("================\n");
#endif
}
