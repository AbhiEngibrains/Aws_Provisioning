/******************************************************************************
* File Name:   device_provisioning.h
*
* Description: This file is the public interface of device_provisioning.c
*
* Related Document: See README.md
*
*
*******************************************************************************
# \copyright
# (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG.  SPDX-License-Identifier: Apache-2.0
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
#     http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*******************************************************************************/

#ifndef DEVICE_PROVISIONING_H_
#define DEVICE_PROVISIONING_H_

#include "cybsp.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "cy_mqtt_api.h"
#include "psa/crypto.h"
#include "psa/protected_storage.h"
#include "mbedtls/psa_util.h"
#include "mbedtls/pk.h"
#include "mbedtls/pem.h"
#include "mbedtls/x509_csr.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Buffer sizes */
#define CSR_PEM_BUF_SIZE        1024
#define MQTT_PAYLOAD_BUF_SIZE   4096

#define FP_CSR_CREATE_TOPIC     "$aws/certificates/create-from-csr/json"
#define FP_CSR_ACCEPTED_TOPIC   "$aws/certificates/create-from-csr/json/accepted"
#define FP_CSR_REJECTED_TOPIC   "$aws/certificates/create-from-csr/json/rejected"
#define FP_REGISTER_TOPIC       "$aws/provisioning-templates/%s/provision/json"
#define FP_REGISTER_ACCEPTED    "$aws/provisioning-templates/%s/provision/json/accepted"
#define FP_REGISTER_REJECTED    "$aws/provisioning-templates/%s/provision/json/rejected"

/* Provisioning template name - must match your AWS Fleet Provisioning template */
#define PROVISIONING_TEMPLATE_NAME   "FleetProvisioningTemplate"

/* Buffer sizes for provisioning response data */
#define DEVICE_CERT_PEM_BUF_SIZE     2048
#define OWNERSHIP_TOKEN_BUF_SIZE     1024
#define CERTIFICATE_ID_BUF_SIZE      128

#define DELAY_MS                     2000
#define CSR_RES_DELAY                30000

/* PSA Protected Storage UID for the device certificate */
#define DEVICE_CERT_PS_UID      ((psa_storage_uid_t)0x100)

/* PSA persistent key ID of the device key pair */
#define DEVICE_KEY_ID           ((psa_key_id_t)0x1)

/* Set PSA_KEYGEN_DEBUG to 1 (DEFINES+=PSA_KEYGEN_DEBUG=1 in the Makefile) to
 * print debug logs of the PSA key generation process. Only key attributes,
 * status codes and the public key are printed; the private key never leaves
 * TF-M. */
#ifndef PSA_KEYGEN_DEBUG
#define PSA_KEYGEN_DEBUG        (0)
#endif

#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1) && \
    defined(CM55_TFM_SE_KEY) && (CM55_TFM_SE_KEY == 1)
#define PSA_ENGINE_NAME         "CM55 PSA core / TF-M SE driver"
#define PSA_ENGINE_PATH         "CM55 PSA core -> SE driver -> IPC -> TF-M Crypto (key in secure SRAM)"
#elif defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1)
#define PSA_ENGINE_NAME         "CM55 PSA core"
#define PSA_ENGINE_PATH         "CM55 mbedTLS PSA core (RNG + ITS via TF-M)"
#else
#define PSA_ENGINE_NAME         "TF-M"
#define PSA_ENGINE_PATH         "CM55 -> IPC -> CM33-NS relay -> TF-M Crypto"
#endif /* CM55_LOCAL_PSA_CRYPTO */

#if PSA_KEYGEN_DEBUG
#define KEYGEN_DEBUG_PRINT(...) printf("[PSA-KEYGEN] " __VA_ARGS__)
#else
#define KEYGEN_DEBUG_PRINT(...) do { } while (0)
#endif /* PSA_KEYGEN_DEBUG */

/*******************************************************************************
* Extern Variables (defined in device_provisioning.c)
********************************************************************************/
extern unsigned char aws_client_key[];
extern const size_t aws_client_key_size;

extern char device_cert_pem[];
extern char ownership_token[];
extern char certificate_id[];

extern SemaphoreHandle_t provisioning_sem;
extern volatile bool provisioning_accepted;
extern volatile bool provisioning_in_progress;
extern volatile bool register_thing_accepted;
extern SemaphoreHandle_t register_thing_sem;
extern psa_key_id_t device_key_id;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
bool handle_provisioning_message(const cy_mqtt_publish_info_t *msg);
cy_rslt_t provision_device_with_aws(cy_mqtt_t mqtt_connection);
psa_status_t import_claim_key(psa_key_id_t *key_handle);
psa_status_t store_device_cert_secure(void);
psa_status_t load_device_cert_secure(void);
psa_status_t delete_device_cert_secure(void);
#if !defined(CM55_LOCAL_PSA_CRYPTO) || (CM55_LOCAL_PSA_CRYPTO == 0)
psa_status_t remove_stale_ns_key_file(void);
#endif

/*******************************************************************************
 * Function Name: clear_device_cert_ram
 ********************************************************************************
 * \brief Securely erases the device certificate from the in-memory buffer.
 *
 * Call this after the TLS handshake is complete and the certificate
 * is no longer needed in RAM. The certificate remains in Protected Storage
 * and can be reloaded with load_device_cert_secure() if needed.
 *
 ******************************************************************************/
void clear_device_cert_ram(void);

#endif /* DEVICE_PROVISIONING_H_ */

/* [] END OF FILE */