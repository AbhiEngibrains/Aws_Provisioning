/*******************************************************************************
* File Name        : psa_tfm_se_driver.c
*
* Description      : PSA secure element driver for the CM55 PSA Crypto core.
*                    Keys created with location PSA_TFM_SE_LOCATION are handed
*                    to the TF-M Crypto partition (CM33 Secure) over the
*                    CM55 -> CM33-NS -> TF-M mailbox:
*                     - the private key is generated and kept in TF-M secure
*                       SRAM and stored in TF-M ITS under TF-M ownership,
*                     - signing and public key export run inside TF-M,
*                     - the CM55 PSA core only stores a reference (slot number
*                       = TF-M key ID) in its own key file.
*
*                    Requests are encoded like the TF-M NS client stubs in
*                    tfm_crypto_api.c, which are not linked when the PSA core
*                    runs on CM55.
*
* Related Document : See README.md
*
*******************************************************************************/

#include "psa_tfm_se_driver.h"

#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1) && \
    defined(CM55_TFM_SE_KEY) && (CM55_TFM_SE_KEY == 1)

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <string.h>
#include "psa/crypto.h"
#include "psa/crypto_se_driver.h"
#include "psa/client.h"
#include "psa_manifest/sid.h"
#include "tfm_crypto_defs.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define TFM_IOVEC_LEN(vec)          (sizeof(vec) / sizeof((vec)[0]))

/* Size of the driver persistent data (not used, kept non-zero so that the
 * PSA core stores a small ITS file instead of a zero-length one) */
#define TFM_SE_PERSISTENT_DATA_SIZE (4U)

/*******************************************************************************
* Data Types
*******************************************************************************/
/* Client view of the key attributes expected by TF-M (TF-M copies exactly
 * sizeof(service attributes) - sizeof(owner) bytes). The CM55 struct
 * psa_key_attributes_t can't be sent as is: with MBEDTLS_PSA_CRYPTO_SE_C it
 * starts with slot_number/has_slot_number fields that TF-M doesn't have. */
typedef struct
{
    psa_key_type_t      type;
    uint16_t            bits;
    psa_key_lifetime_t  lifetime;
    psa_key_usage_t     usage;
    psa_algorithm_t     alg;
    psa_algorithm_t     alg2;
    psa_key_id_t        id;
} tfm_client_key_attributes_t;

_Static_assert(sizeof(tfm_client_key_attributes_t) == 24U,
               "TF-M client key attributes must be 24 bytes");

/*******************************************************************************
* Function Name: tfm_attributes_from_psa
********************************************************************************
* Summary:
*  Builds the TF-M attributes for key_slot: same type, size and policy as the
*  CM55 key, persistent in TF-M local storage with ID = key_slot.
*
*******************************************************************************/
static void tfm_attributes_from_psa(const psa_key_attributes_t *attributes,
                                    psa_key_slot_number_t key_slot,
                                    tfm_client_key_attributes_t *tfm_attr)
{
    memset(tfm_attr, 0, sizeof(*tfm_attr));
    tfm_attr->type     = psa_get_key_type(attributes);
    tfm_attr->bits     = (uint16_t)psa_get_key_bits(attributes);
    tfm_attr->lifetime = PSA_KEY_LIFETIME_PERSISTENT;
    tfm_attr->usage    = psa_get_key_usage_flags(attributes);
    tfm_attr->alg      = psa_get_key_algorithm(attributes);
    tfm_attr->alg2     = 0;
    tfm_attr->id       = (psa_key_id_t)key_slot;
}

/*******************************************************************************
* Function Name: tfm_crypto_call
********************************************************************************
* Summary:
*  Sends one request to the TF-M Crypto partition.
*
*******************************************************************************/
static psa_status_t tfm_crypto_call(const psa_invec *in_vec, size_t in_len,
                                    psa_outvec *out_vec, size_t out_len)
{
    return psa_call(TFM_CRYPTO_HANDLE, PSA_IPC_CALL, in_vec, in_len, out_vec, out_len);
}

/*******************************************************************************
* Function Name: tfm_destroy_key
*******************************************************************************/
static psa_status_t tfm_destroy_key(psa_key_id_t key_id)
{
    struct tfm_crypto_pack_iovec iov;

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_DESTROY_KEY_SID;
    iov.key_id = key_id;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
    };

    return tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), NULL, 0);
}

/*******************************************************************************
* Function Name: tfm_export_public_key
*******************************************************************************/
static psa_status_t tfm_export_public_key(psa_key_id_t key_id, uint8_t *data,
                                          size_t data_size, size_t *data_length)
{
    psa_status_t status;
    struct tfm_crypto_pack_iovec iov;

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_EXPORT_PUBLIC_KEY_SID;
    iov.key_id = key_id;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
    };
    psa_outvec out_vec[] = {
        {.base = data, .len = data_size},
    };

    status = tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), out_vec, TFM_IOVEC_LEN(out_vec));
    *data_length = (PSA_SUCCESS == status) ? out_vec[0].len : 0U;
    return status;
}

