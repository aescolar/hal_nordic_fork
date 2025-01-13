/*
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * Minimal driver for the CRACEN CryptoMaster AES ECB for the cracen_ctr_drbg driver
 */

#include <stdint.h>
#include <stddef.h>
#include "hal/nrf_cracen.h"
#include "hal/nrf_cracen_cm.h"
#include "helpers/nrf_cracen_cm_dma.h"
#include "soc/nrfx_coredep.h"

/*
 * Check if the CryptoMaster is done
 *
 * returns 0 if done, -1 if busy, and -2 on error
 */
static int cracen_cm_check_done(void)
{
	uint32_t ret;
	uint32_t busy;

	ret = nrf_cracen_cm_int_pending_get(NRF_CRACENCORE);

	if (ret & (NRF_CRACEN_CM_INT_FETCH_ERROR_MASK | NRF_CRACEN_CM_INT_PUSH_ERROR_MASK)) {
		return -2;
	}

	busy = nrf_cracen_cm_status_get(NRF_CRACENCORE,
					(NRF_CRACEN_CM_STATUS_BUSY_FETCH_MASK
					| NRF_CRACEN_CM_STATUS_BUSY_PUSH_MASK
					| NRF_CRACEN_CM_STATUS_PUSH_WAITING_MASK));

	if (busy) {
		return -1;
	}

	return 0;
}

int nrfx_cracen_cm_aes_ecb(uint8_t *key_p, size_t key_size, uint8_t *input_p, uint8_t *output_p)
{
#define ECB_BLK_SZ (16U)

	int ret;

	const uint32_t aes_config_value = NRF_CRACEN_CM_AES_CONFIG(
			NRF_CRACEN_CM_AES_CONFIG_MODE_ECB,
			NRF_CRACEN_CM_AES_CONFIG_KEY_SW_PROGRAMMED,
			false, false, false);

	struct nrf_cracen_cm_dma_desc in_descs[3];

	in_descs[0].p_addr = (uint8_t *)&aes_config_value;
	in_descs[0].length = sizeof(aes_config_value) | NRF_CRACEN_CM_DMA_DESC_LENGTH_REALIGN;
	in_descs[0].dmatag = NRF_CRACEN_CM_DMA_TAG_AES_CONFIG(NRF_CRACEN_CM_AES_REG_OFFSET_CONFIG);
	in_descs[0].p_next = &in_descs[1];

	in_descs[1].p_addr = key_p;
	in_descs[1].length = key_size | NRF_CRACEN_CM_DMA_DESC_LENGTH_REALIGN;
	in_descs[1].dmatag = NRF_CRACEN_CM_DMA_TAG_AES_CONFIG(NRF_CRACEN_CM_AES_REG_OFFSET_KEY);
	in_descs[1].p_next = &in_descs[2];

	in_descs[2].p_addr = input_p;
	in_descs[2].length = ECB_BLK_SZ | NRF_CRACEN_CM_DMA_DESC_LENGTH_REALIGN;
	in_descs[2].dmatag = NRF_CRACEN_CM_DMA_TAG_LAST | NRF_CRACEN_CM_DMA_TAG_ENGINE_AES
			    | NRF_CRACEN_CM_DMA_TAG_DATATYPE_AES_PAYLOAD;
	in_descs[2].p_next = NRF_CRACEN_CM_DMA_DESC_STOP;

	struct nrf_cracen_cm_dma_desc out_desc;

	out_desc.p_addr = output_p;
	out_desc.length = ECB_BLK_SZ | NRF_CRACEN_CM_DMA_DESC_LENGTH_REALIGN;
	out_desc.p_next = NRF_CRACEN_CM_DMA_DESC_STOP;
	out_desc.dmatag = NRF_CRACEN_CM_DMA_TAG_LAST;

	nrf_cracen_module_enable(NRF_CRACEN, CRACEN_ENABLE_CRYPTOMASTER_Msk);

	nrf_cracen_cm_fetch_addr_set(NRF_CRACENCORE, (void *)in_descs);
	nrf_cracen_cm_push_addr_set(NRF_CRACENCORE, (void *)&out_desc);

	nrf_cracen_cm_config_indirect_set(NRF_CRACENCORE, NRF_CRACEN_CM_CONFIG_INDIRECT_FETCH_MASK
					  | NRF_CRACEN_CM_CONFIG_INDIRECT_PUSH_MASK);

	__DMB();

	nrf_cracen_cm_start(NRF_CRACENCORE);

	do {
		/* The HW is so fast that it is better to "busy wait" here than program an
		 * interrupt. This will normally already succeed in the first try
		 */
#if defined(CONFIG_SOC_SERIES_BSIM_NRFXX)
		nrfx_coredep_delay_us(1);
#endif
		ret = cracen_cm_check_done();
	} while (ret == -1);

	nrf_cracen_cm_softreset(NRF_CRACENCORE);
	nrf_cracen_module_disable(NRF_CRACEN, CRACEN_ENABLE_CRYPTOMASTER_Msk);

	return ret;
}
