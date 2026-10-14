/******************************************************************************
* File Name:   mqtt_task.c
*
* Description: This file contains the task that handles initialization & 
*              connection of Wi-Fi and the MQTT client. The task then starts 
*              the subscriber and the publisher tasks. The task also implements
*              reconnection mechanisms to handle WiFi and MQTT disconnections.
*              The task also handles all the cleanup operations to gracefully 
*              terminate the Wi-Fi and MQTT connections in case of any failure.
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
#include "cybsp.h"

/* FreeRTOS header files */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Task header files */
#include "mqtt_task.h"
#include "subscriber_task.h"
#include "publisher_task.h"

/* Configuration file for Wi-Fi and MQTT client */
#include "wifi_config.h"
#include "mqtt_client_config.h"
#include "device_provisioning.h"

/* Middleware libraries */
#include "cy_retarget_io.h"
#include "cy_wcm.h"

#include "cy_mqtt_api.h"
#include "clock.h"

/* LwIP header files */
#include "lwip/netif.h"
#include "retarget_io_init.h"

//#include "tfm_multi_core_api.h"
#include "tfm_ns_interface.h"
//#include "tfm_ns_mailbox.h"
#include "psa/protected_storage.h"
#include "mqtt_client_config.h"

#include "psa/crypto.h"


/******************************************************************************
* Macros
******************************************************************************/
/* Queue length of a message queue that is used to communicate the status of 
 * various operations.
 */
#define MQTT_TASK_QUEUE_LENGTH           (3u)

/* Time in milliseconds to wait before creating the publisher task. */
#define TASK_CREATION_DELAY_MS           (2000u)

/* Flag Masks for tracking which cleanup functions must be called. */
#define WCM_INITIALIZED                             (1lu << 0)
#define WIFI_CONNECTED                              (1lu << 1)
#define LIBS_INITIALIZED                            (1lu << 2)
#define BUFFER_INITIALIZED                          (1lu << 3)
#define MQTT_INSTANCE_CREATED                       (1lu << 4)
#define MQTT_CONNECTION_SUCCESS                     (1lu << 5)
#define MQTT_MSG_RECEIVED                           (1lu << 6)
#define TIME_DIV_MS                                 (60000u)
#define APP_SDIO_INTERRUPT_PRIORITY                 (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY            (2U)
#define APP_SDIO_FREQUENCY_HZ                       (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK                     (64U)

/*String that describes the MQTT handle that is being created in order to uniquely identify it*/
#define MQTT_HANDLE_DESCRIPTOR                       "MQTThandleID"

/* Macro to check if the result of an operation was successful and set the 
 * corresponding bit in the status_flag based on 'init_mask' parameter. When 
 * it has failed, print the error message and return the result to the 
 * calling function.
 */
#define CHECK_RESULT(result, init_mask, error_message...)      \
                     do                                        \
                     {                                         \
                         if ((int)result == CY_RSLT_SUCCESS)   \
                         {                                     \
                             status_flag |= init_mask;         \
                         }                                     \
                         else                                  \
                         {                                     \
                             printf(error_message);            \
                             return result;                    \
                         }                                     \
                     } while(0)


/******************************************************************************
* Global Variables
*******************************************************************************/
/* MQTT connection handle. */
cy_mqtt_t mqtt_connection;

/* Queue handle used to communicate results of various operations - MQTT 
 * Publish, MQTT Subscribe, MQTT connection, and Wi-Fi connection between tasks 
 * and callbacks.
 */
QueueHandle_t mqtt_task_q;

/* Flag to denote initialization status of various operations. */
uint32_t status_flag;

/* Pointer to the network buffer needed by the MQTT library for MQTT send and 
 * receive operations.
 */
uint8_t *mqtt_network_buffer = NULL;
static mtb_hal_sdio_t sdio_instance;
cy_stc_sd_host_context_t sdhc_host_context;
static cy_wcm_config_t wcm_config;

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)

/* SysPm callback parameter structure for SDHC */
static cy_stc_syspm_callback_params_t sdcardDSParams =
{
    .context   = &sdhc_host_context,
    .base      = CYBSP_WIFI_SDIO_HW
};

