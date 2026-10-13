#-------------------------------------------------------------------------------
# (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
# Technologies AG. All rights reserved.
#
# SPDX-License-Identifier: BSD-3-Clause
#
#-------------------------------------------------------------------------------

# Include tests on post step of tf-m-tests suite
if(TFM_NS_REG_TEST)
    add_subdirectory_ext(${IFX_COMMON_SOURCE_DIR}/tests tests)
endif()

################################ Code coverage #################################

# Code coverage flags must be added after all files have been added
# to the build

if(TEST_NS_IFX_CODE_COVERAGE)
    include(${IFX_COMMON_SOURCE_DIR}/cmake/code_coverage.cmake)

    ifx_add_code_coverage_ns()
endif()

################################################################################
