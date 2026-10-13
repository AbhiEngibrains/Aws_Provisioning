/*
 * (c) 2025-2026, Infineon Technologies AG, or an affiliate of Infineon
 * Technologies AG. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stddef.h>
#include <string.h>

#include "config_tfm.h"

#include "cmsis.h"
#include "ifx_platform_private.h"
#include "mtb_srf.h"
#include "psa_manifest/sid.h"
#include "tfm_platform_api.h"

cy_rslt_t mtb_srf_request_submit(mtb_srf_invec_ns_t* inVec_ns, uint8_t inVec_cnt_ns,
                                 mtb_srf_outvec_ns_t* outVec_ns, uint8_t outVec_cnt_ns)
{
    return psa_call(TFM_PLATFORM_SERVICE_HANDLE, IFX_PLATFORM_API_ID_MTB_SRF,
                    inVec_ns, inVec_cnt_ns,
                    outVec_ns, outVec_cnt_ns);
}
