/*******************************************************************************
* File Name        : key_security_check.c
*
* Description      : Debug self-test that validates the security properties of
*                    the device PSA key:
*                     1. Attributes  : P-256 key pair, persistent, no EXPORT/COPY
*                     2. Export      : private key export is refused
*                    2b. ITS read    : key file can't be read with psa_its_get()
*                     3. Public key  : 65-byte uncompressed point on the curve
*                     4. RNG         : random output is not constant/repeating
*                     5. Sign/verify : signature verifies with the public key
*                     6. Tamper      : modified hash is rejected
*                     7. Nonce       : two signatures of one hash differ
*                     8. Persistence : key reloads from storage unchanged
*                     9. Uniqueness  : new key differs from the replaced key
*                    and reports where the private key is used (isolation).
*
* Related Document : See README.md
*
*******************************************************************************/

#include "key_security_check.h"

#if PSA_KEY_SECURITY_CHECK

/*******************************************************************************
* Header Files
*******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "cybsp.h"
#include "psa/internal_trusted_storage.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define KEY_CHECK_TYPE          PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_SECP_R1)
#define KEY_CHECK_BITS          (256U)
#define KEY_CHECK_SIGN_ALG      PSA_ALG_ECDSA(PSA_ALG_SHA_256)
#define KEY_CHECK_PUB_SIZE      PSA_EXPORT_PUBLIC_KEY_OUTPUT_SIZE(KEY_CHECK_TYPE, KEY_CHECK_BITS)
#define KEY_CHECK_PRIV_SIZE     PSA_EXPORT_KEY_OUTPUT_SIZE(KEY_CHECK_TYPE, KEY_CHECK_BITS)
#define KEY_CHECK_SIG_SIZE      PSA_ECDSA_SIGNATURE_SIZE(KEY_CHECK_BITS)
#define KEY_CHECK_HASH_SIZE     PSA_HASH_LENGTH(PSA_ALG_SHA_256)

/* mbedTLS persistent key file: magic header "PSA\0KEY\0" followed by the
 * attributes and the raw key material (psa_crypto_storage.c). */
#define KEY_CHECK_FILE_MAGIC        "PSA\0KEY"
#define KEY_CHECK_FILE_MAGIC_LEN    (sizeof(KEY_CHECK_FILE_MAGIC))
#define KEY_CHECK_ITS_BUF_SIZE      (128U)
/* Key file layout: magic[8] version[4] lifetime[4] type[2] bits[2]
 * policy[12] data_len[4] key_data[] */
#define KEY_CHECK_FILE_LIFETIME_OFS (12U)
#define KEY_CHECK_FILE_DATALEN_OFS  (32U)
#define KEY_CHECK_FILE_HEADER_SIZE  (36U)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Buffers passed to PSA calls are placed in SOCMEM (as in device_provisioning.c)
 * so that TF-M can access them when the PSA engine runs on CM33. */
typedef struct
{
    psa_key_attributes_t attributes;
    uint8_t pub_key[KEY_CHECK_PUB_SIZE];
    uint8_t pub_key_reloaded[KEY_CHECK_PUB_SIZE];
    uint8_t prev_pub_key[KEY_CHECK_PUB_SIZE];
    uint8_t priv_key[KEY_CHECK_PRIV_SIZE];
    uint8_t hash[KEY_CHECK_HASH_SIZE];
    uint8_t rnd_a[32];
    uint8_t rnd_b[32];
    uint8_t sig_a[KEY_CHECK_SIG_SIZE];
    uint8_t sig_b[KEY_CHECK_SIG_SIZE];
    uint8_t its_blob[KEY_CHECK_ITS_BUF_SIZE];
    struct psa_storage_info_t its_info;
    size_t its_len;
    size_t pub_len;
    size_t pub_reloaded_len;
    size_t prev_pub_len;
    size_t out_len;
    size_t sig_a_len;
    size_t sig_b_len;
    psa_key_id_t verify_key_id;
} key_check_buffers_t;

CY_SECTION(".cy_socmem_data") static key_check_buffers_t kc;

static uint32_t checks_passed;
static uint32_t checks_failed;

