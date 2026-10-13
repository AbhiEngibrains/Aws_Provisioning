/*
 * Copyright (c) 2021-2024, Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TFM_HAL_MULTI_CORE_H__
#define __TFM_HAL_MULTI_CORE_H__

#include <stdint.h>

/**
 * \brief Performs the necessary actions to start the non-secure CPU running
 *        the code at the specified address.
 *
 * \param[in] start_addr       The entry point address of non-secure code.
 */
void tfm_hal_boot_ns_cpu(uintptr_t start_addr);

/**
 * \brief Called on the secure CPU.
 *        Flags that the secure CPU has completed its initialization
 *        Waits, if necessary, for the non-secure CPU to flag that
 *        it has completed its initialisation
 */
void tfm_hal_wait_for_ns_cpu_ready(void);

#if CONFIG_TFM_PSA_CALL_ADDRESS_REMAP
/**
 * \brief Remap the address from the non-secure CPU to the SPM address space.
 *
 * \param[in]   addr        Address to be remapped
 *
 * \return Returns the remapped address.
 */
void* tfm_hal_remap_ns_cpu_address(const void* addr);

/**
 * \brief Wrapper for tfm_hal_remap_ns_cpu_address to be used in places where
 *        addr can be provided by any partition.
 *
 * \param[in]   p_ldinf     Pointer to the partition information
 * \param[in]   addr        Address to be remapped
 *
 * \return Returns the remapped address.
 */
#define tfm_hal_remap_address(p_ldinf, addr)    (IS_NS_AGENT_MAILBOX(p_ldinf) ? \
                                                    (tfm_hal_remap_ns_cpu_address(addr)) : \
                                                    (addr))
#else /* CONFIG_TFM_PSA_CALL_ADDRESS_REMAP */
#define tfm_hal_remap_ns_cpu_address(addr)      (addr)
#define tfm_hal_remap_address(p_ldinf, addr)    ((void)p_ldinf, addr)
#endif /* CONFIG_TFM_PSA_CALL_ADDRESS_REMAP */

#endif /* __TFM_HAL_MULTI_CORE_H__ */
