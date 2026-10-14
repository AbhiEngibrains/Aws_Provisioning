/******************************************************************************
* File Name:   device_provisioning.c
*
* Description: This file contains  functions for AWS device provisioning.
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
#include "device_provisioning.h"
#include "mqtt_client_config.h"
#include "psa/protected_storage.h"
#include "core_json.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/*******************************************************************************
* Global Variable Definitions
*******************************************************************************/
unsigned char aws_client_key[] = { CLAIM_KEY_DATA };

const size_t aws_client_key_size = sizeof(aws_client_key);

char device_cert_pem[DEVICE_CERT_PEM_BUF_SIZE];
char ownership_token[OWNERSHIP_TOKEN_BUF_SIZE];
char certificate_id[CERTIFICATE_ID_BUF_SIZE];

SemaphoreHandle_t provisioning_sem = NULL;
volatile bool provisioning_accepted = false;
volatile bool provisioning_in_progress = false;
volatile bool register_thing_accepted = false;
SemaphoreHandle_t register_thing_sem = NULL;
psa_key_id_t device_key_id = 0;

/*******************************************************************************
* Function Name: json_extract_string
********************************************************************************
* \brief Extracts a string value for a given key from a JSON payload using
*        the coreJSON library (AWS SDK).
*
* Handles JSON-escaped characters (\n, \", \\, etc.) by copying and
* unescaping the value into the output buffer.
*
* \param[in]  json            Pointer to the JSON string.
* \param[in]  json_len        Length of the JSON string.
* \param[in]  key             The JSON key to search for.
* \param[out] value_buf       Buffer to store the extracted value.
* \param[in]  value_buf_size  Size of the value buffer.
*
* \return Length of the extracted value, or -1 on failure.
*
*******************************************************************************/
static int json_extract_string(const char *json, size_t json_len,
                                const char *key,
                                char *value_buf, size_t value_buf_size)
{
    char *value = NULL;
    size_t value_len = 0;
    JSONStatus_t status;

    status = JSON_Search((char *)json, json_len,
                         key, strlen(key),
                         &value, &value_len);
    if (status != JSONSuccess || value == NULL || value_len == 0) {
        return -1;
    }

    /* coreJSON returns the raw value pointer into the buffer.
     * For strings, it points past the opening quote and value_len
     * excludes the quotes. We need to unescape JSON escape sequences
     * (e.g., \\n -> \n) while copying into value_buf. */
    int pos = 0;
    const char *p = value;
    const char *end = value + value_len;
    while (p < end && pos < (int)value_buf_size - 1) {
        if (*p == '\\' && (p + 1) < end) {
            p++; /* skip backslash */
            switch (*p) {
                case 'n':  value_buf[pos++] = '\n'; break;
                case 't':  value_buf[pos++] = '\t'; break;
                case '"':  value_buf[pos++] = '"';  break;
                case '\\': value_buf[pos++] = '\\'; break;
                case '/':  value_buf[pos++] = '/';  break;
                default:   value_buf[pos++] = *p;   break;
            }
        } else {
            value_buf[pos++] = *p;
        }
        p++;
    }
    value_buf[pos] = '\0';
    return pos;
}

/*******************************************************************************
 * Function Name: handle_provisioning_csr_accepted
 ********************************************************************************
 * \brief Handles the Fleet Provisioning CSR accepted response.
 *
 * Parses the JSON from $aws/certificates/create-from-csr/json/accepted
 * and extracts certificatePem, certificateOwnershipToken, and certificateId.
 *
 * \param[in] payload      Pointer to the MQTT payload.
 * \param[in] payload_len  Length of the payload.
 *
 ******************************************************************************/
