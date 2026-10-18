/***********************************************************************************************************************
 * File Name    : usb_comms.c
 * Description  : Contains downloader communication related functions
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

#include "comms.h"
#include "downl_thread.h"
#include "queue.h"

#define COMMS_RX_QUEUE_LENGTH       (256U)
#define COMMS_XMODEM_SOH            (0x01U)

typedef struct st_comms_rx_item
{
    uint8_t data;
    fsp_err_t error;
} comms_rx_item_t;

static QueueHandle_t g_comms_rx_queue;
static SemaphoreHandle_t g_comms_tx_complete_semaphore;
static volatile fsp_err_t g_comms_rx_error = FSP_SUCCESS;

static void comms_rx_error_send_from_isr(fsp_err_t error, BaseType_t * p_higher_priority_task_woken)
{
    comms_rx_item_t rx_item = { .data = 0U, .error = error };

    if (pdTRUE != xQueueSendFromISR(g_comms_rx_queue, &rx_item, p_higher_priority_task_woken))
    {
        g_comms_rx_error = error;
    }
}

static void comms_uart_callback(uart_callback_args_t * p_args)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    switch (p_args->event)
    {
        case UART_EVENT_RX_CHAR:
        {
            comms_rx_item_t rx_item = { .data = (uint8_t) p_args->data, .error = FSP_SUCCESS };
            if (pdTRUE != xQueueSendFromISR(g_comms_rx_queue, &rx_item, &higher_priority_task_woken))
            {
                g_comms_rx_error = FSP_ERR_RXBUF_OVERFLOW;
            }
            break;
        }

        case UART_EVENT_TX_COMPLETE:
            xSemaphoreGiveFromISR(g_comms_tx_complete_semaphore, &higher_priority_task_woken);
            break;

        case UART_EVENT_ERR_OVERFLOW:
            comms_rx_error_send_from_isr(FSP_ERR_OVERFLOW, &higher_priority_task_woken);
            break;

        case UART_EVENT_ERR_FRAMING:
            comms_rx_error_send_from_isr(FSP_ERR_FRAMING, &higher_priority_task_woken);
            break;

        case UART_EVENT_ERR_PARITY:
            comms_rx_error_send_from_isr(FSP_ERR_PARITY, &higher_priority_task_woken);
            break;

        case UART_EVENT_BREAK_DETECT:
            comms_rx_error_send_from_isr(FSP_ERR_BREAK_DETECT, &higher_priority_task_woken);
            break;

        default:
            break;
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
}

static void comms_rtos_objects_delete(void)
{
    if (NULL != g_comms_rx_queue)
    {
        vQueueDelete(g_comms_rx_queue);
        g_comms_rx_queue = NULL;
    }

    if (NULL != g_comms_tx_complete_semaphore)
    {
        vSemaphoreDelete(g_comms_tx_complete_semaphore);
        g_comms_tx_complete_semaphore = NULL;
    }

    g_comms_rx_error = FSP_SUCCESS;
}

static fsp_err_t comms_receive_error_get(void)
{
    fsp_err_t rx_error = g_comms_rx_error;

    if (FSP_SUCCESS != rx_error)
    {
        g_comms_rx_error = FSP_SUCCESS;
        xQueueReset(g_comms_rx_queue);
    }

    return rx_error;
}

fsp_err_t comms_open(void)
{
    fsp_err_t err;

    if ((NULL != g_comms_rx_queue) || (NULL != g_comms_tx_complete_semaphore))
    {
        return FSP_ERR_ALREADY_OPEN;
    }

    g_comms_rx_queue = xQueueCreate(COMMS_RX_QUEUE_LENGTH, sizeof(comms_rx_item_t));
    if (NULL == g_comms_rx_queue)
    {
        return FSP_ERR_OUT_OF_MEMORY;
    }

    g_comms_tx_complete_semaphore = xSemaphoreCreateBinary();
    if (NULL == g_comms_tx_complete_semaphore)
    {
        comms_rtos_objects_delete();
        return FSP_ERR_OUT_OF_MEMORY;
    }

    err = g_uart9.p_api->open(g_uart9.p_ctrl, g_uart9.p_cfg);
    if (FSP_SUCCESS != err)
    {
        comms_rtos_objects_delete();
        return err;
    }

    err = g_uart9.p_api->callbackSet(g_uart9.p_ctrl, comms_uart_callback, NULL, NULL);
    if (FSP_SUCCESS != err)
    {
        fsp_err_t close_err = g_uart9.p_api->close(g_uart9.p_ctrl);
        comms_rtos_objects_delete();
        return (FSP_SUCCESS == close_err) ? err : close_err;
    }

    return FSP_SUCCESS;
}

fsp_err_t comms_close(void)
{
    fsp_err_t err;

    if ((NULL == g_comms_rx_queue) || (NULL == g_comms_tx_complete_semaphore))
    {
        return FSP_ERR_NOT_OPEN;
    }

    err = g_uart9.p_api->close(g_uart9.p_ctrl);
    if (FSP_SUCCESS != err)
    {
        return err;
    }

    comms_rtos_objects_delete();
    return FSP_SUCCESS;
}

fsp_err_t comms_send(uint8_t * p_src, uint32_t len)
{
    fsp_err_t err;
    if ((0U != len) && (NULL == p_src))
    {
        return FSP_ERR_INVALID_POINTER;
    }

    if ((NULL == g_comms_rx_queue) || (NULL == g_comms_tx_complete_semaphore))
    {
        return FSP_ERR_NOT_OPEN;
    }

    for (uint32_t i = 0U; i < len; i++)
    {
        err = g_uart9.p_api->write(g_uart9.p_ctrl, &p_src[i], 1U);
        if (FSP_SUCCESS != err)
        {
            return err;
        }

        if (pdTRUE != xSemaphoreTake(g_comms_tx_complete_semaphore, portMAX_DELAY))
        {
            return FSP_ERR_INTERNAL;
        }
    }

    return FSP_SUCCESS;
}

fsp_err_t comms_read(uint8_t * p_dest, uint32_t * len, uint32_t timeout_milliseconds)
{
    uint32_t requested_length;
    uint32_t received_length = 0U;
    TickType_t start_tick;
    TickType_t timeout_ticks;
    bool wait_forever;
    comms_rx_item_t rx_item;

    if ((NULL == p_dest) || (NULL == len) || (0U == *len))
    {
        return FSP_ERR_INVALID_ARGUMENT;
    }

    if ((NULL == g_comms_rx_queue) || (NULL == g_comms_tx_complete_semaphore))
    {
        return FSP_ERR_NOT_OPEN;
    }

    requested_length = *len;
    *len = 0U;
    wait_forever = (timeout_milliseconds == (uint32_t) portMAX_DELAY);
    timeout_ticks = wait_forever ? portMAX_DELAY : pdMS_TO_TICKS(timeout_milliseconds);
    start_tick = xTaskGetTickCount();

    while (received_length < requested_length)
    {
        TickType_t wait_ticks = timeout_ticks;

        if (!wait_forever && (0U != received_length))
        {
            TickType_t elapsed_ticks = xTaskGetTickCount() - start_tick;
            if (elapsed_ticks >= timeout_ticks)
            {
                xQueueReset(g_comms_rx_queue);
                return FSP_ERR_TIMEOUT;
            }
            wait_ticks = timeout_ticks - elapsed_ticks;
        }

        if (pdTRUE != xQueueReceive(g_comms_rx_queue, &rx_item, wait_ticks))
        {
            xQueueReset(g_comms_rx_queue);
            return FSP_ERR_TIMEOUT;
        }

        fsp_err_t rx_error = comms_receive_error_get();
        if (FSP_SUCCESS != rx_error)
        {
            return rx_error;
        }
        if (FSP_SUCCESS != rx_item.error)
        {
            xQueueReset(g_comms_rx_queue);
            return rx_item.error;
        }

        p_dest[received_length] = rx_item.data;
        received_length++;

        if ((1U == received_length) && ((1U == requested_length) || (COMMS_XMODEM_SOH != p_dest[0])))
        {
            *len = received_length;
            return FSP_SUCCESS;
        }
    }

    *len = received_length;
    return FSP_SUCCESS;
}

fsp_err_t comms_read_stop(void)
{
    if (NULL == g_comms_rx_queue)
    {
        return FSP_ERR_NOT_OPEN;
    }

    xQueueReset(g_comms_rx_queue);
    g_comms_rx_error = FSP_SUCCESS;
    return FSP_SUCCESS;
}
