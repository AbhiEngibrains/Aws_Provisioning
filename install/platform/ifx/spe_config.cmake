#-------------------------------------------------------------------------------
# (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

set(IFX_MTB_SRF                             ON)

# In NSPE SRF framework requires RAM_VECTORS_SUPPORT
if(IFX_MTB_SRF)
    set(RAM_VECTORS_SUPPORT                 ON)
    if (IFX_NS_INTERFACE_MAILBOX)
        # In MTB Mailbox case, for remote NSPE, TFM_NS_MANAGE_NSID is not
        # supported because, local NSPE, that acts like a proxy, has no way
        # to pass remote NSPE client ID to SPM
        set(TFM_NS_MANAGE_NSID              OFF CACHE BOOL "Support NSPE OS providing NSPE client_id" FORCE)
        set(TEST_NS_MANAGE_NSID             OFF CACHE BOOL "Whether to build NS regression NSID management tests" FORCE)
    endif()
endif()

# Optional tests
include(${IFX_COMMON_SOURCE_DIR}/tests/spe_config.cmake OPTIONAL)
