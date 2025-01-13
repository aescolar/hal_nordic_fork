/*
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NRFX_CRACEN_CM_CTR_DRBG_H
#define _NRFX_CRACEN_CM_CTR_DRBG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the CRACEN CTR DRBG pseudo random generator
 *
 * Returns 0 on success, -1 on error
 *
 * @note This function is only meant to be called once.
 *
 * @note It is not required to call this function before @cracen_ctr_drbg_get_random.
 *       If @ref cracen_get_random() were to be called without ever calling this,
 *       the same initialization would be done with its first call.
 *       But this initialization is relatively slow and power consuming. So this function
 *       allows initializing in it what may be a less constrained moment.
 *
 * @note This function assumes exclusive access to the CRACEN TRNG and CryptoMaster, and may
 *       not be used while any other component is using those peripherals
 *
 * @note This function is not reentrant.
 */
int nrfx_cracen_ctr_drbg_init(void);

/**
 * @brief Fill the /p output buffer with /p size bytes of random data
 *
 * @param[out] p_buf Buffer into which to copy \p size bytes of entropy
 * @param[in]  size  Number of bytes to copy
 *
 * @return 0 on success ; -2 if the input is invalid; -1 on other errors
 *
 * @note This function assumes exclusive access to the CRACEN TRNG and CryptoMaster, and may
 *       not be used while any other component is using those peripherals
 *
 * @note This function is not reentrant.
 */
int nrfx_cracen_ctr_drbg_get_random(uint8_t *p_buf, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* _NRFX_CRACEN_CM_CTR_DRBG_H */