/* SysPm callback structure for SDHC*/
static cy_stc_syspm_callback_t sdhcDeepSleepCallbackHandler =
{
    .callback           = Cy_SD_Host_DeepSleepCallback,
    .skipMode           = SYSPM_SKIP_MODE,
    .type               = CY_SYSPM_DEEPSLEEP,
    .callbackParams     = &sdcardDSParams,
    .prevItm            = NULL,
    .nextItm            = NULL,
    .order              = SYSPM_CALLBACK_ORDER
};
#endif

psa_key_handle_t key_handle;



/******************************************************************************
 * Function Name: cleanup
 ******************************************************************************
 * Summary:
 *  Function that invokes the deinit and cleanup functions for various
 *  operations based on the status_flag.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void cleanup(void)
{
    cy_rslt_t status = CY_RSLT_SUCCESS;

    /* Disconnect the MQTT connection if it was established. */
    if (status_flag & MQTT_CONNECTION_SUCCESS)
    {
        status = cy_mqtt_disconnect(mqtt_connection);

        if (CY_RSLT_SUCCESS == status)
        {
            printf("Disconnected from the MQTT Broker...\n");
        }
        else
        {
            printf("MQTT disconnect API failed unexpectedly.\n");
        }
    }
    /* Delete the MQTT instance if it was created. */
    if (status_flag & MQTT_INSTANCE_CREATED)
    {
        status = cy_mqtt_delete(mqtt_connection);

        if (CY_RSLT_SUCCESS == status)
        {
            printf("Removed MQTT connection info from stack...\n");
        }
        else
        {
            printf("MQTT delete API failed unexpectedly.\n");
        }
    }
    /* Deallocate the network buffer. */
    if (status_flag & BUFFER_INITIALIZED)
    {
        vPortFree((void *) mqtt_network_buffer);
    }
    /* Deinit the MQTT library. */
    if (status_flag & LIBS_INITIALIZED)
    {
        status = cy_mqtt_deinit();

        if (CY_RSLT_SUCCESS == status)
        {
            printf("Deinitialized MQTT stack...\n");
        }
        else
        {
            printf("MQTT deinit API failed unexpectedly.\n");
        }
    }
    /* Disconnect from Wi-Fi AP. */
    if (status_flag & WIFI_CONNECTED)
    {
        status = cy_wcm_disconnect_ap();

        if (CY_RSLT_SUCCESS == status)
        {
            printf("Disconnected from the Wi-Fi AP!\n");
        }
        else
        {
            printf("WCM disconnect AP failed unexpectedly.\n");
        }
    }
    /* De-initialize the Wi-Fi Connection Manager. */
    if (status_flag & WCM_INITIALIZED)
    {
        status = cy_wcm_deinit();

        if (CY_RSLT_SUCCESS == status)
        {
            printf("Deinitialized Wifi connection...\n");
        }
        else
        {
            printf("WCM deinit API failed unexpectedly.\n");
        }
    }
}


/******************************************************************************
 * Function Name: wifi_connect
 ******************************************************************************
 * Summary:
 *  Function that initiates connection to the Wi-Fi Access Point using the
 *  specified SSID and PASSWORD. The connection is retried a maximum of
 *  'MAX_WIFI_CONN_RETRIES' times with interval of 'WIFI_CONN_RETRY_INTERVAL_MS'
 *  milliseconds.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t : CY_RSLT_SUCCESS upon a successful Wi-Fi connection, else an
 *              error code indicating the failure.
 *
 ******************************************************************************/
