/*******************************************************************************
* File Name        : key_security_check.h
*
* Description      : Debug self-test that validates the security properties of
*                    the device PSA key (key 0x1). Enabled with
*                    PSA_KEY_SECURITY_CHECK=1 (DEFINES in the Makefile).
*
* Related Document : See README.md
*
*******************************************************************************/

#ifndef KEY_SECURITY_CHECK_H_
#define KEY_SECURITY_CHECK_H_

#include <stdbool.h>
#include "psa/crypto.h"

/*******************************************************************************
* Macros
*******************************************************************************/
/* Set PSA_KEY_SECURITY_CHECK to 1 (DEFINES+=PSA_KEY_SECURITY_CHECK=1 in the
 * Makefile) to run the key security self-test and print its results. The test
 * never prints private key material. */
#ifndef PSA_KEY_SECURITY_CHECK
#define PSA_KEY_SECURITY_CHECK          (0)
#endif

#if PSA_KEY_SECURITY_CHECK

#define KEY_CHECK_PRINT(...)            printf("[KEY-CHECK] " __VA_ARGS__)

/* Saves the public key of the key that is about to be replaced, so the new
 * key can be checked for uniqueness. Call before psa_destroy_key(). */
#define KEY_SECURITY_CHECK_CAPTURE_PREVIOUS(key_id) \
    key_security_check_capture_previous(key_id)

/* Runs all checks on key_id. is_new_key = true right after generation. */
#define KEY_SECURITY_CHECK_RUN(key_id, is_new_key) \
    (void)key_security_check_run((key_id), (is_new_key))

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void key_security_check_capture_previous(psa_key_id_t key_id);
bool key_security_check_run(psa_key_id_t key_id, bool is_new_key);

#else

#define KEY_CHECK_PRINT(...)                                do { } while (0)
#define KEY_SECURITY_CHECK_CAPTURE_PREVIOUS(key_id)         do { } while (0)
#define KEY_SECURITY_CHECK_RUN(key_id, is_new_key)          do { } while (0)

#endif /* PSA_KEY_SECURITY_CHECK */

#endif /* KEY_SECURITY_CHECK_H_ */

/* [] END OF FILE */