static void handle_provisioning_csr_accepted(const char *payload, size_t payload_len)
{
    int ret;


    /* Validate JSON document before extracting fields */
    if (JSON_Validate(payload, payload_len) != JSONSuccess) {
        printf("[Provisioning] ERROR: Invalid JSON in CSR response\n");
        provisioning_accepted = false;
        goto done;
    }

    /* Extract certificatePem */
    ret = json_extract_string(payload, payload_len,
                               "certificatePem",
                               device_cert_pem, sizeof(device_cert_pem));
    if (ret <= 0) {
        printf("[Provisioning] ERROR: Failed to extract certificatePem\n");
        provisioning_accepted = false;
        goto done;
    }

    /* Extract certificateOwnershipToken */
    ret = json_extract_string(payload, payload_len,
                               "certificateOwnershipToken",
                               ownership_token, sizeof(ownership_token));
    if (ret <= 0) {
        printf("[Provisioning] ERROR: Failed to extract certificateOwnershipToken\n");
        provisioning_accepted = false;
        goto done;
    }

    /* Extract certificateId (optional but useful for logging) */
    ret = json_extract_string(payload, payload_len,
                               "certificateId",
                               certificate_id, sizeof(certificate_id));

    provisioning_accepted = true;

done:
    /* Signal the provisioning task that the response has been processed */
    if (provisioning_sem != NULL) {
        xSemaphoreGive(provisioning_sem);
    }
}
/*******************************************************************************
 * Function Name: handle_provisioning_csr_rejected
 ********************************************************************************
 * \brief Handles the Fleet Provisioning CSR rejected response.
 *
 * Logs the rejection and signals the provisioning semaphore.
 *
 * \param[in] payload      Pointer to the MQTT payload.
 * \param[in] payload_len  Length of the payload.
 *
 ******************************************************************************/
static void handle_provisioning_csr_rejected(const char *payload, size_t payload_len)
{
    printf("\n[Provisioning] CSR REJECTED by AWS IoT!\n");
    printf("[Provisioning] Response: %.*s\n",
           (int)(payload_len < 512 ? payload_len : 512), payload);

    provisioning_accepted = false;

    if (provisioning_sem != NULL) {
        xSemaphoreGive(provisioning_sem);
    }
}

/*******************************************************************************
 * Function Name: handle_register_thing_accepted
 ********************************************************************************
 * \brief Handles the Fleet Provisioning RegisterThing accepted response.
 *
 * \param[in] payload      Pointer to the MQTT payload.
 * \param[in] payload_len  Length of the payload.
 *
 ******************************************************************************/
static void handle_register_thing_accepted(const char *payload, size_t payload_len)
{
    register_thing_accepted = true;

    if (register_thing_sem != NULL) {
        xSemaphoreGive(register_thing_sem);
    }
}

/*******************************************************************************
 * Function Name: handle_register_thing_rejected
 ********************************************************************************
 * \brief Handles the Fleet Provisioning RegisterThing rejected response.
 *
 * \param[in] payload      Pointer to the MQTT payload.
 * \param[in] payload_len  Length of the payload.
 *
 ******************************************************************************/
static void handle_register_thing_rejected(const char *payload, size_t payload_len)
{
    printf("\n[Provisioning] RegisterThing REJECTED by AWS IoT!\n");
    printf("[Provisioning] Response: %.*s\n",
           (int)(payload_len < 512 ? payload_len : 512), payload);
    register_thing_accepted = false;

    if (register_thing_sem != NULL) {
        xSemaphoreGive(register_thing_sem);
    }
}

/*******************************************************************************
 * Function Name: handle_provisioning_message
 ********************************************************************************
 * \brief Checks if an incoming MQTT message is a provisioning response and handles it.
 *
 * Called from mqtt_event_callback. Routes the message to the appropriate
 * handler based on the topic.
 *
 * \param[in] msg  Pointer to the received MQTT publish info structure.
 *
 * \return true if the message was a provisioning topic (handled),
 *         false if it should be passed to the normal subscriber callback.
 *
 ******************************************************************************/
bool handle_provisioning_message(const cy_mqtt_publish_info_t *msg)
{
    const char *topic = msg->topic;
    size_t topic_len = msg->topic_len;
    const char *payload = (const char *)msg->payload;
    size_t payload_len = msg->payload_len;

    /* Check for CSR accepted topic */
    if (topic_len == strlen(FP_CSR_ACCEPTED_TOPIC) &&
        memcmp(topic, FP_CSR_ACCEPTED_TOPIC, topic_len) == 0) {
        handle_provisioning_csr_accepted(payload, payload_len);
        return true;
    }

    /* Check for CSR rejected topic */
    if (topic_len == strlen(FP_CSR_REJECTED_TOPIC) &&
        memcmp(topic, FP_CSR_REJECTED_TOPIC, topic_len) == 0) {
        handle_provisioning_csr_rejected(payload, payload_len);
        return true;
    }

    /* Check for RegisterThing accepted/rejected topics */
    {
        char reg_accepted[128], reg_rejected[128];
        snprintf(reg_accepted, sizeof(reg_accepted), FP_REGISTER_ACCEPTED, PROVISIONING_TEMPLATE_NAME);
        snprintf(reg_rejected, sizeof(reg_rejected), FP_REGISTER_REJECTED, PROVISIONING_TEMPLATE_NAME);

        if (topic_len == strlen(reg_accepted) &&
            memcmp(topic, reg_accepted, topic_len) == 0) {
            handle_register_thing_accepted(payload, payload_len);
            return true;
        }
        if (topic_len == strlen(reg_rejected) &&
            memcmp(topic, reg_rejected, topic_len) == 0) {
            handle_register_thing_rejected(payload, payload_len);
            return true;
        }
    }

    return false; /* Not a provisioning message */
}

