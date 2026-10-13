/*******************************************************************************
* File Name        : main.c
*
* Description      : This source file contains the main routine for CM55 CPU
*
* Related Document : See README.md
*
********************************************************************************
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

/*******************************************************************************
* Header File
*******************************************************************************/
#include <stdio.h>
#include "cybsp.h"
#include "retarget_io_init.h"
#include "mqtt_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cyabs_rtos.h"
#include "cyabs_rtos_impl.h"
#include "cy_time.h"
#include "psa/crypto.h"
#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1)
#include "cy_tls.h"
#include "psa_tfm_se_driver.h"
#endif

/*******************************************************************************
* Macros
*******************************************************************************/
/* Stack size and priority of the application init task */
#define APP_INIT_TASK_STACK_SIZE            (1024U)
#define APP_INIT_TASK_PRIORITY              (MQTT_CLIENT_TASK_PRIORITY)

/* Enabling or disabling a MCWDT requires a wait time of upto 2 CLK_LF cycles
 * to come into effect. This wait time value will depend on the actual CLK_LF
 * frequency set by the BSP.
 */
#define LPTIMER_1_WAIT_TIME_USEC            (62U)
/* Define the LPTimer interrupt priority number. '1' implies highest priority.
 */
#define APP_LPTIMER_INTERRUPT_PRIORITY      (1U)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* LPTimer HAL object */
static mtb_hal_lptimer_t lptimer_obj;

/* RTC HAL object */
static mtb_hal_rtc_t rtc_obj;

/*******************************************************************************
* Function Name: lptimer_interrupt_handler
********************************************************************************
* Summary:
* Interrupt handler function for LPTimer instance.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void lptimer_interrupt_handler(void)
{
    mtb_hal_lptimer_process_interrupt(&lptimer_obj);
}

/*******************************************************************************
* Function Name: setup_tickless_idle_timer
********************************************************************************
* Summary:
* 1. This function first configures and initializes an interrupt for LPTimer.
* 2. Then it initializes the LPTimer HAL object to be used in the RTOS
*    tickless idle mode implementation to allow the device enter deep sleep
*    when idle task runs. LPTIMER_1 instance is configured for CM55 CPU.
* 3. It then passes the LPTimer object to abstraction RTOS library that
*    implements tickless idle mode
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_tickless_idle_timer(void)
{
    /* Interrupt configuration structure for LPTimer */
    cy_stc_sysint_t lptimer_intr_cfg =
    {
        .intrSrc = CYBSP_CM55_LPTIMER_1_IRQ,
        .intrPriority = APP_LPTIMER_INTERRUPT_PRIORITY
    };

    /* Initialize the LPTimer interrupt and specify the interrupt handler. */
    cy_en_sysint_status_t interrupt_init_status =
                                    Cy_SysInt_Init(&lptimer_intr_cfg,
                                                    lptimer_interrupt_handler);

    /* LPTimer interrupt initialization failed. Stop program execution. */
    if(CY_SYSINT_SUCCESS != interrupt_init_status)
    {
        handle_app_error();
    }

    /* Enable NVIC interrupt. */
    NVIC_EnableIRQ(lptimer_intr_cfg.intrSrc);

    /* Initialize the MCWDT block */
    cy_en_mcwdt_status_t mcwdt_init_status =
                                    Cy_MCWDT_Init(CYBSP_CM55_LPTIMER_1_HW,
                                                &CYBSP_CM55_LPTIMER_1_config);

    /* MCWDT initialization failed. Stop program execution. */
    if(CY_MCWDT_SUCCESS != mcwdt_init_status)
    {
        handle_app_error();
    }

    /* Enable MCWDT instance */
    Cy_MCWDT_Enable(CYBSP_CM55_LPTIMER_1_HW,
                    CY_MCWDT_CTR_Msk,
                    LPTIMER_1_WAIT_TIME_USEC);

    /* Setup LPTimer using the HAL object and desired configuration as defined
     * in the device configurator. */
    cy_rslt_t result = mtb_hal_lptimer_setup(&lptimer_obj,
                                            &CYBSP_CM55_LPTIMER_1_hal_config);

    /* LPTimer setup failed. Stop program execution. */
    if(CY_RSLT_SUCCESS != result)
    {
        handle_app_error();
    }

    /* Pass the LPTimer object to abstraction RTOS library that implements
     * tickless idle mode
     */
    cyabs_rtos_set_lptimer(&lptimer_obj);
}

