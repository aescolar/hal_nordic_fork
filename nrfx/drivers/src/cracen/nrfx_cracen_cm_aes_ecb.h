/*
 * Copyright (c) 2025, Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _NRFX_CRACEN_CM_AES_ECB_H
#define _NRFX_CRACEN_CM_AES_ECB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Encrypt with AES-ECB the input data using the CRACEN CryptoMaster module
 *
 * @param[in] key_p    Pointer to the key
 * @param[in] key_size Size of the key in bytes (valid sizes 16, 24 or 32 => 128, 192 or 256 bits)
 * @param[in] input_p  Pointer to the input data (16 bytes/128 bits)
 * @param[in] output_p Pointer to the output data (16 bytes/128 bits)
 *
 * @return 0 on success, a negative number on failure
 *
 * @note The key, input and output data are in big endian/cryptographic order. That is, input[0]
 *       corresponds to the highest byte of the 128bit input.
 *
 * @note The only failure one can normally expect are bus failures due to incorrect pointers.
 *
 * @note This function is meant to be used by the nrfx_random_ctr_drbg driver.
 *       If using it outside of this driver it must be used with care specially if any other
 *       component is using CRACEN.
 *       It may not be used if any other component is using the CRACEN CM at the same time.
 *
 * @note This function is not reentrant.
 *
 * @note The key size needs to be supported by the CRACEN CryptoMaster AES engine.
 */
int nrfx_cracen_cm_aes_ecb(uint8_t *key_p, size_t key_size, uint8_t *input_p, uint8_t *output_p);

#ifdef __cplusplus
}
#endif

#endif /* _NRFX_CRACEN_CM_AES_ECB_H */
