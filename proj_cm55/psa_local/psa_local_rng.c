/*******************************************************************************
* File Name        : psa_local_rng.c
*
* Description      : Entropy source for the PSA Crypto core running on CM55.
*                    The CRYPTO block (TRNG) is PPC-secured and owned by TF-M,
*                    so random bytes are requested from the TF-M Crypto
*                    partition over the CM55 -> CM33-NS -> TF-M mailbox.
*
* Related Document : See README.md
*
*******************************************************************************/

#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1)

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdint.h>
#include <string.h>
#include "psa/crypto.h"
#include "psa/client.h"
#include "psa_manifest/sid.h"
#include "tfm_crypto_defs.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Maximum number of random bytes requested from TF-M in one call. Keeps each
 * request well below the TF-M crypto IOVEC buffer size. */
#define TFM_RNG_MAX_CHUNK_SIZE      (256U)

/*******************************************************************************
* Function Name: tfm_generate_random
********************************************************************************
* Summary:
*  Sends a TFM_CRYPTO_GENERATE_RANDOM request to the TF-M Crypto partition.
*  Same encoding as psa_generate_random() in the TF-M NS interface, which is not
*  linked on CM55 in this configuration.
*
* Parameters:
*  output      : Buffer to fill with random bytes
*  output_size : Number of bytes requested
*
* Return:
*  psa_status_t : PSA_SUCCESS or a PSA error code from TF-M
*
*******************************************************************************/
static psa_status_t tfm_generate_random(uint8_t *output, size_t output_size)
{
    struct tfm_crypto_pack_iovec iov;

    memset(&iov, 0, sizeof(iov));
    iov.function_id = TFM_CRYPTO_GENERATE_RANDOM_SID;

    psa_invec in_vec[] = {
        {.base = &iov, .len = sizeof(struct tfm_crypto_pack_iovec)},
    };

    psa_outvec out_vec[] = {
        {.base = output, .len = output_size},
    };

    return psa_call(TFM_CRYPTO_HANDLE, PSA_IPC_CALL,
                    in_vec, sizeof(in_vec) / sizeof(in_vec[0]),
                    out_vec, sizeof(out_vec) / sizeof(out_vec[0]));
}

/*******************************************************************************
* Function Name: mbedtls_psa_external_get_random
********************************************************************************
* Summary:
*  External RNG hook of the mbedTLS PSA core (MBEDTLS_PSA_CRYPTO_EXTERNAL_RNG).
*  Every psa_generate_random() / key generation / ECDSA nonce on CM55 gets its
*  randomness from here.
*
* Parameters:
*  context       : Unused
*  output        : Buffer to fill with random bytes
*  output_size   : Number of bytes requested
*  output_length : Number of bytes written
*
* Return:
*  psa_status_t : PSA_SUCCESS, or PSA_ERROR_INSUFFICIENT_ENTROPY on failure
*
*******************************************************************************/
psa_status_t mbedtls_psa_external_get_random(
    mbedtls_psa_external_random_context_t *context,
    uint8_t *output, size_t output_size, size_t *output_length)
{
    psa_status_t status;
    size_t offset = 0;
    size_t chunk;

    (void)context;
    *output_length = 0;

    while (offset < output_size)
    {
        chunk = output_size - offset;
        if (chunk > TFM_RNG_MAX_CHUNK_SIZE)
        {
            chunk = TFM_RNG_MAX_CHUNK_SIZE;
        }

        status = tfm_generate_random(output + offset, chunk);
        if (PSA_SUCCESS != status)
        {
            /* Never return partially filled or predictable data */
            memset(output, 0, output_size);
            return PSA_ERROR_INSUFFICIENT_ENTROPY;
        }
        offset += chunk;
    }

    *output_length = output_size;
    return PSA_SUCCESS;
}

#endif /* CM55_LOCAL_PSA_CRYPTO */

/* [] END OF FILE */
