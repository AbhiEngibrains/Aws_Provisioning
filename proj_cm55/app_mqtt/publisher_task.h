/******************************************************************************
* File Name:   publisher_task.h
*
* Description: This file is the public interface of publisher_task.c
*
* Related Document: See README.md
*
*
*******************************************************************************
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
*******************************************************************************/

#ifndef PUBLISHER_TASK_H_
#define PUBLISHER_TASK_H_

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/*******************************************************************************
* Macros
********************************************************************************/
/* Task parameters for Button Task. */
#define PUBLISHER_TASK_PRIORITY               (2U)
#define PUBLISHER_TASK_STACK_SIZE             (1024U *2U)

/*******************************************************************************
* Global Variables
********************************************************************************/
/* Commands for the Publisher Task. */
typedef enum
{
    PUBLISHER_INIT,
    PUBLISHER_DEINIT,
    PUBLISH_MQTT_MSG
} publisher_cmd_t;

/* Struct to be passed via the publisher task queue */
typedef struct{
    publisher_cmd_t cmd;
    char *data;
} publisher_data_t;

/*******************************************************************************
* Extern Variables
********************************************************************************/
extern TaskHandle_t publisher_task_handle;
extern QueueHandle_t publisher_task_q;

/*******************************************************************************
* Function Prototypes
********************************************************************************/
void publisher_task(void *pvParameters);

#endif /* PUBLISHER_TASK_H_ */

/* [] END OF FILE */
