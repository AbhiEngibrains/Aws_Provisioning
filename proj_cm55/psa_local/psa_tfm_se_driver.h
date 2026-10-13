/*******************************************************************************
* File Name        : psa_tfm_se_driver.h
*
* Description      : PSA secure element driver that keeps keys inside TF-M
*                    (CM33 Secure) while the PSA Crypto core runs on CM55.
*                    Enabled with CM55_TFM_SE_KEY=1 (requires
*                    CM55_LOCAL_PSA_CRYPTO=1).
*
* Related Document : See README.md
*
*******************************************************************************/

#ifndef PSA_TFM_SE_DRIVER_H_
#define PSA_TFM_SE_DRIVER_H_

#include "psa/crypto.h"

#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1) && \
    defined(CM55_TFM_SE_KEY) && (CM55_TFM_SE_KEY == 1)

/*******************************************************************************
* Macros
*******************************************************************************/
/* PSA key location served by this driver (0x000001 = primary secure element).
 * A key whose lifetime has this location is generated, stored and used inside
 * TF-M; the CM55 PSA core only keeps a reference (the TF-M key ID). */
#define PSA_TFM_SE_LOCATION             ((psa_key_location_t)0x000001)

/* Lifetime for a persistent key kept in TF-M */
#define PSA_TFM_SE_LIFETIME_PERSISTENT \
    PSA_KEY_LIFETIME_FROM_PERSISTENCE_AND_LOCATION(PSA_KEY_PERSISTENCE_DEFAULT, \
                                                   PSA_TFM_SE_LOCATION)

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
/* Registers the driver with the CM55 PSA core. Must be called after
 * cy_tls_init() (mbedTLS heap) and before psa_crypto_init(). */
psa_status_t psa_tfm_se_register(void);

#endif /* CM55_LOCAL_PSA_CRYPTO && CM55_TFM_SE_KEY */

#endif /* PSA_TFM_SE_DRIVER_H_ */

/* [] END OF FILE */