/*******************************************************************************
 * Function Name: generate_ecc_keypair
 ********************************************************************************
 * \brief Generates an ECC P-256 key pair using PSA Crypto.
 *
 * Creates a persistent key pair suitable for TLS client authentication
 * and CSR signing.
 *
 * \param[out] key_id  Pointer to store the generated PSA key identifier.
 *
 * \return PSA_SUCCESS on success, or a PSA error code on failure.
 *
 ******************************************************************************/
static psa_status_t generate_ecc_keypair(psa_key_id_t *key_id)
{
    psa_status_t status;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    /* Configure key attributes */
    psa_set_key_type(&attributes,
        PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&attributes, 256);
    psa_set_key_usage_flags(&attributes,
        PSA_KEY_USAGE_SIGN_HASH |
        PSA_KEY_USAGE_SIGN_MESSAGE |PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&attributes,
        PSA_ALG_ECDSA(PSA_ALG_ANY_HASH));

    /*  make key persistent so it survives reboots */
     psa_set_key_id(&attributes, 0x1);
     psa_set_key_lifetime(&attributes, PSA_KEY_LIFETIME_PERSISTENT);

    /* Destroy any previously stored key with the same ID to avoid PSA_ERROR_ALREADY_EXISTS */
    psa_destroy_key(0x1);

    /* Generate the key pair */
    status = psa_generate_key(&attributes, key_id);
    if (status != PSA_SUCCESS) {
        printf("Key generation failed: %ld\n", (long)status);
        return status;
    }

    return PSA_SUCCESS;
}
/*******************************************************************************
 * Function Name: setup_pk_from_psa_key
 ********************************************************************************
 * \brief Sets up an mbedtls PK context from a PSA key.
 *
 * Configures the PK context in opaque mode so that mbedtls can use
 * the PSA-managed key for CSR generation.
 *
 * \param[out] pk      Pointer to the mbedtls PK context to initialize.
 * \param[in]  key_id  PSA key identifier of the key to use.
 *
 * \return 0 on success, or a negative mbedtls error code on failure.
 *
 ******************************************************************************/
static int setup_pk_from_psa_key(mbedtls_pk_context *pk, psa_key_id_t key_id)
{
    int ret;

    mbedtls_pk_init(pk);
    ret = mbedtls_pk_setup_opaque(pk, key_id);
    if (ret != 0) {
        printf("PK opaque setup failed: -0x%04x\n", -ret);
    }
    
    return ret;
}

/*******************************************************************************
 * Function Name: generate_csr
 ********************************************************************************
 * \brief Generates a PEM-encoded Certificate Signing Request (CSR).
 *
 * Creates a CSR using the specified PSA key pair with subject name
 * "CN=PSE84-Device,O=MyOrg,C=US" and SHA-256 signature algorithm.
 *
 * \param[in]  key_id       PSA key identifier for signing the CSR.
 * \param[out] csr_pem_buf  Buffer to store the PEM-encoded CSR.
 * \param[in]  buf_size     Size of the CSR buffer.
 *
 * \return 0 on success, or a negative mbedtls error code on failure.
 *
 ******************************************************************************/