/*******************************************************************************
* Function Name: setup_clib_support
********************************************************************************
* Summary:
* Initializes the CLIB support library with the RTC so that time() works on
* CM55. The RTC hardware itself is initialized and set by CM33 before CM55
* is enabled.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
static void setup_clib_support(void)
{
    /* Initialize the ModusToolbox CLIB support library */
    mtb_clib_support_init(&rtc_obj);
}

/*******************************************************************************
* Function Name: app_init_task
********************************************************************************
* Summary:
* Initializes PSA Crypto (over the mailbox to TF-M, which needs the scheduler
* running) and then starts the MQTT client task.
*
* Parameters:
*  arg: Unused
*
* Return:
*  void
*
*******************************************************************************/
static void app_init_task(void *arg)
{
    psa_status_t psa_status;

    CY_UNUSED_PARAMETER(arg);

#if defined(CM55_LOCAL_PSA_CRYPTO) && (CM55_LOCAL_PSA_CRYPTO == 1)
    /* The PSA Crypto core runs on CM55: it needs the mbedTLS mutex hooks and
     * heap (memory buffer allocator), which cy_tls_init() installs. */
    if (CY_RSLT_SUCCESS != cy_tls_init())
    {
        printf("Failed to initialize TLS library\n");
        handle_app_error();
    }

#if defined(CM55_TFM_SE_KEY) && (CM55_TFM_SE_KEY == 1)
    /* Secure element driver that keeps the device key in TF-M. Must be
     * registered before psa_crypto_init(). */
    psa_status = psa_tfm_se_register();
    if (PSA_SUCCESS != psa_status)
    {
        printf("Failed to register TF-M secure element driver: %d\n", (int)psa_status);
        handle_app_error();
    }
#endif /* CM55_TFM_SE_KEY */
#endif /* CM55_LOCAL_PSA_CRYPTO */

    /* Initialize PSA Crypto */
    psa_status = psa_crypto_init();
    if (PSA_SUCCESS != psa_status)
    {
        printf("Failed to initialize PSA crypto: %d\n", (int)psa_status);
        handle_app_error();
    }

    /* Create the MQTT Client task. */
    if (pdPASS != xTaskCreate(mqtt_client_task, "MQTT Client task",
                              MQTT_CLIENT_TASK_STACK_SIZE, NULL,
                              MQTT_CLIENT_TASK_PRIORITY, NULL))
    {
        handle_app_error();
    }

    vTaskDelete(NULL);
}

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM55 application.
*
* CM33 application enables the CM55 CPU. CM55 initializes the BSP (which also
* sets up the SRF IPC client used for PSA calls), the tickless idle timer,
* retarget-io and CLIB support, and then starts the RTOS scheduler. PSA Crypto
* initialization and the MQTT client task are started from app_init_task.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals. */
    result = cybsp_init();

    /* Board init failed. Stop program execution. */
    if (CY_RSLT_SUCCESS != result)
    {
        /* Disable all interrupts. */
        __disable_irq();

        CY_ASSERT(0);

        /* Infinite loop */
        while(true);
    }

    /* Enable global interrupts. */
    __enable_irq();

    /* Setup the LPTimer instance for CM55 CPU. */
    setup_tickless_idle_timer();

    /* Initialize retarget-io middleware */
    init_retarget_io();

    /* Setup CLIB support library. */
    setup_clib_support();

    /* \x1b[2J\x1b[;H - ANSI ESC sequence to clear screen. */
    printf("\x1b[2J\x1b[;H");
    printf("===============================================================\n");
    printf("PSOC Edge MCU: AWS Device Provisioning (CM55)\n");
    printf("===============================================================\n\n");

    /* Create the application init task. */
    if (pdPASS == xTaskCreate(app_init_task, "App init task",
                              APP_INIT_TASK_STACK_SIZE, NULL,
                              APP_INIT_TASK_PRIORITY, NULL))
    {
        /* Start the FreeRTOS scheduler. */
        vTaskStartScheduler();
    }

    /* Should never get here. */
    handle_app_error();
}

/* [] END OF FILE */