static cy_rslt_t wifi_connect(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    cy_wcm_connect_params_t connect_param;
    cy_wcm_ip_address_t ip_address;

    /* Check if Wi-Fi connection is already established. */
    if (!(cy_wcm_is_connected_to_ap()))
    {
        /* Configure the connection parameters for the Wi-Fi interface. */
        memset(&connect_param, 0, sizeof(cy_wcm_connect_params_t));
        memcpy(connect_param.ap_credentials.SSID, WIFI_SSID, sizeof(WIFI_SSID));
        memcpy(connect_param.ap_credentials.password, WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
        connect_param.ap_credentials.security = WIFI_SECURITY;

        printf("\nWi-Fi Connecting to '%s'\n", connect_param.ap_credentials.SSID);

        /* Connect to the Wi-Fi AP. */
        for (uint32_t retry_count = 0; retry_count < MAX_WIFI_CONN_RETRIES; retry_count++)
        {
            result = cy_wcm_connect_ap(&connect_param, &ip_address);

            if (CY_RSLT_SUCCESS == result)
            {
                printf("\nSuccessfully connected to Wi-Fi network '%s'.\n", connect_param.ap_credentials.SSID);

                /* Set the appropriate bit in the status_flag to denote
                 * successful Wi-Fi connection, print the assigned IP address.
                 */
                status_flag |= WIFI_CONNECTED;
                if (ip_address.version == CY_WCM_IP_VER_V4)
                {
                    printf("IPv4 Address Assigned: %s\n\n", ip4addr_ntoa((const ip4_addr_t *) &ip_address.ip.v4));
                }
                else if (ip_address.version == CY_WCM_IP_VER_V6)
                {
                    printf("IPv6 Address Assigned: %s\n\n", ip6addr_ntoa((const ip6_addr_t *) &ip_address.ip.v6));
                }
                return result;
            }

            printf("Wi-Fi Connection failed. Error code:0x%0X. Retrying in %d ms. Retries left: %d\n",
                (int)result, WIFI_CONN_RETRY_INTERVAL_MS, (int)(MAX_WIFI_CONN_RETRIES - retry_count - 1));
            vTaskDelay(pdMS_TO_TICKS(WIFI_CONN_RETRY_INTERVAL_MS));
        }

        printf("\nExceeded maximum Wi-Fi connection attempts!\n");
        printf("Wi-Fi connection failed after retrying for %d mins\n\n",
            (int)(WIFI_CONN_RETRY_INTERVAL_MS * MAX_WIFI_CONN_RETRIES) / TIME_DIV_MS);
    }
    return result;
}

/******************************************************************************
 * Function Name: mqtt_event_callback
 ******************************************************************************
 * Summary:
 *  Callback invoked by the MQTT library for events like MQTT disconnection,
 *  incoming MQTT subscription messages from the MQTT broker.
 *    1. In case of MQTT disconnection, the MQTT client task is communicated
 *       about the disconnection using a message queue.
 *    2. When an MQTT subscription message is received, the subscriber callback
 *       function implemented in subscriber_task.c is invoked to handle the
 *       incoming MQTT message.
 *
 * Parameters:
 *  cy_mqtt_t mqtt_handle : MQTT handle corresponding to the MQTT event (unused)
 *  cy_mqtt_event_t event : MQTT event information
 *  void *user_data : User data pointer passed during cy_mqtt_create() (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
static void mqtt_event_callback(cy_mqtt_t mqtt_handle, cy_mqtt_event_t event, void *user_data)
{
    cy_mqtt_publish_info_t *received_msg;
    mqtt_task_cmd_t mqtt_task_cmd;

    CY_UNUSED_PARAMETER(mqtt_handle);
    CY_UNUSED_PARAMETER(user_data);

    switch(event.type)
    {
        case CY_MQTT_EVENT_TYPE_DISCONNECT:
        {
            /* Clear the status flag bit to indicate MQTT disconnection. */
            status_flag &= ~(MQTT_CONNECTION_SUCCESS);

            /* During provisioning, AWS IoT may disconnect after sending the
             * certificate response. This is normal. Don't queue HANDLE_DISCONNECTION
             * as it would terminate the task before provisioning completes.
             */
            if (provisioning_in_progress) {
                printf("\n[Provisioning] MQTT disconnect during provisioning (expected).\n");
                /* Give a short delay for any pending subscription message to be
                 * delivered before we signal the semaphore as a fallback.
                 * The cy_mqtt library may have the PUBLISH data buffered but
                 * delivers DISCONNECT before SUBSCRIPTION_MESSAGE_RECEIVE. */
                vTaskDelay(pdMS_TO_TICKS(500));
                /* If the subscription callback hasn't signaled yet, unblock
                 * the provisioning wait so it doesn't hang for 30s */
                if (provisioning_sem != NULL && !provisioning_accepted) {
                    printf("[Provisioning] Signaling semaphore from disconnect handler (fallback).\n");
                    xSemaphoreGive(provisioning_sem);
                }
                break;
            }

            printf("\nUnexpectedly disconnected from MQTT broker!\n");
            mqtt_task_cmd = HANDLE_DISCONNECTION;

            /* Send the message to the MQTT client task to handle the
             * disconnection.
             */
            xQueueSend(mqtt_task_q, &mqtt_task_cmd, portMAX_DELAY);
            break;
        }

        case CY_MQTT_EVENT_TYPE_SUBSCRIPTION_MESSAGE_RECEIVE:
        {
            status_flag |= MQTT_MSG_RECEIVED;

            /* Incoming MQTT message has been received. Send this message to
             * the subscriber callback function to handle it.
             */

            received_msg = &(event.data.pub_msg.received_message);

            /* Check if this is a Fleet Provisioning response first */
            if (handle_provisioning_message(received_msg)) {
                break; /* Handled by provisioning logic */
            }

            /* Otherwise pass to normal subscriber callback */
            mqtt_subscription_callback(received_msg);
            break;
        }
        default :
        {
            /* Unknown MQTT event */
            printf("\nUnknown Event received from MQTT callback!\n");
            break;
        }
    }
}

