/*
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * Minimal driver for the CRACEN TRNG for the cracen_ctr_drbg driver
 */

#include <nrf.h>
#include "hal/nrf_cracen.h"
#include "hal/nrf_cracen_rng.h"
#include "soc/nrfx_coredep.h"

/* TRNG HW chosen configuration options */
#define TRNG_CLK_DIV                0
#define TRNG_OFF_TIMER_VAL          0
#define TRNG_INIT_WAIT_VAL        512
#define TRNG_NUMBER_128BIT_BLOCKS   4

#define CONDITIONING_KEY_SIZE 4 /* Size of the conditioning key: 4 words, 16 bytes */

/* Return values common between these internal functions: */
#define OK                 0 /* The function or operation succeeded */
#define HW_PROCESSING     -1 /* Waiting for the hardware to produce data */
#define ERR_TOO_BIG       -2 /* Requested size too large */
#define TRNG_RESET_NEEDED -5 /* Reset needed */

static bool conditioning_key_set;

/*
 * Initialize the TRNG HW and this driver status
 */
static void cracen_trng_init(void)
{
	conditioning_key_set = false;

	/* Disable and softreset the RNG */
	const nrf_cracen_rng_control_t control_reset = {.soft_reset = true};

	nrf_cracen_rng_control_set(NRF_CRACENCORE, &control_reset);

	/* Change from configuration defaults to what we prefer: */
	nrf_cracen_rng_off_timer_set(NRF_CRACENCORE, TRNG_OFF_TIMER_VAL);
	nrf_cracen_rng_clk_div_set(NRF_CRACENCORE, TRNG_CLK_DIV);
	nrf_cracen_rng_init_wait_val_set(NRF_CRACENCORE, TRNG_INIT_WAIT_VAL);

	/* Configure the control register and enable */
	const nrf_cracen_rng_control_t control_enable = {
		.number_128_blocks = TRNG_NUMBER_128BIT_BLOCKS,
		.enable = true
	};

	nrf_cracen_rng_control_set(NRF_CRACENCORE, &control_enable);
}

/*
 * Set the TRNG HW conditioning key.
 *
 * If there is not yet enough data to do so, return HW_PROCESSING
 * otherwise return OK.
 */
static int cracen_trng_setup_conditioning_key(void)
{
	uint32_t level = nrf_cracen_rng_fifo_level_get(NRF_CRACENCORE);

	if (level < CONDITIONING_KEY_SIZE) {
		return HW_PROCESSING;
	}

	for (size_t i = 0; i < CONDITIONING_KEY_SIZE; i++) {
		uint32_t key;
		key = nrf_cracen_rng_fifo_get(NRF_CRACENCORE);
		nrf_cracen_rng_key_set(NRF_CRACENCORE, i, key);
	}

	conditioning_key_set = true;

	return OK;
}

/*
 * If the TRNG HW detected the entropy quality was not ok, return TRNG_RESET_NEEDED
 * If the HW is still starting or there is not enough data, return HW_PROCESSING
 * If the conditioning key is not yet setup, attempt to fill it or return HW_PROCESSING if
 * we don't have enough data to fill it yet.
 * If enough data is ready, fill the /p dst buffer with /p size bytes and return OK
 */
static int cracen_trng_get(char *dst, size_t size)
{
	/* Check that startup tests did not fail and we are ready to read data */
	switch (nrf_cracen_rng_fsm_state_get(NRF_CRACENCORE)) {
		case CRACENCORE_RNGCONTROL_STATUS_STATE_ERROR:
			return TRNG_RESET_NEEDED;
			break;
		case CRACENCORE_RNGCONTROL_STATUS_STATE_RESET:
			return HW_PROCESSING;
			break;
		case CRACENCORE_RNGCONTROL_STATUS_STATE_STARTUP:
		default:
			break;
	}

	/* Program random key for the conditioning function */
	if (!conditioning_key_set) {
		int status = cracen_trng_setup_conditioning_key();

		if (status != OK) {
			return status;
		}
	}

	uint32_t level = nrf_cracen_rng_fifo_level_get(NRF_CRACENCORE);

	if (size > level * 4) { /* FIFO level in 4-byte words */
		return HW_PROCESSING;
	}

	while (size) {
		uint32_t data = nrf_cracen_rng_fifo_get(NRF_CRACENCORE);

		for (int i = 0; i < 4 && size; i++, size--) {
			*dst = (char)(data & 0xFF);
			dst++;
			data >>= 8;
		}
	}

	return OK;
}

int nrfx_cracen_rng_get_entropy(uint8_t *p_buf, size_t size)
{
	/* Block sizes above the FIFO wakeup level to guarantee that the
	 * hardware will be able at some time to provide the requested bytes. */
	if (size > (CRACENCORE_RNGCONTROL_FIFOTHRESHOLD_ResetValue + 1) * 16) {
		return ERR_TOO_BIG;
	}

	nrf_cracen_module_enable(NRF_CRACEN, CRACEN_ENABLE_RNG_Msk);

	int ret = TRNG_RESET_NEEDED;

	while (true) {
		if (ret == TRNG_RESET_NEEDED) {
			cracen_trng_init();
		}
		ret = cracen_trng_get(p_buf, size);
		if (ret == OK) {
			break;
		}
#if defined(CONFIG_SOC_SERIES_BSIM_NRFXX)
		nrfx_coredep_delay_us(1);
#endif
	}

	nrf_cracen_module_disable(NRF_CRACEN, CRACEN_ENABLE_RNG_Msk);

	return OK;
}