/*******************************************************************************
* Function Name: key_check_result
********************************************************************************
* Summary:
*  Prints and counts the result of one check.
*
*******************************************************************************/
static void key_check_result(bool passed, const char *name, const char *detail)
{
    if (passed)
    {
        checks_passed++;
    }
    else
    {
        checks_failed++;
    }
    KEY_CHECK_PRINT("[%s] %-12s %s\n", passed ? "PASS" : "FAIL", name, detail);
}

/*******************************************************************************
* Function Name: key_check_print_hex
*******************************************************************************/
static void key_check_print_hex(const char *label, const uint8_t *data, size_t len)
{
    KEY_CHECK_PRINT("%s (%u bytes):", label, (unsigned int)len);
    for (size_t i = 0; i < len; i++)
    {
        printf("%s%02x", (i % 16 == 0) ? "\n[KEY-CHECK]     " : "", data[i]);
    }
    printf("\n");
}

/*******************************************************************************
* Function Name: key_check_is_constant
********************************************************************************
* Summary:
*  Returns true if all bytes of the buffer have the same value.
*
*******************************************************************************/
static bool key_check_is_constant(const uint8_t *data, size_t len)
{
    for (size_t i = 1; i < len; i++)
    {
        if (data[i] != data[0])
        {
            return false;
        }
    }
    return true;
}

/*******************************************************************************
* Function Name: check_attributes
*******************************************************************************/
static void check_attributes(psa_key_id_t key_id)
{
    char detail[96];
    psa_status_t status;

    kc.attributes = psa_key_attributes_init();
    status = psa_get_key_attributes(key_id, &kc.attributes);
    if (PSA_SUCCESS != status)
    {
        snprintf(detail, sizeof(detail), "psa_get_key_attributes -> %ld", (long)status);
        key_check_result(false, "Attributes", detail);
        return;
    }

    psa_key_type_t type = psa_get_key_type(&kc.attributes);
    size_t bits = psa_get_key_bits(&kc.attributes);
    psa_key_lifetime_t lifetime = psa_get_key_lifetime(&kc.attributes);
    psa_key_usage_t usage = psa_get_key_usage_flags(&kc.attributes);

    snprintf(detail, sizeof(detail), "type 0x%04x (P-256 key pair), bits %u",
             (unsigned int)type, (unsigned int)bits);
    key_check_result((KEY_CHECK_TYPE == type) && (KEY_CHECK_BITS == bits),
                     "Key type", detail);

    snprintf(detail, sizeof(detail), "lifetime 0x%08lx (persistent)", (unsigned long)lifetime);
    key_check_result(!PSA_KEY_LIFETIME_IS_VOLATILE(lifetime), "Persistent", detail);

    snprintf(detail, sizeof(detail), "usage 0x%08lx: EXPORT %s, COPY %s, SIGN_HASH %s",
             (unsigned long)usage,
             (usage & PSA_KEY_USAGE_EXPORT) ? "SET" : "clear",
             (usage & PSA_KEY_USAGE_COPY) ? "SET" : "clear",
             (usage & PSA_KEY_USAGE_SIGN_HASH) ? "set" : "CLEAR");
    key_check_result((0U == (usage & (PSA_KEY_USAGE_EXPORT | PSA_KEY_USAGE_COPY))) &&
                     (0U != (usage & PSA_KEY_USAGE_SIGN_HASH)),
                     "Usage flags", detail);

    psa_reset_key_attributes(&kc.attributes);
}

/*******************************************************************************
* Function Name: check_export_refused
*******************************************************************************/
static void check_export_refused(psa_key_id_t key_id)
{
    char detail[96];
    psa_status_t status;

    kc.out_len = 0;
    status = psa_export_key(key_id, kc.priv_key, sizeof(kc.priv_key), &kc.out_len);
    snprintf(detail, sizeof(detail), "psa_export_key -> %ld (expect -133 NOT_PERMITTED)",
             (long)status);
    key_check_result(PSA_ERROR_NOT_PERMITTED == status, "No export", detail);

    /* Never keep private key bytes around, even if the export was allowed */
    memset(kc.priv_key, 0, sizeof(kc.priv_key));
    kc.out_len = 0;
}