static int generate_csr(psa_key_id_t key_id, char *csr_pem_buf, size_t buf_size)
{
    int ret;
    mbedtls_x509write_csr csr;
    mbedtls_pk_context pk;

    mbedtls_x509write_csr_init(&csr);

    /* Setup PK context from PSA key */
    ret = setup_pk_from_psa_key(&pk, key_id);
    if (ret != 0) {
        printf("Failed to setup PK from PSA key: -0x%04x\n", -ret);
        goto cleanup;
    }

    /* Set CSR subject name */
    ret = mbedtls_x509write_csr_set_subject_name(&csr,
        "CN=PSE84-Device,O=MyOrg,C=US");
    if (ret != 0) {
        printf("Failed to set CSR subject: -0x%04x\n", -ret);
        goto cleanup;
    }

    /* Set the key and signature algorithm */
    mbedtls_x509write_csr_set_key(&csr, &pk);
    mbedtls_x509write_csr_set_md_alg(&csr, MBEDTLS_MD_SHA256);

    /* Write CSR in PEM format */
    ret = mbedtls_x509write_csr_pem(&csr,
        (unsigned char *)csr_pem_buf, buf_size,
        mbedtls_psa_get_random, MBEDTLS_PSA_RANDOM_STATE);
    if (ret != 0) {
        printf("Failed to write CSR PEM: -0x%04x\n", -ret);
        goto cleanup;
    }


cleanup:
    mbedtls_x509write_csr_free(&csr);
    mbedtls_pk_free(&pk);
    return ret;
}


/*******************************************************************************
 * Function Name: build_csr_request_payload
 ********************************************************************************
 * \brief Builds the JSON payload for the CreateCertificateFromCSR request.
 *
 * Escapes newlines in the PEM-encoded CSR for JSON encoding.
 *
 * \param[in]  csr_pem      PEM-encoded CSR string.
 * \param[out] payload_buf  Buffer to store the JSON payload.
 * \param[in]  buf_size     Size of the payload buffer.
 *
 * \return Length of the generated payload.
 *
 ******************************************************************************/
static int build_csr_request_payload(const char *csr_pem,
                                      char *payload_buf,
                                      size_t buf_size)
{
    /* Escape newlines in PEM for JSON */
    int pos = 0;
    pos += snprintf(payload_buf + pos, buf_size - pos,
        "{\"certificateSigningRequest\":\"");

    const char *p = csr_pem;
    while (*p && pos < (int)buf_size - 10) {
        if (*p == '\n') {
            payload_buf[pos++] = '\\';
            payload_buf[pos++] = 'n';
        } else if (*p == '\r') {
            /* skip CR */
        } else {
            payload_buf[pos++] = *p;
        }
        p++;
    }
    pos += snprintf(payload_buf + pos, buf_size - pos, "\"}");
    return pos;
}
/*******************************************************************************
 * Function Name: build_register_thing_payload
 ********************************************************************************
 * \brief Builds the JSON payload for the RegisterThing request.
 *
 * Constructs a JSON object containing the certificate ownership token
 * and device parameters (ThingName).
 *
 * \param[in]  certificate_ownership_token  The ownership token from CSR response.
 * \param[out] payload_buf                  Buffer to store the JSON payload.
 * \param[in]  buf_size                     Size of the payload buffer.
 *
 * \return Length of the generated payload.
 *
 ******************************************************************************/
static int build_register_thing_payload(const char *certificate_ownership_token,
                                         char *payload_buf,
                                         size_t buf_size)
{
    return snprintf(payload_buf, buf_size,
        "{\"certificateOwnershipToken\":\"%s\","
        "\"parameters\":{\"ThingName\":\"PSE84-Device\",\"SerialNumber\":\"PSE84-0001\"}}",
        certificate_ownership_token);
}


/*******************************************************************************
 * Function Name: import_claim_key
 ********************************************************************************
 * \brief Imports the claim certificate private key into PSA Crypto.
 *
 * Imports the ECC P-256 claim key (aws_client_key) as a volatile PSA key
 * for use in TLS client authentication during fleet provisioning.
 *
 * \param[out] key_handle  Pointer to store the imported PSA key handle.
 *
 * \return PSA_SUCCESS on success, or a PSA error code on failure.
 *
 ******************************************************************************/
psa_status_t import_claim_key(psa_key_id_t *key_handle)
{
    psa_status_t status;
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;

    /* Set attributes for ECC P-256 key */
    psa_set_key_type(&key_attributes,
        PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&key_attributes, 256);

    /* Set both SIGN_HASH and SIGN_MESSAGE for mbedtls 3.x */
    psa_set_key_usage_flags(&key_attributes,
        PSA_KEY_USAGE_SIGN_HASH | PSA_KEY_USAGE_SIGN_MESSAGE);

    /* Use ECDSA for ECC keys.
     * PSA_ALG_ANY_HASH allows signing with any hash (SHA-256, SHA-384, etc.)
     * This is required because the TLS stack negotiates the hash algorithm. */
    psa_set_key_algorithm(&key_attributes,
        PSA_ALG_ECDSA(PSA_ALG_ANY_HASH));

    /* Import the DER-encoded private key */
    status = psa_import_key(&key_attributes,
        aws_client_key,
        aws_client_key_size,
        key_handle);

    if (status != PSA_SUCCESS) {
        printf("ECC key import failed: %ld\n", (long)status);
        return status;
    }


    /* Securely erase the raw key material from RAM now that it's in PSA Crypto */
    memset(aws_client_key, 0, aws_client_key_size);

    return PSA_SUCCESS;
}