/*******************************************************************************
* Function Name: se_init
*******************************************************************************/
static psa_status_t se_init(psa_drv_se_context_t *drv_context,
                            void *persistent_data,
                            psa_key_location_t location)
{
    (void)drv_context;
    (void)persistent_data;
    (void)location;
    return PSA_SUCCESS;
}

/*******************************************************************************
* Function Name: se_allocate
********************************************************************************
* Summary:
*  The slot number is the TF-M key ID, taken from the application key ID, so
*  key 0x1 on CM55 refers to key 0x1 in TF-M. Only persistent keys are
*  supported (TF-M assigns volatile key IDs itself).
*
*******************************************************************************/
static psa_status_t se_allocate(psa_drv_se_context_t *drv_context,
                                void *persistent_data,
                                const psa_key_attributes_t *attributes,
                                psa_key_creation_method_t method,
                                psa_key_slot_number_t *key_slot)
{
    (void)drv_context;
    (void)persistent_data;

    if ((PSA_KEY_CREATION_GENERATE != method) && (PSA_KEY_CREATION_IMPORT != method))
    {
        return PSA_ERROR_NOT_SUPPORTED;
    }
    if (PSA_KEY_LIFETIME_IS_VOLATILE(psa_get_key_lifetime(attributes)))
    {
        return PSA_ERROR_NOT_SUPPORTED;
    }

    *key_slot = (psa_key_slot_number_t)psa_get_key_id(attributes);
    return PSA_SUCCESS;
}

/*******************************************************************************
* Function Name: se_generate
********************************************************************************
* Summary:
*  Generates the key inside TF-M. The private key never reaches CM55.
*
*******************************************************************************/
static psa_status_t se_generate(psa_drv_se_context_t *drv_context,
                                psa_key_slot_number_t key_slot,
                                const psa_key_attributes_t *attributes,
                                uint8_t *pubkey, size_t pubkey_size,
                                size_t *pubkey_length)
{
    psa_status_t status;
    psa_key_id_t tfm_key_id = PSA_KEY_ID_NULL;
    tfm_client_key_attributes_t tfm_attr;
    struct tfm_crypto_pack_iovec iov;

    (void)drv_context;

    /* A TF-M key with this ID may be left from an earlier provisioning (for
     * example a CM55_LOCAL_PSA_CRYPTO=0 build): replace it */
    (void)tfm_destroy_key((psa_key_id_t)key_slot);

    tfm_attributes_from_psa(attributes, key_slot, &tfm_attr);

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_GENERATE_KEY_SID;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
        {.base = &tfm_attr, .len = sizeof(tfm_attr)},
    };
    psa_outvec out_vec[] = {
        {.base = &tfm_key_id, .len = sizeof(tfm_key_id)},
    };

    status = tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), out_vec, TFM_IOVEC_LEN(out_vec));
    if (PSA_SUCCESS != status)
    {
        return status;
    }
    if (tfm_key_id != (psa_key_id_t)key_slot)
    {
        (void)tfm_destroy_key(tfm_key_id);
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    /* Optional public key output requested by the core */
    if ((NULL != pubkey) && (0U != pubkey_size) && (NULL != pubkey_length))
    {
        status = tfm_export_public_key(tfm_key_id, pubkey, pubkey_size, pubkey_length);
    }

    return status;
}

/*******************************************************************************
* Function Name: se_import
********************************************************************************
* Summary:
*  Imports key material into TF-M. The caller's buffer still holds the key and
*  must be wiped by the caller.
*
*******************************************************************************/
static psa_status_t se_import(psa_drv_se_context_t *drv_context,
                              psa_key_slot_number_t key_slot,
                              const psa_key_attributes_t *attributes,
                              const uint8_t *data, size_t data_length,
                              size_t *bits)
{
    psa_status_t status;
    psa_key_id_t tfm_key_id = PSA_KEY_ID_NULL;
    tfm_client_key_attributes_t tfm_attr;
    struct tfm_crypto_pack_iovec iov;

    (void)drv_context;

    (void)tfm_destroy_key((psa_key_id_t)key_slot);

    tfm_attributes_from_psa(attributes, key_slot, &tfm_attr);

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_IMPORT_KEY_SID;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
        {.base = &tfm_attr, .len = sizeof(tfm_attr)},
        {.base = data, .len = data_length},
    };
    psa_outvec out_vec[] = {
        {.base = &tfm_key_id, .len = sizeof(tfm_key_id)},
    };

    status = tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), out_vec, TFM_IOVEC_LEN(out_vec));
    if (PSA_SUCCESS != status)
    {
        return status;
    }
    if (tfm_key_id != (psa_key_id_t)key_slot)
    {
        (void)tfm_destroy_key(tfm_key_id);
        return PSA_ERROR_CORRUPTION_DETECTED;
    }

    *bits = psa_get_key_bits(attributes);
    return PSA_SUCCESS;
}

