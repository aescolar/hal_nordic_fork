/**
 * Copyright (c) 2025 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 *
 * CTR DRBG PRNG as per NIST.SP.800-90Ar1 based on the CRACEN TRNG and CryptoMaster AES HW
 * using AES ECB with a 256bits key.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "nrfx_cracen_trng.h"
#include "nrfx_cracen_cm_aes_ecb.h"

#define MAX_BYTES_PER_REQUEST  (1 << 16)               /* NIST.SP.800-90Ar1:Table 3 */
#define RESEED_INTERVAL        ((uint64_t)1 << 48)     /* 2^48 as per NIST spec */
#define KEY_SIZE               32U                     /* 256 bits AES Key */
#define AES_BLK_SZ             16U                     /* 128 bits */
#define ENTROPY_SIZE           (KEY_SIZE + AES_BLK_SZ) /* Seed equals key-len + 16 */

/** @brief Internal status of this PRNG driver */
struct cracen_prng_status {
	uint8_t  key[KEY_SIZE];
	uint8_t  V[AES_BLK_SZ];
	uint64_t reseed_counter;
	bool     initialized;
};
static struct cracen_prng_status prng;

/**
 * Increment by 1 a number stored in memory in big endian representation.
 * /p v is a pointer to the first byte storing the number.
 * /p size is the size of the number.
 */
static void be_incr(unsigned char *v, size_t size)
{
	unsigned int add = 1;

	do {
		size--;
		add += v[size];
		v[size] = add & 0xFF;
		add >>= 8;
	} while ((add != 0) && (size > 0));
}

/*
 * XOR two arrays of /p size bytes.
 * /p size must be a multiple of 4.
 */
static void xor_array(uint32_t *a, const uint32_t *b, size_t size)
{
	uintptr_t end = (uintptr_t)a + size;

	for (; (uintptr_t)a < end; a++, b++) {
		*a = *a ^ *b;
	}
}

/*
 * Implementation of the CTR_DRBG_Update process as described in NIST.SP.800-90Ar1
 * with ctr_len equal to blocklen.
 * Returns 0 on success, -1 on error
 */
static int ctr_drbg_update(uint8_t *data)
{
	int r = 0;
	char temp[ENTROPY_SIZE];
	size_t temp_length = 0;

	while (temp_length < sizeof(temp)) {
		be_incr(prng.V, AES_BLK_SZ);

		r = nrfx_cracen_cm_aes_ecb(prng.key, sizeof(prng.key), prng.V, temp + temp_length);

		if (r != 0) {
			return -1;
		}
		temp_length += AES_BLK_SZ;
	}

	if (data) {
		xor_array((uint32_t *)temp, (uint32_t *)data, sizeof(temp));
	}

	memcpy(prng.key, temp, sizeof(prng.key));
	memcpy(prng.V, temp + sizeof(prng.key), sizeof(prng.V));

	return 0;
}

/*
 * Re-seed the CTR DRBG
 *
 * return 0 on success, -1 on error
 */
static int cracen_ctr_drbg_reseed(void)
{
	int r;
	char entropy[ENTROPY_SIZE];

	/* Get the entropy used to seed the DRBG */
	r = nrfx_cracen_rng_get_entropy(entropy, sizeof(entropy));
	if (r != 0) {
		return -1;
	}

	r = ctr_drbg_update(entropy);
	if (r != 0) {
		return -1;
	}

	prng.reseed_counter = 1;

	return 0;
}

int nrfx_cracen_ctr_drbg_init(void) {
	int r;

	memset(&prng, 0, sizeof(prng));

	r = cracen_ctr_drbg_reseed();
	if (r != 0) {
		return r;
	}

	prng.initialized = 1;
	return 0;
}

int nrfx_cracen_ctr_drbg_get_random(uint8_t *p_buf, size_t size)
{
	int r = 0;

	if (size > 0 && p_buf == NULL) {
		return -2;
	}

	if (size > MAX_BYTES_PER_REQUEST ) {
		return -2;
	}

	if (!prng.initialized) {
		r = nrfx_cracen_ctr_drbg_init();
		if (r != 0) {
			return -1;
		}
	}

	if (prng.reseed_counter >= RESEED_INTERVAL) {
		r = cracen_ctr_drbg_reseed();
		if (r != 0) {
			return -1;
		}
	}

	while (size > 0) {
		char temp[AES_BLK_SZ];
		size_t cur_len = (size < AES_BLK_SZ) ? size : AES_BLK_SZ;

		be_incr(prng.V, AES_BLK_SZ);

		r = nrfx_cracen_cm_aes_ecb(prng.key, sizeof(prng.key), prng.V, temp);
		if (r != 0) {
			return -1;
		}

		memcpy(p_buf, temp, cur_len);

		size -= cur_len;
		p_buf += cur_len;
	}

	r = ctr_drbg_update(NULL);
	if (r != 0) {
		return -1;
	}

	prng.reseed_counter += 1;

	return 0;
}
