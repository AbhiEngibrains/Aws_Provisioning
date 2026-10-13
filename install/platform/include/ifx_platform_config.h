/*
 * (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef IFX_PLATFORM_CONFIG_H
#define IFX_PLATFORM_CONFIG_H

/* In this file must be placed config options that are individual for EPC2 */

#define IFX_PSE84_EPC2

/*
 * Use default implementation of crypto keys derivation
 * for Protected Storage
 */
#define PS_DEFAULT_CRYPTO_CONFIG               1

#if (IFX_CM33_NS_PRESENT && (IFX_CM55_NS_PRESENT && !IFX_MTB_MAILBOX)) || TEST_NS_IFX_IRQ_TEST_MASKED
/* Run the scheduler after handling a secure interrupt if the NSPE was pre-empted */
#define CONFIG_TFM_SCHEDULE_WHEN_NS_INTERRUPTED             1
/* Mask Non-Secure interrupts when executing in secure state.
 * This option reduces latency for TF-M calls from the CM55 NSPE. */
#define CONFIG_TFM_SECURE_THREAD_MASK_NS_INTERRUPT          1
#else
/* Do not run the scheduler after handling a secure interrupt if the NSPE was pre-empted */
#define CONFIG_TFM_SCHEDULE_WHEN_NS_INTERRUPTED             0
/* Do not mask Non-Secure interrupts when executing in secure state. */
#define CONFIG_TFM_SECURE_THREAD_MASK_NS_INTERRUPT          0
#endif

#endif /* IFX_PLATFORM_CONFIG_H */
