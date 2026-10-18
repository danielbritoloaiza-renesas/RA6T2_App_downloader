/***********************************************************************************************************************
 * File Name    : common_utils.c
 * Description  :
 ***********************************************************************************************************************/
/***********************************************************************************************************************
 * Copyright [2020-2022] Renesas Electronics Corporation and/or its affiliates.  All Rights Reserved.
 *
 * This software and documentation are supplied by Renesas Electronics America Inc. and may only be used with products
 * of Renesas Electronics Corp. and its affiliates ("Renesas").  No other uses are authorized.  Renesas products are
 * sold pursuant to Renesas terms and conditions of sale.  Purchasers are solely responsible for the selection and use
 * of Renesas products and Renesas assumes no liability.  No license, express or implied, to any intellectual property
 * right is granted by Renesas. This software is protected under all applicable laws, including copyright laws. Renesas
 * reserves the right to change or discontinue this software and/or this documentation. THE SOFTWARE AND DOCUMENTATION
 * IS DELIVERED TO YOU "AS IS," AND RENESAS MAKES NO REPRESENTATIONS OR WARRANTIES, AND TO THE FULLEST EXTENT
 * PERMISSIBLE UNDER APPLICABLE LAW, DISCLAIMS ALL WARRANTIES, WHETHER EXPLICITLY OR IMPLICITLY, INCLUDING WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NONINFRINGEMENT, WITH RESPECT TO THE SOFTWARE OR
 * DOCUMENTATION.  RENESAS SHALL HAVE NO LIABILITY ARISING OUT OF ANY SECURITY VULNERABILITY OR BREACH.  TO THE MAXIMUM
 * EXTENT PERMITTED BY LAW, IN NO EVENT WILL RENESAS BE LIABLE TO YOU IN CONNECTION WITH THE SOFTWARE OR DOCUMENTATION
 * (OR ANY PERSON OR ENTITY CLAIMING RIGHTS DERIVED FROM YOU) FOR ANY LOSS, DAMAGES, OR CLAIMS WHATSOEVER, INCLUDING,
 * WITHOUT LIMITATION, ANY DIRECT, CONSEQUENTIAL, SPECIAL, INDIRECT, PUNITIVE, OR INCIDENTAL DAMAGES; ANY LOST PROFITS,
 * OTHER ECONOMIC DAMAGE, PROPERTY DAMAGE, OR PERSONAL INJURY; AND EVEN IF RENESAS HAS BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH LOSS, DAMAGES, CLAIMS OR COSTS.
 **********************************************************************************************************************/

#include "hal_data.h"
#include "common_utils.h"
#if (BSP_CFG_RTOS == 2)
#include "FreeRTOS.h"
#include "task.h"
#endif

void system_delay_ms (uint32_t time)
{
#if (BSP_CFG_RTOS == 0)
    R_BSP_SoftwareDelay((uint32_t) time, BSP_DELAY_UNITS_MILLISECONDS);
#elif (BSP_CFG_RTOS == 1)
    tx_thread_sleep(time);
#elif (BSP_CFG_RTOS == 2)
    vTaskDelay((TickType_t) (time / portTICK_PERIOD_MS));
#endif                                 /* #if (BSP_CFG_RTOS == 0) */
}

void ThreadsAndInterrupts (enable_disable_t EnableDisable)
{
    static uint32_t control_reg_value;
    static uint32_t old_mask_level = 0U;

    if( DISABLE == EnableDisable )
    {
#if (BSP_CFG_RTOS == 2)
        /* Suspend all tasks */
        vTaskSuspendAll();
#endif
        /* Disable the SysTick timer */
        control_reg_value = SysTick->CTRL;
        SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk;

        /* Store the interrupt state */
        FSP_CRITICAL_SECTION_ENTER;
    }
    else
    {
        /* Restore the interrupt state */
        FSP_CRITICAL_SECTION_EXIT;

        /* Restart the SysTick timer */
        SysTick->CTRL = control_reg_value & ~SysTick_CTRL_ENABLE_Msk;
        SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;

#if (BSP_CFG_RTOS == 2)
        /* Resume all tasks */
        xTaskResumeAll();
#endif
    }
}