/******************************************************************************
 * Function Name: mqtt_init
 ******************************************************************************
 * Summary:
 *  Function that initializes the MQTT library and creates an instance for the
 *  MQTT client. The network buffer needed by the MQTT library for MQTT send
 *  send and receive operations is also allocated by this function.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t : CY_RSLT_SUCCESS on a successful initialization, else an error
 *              code indicating the failure.
 *
 ******************************************************************************/
static cy_rslt_t mqtt_init(void)
{
    /* Variable to indicate status of various operations. */
    cy_rslt_t result = CY_RSLT_SUCCESS;

    /* Initialize the MQTT library. */
    result = cy_mqtt_init();
    CHECK_RESULT(result, LIBS_INITIALIZED, "\nMQTT library initialization failed!\n");

    /* Allocate buffer for MQTT send and receive operations. */
    mqtt_network_buffer = (uint8_t *) pvPortMalloc(sizeof(uint8_t) * MQTT_NETWORK_BUFFER_SIZE);
    if(NULL == mqtt_network_buffer)
    {
        result = ~CY_RSLT_SUCCESS;
    }
    CHECK_RESULT(result, BUFFER_INITIALIZED, "Network Buffer allocation failed!\n\n");

   /* Import claim certificate key */
    if (import_claim_key(&key_handle) != PSA_SUCCESS) {
        return CY_RSLT_TYPE_ERROR;
    }

   /* Use the PSA key handle for private key. */
    security_info->private_key = (const char *)&(key_handle);
    security_info->private_key_size = sizeof(key_handle);

    /* Create the MQTT client instance. */
    result = cy_mqtt_create(mqtt_network_buffer, MQTT_NETWORK_BUFFER_SIZE,
                            security_info, &broker_info,MQTT_HANDLE_DESCRIPTOR,
                            &mqtt_connection);

    CHECK_RESULT(result, MQTT_INSTANCE_CREATED, "\nMQTT instance creation failed!\n");
    if(CY_RSLT_SUCCESS == result)
    {
        /* Register a MQTT event callback */
        result = cy_mqtt_register_event_callback( mqtt_connection, (cy_mqtt_callback_t)mqtt_event_callback, NULL );
        if(CY_RSLT_SUCCESS == result)
        {
            printf("\nMQTT library initialization successful.\n");
        }
    }
    return result;
}

/******************************************************************************
 * Function Name: mqtt_connect
 ******************************************************************************
 * Summary:
 *  Function that initiates MQTT connect operation. The connection is retried
 *  a maximum of 'MAX_MQTT_CONN_RETRIES' times with interval of
 *  'MQTT_CONN_RETRY_INTERVAL_MS' milliseconds.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t : CY_RSLT_SUCCESS upon a successful MQTT connection, else an
 *              error code indicating the failure.
 *
 ******************************************************************************/