/*******************************************************************************
* Function Name: check_public_key
********************************************************************************
* Summary:
*  Exports the public key and imports it as a separate public key. The import
*  fails if the point is not on the P-256 curve. The imported key is kept in
*  kc.verify_key_id for the signature checks.
*
*******************************************************************************/
static bool check_public_key(psa_key_id_t key_id)
{
    char detail[96];
    psa_status_t status;
    psa_key_attributes_t pub_attr = PSA_KEY_ATTRIBUTES_INIT;

    kc.pub_len = 0;
    status = psa_export_public_key(key_id, kc.pub_key, sizeof(kc.pub_key), &kc.pub_len);
    if (PSA_SUCCESS != status)
    {
        snprintf(detail, sizeof(detail), "psa_export_public_key -> %ld", (long)status);
        key_check_result(false, "Public key", detail);
        return false;
    }
    key_check_print_hex("Public key", kc.pub_key, kc.pub_len);

    psa_set_key_type(&pub_attr, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&pub_attr, KEY_CHECK_BITS);
    psa_set_key_usage_flags(&pub_attr, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&pub_attr, KEY_CHECK_SIGN_ALG);

    kc.verify_key_id = PSA_KEY_ID_NULL;
    status = psa_import_key(&pub_attr, kc.pub_key, kc.pub_len, &kc.verify_key_id);

    snprintf(detail, sizeof(detail), "%u bytes, prefix 0x%02x, on-curve import -> %ld",
             (unsigned int)kc.pub_len, kc.pub_key[0], (long)status);
    key_check_result((KEY_CHECK_PUB_SIZE == kc.pub_len) && (0x04U == kc.pub_key[0]) &&
                     (PSA_SUCCESS == status), "Public key", detail);

    return (PSA_SUCCESS == status);
}

/*******************************************************************************
* Function Name: check_its_direct_read
********************************************************************************
* Summary:
*  Tries to read the stored key file straight from ITS with psa_its_get(),
*  bypassing the PSA Crypto API (and its EXPORT policy). ITS isolates entries
*  by owner:
*   - PSA core in TF-M : the key file belongs to the TF-M Crypto partition, so
*                        a non-secure psa_its_get() finds nothing -> PASS.
*   - PSA core on CM55 : the key file belongs to the non-secure client, so any
*                        non-secure code can read it, private key included
*                        -> FAIL.
*  The blob is never printed and is wiped after the check.
*
*******************************************************************************/
/* Name of a PSA status code returned by the ITS API, for the debug log */
static const char *its_status_name(psa_status_t status)
{
    switch (status)
    {
        case PSA_SUCCESS:                   return "PSA_SUCCESS";
        case PSA_ERROR_DOES_NOT_EXIST:      return "PSA_ERROR_DOES_NOT_EXIST";
        case PSA_ERROR_NOT_PERMITTED:       return "PSA_ERROR_NOT_PERMITTED";
        case PSA_ERROR_INVALID_ARGUMENT:    return "PSA_ERROR_INVALID_ARGUMENT";
        case PSA_ERROR_BUFFER_TOO_SMALL:    return "PSA_ERROR_BUFFER_TOO_SMALL";
        case PSA_ERROR_STORAGE_FAILURE:     return "PSA_ERROR_STORAGE_FAILURE";
        case PSA_ERROR_DATA_CORRUPT:        return "PSA_ERROR_DATA_CORRUPT";
        case PSA_ERROR_DATA_INVALID:        return "PSA_ERROR_DATA_INVALID";
        case PSA_ERROR_COMMUNICATION_FAILURE: return "PSA_ERROR_COMMUNICATION_FAILURE";
        case PSA_ERROR_GENERIC_ERROR:       return "PSA_ERROR_GENERIC_ERROR";
        default:                            return "unknown";
    }
}