/*******************************************************************************
 * Function Name: store_device_cert_secure
 ********************************************************************************
 * \brief Stores the device certificate in PSA Protected Storage.
 *
 * Writes the PEM-encoded device certificate to TF-M Protected Storage
 * so it persists across reboots 
 *
 * \return PSA_SUCCESS on success, or a PSA error code on failure.
 *
 ******************************************************************************/
psa_status_t store_device_cert_secure(void)
{
    size_t cert_len = strlen(device_cert_pem) + 1; /* include null terminator */

    psa_status_t status = psa_ps_set(DEVICE_CERT_PS_UID,
                                     cert_len,
                                     (const void *)device_cert_pem,
                                     PSA_STORAGE_FLAG_NONE);
    if (status != PSA_SUCCESS) {
        printf("[Provisioning] ERROR: Failed to store cert in PS: %ld\n", (long)status);
    }
    return status;
}

/*******************************************************************************
 * Function Name: load_device_cert_secure
 ********************************************************************************
 * \brief Loads the device certificate from PSA Protected Storage.
 *
 * Reads the PEM-encoded device certificate from TF-M Protected Storage
 * back into the device_cert_pem buffer.
 *
 * \return PSA_SUCCESS on success, or a PSA error code on failure.
 *
 ******************************************************************************/
psa_status_t load_device_cert_secure(void)
{
    size_t data_length = 0;

    psa_status_t status = psa_ps_get(DEVICE_CERT_PS_UID,
                                     0,
                                     DEVICE_CERT_PEM_BUF_SIZE,
                                     (void *)device_cert_pem,
                                     &data_length);
    if (status != PSA_SUCCESS) {
        printf("[Provisioning] No device cert in secure storage (status=%ld)\n", (long)status);
    } 
    return status;
}

/*******************************************************************************
 * Function Name: delete_device_cert_secure
 ********************************************************************************
 * \brief Deletes the device certificate from PSA Protected Storage.
 *
 * Removes the stored device certificate so that the next boot will
 * trigger a fresh fleet provisioning flow.
 *
 * \return PSA_SUCCESS on success, or a PSA error code on failure.
 *
 ******************************************************************************/
psa_status_t delete_device_cert_secure(void)
{
    psa_status_t status = psa_ps_remove(DEVICE_CERT_PS_UID);
    if (status != PSA_SUCCESS) {
        printf("[Provisioning] Failed to delete cert from PS (status=%ld)\n", (long)status);
    } 
    return status;
}

/*******************************************************************************
 * Function Name: clear_device_cert_ram
 ******************************************************************************/
void clear_device_cert_ram(void)
{
    memset(device_cert_pem, 0, DEVICE_CERT_PEM_BUF_SIZE);
}

/*******************************************************************************
 * Function Name: provision_device_with_aws
 ********************************************************************************
 * \brief Main provisioning function for AWS IoT Fleet Provisioning.
 *
 * Performs the complete device provisioning flow:
 * 1. Generates an ECC P-256 key pair
 * 2. Creates a CSR
 * 3. Publishes the CSR to AWS IoT
 * 4. Waits for the certificate response
 * 5. Registers the device (Thing) with AWS IoT
 *
 * Call this after connecting to AWS IoT with a claim certificate.
 *
 * \param[in] mqtt_connection  Active cy_mqtt_t handle.
 *
 * \return CY_RSLT_SUCCESS on success, or an error code on failure.
 *
 ******************************************************************************/
