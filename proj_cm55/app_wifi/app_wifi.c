/******************************************************************************
* File Name:   app_wifi.c
*
* Description: This file brings up the SDIO interface to the Wi-Fi device,
*              initializes the Wi-Fi Connection Manager (WCM) and connects to
*              the Wi-Fi AP configured in wifi_config.h.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/* Header file includes */
#include <stdio.h>
#include <string.h>
#include "cybsp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_wifi.h"
#include "wifi_config.h"
#include "retarget_io_init.h"

/* Middleware libraries */
#include "cy_wcm.h"

/* LwIP header files */
#include "lwip/ip_addr.h"

/******************************************************************************
* Macros
******************************************************************************/
#define TIME_DIV_MS                                 (60000u)
#define APP_SDIO_INTERRUPT_PRIORITY                 (7U)
#define APP_HOST_WAKE_INTERRUPT_PRIORITY            (2U)
#define APP_SDIO_FREQUENCY_HZ                       (25000000U)
#define SDHC_SDIO_64BYTES_BLOCK                     (64U)

/******************************************************************************
* Global Variables
*******************************************************************************/
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
 * Function Name: app_wifi_init
 ******************************************************************************
 * Summary:
 *  Initializes the SDIO interface to the Wi-Fi device and the Wi-Fi
 *  Connection Manager in STA mode.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  cy_rslt_t : CY_RSLT_SUCCESS upon successful WCM initialization, else an
 *              error code indicating the failure.
 *
 ******************************************************************************/
cy_rslt_t app_wifi_init(void)
{
    cy_rslt_t result;

    app_sdio_init();

    /* Configure the Wi-Fi interface as a Wi-Fi STA (i.e. Client). */
    wcm_config.interface = CY_WCM_INTERFACE_TYPE_STA;
    wcm_config.wifi_interface_instance = &sdio_instance;

    result = cy_wcm_init(&wcm_config);
    if (CY_RSLT_SUCCESS == result)
    {
        printf("\nWi-Fi Connection Manager initialized.\n");
    }
    else
    {
        printf("\nWi-Fi Connection Manager init failed. Error code:0x%0X\n", (int)result);
    }

    return result;
}

/******************************************************************************
 * Function Name: app_wifi_connect
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
cy_rslt_t app_wifi_connect(void)
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

                /* Print the assigned IP address. */
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

/* [] END OF FILE */
