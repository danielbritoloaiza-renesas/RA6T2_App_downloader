/***********************************************************************************************************************
 * File Name    : xmodem.c
 * Description  : Contains XModem protocol implementations
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

#include "downloader_application_code/common_utils.h"
#include "downloader_application_code/comms.h"
#include "downloader_application_code/menu.h"
#include "downloader_application_code/xmodem.h"
#include "string.h"
#include "hal_data.h"
#include "downloader_thread.h"
#include "bootloader_image_info.h"

#define XM_CTRL_CODE_SOH        0x01
#define XM_CTRL_CODE_EOT        0x04
#define XM_CTRL_CODE_ACK        0x06
#define XM_CTRL_CODE_NAK        0x15
#define XM_CTRL_CODE_CAN        0x18
#define XM_CTRL_CODE_EXTRA_ESC  0x1B

// internal status info
#define XM_OK               0x0
#define XM_ERROR            0x1
#define XM_TOUT             0x2

static bool StartCondition;
unsigned char   RxByteBuffer[192] BSP_ALIGN_VARIABLE(4);

/*******************************************************************************************************************//**
 * @brief This function implements the XModem protocol and handles the flash operation
 * @param[IN]   FlashAddress            Start address of the flash where the new image will be downloaded
 * @retval      FSP_SUCCESS             Upon successful FLash_HP data flash operations.
 * @retval      Any Other Error code    Upon unsuccessful Flash_HP data flash operations.
 **********************************************************************************************************************/
