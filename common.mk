################################################################################
# \file common.mk
# \version 1.0
#
# \brief
# Settings shared across all projects.
#
################################################################################
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
################################################################################

MTB_TYPE=PROJECT

# Target board/hardware (BSP).
# To change the target, it is recommended to use the Library manager
# ('make library-manager' from command line), which will also update 
# Eclipse IDE launch configurations.
TARGET=APP_KIT_PSE84_EVAL_EPC2

# Name of toolchain to use. Options include:
#
# GCC_ARM 	-- GCC is available as part of ModusToolbox Setup program
# ARM     -- ARM Compiler (must be installed separately)
# IAR     -- IAR Compiler (must be installed separately)
#
# See also: CY_COMPILER_PATH below
TOOLCHAIN=GCC_ARM

# Toolchains supported by this code example. See README.md file.
# This is used by automated build systems to identify the supported toolchains.
MTB_SUPPORTED_TOOLCHAINS?=GCC_ARM ARM IAR

# Default build configuration. Options include:
#
# Debug -- build with minimal optimizations, focus on debugging.
# Release -- build with full optimizations
# Custom -- build with custom configuration, set the optimization flag in CFLAGS
# 
# If CONFIG is manually edited, ensure to update or regenerate 
# launch configurations for your IDE.
CONFIG=Debug

# Building ifx-mcuboot with ARM compiler requries some
# specific symbols. However, linking when ifx-mcuboot is 
# added as a library to an existing project,
# those symbols are not required. Excluding those symbols, 
#which are not needed for this application build
ifeq ($(TOOLCHAIN),ARM)
  DEFINES+=MCUBOOT_SKIP_CLEANUP_RAM=1
endif

#Config file for postbuild sign and merge operations.
#NOTE:Check the JSON file for the command parameters
COMBINE_SIGN_JSON?=bsps/TARGET_APP_KIT_PSE84_EVAL_EPC2/config/GeneratedSource/boot_with_bldr.json

include ../common_app.mk