static cy_rslt_t mqtt_connect(void)
{
    /* Variable to indicate status of various operations. */
    cy_rslt_t result = CY_RSLT_SUCCESS;
    bool mqtt_conn_status = false;

    /* MQTT client identifier string. */
    char mqtt_client_identifier[(MQTT_CLIENT_IDENTIFIER_MAX_LEN + 1)] = MQTT_CLIENT_IDENTIFIER;

    /* Configure the user credentials as a part of MQTT Connect packet */
    if (strlen(MQTT_USERNAME) > 0)
    {
        connection_info.username = MQTT_USERNAME;
        connection_info.password = MQTT_PASSWORD;
        connection_info.username_len = sizeof(MQTT_USERNAME) - 1;
        connection_info.password_len = sizeof(MQTT_PASSWORD) - 1;
    }

    /* Set the client identifier buffer and length. */
    connection_info.client_id = mqtt_client_identifier;
    connection_info.client_id_len = strlen(mqtt_client_identifier);

    printf("\n'%.*s' connecting to MQTT broker '%.*s'...\n",
           connection_info.client_id_len,
           connection_info.client_id,
           broker_info.hostname_len,
           broker_info.hostname);

    for (uint32_t retry_count = 0; retry_count < MAX_MQTT_CONN_RETRIES; retry_count++)
    {
        if (cy_wcm_is_connected_to_ap() == 0)
        {
            printf("\nUnexpectedly disconnected from Wi-Fi network! \nInitiating Wi-Fi reconnection...\n");
            status_flag &= ~(WIFI_CONNECTED);

            /* Initiate Wi-Fi reconnection. */
            result = wifi_connect();
            if (CY_RSLT_SUCCESS != result)
            {
                break;
            }
        }

        mqtt_conn_status = false;

        /* Establish the MQTT connection. */
        result = cy_mqtt_connect(mqtt_connection, &connection_info);

        if (CY_RSLT_SUCCESS == result)
        {
            printf("MQTT connection successful.\r\n");

            /* Set the appropriate bit in the status_flag to denote successful
             * MQTT connection, and return the result to the calling function.
             */
            status_flag |= MQTT_CONNECTION_SUCCESS;
            mqtt_conn_status = true;
            break;
        }

        printf("\nMQTT connection failed with error code 0x%0X. \nRetrying in %d ms. Retries left: %d\n",
               (int)result, MQTT_CONN_RETRY_INTERVAL_MS, (int)(MAX_MQTT_CONN_RETRIES - retry_count - 1));
        vTaskDelay(pdMS_TO_TICKS(MQTT_CONN_RETRY_INTERVAL_MS));
    }

    if(!mqtt_conn_status)
    {
        printf("\nExceeded maximum MQTT connection attempts\n");
        printf("MQTT connection failed after retrying for %d mins\n\n",
               (int)(MQTT_CONN_RETRY_INTERVAL_MS * MAX_MQTT_CONN_RETRIES) / TIME_DIV_MS);
    }
    return result;
}

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: sdio_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for SDIO instance.
*******************************************************************************/
static void sdio_interrupt_handler(void)
{
    mtb_hal_sdio_process_interrupt(&sdio_instance);
}

/*******************************************************************************
* Function Name: host_wake_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for the host wake up input pin.
*******************************************************************************/
static void host_wake_interrupt_handler(void)
{
    mtb_hal_gpio_process_interrupt(&wcm_config.wifi_host_wake_pin);
}