cy_rslt_t provision_device_with_aws(cy_mqtt_t mqtt_connection)
{
    cy_rslt_t result=CY_RSLT_SUCCESS;
    psa_status_t psa_status;
    int ret;
    int payload_len = 0;
    cy_mqtt_publish_info_t pub_info;

    /* Allocate buffers */
    char *csr_pem = pvPortMalloc(CSR_PEM_BUF_SIZE);
    char *mqtt_payload = pvPortMalloc(MQTT_PAYLOAD_BUF_SIZE);


    if (!csr_pem || !mqtt_payload) {
        printf("Buffer allocation failed\n");
        if (csr_pem) vPortFree(csr_pem);
        if (mqtt_payload) vPortFree(mqtt_payload);
        return CY_RSLT_TYPE_ERROR;
    }

    /* Create semaphore for waiting on async MQTT response */
    if (provisioning_sem == NULL) {
        provisioning_sem = xSemaphoreCreateBinary();
    }
    if (register_thing_sem == NULL) {
        register_thing_sem = xSemaphoreCreateBinary();
    }

    provisioning_accepted = false;
    register_thing_accepted = false;
    provisioning_in_progress = true;
    device_cert_pem[0] = '\0';
    ownership_token[0] = '\0';
    certificate_id[0] = '\0';

    /* --- Step 1: Generate ECC key pair --- */
    printf("\n[Provisioning] Generating ECC P-256 key pair...\n");
    psa_status = generate_ecc_keypair(&device_key_id);
    if (psa_status != PSA_SUCCESS) {
        printf("Key generation failed\n");
        goto cleanup;
    }

    /* --- Step 2: Generate CSR --- */
    printf("[Provisioning] Generating CSR...\n");
    ret = generate_csr(device_key_id, csr_pem, CSR_PEM_BUF_SIZE);
    if (ret != 0) {
        printf("CSR generation failed\n");
        goto cleanup;
    }

    /* --- Step 3: Subscribe to response topics --- */
    cy_mqtt_subscribe_info_t sub_info[2];

    sub_info[0].qos = CY_MQTT_QOS1;
    sub_info[0].topic = FP_CSR_ACCEPTED_TOPIC;
    sub_info[0].topic_len = strlen(FP_CSR_ACCEPTED_TOPIC);

    sub_info[1].qos = CY_MQTT_QOS1;
    sub_info[1].topic = FP_CSR_REJECTED_TOPIC;
    sub_info[1].topic_len = strlen(FP_CSR_REJECTED_TOPIC);

    result = cy_mqtt_subscribe(mqtt_connection, &sub_info[0], 1);
    if (result != CY_RSLT_SUCCESS) {
        printf("Subscribe to accepted topic failed: 0x%lx\n", (long)result);
        goto cleanup;
    }


    result = cy_mqtt_subscribe(mqtt_connection, &sub_info[1], 1);
    if (result != CY_RSLT_SUCCESS) {
        printf("Subscribe to rejected topic failed: 0x%lx\n", (long)result);
        goto cleanup;
    }


    /* Wait for subscriptions to be fully active on the broker */
    vTaskDelay(pdMS_TO_TICKS(DELAY_MS));

    /* --- Step 4: Publish CSR to create certificate --- */
    printf("[Provisioning] Publishing CSR to AWS IoT...\n");
    payload_len = build_csr_request_payload(csr_pem, mqtt_payload,
                                                 MQTT_PAYLOAD_BUF_SIZE);

    memset(&pub_info, 0, sizeof(pub_info));
    pub_info.qos = CY_MQTT_QOS0;
    pub_info.topic = FP_CSR_CREATE_TOPIC;
    pub_info.topic_len = strlen(FP_CSR_CREATE_TOPIC);
    pub_info.payload = mqtt_payload;
    pub_info.payload_len = payload_len;

    result = cy_mqtt_publish(mqtt_connection, &pub_info);
    if (result != CY_RSLT_SUCCESS) {
        printf("Publish CSR failed: 0x%lx\n", (long)result);
        goto cleanup;
    }


    /* --- Step 5: Wait for async response via callback --- */
    /* The response arrives in mqtt_event_callback -> handle_provisioning_message
     * which parses the JSON and signals the semaphore when done.
     * Note: AWS IoT may disconnect after sending the response. The cy_mqtt
     * library may fire DISCONNECT before delivering the subscription message.
     * The disconnect handler also signals the semaphore as a fallback. */
    if (xSemaphoreTake(provisioning_sem, pdMS_TO_TICKS(CSR_RES_DELAY)) != pdTRUE) {
        printf("[Provisioning] ERROR: Timed out waiting for CSR response (30s)\n");
        result = CY_RSLT_TYPE_ERROR;
        goto cleanup;
    }

    if (!provisioning_accepted) {
        printf("[Provisioning] CSR response not received via callback.\n");
        printf("[Provisioning] This may be because AWS IoT disconnected before\n");
        printf("[Provisioning] the MQTT library could deliver the subscription message.\n");
        printf("[Provisioning] Please check your AWS IoT claim certificate policy allows:\n");
        printf("[Provisioning]   iot:Connect, iot:Subscribe, iot:Publish, iot:Receive\n");
        printf("[Provisioning] on the Fleet Provisioning topics.\n");
        result = CY_RSLT_TYPE_ERROR;
        goto cleanup;
    }


    /* --- Step 6: Subscribe to RegisterThing response topics --- */
    {
        char reg_accepted[128], reg_rejected[128];
        snprintf(reg_accepted, sizeof(reg_accepted), FP_REGISTER_ACCEPTED, PROVISIONING_TEMPLATE_NAME);
        snprintf(reg_rejected, sizeof(reg_rejected), FP_REGISTER_REJECTED, PROVISIONING_TEMPLATE_NAME);

        cy_mqtt_subscribe_info_t reg_sub[2];
        reg_sub[0].qos = CY_MQTT_QOS1;
        reg_sub[0].topic = reg_accepted;
        reg_sub[0].topic_len = strlen(reg_accepted);
        reg_sub[1].qos = CY_MQTT_QOS1;
        reg_sub[1].topic = reg_rejected;
        reg_sub[1].topic_len = strlen(reg_rejected);

        result = cy_mqtt_subscribe(mqtt_connection, reg_sub, 2);
        if (result != CY_RSLT_SUCCESS) {
            printf("[Provisioning] RegisterThing subscribe failed: 0x%lx\n", (long)result);
            goto cleanup;
        }
    }

    /* --- Step 7: Publish RegisterThing request with ownership token --- */
    {
        char reg_topic[128];
        snprintf(reg_topic, sizeof(reg_topic), FP_REGISTER_TOPIC, PROVISIONING_TEMPLATE_NAME);

        payload_len = build_register_thing_payload(ownership_token, mqtt_payload,
                                                    MQTT_PAYLOAD_BUF_SIZE);

        cy_mqtt_publish_info_t reg_pub = {
            .qos = CY_MQTT_QOS1,
            .topic = reg_topic,
            .topic_len = strlen(reg_topic),
            .payload = mqtt_payload,
            .payload_len = payload_len
        };

        result = cy_mqtt_publish(mqtt_connection, &reg_pub);
        if (result != CY_RSLT_SUCCESS) {
            printf("[Provisioning] RegisterThing publish failed: 0x%lx\n", (long)result);
            goto cleanup;
        }

    }

    /* --- Step 8: Wait for RegisterThing response --- */
    if (xSemaphoreTake(register_thing_sem, pdMS_TO_TICKS(CSR_RES_DELAY)) != pdTRUE) {
        printf("[Provisioning] ERROR: Timed out waiting for RegisterThing response (30s)\n");
        result = CY_RSLT_TYPE_ERROR;
        goto cleanup;
    }

    if (!register_thing_accepted) {
        printf("[Provisioning] ERROR: RegisterThing was rejected by AWS IoT.\n");
        printf("[Provisioning] Ensure the device IoT policy (referenced in the provisioning template) exists.\n");
        result = CY_RSLT_TYPE_ERROR;
        goto cleanup;
    }

    printf("[Provisioning] Device certificate is stored in PS\n");

    /* --- Step 9: Store device certificate in secure storage --- */
    {
        psa_status_t store_status = store_device_cert_secure();
        if (store_status != PSA_SUCCESS) {
            printf("[Provisioning] WARNING: Device certificate store failed (provisioning may succeed, but cert will not persist across reboots)\n");
        }
    }

    result = CY_RSLT_SUCCESS;

cleanup:
    provisioning_in_progress = false;

    /* Securely erase temporary sensitive buffers before freeing */
    if (csr_pem) {
        memset(csr_pem, 0, CSR_PEM_BUF_SIZE);
        vPortFree(csr_pem);
    }
    if (mqtt_payload) {
        memset(mqtt_payload, 0, MQTT_PAYLOAD_BUF_SIZE);
        vPortFree(mqtt_payload);
    }

    /* Zero out single-use provisioning secrets that are no longer needed */
    memset(ownership_token, 0, sizeof(ownership_token));
    memset(certificate_id, 0, sizeof(certificate_id));

    return result;
}



/* [] END OF FILE */
