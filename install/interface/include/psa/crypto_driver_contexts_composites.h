/*
 *  Declaration of context structures for use with the PSA driver wrapper
 *  interface. This file contains the context structures for 'composite'
 *  operations, i.e. those operations which need to make use of other operations
 *  from the primitives (crypto_driver_contexts_primitives.h)
 *
 *  Warning: This file will be auto-generated in the future.
 *
 * \note This file may not be included directly. Applications must
 * include psa/crypto.h.
 *
 * \note This header and its content are not part of the Mbed TLS API and
 * applications must not depend on it. Its main purpose is to define the
 * multi-part state objects of the PSA drivers included in the cryptographic
 * library. The definitions of these objects are then used by crypto_struct.h
 * to define the implementation-defined types of PSA multi-part state objects.
 */
/*  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-or-later
 */

#ifndef PSA_CRYPTO_DRIVER_CONTEXTS_COMPOSITES_H
#define PSA_CRYPTO_DRIVER_CONTEXTS_COMPOSITES_H

#include "psa/crypto_driver_common.h"

/* Include the context structure definitions for the Mbed TLS software drivers */
#include "psa/crypto_builtin_composites.h"

/* Include the context structure definitions for those drivers that were
 * declared during the autogeneration process. */

#if defined(MBEDTLS_PSA_CRYPTO_DRIVERS)
#if defined(IFX_PSA_SE_DPA_PRESENT)
#include "ifx_se_psacrypto.h"
#endif /* IFX_PSA_SE_DPA_PRESENT */
#if defined(IFX_PSA_MXCRYPTO_PRESENT)
#include "ifx_mxcrypto_transparent_types.h"
#endif /* IFX_PSA_MXCRYPTO_PRESENT */
#if defined(IFX_PSA_CRYPTOLITE_PRESENT)
#include "ifx_cryptolite_transparent_types.h"
#endif /* IFX_PSA_CRYPTOLITE_PRESENT */
#if defined(IFX_PSA_CRYPTOSUITE_PRESENT)
#include "ifx_cryptosuite_transparent_types.h"
#endif /* IFX_PSA_CRYPTOSUITE_PRESENT */
#endif /* MBEDTLS_PSA_CRYPTO_DRIVERS */

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1)
#include <libtestdriver1/include/psa/crypto.h>
#endif

#if defined(PSA_CRYPTO_DRIVER_TEST)
#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_MAC)
typedef libtestdriver1_mbedtls_psa_mac_operation_t
    mbedtls_transparent_test_driver_mac_operation_t;
typedef libtestdriver1_mbedtls_psa_mac_operation_t
    mbedtls_opaque_test_driver_mac_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_MAC_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_MAC_OPERATION_INIT
#define MBEDTLS_OPAQUE_TEST_DRIVER_MAC_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_MAC_OPERATION_INIT

#else
typedef mbedtls_psa_mac_operation_t
    mbedtls_transparent_test_driver_mac_operation_t;
typedef mbedtls_psa_mac_operation_t
    mbedtls_opaque_test_driver_mac_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_MAC_OPERATION_INIT \
    MBEDTLS_PSA_MAC_OPERATION_INIT
#define MBEDTLS_OPAQUE_TEST_DRIVER_MAC_OPERATION_INIT \
    MBEDTLS_PSA_MAC_OPERATION_INIT

#endif /* MBEDTLS_TEST_LIBTESTDRIVER1 && LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_MAC */

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_AEAD)
typedef libtestdriver1_mbedtls_psa_aead_operation_t
    mbedtls_transparent_test_driver_aead_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_AEAD_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_AEAD_OPERATION_INIT
#else
typedef mbedtls_psa_aead_operation_t
    mbedtls_transparent_test_driver_aead_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_AEAD_OPERATION_INIT \
    MBEDTLS_PSA_AEAD_OPERATION_INIT

#endif /* MBEDTLS_TEST_LIBTESTDRIVER1 && LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_AEAD */

#if defined(MBEDTLS_TEST_LIBTESTDRIVER1) && \
    defined(LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_PAKE)

typedef libtestdriver1_mbedtls_psa_pake_operation_t
    mbedtls_transparent_test_driver_pake_operation_t;
typedef libtestdriver1_mbedtls_psa_pake_operation_t
    mbedtls_opaque_test_driver_pake_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_PAKE_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_PAKE_OPERATION_INIT
#define MBEDTLS_OPAQUE_TEST_DRIVER_PAKE_OPERATION_INIT \
    LIBTESTDRIVER1_MBEDTLS_PSA_PAKE_OPERATION_INIT

#else
typedef mbedtls_psa_pake_operation_t
    mbedtls_transparent_test_driver_pake_operation_t;
typedef mbedtls_psa_pake_operation_t
    mbedtls_opaque_test_driver_pake_operation_t;

#define MBEDTLS_TRANSPARENT_TEST_DRIVER_PAKE_OPERATION_INIT \
    MBEDTLS_PSA_PAKE_OPERATION_INIT