static void check_its_direct_read(psa_key_id_t key_id)
{
    char detail[112];
    psa_status_t info_status;
    psa_status_t status;
    psa_storage_uid_t uid = (psa_storage_uid_t)key_id;
    size_t read_size = sizeof(kc.its_blob);

    /* Size of the entry, if it exists, to request at most that many bytes */
    memset(&kc.its_info, 0, sizeof(kc.its_info));
    info_status = psa_its_get_info(uid, &kc.its_info);
    KEY_CHECK_PRINT("       psa_its_get_info(uid %lu) -> %ld (%s), size %u\n",
                    (unsigned long)uid, (long)info_status, its_status_name(info_status),
                    (unsigned int)kc.its_info.size);
    if ((PSA_SUCCESS == info_status) && (kc.its_info.size < read_size))
    {
        read_size = kc.its_info.size;
    }

    /* Direct read of the key file, bypassing the PSA Crypto API */
    kc.its_len = 0;
    status = psa_its_get(uid, 0, read_size, kc.its_blob, &kc.its_len);
    KEY_CHECK_PRINT("       psa_its_get(uid %lu, offset 0, size %u) -> %ld (%s), %u bytes read\n",
                    (unsigned long)uid, (unsigned int)read_size, (long)status,
                    its_status_name(status), (unsigned int)kc.its_len);

    bool is_key_file = (PSA_SUCCESS == status) &&
                       (kc.its_len >= KEY_CHECK_FILE_HEADER_SIZE) &&
                       (0 == memcmp(kc.its_blob, KEY_CHECK_FILE_MAGIC, KEY_CHECK_FILE_MAGIC_LEN));
    bool holds_private_key = is_key_file;

    if (is_key_file)
    {
        uint32_t lifetime;
        uint32_t data_len;

        memcpy(&lifetime, &kc.its_blob[KEY_CHECK_FILE_LIFETIME_OFS], sizeof(lifetime));
        memcpy(&data_len, &kc.its_blob[KEY_CHECK_FILE_DATALEN_OFS], sizeof(data_len));
        KEY_CHECK_PRINT("       key file: lifetime 0x%08lx (location 0x%06lx), key data %lu bytes\n",
                        (unsigned long)lifetime,
                        (unsigned long)PSA_KEY_LIFETIME_GET_LOCATION(lifetime),
                        (unsigned long)data_len);

        /* A key kept in a secure element is stored by the PSA core as a
         * reference only: the key data is the slot number, not the key. */
        if ((PSA_KEY_LOCATION_LOCAL_STORAGE != PSA_KEY_LIFETIME_GET_LOCATION(lifetime)) &&
            (sizeof(uint64_t) == data_len))     /* psa_key_slot_number_t */
        {
            holds_private_key = false;
        }
    }

    if (holds_private_key)
    {
        snprintf(detail, sizeof(detail),
                 "psa_its_get returned the %u-byte key file incl. private key!",
                 (unsigned int)kc.its_info.size);
    }
    else if (is_key_file)
    {
        snprintf(detail, sizeof(detail),
                 "key file is a reference only (secure element slot), no private key");
    }
    else if (PSA_SUCCESS == status)
    {
        snprintf(detail, sizeof(detail), "data read, but not a PSA key file");
    }
    else
    {
        snprintf(detail, sizeof(detail), "key file not readable by NS code (%s)",
                 its_status_name(status));
    }
    key_check_result(!holds_private_key, "ITS read", detail);

    /* Never keep key file bytes around */
    memset(kc.its_blob, 0, sizeof(kc.its_blob));
    kc.its_len = 0;
}

/*******************************************************************************
* Function Name: check_rng
*******************************************************************************/
static void check_rng(void)
{
    char detail[96];
    psa_status_t status_a = psa_generate_random(kc.rnd_a, sizeof(kc.rnd_a));
    psa_status_t status_b = psa_generate_random(kc.rnd_b, sizeof(kc.rnd_b));

    bool passed = (PSA_SUCCESS == status_a) && (PSA_SUCCESS == status_b) &&
                  (0 != memcmp(kc.rnd_a, kc.rnd_b, sizeof(kc.rnd_a))) &&
                  !key_check_is_constant(kc.rnd_a, sizeof(kc.rnd_a)) &&
                  !key_check_is_constant(kc.rnd_b, sizeof(kc.rnd_b));

    snprintf(detail, sizeof(detail), "2 x 32 bytes: status %ld/%ld, distinct, not constant",
             (long)status_a, (long)status_b);
    key_check_result(passed, "RNG", detail);
}

