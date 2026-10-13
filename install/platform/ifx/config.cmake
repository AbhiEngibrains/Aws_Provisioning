#-------------------------------------------------------------------------------
# (c) 2024-2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# Platform can optionally provide configuration for Infineon non-secure test suite
include(${IFX_PLATFORM_SOURCE_DIR}/tests/ifx_tests_ns_config.cmake OPTIONAL)

include(${IFX_COMMON_SOURCE_DIR}/shared/config.cmake)

# BSP source generation
include(${IFX_COMMON_SOURCE_DIR}/cmake/generate_sources.cmake)

include(${IFX_COMMON_SOURCE_DIR}/post_config.cmake)