#define MBEDTLS_OPAQUE_TEST_DRIVER_PAKE_OPERATION_INIT \
    MBEDTLS_PSA_PAKE_OPERATION_INIT

#endif /* MBEDTLS_TEST_LIBTESTDRIVER1 && LIBTESTDRIVER1_MBEDTLS_PSA_BUILTIN_PAKE */

#endif /* PSA_CRYPTO_DRIVER_TEST */

/* Define the context to be used for an operation that is executed through the
 * PSA Driver wrapper layer as the union of all possible driver's contexts.
 *
 * The union members are the driver's context structures, and the member names
 * are formatted as `'drivername'_ctx`. This allows for procedural generation
 * of both this file and the content of psa_crypto_driver_wrappers.h */

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_mac_operation_t mbedtls_ctx;
#if defined(PSA_CRYPTO_DRIVER_TEST)
    mbedtls_transparent_test_driver_mac_operation_t transparent_test_driver_ctx;
    mbedtls_opaque_test_driver_mac_operation_t opaque_test_driver_ctx;
#endif
#if defined(IFX_PSA_SE_DPA_PRESENT)
    ifx_se_mac_operation_t ifx_se_dpa_ctx;
#endif /* IFX_PSA_SE_DPA_PRESENT */
#if defined(IFX_PSA_MXCRYPTO_PRESENT) && defined(IFX_PSA_MXCRYPTO_MAC)
    ifx_mxcrypto_transparent_mac_operation_t ifx_mxcrypto_ctx;
#endif /* IFX_PSA_CRYPTOLITE_PRESENT && IFX_PSA_MXCRYPTO_MAC */
#if defined(IFX_PSA_CRYPTOLITE_PRESENT) && defined(IFX_PSA_CRYPTOLITE_HMAC)
    ifx_cryptolite_transparent_mac_operation_t ifx_cryptolite_ctx;
#endif /* IFX_PSA_CRYPTOLITE_PRESENT && IFX_PSA_CRYPTOLITE_HMAC */
#if defined(IFX_PSA_CRYPTOSUITE_PRESENT) && defined(IFX_PSA_CRYPTOSUITE_CMAC)
    ifx_cryptosuite_transparent_mac_operation_t ifx_cryptosuite_ctx;
#endif /* IFX_PSA_CRYPTOSUITE_PRESENT && IFX_PSA_CRYPTOSUITE_CMAC */
} psa_driver_mac_context_t;

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_aead_operation_t mbedtls_ctx;
#if defined(PSA_CRYPTO_DRIVER_TEST)
    mbedtls_transparent_test_driver_aead_operation_t transparent_test_driver_ctx;
#endif
#if defined(IFX_PSA_SE_DPA_PRESENT)
    ifx_se_aead_operation_t ifx_se_dpa_ctx;
#endif /* IFX_PSA_SE_DPA_PRESENT */
#if defined(IFX_PSA_MXCRYPTO_PRESENT) && defined(IFX_PSA_MXCRYPTO_AEAD)
    ifx_mxcrypto_transparent_aead_operation_t ifx_mxcrypto_ctx;
#endif /* IFX_PSA_MXCRYPTO_PRESENT && IFX_PSA_MXCRYPTO_AEAD */
#if defined(IFX_PSA_CRYPTOLITE_PRESENT) && defined(IFX_PSA_CRYPTOLITE_AEAD)
    ifx_cryptolite_transparent_aead_operation_t ifx_cryptolite_ctx;
#endif /* IFX_PSA_CRYPTOLITE_PRESENT && IFX_PSA_CRYPTOLITE_AEAD */
#if defined(IFX_PSA_CRYPTOSUITE_PRESENT) && defined(IFX_PSA_CRYPTOSUITE_CCM)
    ifx_cryptosuite_transparent_aead_operation_t ifx_cryptosuite_ctx;
#endif /* IFX_PSA_CRYPTOSUITE_PRESENT && IFX_PSA_CRYPTOSUITE_CCM */
} psa_driver_aead_context_t;

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_sign_hash_interruptible_operation_t mbedtls_ctx;
} psa_driver_sign_hash_interruptible_context_t;

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_verify_hash_interruptible_operation_t mbedtls_ctx;
} psa_driver_verify_hash_interruptible_context_t;

typedef union {
    unsigned dummy; /* Make sure this union is always non-empty */
    mbedtls_psa_pake_operation_t mbedtls_ctx;
#if defined(PSA_CRYPTO_DRIVER_TEST)
    mbedtls_transparent_test_driver_pake_operation_t transparent_test_driver_ctx;
    mbedtls_opaque_test_driver_pake_operation_t opaque_test_driver_ctx;
#endif
} psa_driver_pake_context_t;

#endif /* PSA_CRYPTO_DRIVER_CONTEXTS_COMPOSITES_H */
/* End of automatically generated file. */
