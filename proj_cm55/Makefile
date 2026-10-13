################################################################################
# \file Makefile
# \version 1.0
#
# \brief
# Makefile for configuring the Cortex-M55 core project
#
################################################################################
# \copyright
# (c) 2023-2026, Infineon Technologies AG, or an affiliate of Infineon
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
################################################################################

include ../common.mk

################################################################################
# Basic Configuration
################################################################################

# Name of application (used to derive name of final linked file).
#
# If APPNAME is edited, ensure to update or regenerate launch
# configurations for your IDE.
APPNAME=proj_cm55

# If set to "true" or "1", display full command-lines when building.
VERBOSE=


################################################################################
# Advanced Configuration
################################################################################

# Enable optional code that is ordinarily disabled by default.
#
# Available components depend on the specific targeted hardware and firmware
# in use. In general, if you have
#
#    COMPONENTS=foo bar
#
# ... then code in directories named COMPONENT_foo and COMPONENT_bar will be
# added to the build
#
COMPONENTS+=FREERTOS LWIP MBEDTLS WCM SECURE_SOCKETS

# Like COMPONENTS, but disable optional code that was enabled by default.
DISABLE_COMPONENTS=

CORE=CM55
CORE_NAME=CM55_0

# By default the build system automatically looks in the Makefile's directory
# tree for source code and builds it. The SOURCES variable can be used to
# manually add source code to the build process from a location not searched
# by default, or otherwise not found by the build system.
SOURCES+=

# Like SOURCES, but for include directories. Value should be paths to
# directories (without a leading -I).
INCLUDES+=

# Custom configuration of mbedtls library.
MBEDTLSFLAGS = MBEDTLS_USER_CONFIG_FILE='"configs/mbedtls_user_config.h"'

# Add additional defines to the build process (without a leading -D).
DEFINES+=$(MBEDTLSFLAGS) CYBSP_WIFI_CAPABLE CY_RETARGET_IO_CONVERT_LF_TO_CRLF CY_RTOS_AWARE 

#Add PSA support define for connectivity MW to use PSA APIs.
DEFINES+=CY_TFM_PSA_SUPPORTED

# Default configuration of mbedtls library.
DEFINES+=MBEDTLS_CONFIG_FILE='"mbedtls/mbedtls_config.h"'
# PSA configuration of mbedtls library.
DEFINES+=MBEDTLS_PSA_CRYPTO_CONFIG_FILE='"configs/ifx_psa_crypto_config.h"'

DEFINES+=MBEDTLS_USER_CONFIG_FILE='"configs/mbedtls_user_config.h"'

DEFINES+=IFX_PSA_MXCRYPTO_USER_CONFIG_FILE='"configs/ifx_psa_mxcrypto_config.h"'

ifeq ($(TOOLCHAIN),LLVM_ARM)
DEFINES+=CY_MQTT_EVENT_THREAD_STACK_SIZE=10*1024
endif

# Comment below line for one-time provisioning/re-use the device certificate from secure storage.
DEFINES+=FORCE_PROVISIONINING

# Debug prints of the PSA key generation process. Set to 0 or comment out to disable.
DEFINES+=PSA_KEYGEN_DEBUG=1

# Key security self-test of the device key (attributes, export, RNG, sign/verify,
# tamper, ECDSA nonce, persistence, uniqueness). Set to 0 or comment out to disable.
DEFINES+=PSA_KEY_SECURITY_CHECK=1

# PSA Crypto engine location.
#  1: PSA Crypto core runs on CM55 (mbedTLS software implementation). Keys are
#     generated and used in CM55 memory. Only entropy (TRNG) and key/cert
#     persistence (ITS/PS) stay in TF-M, because the CRYPTO block and RRAM are
#     secured by the PPC.
#  0: All PSA services run in TF-M on CM33 Secure (default Infineon setup).
CM55_LOCAL_PSA_CRYPTO?=1
ifeq ($(CM55_LOCAL_PSA_CRYPTO),1)
DEFINES+=CM55_LOCAL_PSA_CRYPTO=1
# psa_* crypto functions come from mbedTLS instead of the TF-M client stubs
TFM_IGNORE_SOURCES+=%/interface/src/tfm_crypto_api.c

# Device key (0x1) location when the PSA core runs on CM55.
#  1: Kept in TF-M through a PSA secure element driver (psa_tfm_se_driver.c):
#     the private key is generated, stored and used only in TF-M secure SRAM /
#     TF-M ITS. All other crypto still runs on CM55.
#  0: Kept by the CM55 PSA core (private key in CM55 RAM).
CM55_TFM_SE_KEY?=1
ifeq ($(CM55_TFM_SE_KEY),1)
DEFINES+=CM55_TFM_SE_KEY=1
endif
endif

DEFINES+=CY_RETARGET_IO_CONVERT_LF_TO_CRLF
# Select softfp or hardfp floating point. Default is softfp.
VFP_SELECT+=

# Additional / custom C compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CFLAGS+=

# Additional / custom C++ compiler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
CXXFLAGS+=

# Additional / custom assembler flags.
#
# NOTE: Includes and defines should use the INCLUDES and DEFINES variable
# above.
ASFLAGS+=

# Additional / custom linker flags.
LDFLAGS+=

# Additional / custom libraries to link in to the application.
LDLIBS+=

# Path to the linker script to use (if empty, use the default linker script).
LINKER_SCRIPT=

# Custom pre-build commands to run.
PREBUILD=

# Custom post-build commands to run.
POSTBUILD=


################################################################################
# Paths
################################################################################

# Relative path to the project directory (default is the Makefile's directory).
#
# This controls where automatic source code discovery looks for code.
CY_APP_PATH=

# Relative path to the shared repo location.
#
# All .mtb files have the format, <URI>#<COMMIT>#<LOCATION>. If the <LOCATION> 
# field begins with $$ASSET_REPO$$, then the repo is deposited in the path 
# specified by the CY_GETLIBS_SHARED_PATH variable. The default location is one 
# directory level above the current app directory. This is used with 
# CY_GETLIBS_SHARED_NAME variable, which specifies the directory name.
CY_GETLIBS_SHARED_PATH=../..

# Directory name of the shared repo location.
#
CY_GETLIBS_SHARED_NAME=mtb_shared

CY_IGNORE+=$(CY_GETLIBS_SHARED_PATH)/$(CY_GETLIBS_SHARED_NAME)/aws-iot-device-sdk-embedded-C/202103.00/libraries/standard/coreHTTP/

include $(CY_TOOLS_DIR)/make/start.mk