unsigned char XmodemDownloadAndProgramFlash (unsigned long FlashAddress)
{
    /*
XmodemDownloadAndProgramFlash() takes a memory address as the base address to
which data downloaded is programmed.  The data is downloaded using the XModem
protocol developed in 1977 by Ward Christensen.
The routine detects XM_ERRORs due to XM_TIMEOUTs, comms XM_ERRORs or invalid checksums.
The routine reports the following to the caller:
-Success
-Invalid address    (not implemented here as the address is checked prior to this
                    function being called)
-Comms XM_ERROR
-XM_TIMEOUT XM_ERROR
-Failure to program flash


Expects:
--------
FlashAddress:
32-bit address located in Flash memory space starting on a 128-byte boundary

Returns:
--------
XM_SUCCESS              -   Download and Flash programming performed successful
XM_ADDRESS_XM_ERROR     -   Address was either not on a 128-bit boundary or not in valid Flash
XM_COMMS_XM_ERROR       -   Comms parity, framing or overrun XM_ERROR
XM_XM_TIMEOUT           -   Transmitter did not respond to this receiver
XM_PROG_FAIL        -   Failed to program one or more bytes of the Flash memory
     */
    unsigned char   xm_ret;
    unsigned char   ExpectedBlkNum;
    unsigned char   RetryCounter;
    unsigned char   RxByteBufferIndex;
    unsigned char   checksum;
    fsp_err_t       fsp_err = FSP_SUCCESS;
    unsigned long   Address;
    uint8_t         tx_byte BSP_ALIGN_VARIABLE(4);
    uint32_t        rx_len;

    memset(&RxByteBuffer[0], 0, sizeof(RxByteBuffer));

    // first xmodem block number is 1
    ExpectedBlkNum = 1;
    StartCondition = true;
    Address        = FlashAddress;

    while(1)
    {
        // {1}
        // initialize Rx attempts counter
        RetryCounter = 10;

        //  decrement Rx attempts counter & get Rx byte with a 10 sec TIMEOUT repeat until Rx attempts is 0
        xm_ret = XM_TOUT;
        while ( (RetryCounter > 0) && (xm_ret == XM_TOUT) )
        {
            if(true == StartCondition)
            {
                //  if this is the start of the xmodem frame
                //  send a NAK to the transmitter
                tx_byte = XM_CTRL_CODE_NAK;
                comms_send(&tx_byte, 1);    // Kick off the XModem transfer

                /* Request 132 bytes from host with a delay of 5 seconds */
                rx_len = 132;
                memset((void *)&RxByteBuffer[0], 0, 132);
                fsp_err = comms_read((uint8_t *)&RxByteBuffer[0], &rx_len, 5000);
            }
            else
            {
                /* Request 132 bytes from host with a delay of 1 second */
                rx_len = 132;
                memset((void *)&RxByteBuffer[0], 0, 132);
                fsp_err = comms_read((uint8_t *)&RxByteBuffer[0], &rx_len, 1000);
            }
            if(FSP_SUCCESS == fsp_err)
            {
                StartCondition = false;
                xm_ret = XM_OK;
            }
            else if(FSP_ERR_TIMEOUT == fsp_err)
            {
                xm_ret = XM_TOUT;
            }
            else
            {
                xm_ret = XM_ERROR;
            }
            RetryCounter--;
        }

        if ( xm_ret == XM_ERROR )
        {
            return ( XM_API_ERROR );
        }
        else if ( xm_ret == XM_TOUT )
        {
            return ( XM_TIMEOUT );
        }
        else
        {
            // if first received byte is 'end of frame'
            // return ACK to sender
            if ( RxByteBuffer[0] == XM_CTRL_CODE_EOT )
            {
                system_delay_ms(10);
                tx_byte = XM_CTRL_CODE_ACK;
                comms_send(&tx_byte, 1);
                return( XM_SUCCESS );
            }
            else if(RxByteBuffer[0] == XM_CTRL_CODE_CAN)
            {
                system_delay_ms(10);
                tx_byte = XM_CTRL_CODE_ACK;
                comms_send(&tx_byte, 1);
                return( XM_HOST_CANCEL );
            }
            else if(RxByteBuffer[0] == XM_CTRL_CODE_EXTRA_ESC)
            {
                return( XM_HOST_CANCEL );
            }
            else if(RxByteBuffer[0] == XM_CTRL_CODE_SOH)
            {
                // data Rx ok
                // calculate the checksum of the data bytes only
                checksum = 0;
                int cs = 0;

                /* check if the data make sense */
                for (RxByteBufferIndex=0; RxByteBufferIndex<128; RxByteBufferIndex++)
                {
                    cs += RxByteBuffer[RxByteBufferIndex + 3];
                }

                /* This int to unsigned char conversion needed to eliminate conversion warning with checksum calculation. */
                checksum = (unsigned char)cs;

                //  if SOH, BLK#, 255-BLK# or checksum not valid
                //  (BLK# is valid if the same as expected blk counter or is 1 less
                if ( !( (RxByteBuffer[0] == XM_CTRL_CODE_SOH) && ((RxByteBuffer[1] == ExpectedBlkNum) || (RxByteBuffer[1] == ExpectedBlkNum - 1) ) && (RxByteBuffer[2] + RxByteBuffer[1] == 255 ) && (RxByteBuffer[131] == checksum) ) )
                {
                    //  send NAK and loop back to (1)
                    tx_byte = XM_CTRL_CODE_NAK;
                    comms_send(&tx_byte, 1);
                }
                else
                {
                    //  if blk# is expected blk num
                    if ( RxByteBuffer[1] == ExpectedBlkNum )
                    {
                        // Program the received data into flash
                        // Assume flash area is erased */
                        ThreadsAndInterrupts(DISABLE);
                        FLASH_CTRL_INDICATER
                        fsp_err = R_FLASH_HP_Write(&g_flash0_ctrl, (uint32_t)&RxByteBuffer[3], (uint32_t)Address, 128);
                        FLASH_CTRL_INDICATER_CLEAR
                        ThreadsAndInterrupts(RE_ENABLE);

                        if(FSP_SUCCESS == fsp_err)
                        {
                            Address += 128;
                            ExpectedBlkNum++;
                            tx_byte = XM_CTRL_CODE_ACK;
                            comms_send(&tx_byte, 1);
                        }
                        else
                        {
                            // prog fail
                            tx_byte = XM_CTRL_CODE_NAK;
                            comms_send(&tx_byte, 1);
                            // cancel xmodem download
                            tx_byte = XM_CTRL_CODE_CAN;
                            comms_send(&tx_byte, 1);
                            system_delay_ms(100);
                            return( XM_PROG_FAIL );
                        }
                    }
                    else
                    {
                        //  block number is valid but this data block has already been received
                        //  send ACK and loop to (1)
                        tx_byte = XM_CTRL_CODE_ACK;
                        comms_send(&tx_byte, 1);
                    }
                }
            }
            else
            {
                //  send NAK and loop back to (1)
                tx_byte = XM_CTRL_CODE_NAK;
                comms_send(&tx_byte, 1);
            }
        }
    }
}