/*******************************************************************************
* Function Name: se_destroy
*******************************************************************************/
static psa_status_t se_destroy(psa_drv_se_context_t *drv_context,
                               void *persistent_data,
                               psa_key_slot_number_t key_slot)
{
    psa_status_t status;

    (void)drv_context;
    (void)persistent_data;

    status = tfm_destroy_key((psa_key_id_t)key_slot);

    /* Already gone in TF-M: the CM55 reference can still be removed */
    return (PSA_ERROR_INVALID_HANDLE == status) ? PSA_SUCCESS : status;
}

/*******************************************************************************
* Function Name: se_export_public
*******************************************************************************/
static psa_status_t se_export_public(psa_drv_se_context_t *drv_context,
                                     psa_key_slot_number_t key_slot,
                                     uint8_t *p_data, size_t data_size,
                                     size_t *p_data_length)
{
    (void)drv_context;
    return tfm_export_public_key((psa_key_id_t)key_slot, p_data, data_size, p_data_length);
}

/*******************************************************************************
* Function Name: se_sign
********************************************************************************
* Summary:
*  Signs a hash inside TF-M (CSR, TLS CertificateVerify).
*
*******************************************************************************/
static psa_status_t se_sign(psa_drv_se_context_t *drv_context,
                            psa_key_slot_number_t key_slot,
                            psa_algorithm_t alg,
                            const uint8_t *p_hash, size_t hash_length,
                            uint8_t *p_signature, size_t signature_size,
                            size_t *p_signature_length)
{
    psa_status_t status;
    struct tfm_crypto_pack_iovec iov;

    (void)drv_context;

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_ASYMMETRIC_SIGN_HASH_SID;
    iov.key_id = (psa_key_id_t)key_slot;
    iov.alg = alg;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
        {.base = p_hash, .len = hash_length},
    };
    psa_outvec out_vec[] = {
        {.base = p_signature, .len = signature_size},
    };

    status = tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), out_vec, TFM_IOVEC_LEN(out_vec));
    *p_signature_length = (PSA_SUCCESS == status) ? out_vec[0].len : 0U;
    return status;
}

/*******************************************************************************
* Function Name: se_verify
*******************************************************************************/
static psa_status_t se_verify(psa_drv_se_context_t *drv_context,
                              psa_key_slot_number_t key_slot,
                              psa_algorithm_t alg,
                              const uint8_t *p_hash, size_t hash_length,
                              const uint8_t *p_signature, size_t signature_length)
{
    struct tfm_crypto_pack_iovec iov;

    (void)drv_context;

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_ASYMMETRIC_VERIFY_HASH_SID;
    iov.key_id = (psa_key_id_t)key_slot;
    iov.alg = alg;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(iov)},
        {.base = p_hash, .len = hash_length},
        {.base = p_signature, .len = signature_length},
    };

    return tfm_crypto_call(in_vec, TFM_IOVEC_LEN(in_vec), NULL, 0);
}

/*******************************************************************************
* Global Variables
*******************************************************************************/
static const psa_drv_se_key_management_t tfm_se_key_management =
{
    .MBEDTLS_PRIVATE(p_allocate)             = se_allocate,
    .MBEDTLS_PRIVATE(p_validate_slot_number) = NULL,
    .MBEDTLS_PRIVATE(p_import)               = se_import,
    .MBEDTLS_PRIVATE(p_generate)             = se_generate,
    .MBEDTLS_PRIVATE(p_destroy)              = se_destroy,
    .MBEDTLS_PRIVATE(p_export)               = NULL,    /* private key never leaves TF-M */
    .MBEDTLS_PRIVATE(p_export_public)        = se_export_public,
};

static const psa_drv_se_asymmetric_t tfm_se_asymmetric =
{
    .MBEDTLS_PRIVATE(p_sign)    = se_sign,
    .MBEDTLS_PRIVATE(p_verify)  = se_verify,
    .MBEDTLS_PRIVATE(p_encrypt) = NULL,
    .MBEDTLS_PRIVATE(p_decrypt) = NULL,
};

static const psa_drv_se_t tfm_se_driver =
{
    .MBEDTLS_PRIVATE(hal_version)          = PSA_DRV_SE_HAL_VERSION,
    .MBEDTLS_PRIVATE(persistent_data_size) = TFM_SE_PERSISTENT_DATA_SIZE,
    .MBEDTLS_PRIVATE(p_init)               = se_init,
    .MBEDTLS_PRIVATE(key_management)       = &tfm_se_key_management,
    .MBEDTLS_PRIVATE(mac)                  = NULL,
    .MBEDTLS_PRIVATE(cipher)               = NULL,
    .MBEDTLS_PRIVATE(aead)                 = NULL,
    .MBEDTLS_PRIVATE(asymmetric)           = &tfm_se_asymmetric,
    .MBEDTLS_PRIVATE(derivation)           = NULL,
};

/*******************************************************************************
* Function Name: psa_tfm_se_register
*******************************************************************************/
psa_status_t psa_tfm_se_register(void)
{
    return psa_register_se_driver(PSA_TFM_SE_LOCATION, &tfm_se_driver);
}

#endif /* CM55_LOCAL_PSA_CRYPTO && CM55_TFM_SE_KEY */

/* [] END OF FILE */