/*******************************************************************************
* Function Name: app_sdio_init
********************************************************************************
* Summary:
* This function configures and initializes the SDIO instance used in
* communication between the host MCU and the wireless device.
*******************************************************************************/
static void app_sdio_init(void)
{
    cy_rslt_t result;
    mtb_hal_sdio_cfg_t sdio_hal_cfg;
    cy_stc_sysint_t sdio_intr_cfg =
    {
        .intrSrc = CYBSP_WIFI_SDIO_IRQ,
        .intrPriority = APP_SDIO_INTERRUPT_PRIORITY
    };

    cy_stc_sysint_t host_wake_intr_cfg =
    {
            .intrSrc = CYBSP_WIFI_HOST_WAKE_IRQ,
            .intrPriority = APP_HOST_WAKE_INTERRUPT_PRIORITY
    };

    /* Initialize the SDIO interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status = Cy_SysInt_Init(&sdio_intr_cfg, sdio_interrupt_handler);

    /* SDIO interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_SDIO_IRQ);

    /* Setup SDIO using the HAL object and desired configuration */
    result = mtb_hal_sdio_setup(&sdio_instance, &CYBSP_WIFI_SDIO_sdio_hal_config, NULL, &sdhc_host_context);

    /* SDIO setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Initialize and Enable SD HOST */
    Cy_SD_Host_Enable(CYBSP_WIFI_SDIO_HW);
    Cy_SD_Host_Init(CYBSP_WIFI_SDIO_HW, CYBSP_WIFI_SDIO_sdio_hal_config.host_config, &sdhc_host_context);
    Cy_SD_Host_SetHostBusWidth(CYBSP_WIFI_SDIO_HW, CY_SD_HOST_BUS_WIDTH_4_BIT);

    sdio_hal_cfg.frequencyhal_hz = APP_SDIO_FREQUENCY_HZ;
    sdio_hal_cfg.block_size = SDHC_SDIO_64BYTES_BLOCK;

    /* Configure SDIO */
    mtb_hal_sdio_configure(&sdio_instance, &sdio_hal_cfg);

#if (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP)
    /* SDHC SysPm callback registration */
    Cy_SysPm_RegisterCallback(&sdhcDeepSleepCallbackHandler);
#endif /* (CY_CFG_PWR_SYS_IDLE_MODE == CY_CFG_PWR_MODE_DEEPSLEEP) */

    /* Setup GPIO using the HAL object for WIFI WL REG ON  */
    mtb_hal_gpio_setup(&wcm_config.wifi_wl_pin, CYBSP_WIFI_WL_REG_ON_PORT_NUM, CYBSP_WIFI_WL_REG_ON_PIN);

    /* Setup GPIO using the HAL object for WIFI HOST WAKE PIN  */
    mtb_hal_gpio_setup(&wcm_config.wifi_host_wake_pin, CYBSP_WIFI_HOST_WAKE_PORT_NUM, CYBSP_WIFI_HOST_WAKE_PIN);

    /* Initialize the Host wakeup interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status_host_wake =  Cy_SysInt_Init(&host_wake_intr_cfg, host_wake_interrupt_handler);

    /* Host wake up interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status_host_wake)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(CYBSP_WIFI_HOST_WAKE_IRQ);
}

/******************************************************************************
 * Function Name: terminate_tasks
 ******************************************************************************
 * Summary:
 *  Cleanup section: Delete subscriber and publisher tasks and perform
 *
 * Parameters:
 *  void
 *
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void terminate_tasks(void)
{
    printf("\nTerminating Publisher and Subscriber tasks...\n");
    if (NULL != subscriber_task_handle )
    {
        vTaskDelete(subscriber_task_handle);
    }
    if (NULL != publisher_task_handle )
    {
        vTaskDelete(publisher_task_handle);
    }
    cleanup();
    printf("\nCleanup Done\nTerminating the MQTT task...\n\n");
    vTaskDelete(NULL);
}



/******************************************************************************
 * Function Name: mqtt_client_task
 ******************************************************************************
 * Summary:
 *  Task for handling initialization & connection of Wi-Fi and the MQTT client.
 *  The task also creates and manages the subscriber and publisher tasks upon 
 *  successful MQTT connection. The task also handles the WiFi and MQTT 
 *  connections by initiating reconnection on the event of disconnections.
 *
 * Parameters:
 *  void *pvParameters : Task parameter defined during task creation (unused)
 *
 * Return:
 *  void
 *
 ******************************************************************************/
void mqtt_client_task(void *pvParameters)
{
    /* Structures that store the data to be sent/received to/from various
     * message queues.
     */
    mqtt_task_cmd_t mqtt_status;
    subscriber_data_t subscriber_q_data;
    publisher_data_t publisher_q_data;
    bool mqtt_client_status = false;

    app_sdio_init();

    /* Configure the Wi-Fi interface as a Wi-Fi STA (i.e. Client). */
    wcm_config.interface = CY_WCM_INTERFACE_TYPE_STA;
    wcm_config.wifi_interface_instance = &sdio_instance;

    /* To avoid compiler warnings */
    (void) pvParameters;

    /* Create a message queue to communicate with other tasks and callbacks. */
    mqtt_task_q = xQueueCreate(MQTT_TASK_QUEUE_LENGTH, sizeof(mqtt_task_cmd_t));

    /* Initialize the Wi-Fi Connection Manager and jump to the cleanup block 
     * upon failure.
     */
    if (CY_RSLT_SUCCESS != cy_wcm_init(&wcm_config))
    {
        CY_ASSERT(0);
    }

    /* Set the appropriate bit in the status_flag to denote successful 
     * WCM initialization.
     */
    status_flag |= WCM_INITIALIZED;
    printf("\nWi-Fi Connection Manager initialized.\n");

    /* Initiate connection to the Wi-Fi AP and cleanup if the operation fails. */
    if (CY_RSLT_SUCCESS == wifi_connect())
    {
        /* Set-up the MQTT client and connect to the MQTT broker.
         * cleanup block if any of the operations fail.
         */
        if ( (CY_RSLT_SUCCESS == mqtt_init()) && (CY_RSLT_SUCCESS == mqtt_connect()) )
        {
           
            bool need_provisioning = true;

            printf("\nChecking for existing device certificate in secure storage...\n");
            if (load_device_cert_secure() == PSA_SUCCESS && strlen(device_cert_pem) > 0) {
#if defined(FORCE_PROVISIONINING)
            /*delete the certificate from Protected Storage*/
            delete_device_cert_secure();
#else
            printf("\n[Provisioning] Device certificate loaded from secure storage. Skipping provisioning.\n");
            device_key_id = 0x1; /* Persistent PSA key ID used during provisioning */
            need_provisioning = false; 
#endif
            }

            if (need_provisioning) {
                /* Run Fleet Provisioning before starting normal pub/sub tasks */
                printf("\n[Provisioning] Starting AWS Fleet Provisioning...\n");
                cy_rslt_t prov_result = provision_device_with_aws(mqtt_connection);
                if (prov_result != CY_RSLT_SUCCESS) {
                    printf("[Provisioning] Fleet Provisioning failed: 0x%lx\n", (long)prov_result);
                    printf("[Provisioning] Cannot proceed without valid device certificate.\n");
                    terminate_tasks();
                    return;
                }
                printf("[Provisioning] Fleet Provisioning completed successfully!\n");
            }

            /* --- Reconnect using the device certificate --- */
            printf("[Provisioning] Reconnecting with device certificate...\n");

            /* Disconnect and destroy the old claim-cert MQTT session */
            cy_mqtt_disconnect(mqtt_connection);
            cy_mqtt_delete(mqtt_connection);
            status_flag &= ~(MQTT_CONNECTION_SUCCESS | MQTT_INSTANCE_CREATED);

            /* Switch credentials to the provisioned device certificate and key */
            security_info->client_cert = (const char *)device_cert_pem;
            security_info->client_cert_size = strlen(device_cert_pem) + 1;
            security_info->private_key = (const char *)&device_key_id;
            security_info->private_key_size = sizeof(device_key_id);

            /* Recreate MQTT instance with new credentials */
            cy_rslt_t reconn_result;
            reconn_result = cy_mqtt_create(mqtt_network_buffer, MQTT_NETWORK_BUFFER_SIZE,
                                    security_info, &broker_info, MQTT_HANDLE_DESCRIPTOR,
                                    &mqtt_connection);
            if (reconn_result != CY_RSLT_SUCCESS) {
                printf("[Provisioning] MQTT recreate failed: 0x%lx\n", (long)reconn_result);
                terminate_tasks();
                return;
            }
            status_flag |= MQTT_INSTANCE_CREATED;

            reconn_result = cy_mqtt_register_event_callback(mqtt_connection,
                        (cy_mqtt_callback_t)mqtt_event_callback, NULL);
            if (reconn_result != CY_RSLT_SUCCESS) {
                printf("[Provisioning] Event callback registration failed\n");
                terminate_tasks();
                return;
            }

            /* Connect with the new device certificate */
            reconn_result = mqtt_connect();
            if (reconn_result != CY_RSLT_SUCCESS) {
                printf("[Provisioning] Reconnect with device cert failed: 0x%lx\n", (long)reconn_result);
                terminate_tasks();
                return;
            }
            printf("[Provisioning] Connected with device certificate!\n");

            /* Device cert has been consumed by TLS handshake; erase from RAM.
             * It remains safely in PSA Protected Storage for future boots. */
            clear_device_cert_ram();

            /* Create the subscriber task and cleanup if the operation fails. */
            if (pdPASS == xTaskCreate(subscriber_task, "Subscriber task", SUBSCRIBER_TASK_STACK_SIZE,
                                      NULL, SUBSCRIBER_TASK_PRIORITY, &subscriber_task_handle))
            {
                /* Wait for the subscribe operation to complete. */
                vTaskDelay(pdMS_TO_TICKS(TASK_CREATION_DELAY_MS));

                /* Create the publisher task and cleanup if the operation fails. */
                if (pdPASS == xTaskCreate(publisher_task, "Publisher task", PUBLISHER_TASK_STACK_SIZE,
                                          NULL, PUBLISHER_TASK_PRIORITY, &publisher_task_handle))
                {
                    mqtt_client_status = true;
                }
            }
        }
    }

    if(mqtt_client_status)
    {
        mqtt_client_status = false;
        while (true)
        {
            /* Wait for results of MQTT operations from other tasks and callbacks. */
            if (pdTRUE == xQueueReceive(mqtt_task_q, &mqtt_status, portMAX_DELAY))
            {
                /* In this code example, the disconnection from the MQTT Broker or
                 * the Wi-Fi network is handled by the case 'HANDLE_DISCONNECTION'.
                 *
                 * The publish and subscribe failures (`HANDLE_MQTT_PUBLISH_FAILURE`
                 * and `HANDLE_MQTT_SUBSCRIBE_FAILURE`) does not initiate
                 * reconnection in this example, but they can be handled as per the
                 * application requirement in the following swich cases.
                 */
                switch(mqtt_status)
                {
                    case HANDLE_DISCONNECTION:
                    {
                        /* Deinit the publisher before initiating reconnections. */
                        publisher_q_data.cmd = PUBLISHER_DEINIT;
                        xQueueSend(publisher_task_q, &publisher_q_data, portMAX_DELAY);

                        /* Although the connection with the MQTT Broker is lost,
                         * call the MQTT disconnect API for cleanup of threads and
                         * other resources before reconnection.
                         */
                        cy_mqtt_disconnect(mqtt_connection);

                        /* Check if Wi-Fi connection is active. If not, update the
                         * status flag and initiate Wi-Fi reconnection.
                         */
                        if (cy_wcm_is_connected_to_ap() == 0)
                        {
                            status_flag &= ~(WIFI_CONNECTED);
                            printf("\nInitiating Wi-Fi Reconnection...\n");
                            if (CY_RSLT_SUCCESS == wifi_connect())
                            {
                                printf("\nInitiating MQTT Reconnection...\n");
                                if (CY_RSLT_SUCCESS == mqtt_connect())
                                {
                                    /* Initiate MQTT subscribe post the reconnection. */
                                    subscriber_q_data.cmd = SUBSCRIBE_TO_TOPIC;
                                    xQueueSend(subscriber_task_q, &subscriber_q_data, portMAX_DELAY);

                                    /* Initialize Publisher post the reconnection. */
                                    publisher_q_data.cmd = PUBLISHER_INIT;
                                    xQueueSend(publisher_task_q, &publisher_q_data, portMAX_DELAY);
                                    mqtt_client_status = true;
                                }
                            }
                        }
                        break;
                    }

                    default:
                        break;
                }
            }
            if(!mqtt_client_status){
                terminate_tasks();
                break;
            }
        }
    }else
    {
        terminate_tasks();
    }
}


/* [] END OF FILE */