/*******************************************************************************
* Function Name: check_signatures
********************************************************************************
* Summary:
*  Sign/verify round trip, tamper detection and ECDSA nonce freshness.
*
*******************************************************************************/
static void check_signatures(psa_key_id_t key_id)
{
    char detail[96];
    psa_status_t status;

    /* Random message hash */
    status = psa_generate_random(kc.hash, sizeof(kc.hash));
    if (PSA_SUCCESS != status)
    {
        snprintf(detail, sizeof(detail), "psa_generate_random -> %ld", (long)status);
        key_check_result(false, "Sign/verify", detail);
        return;
    }

    /* Sign/verify: signature made with the private key verifies with the
     * independently imported public key -> d and Q are a matching pair */
    kc.sig_a_len = 0;
    status = psa_sign_hash(key_id, KEY_CHECK_SIGN_ALG, kc.hash, sizeof(kc.hash),
                           kc.sig_a, sizeof(kc.sig_a), &kc.sig_a_len);
    if (PSA_SUCCESS != status)
    {
        snprintf(detail, sizeof(detail), "psa_sign_hash -> %ld", (long)status);
        key_check_result(false, "Sign/verify", detail);
        return;
    }
    status = psa_verify_hash(kc.verify_key_id, KEY_CHECK_SIGN_ALG, kc.hash, sizeof(kc.hash),
                             kc.sig_a, kc.sig_a_len);
    snprintf(detail, sizeof(detail), "%u-byte signature, verify -> %ld",
             (unsigned int)kc.sig_a_len, (long)status);
    key_check_result(PSA_SUCCESS == status, "Sign/verify", detail);

    /* Tamper: one flipped bit in the hash must make verification fail */
    kc.hash[0] ^= 0x01U;
    status = psa_verify_hash(kc.verify_key_id, KEY_CHECK_SIGN_ALG, kc.hash, sizeof(kc.hash),
                             kc.sig_a, kc.sig_a_len);
    kc.hash[0] ^= 0x01U;
    snprintf(detail, sizeof(detail), "modified hash verify -> %ld (expect -149 INVALID_SIGNATURE)",
             (long)status);
    key_check_result(PSA_ERROR_INVALID_SIGNATURE == status, "Tamper", detail);

    /* Nonce: randomized ECDSA must give a different signature for the same
     * hash. A repeated nonce would reveal the private key. */
    kc.sig_b_len = 0;
    status = psa_sign_hash(key_id, KEY_CHECK_SIGN_ALG, kc.hash, sizeof(kc.hash),
                           kc.sig_b, sizeof(kc.sig_b), &kc.sig_b_len);
    snprintf(detail, sizeof(detail), "second signature of same hash -> %ld, differs", (long)status);
    key_check_result((PSA_SUCCESS == status) && (kc.sig_a_len == kc.sig_b_len) &&
                     (0 != memcmp(kc.sig_a, kc.sig_b, kc.sig_a_len)),
                     "ECDSA nonce", detail);
}

/*******************************************************************************
* Function Name: check_persistence
********************************************************************************
* Summary:
*  Removes the key from RAM (psa_purge_key) so the next use reloads it from
*  persistent storage, then compares the public key with the one exported
*  before.
*
*******************************************************************************/
static void check_persistence(psa_key_id_t key_id)
{
    char detail[96];
    psa_status_t purge_status = psa_purge_key(key_id);

    kc.pub_reloaded_len = 0;
    psa_status_t status = psa_export_public_key(key_id, kc.pub_key_reloaded,
                                                sizeof(kc.pub_key_reloaded),
                                                &kc.pub_reloaded_len);

    snprintf(detail, sizeof(detail), "purge -> %ld, reload -> %ld, public key unchanged",
             (long)purge_status, (long)status);
    key_check_result((PSA_SUCCESS == purge_status) && (PSA_SUCCESS == status) &&
                     (kc.pub_reloaded_len == kc.pub_len) &&
                     (0 == memcmp(kc.pub_key_reloaded, kc.pub_key, kc.pub_len)),
                     "Persistence", detail);
}

/*******************************************************************************
* Function Name: check_uniqueness
*******************************************************************************/
static void check_uniqueness(void)
{
    if (0U == kc.prev_pub_len)
    {
        KEY_CHECK_PRINT("[INFO] %-12s no previous key to compare (first generation)\n",
                        "Uniqueness");
        return;
    }

    key_check_print_hex("Previous public key", kc.prev_pub_key, kc.prev_pub_len);
    key_check_result((kc.prev_pub_len != kc.pub_len) ||
                     (0 != memcmp(kc.prev_pub_key, kc.pub_key, kc.pub_len)),
                     "Uniqueness", "new key differs from the replaced key");
}

/*******************************************************************************
* Function Name: key_security_check_capture_previous
*******************************************************************************/
void key_security_check_capture_previous(psa_key_id_t key_id)
{
    kc.prev_pub_len = 0;
    if (PSA_SUCCESS != psa_export_public_key(key_id, kc.prev_pub_key,
                                             sizeof(kc.prev_pub_key), &kc.prev_pub_len))
    {
        /* No previous key stored */
        kc.prev_pub_len = 0;
    }
}

/*******************************************************************************
* Function Name: key_security_check_run
********************************************************************************
* Summary:
*  Runs all key security checks and prints a summary.
*
* Parameters:
*  key_id     : PSA key to validate
*  is_new_key : true right after generation (enables the uniqueness check)
*
* Return:
*  bool : true if all checks passed
*
*******************************************************************************/
bool key_security_check_run(psa_key_id_t key_id, bool is_new_key)
{
    checks_passed = 0;
    checks_failed = 0;

    KEY_CHECK_PRINT("==================== Key security check ====================\n");
    KEY_CHECK_PRINT("Key id 0x%lx (%s key)\n", (unsigned long)key_id,
                    is_new_key ? "newly generated" : "stored");

    check_attributes(key_id);
    check_export_refused(key_id);
    check_its_direct_read(key_id);
    check_rng();

    if (check_public_key(key_id))
    {
        check_signatures(key_id);
        (void)psa_destroy_key(kc.verify_key_id);
        kc.verify_key_id = PSA_KEY_ID_NULL;
    }

    check_persistence(key_id);

    if (is_new_key)
    {
        check_uniqueness();
    }

    /* Isolation is a property of the build configuration, not of the key */
#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1) && \
    defined(CM55_TFM_SE_KEY) && (CM55_TFM_SE_KEY == 1)
    KEY_CHECK_PRINT("[INFO] %-12s private key kept in TF-M (CM33 Secure) via SE driver; "
                    "other crypto on CM55\n", "Isolation");
#elif defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1)
    KEY_CHECK_PRINT("[WARN] %-12s private key is loaded into CM55 RAM for use "
                    "(PSA core on CM55). Stored copy is in TF-M ITS.\n", "Isolation");
#else
    KEY_CHECK_PRINT("[INFO] %-12s private key never leaves TF-M (CM33 Secure)\n",
                    "Isolation");
#endif

    KEY_CHECK_PRINT("RESULT: %s (%lu passed, %lu failed)\n",
                    (0U == checks_failed) ? "PASS" : "FAIL",
                    (unsigned long)checks_passed, (unsigned long)checks_failed);
    KEY_CHECK_PRINT("============================================================\n");

    /* Clear test data */
    memset(kc.hash, 0, sizeof(kc.hash));
    memset(kc.sig_a, 0, sizeof(kc.sig_a));
    memset(kc.sig_b, 0, sizeof(kc.sig_b));
    memset(kc.rnd_a, 0, sizeof(kc.rnd_a));
    memset(kc.rnd_b, 0, sizeof(kc.rnd_b));

    return (0U == checks_failed);
}

#endif /* PSA_KEY_SECURITY_CHECK */

/* [] END OF FILE */
