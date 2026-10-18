/***********************************************************************************************************************
 * File Name    : menu.c
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

#include "downloader_thread.h"
#include "common_utils.h"
#include "comms.h"
#include "mcuboot_image_utils.h"
#include "menu.h"
#include "xmodem.h"
#include "comms.h"
#include "bootloader_image_info.h"

uint8_t rx_str[64] = {0x0};

void project_info(void)
{
    uint8_t tx_str[70];

    COMMS_STR_PRINT(tx_str, "\r\n\r\n#######################################################\n\r");
    COMMS_STR_PRINT(tx_str,         "# FAEAmec2026@Queretaro - MCUboot Britoloaiza#\n\r");
    COMMS_STR_PRINT(tx_str,         "V Downloader thread started                           V\n\r");
}

void menu(void)
{
    fsp_err_t fsp_err = FSP_SUCCESS;
    uint8_t tx_str[80] = {0x0};

    memset(&rx_str[0], 0, sizeof(rx_str));

    COMMS_STR_PRINT(tx_str, "\r\nPlease select from below menu options:\n\r");
    COMMS_STR_PRINT(tx_str, "1) Display image slot info\n\r");
    COMMS_STR_PRINT(tx_str, "2) Display image trailer info\n\r");
    COMMS_STR_PRINT(tx_str, "3) Dump user application image\n\r");
    COMMS_STR_PRINT(tx_str, "4) Download and boot the new image (XModem)\n\r");
#ifdef MCUBOOT_IMAGE_UTILITIES_MCUBOOT_CONFIG_H_
    COMMS_STR_PRINT(tx_str, "5) Confirm primary slot (Write 0x1 to image_ok)\n\r");
#endif
    COMMS_STR_PRINT(tx_str, "9) Reset the system\n\r");
    COMMS_STR_PRINT(tx_str, ">");

    uint32_t len = 1;
    fsp_err = comms_read(rx_str, &len, portMAX_DELAY);
    if(fsp_err != FSP_SUCCESS)
    {
        ERROR_INDICATER
        __BKPT(0);
    }
    /* Echo back + CRCL code */
    COMMS_STR_PRINT(tx_str, "%c\r\n", (int)rx_str[0]);

    switch (rx_str[0])
    {
        case '1':
            display_image_slot_header_info(PRIMARY_IMAGE_START_ADDRESS);
            display_image_slot_header_info(SECONDARY_IMAGE_START_ADDRESS);
            break;

        case '2':
            display_image_trailer_info();
            break;

        case '3':
            display_user_app_image_data(PRIMARY_IMAGE_START_ADDRESS, 0x300);
            display_user_app_image_data(SECONDARY_IMAGE_START_ADDRESS, 0x300);
            break;

        case '4':
            /* Download a new image in the secondary slot using the XModem protocol */
            /* Erase the secondary slot first */
            /* Check if it needs erasing first */
            COMMS_STR_PRINT(tx_str, "Blank checking the secondary slot... ");
            flash_result_t blank_check_result;
            fsp_err = R_FLASH_HP_BlankCheck(&g_flash0_ctrl, SECONDARY_IMAGE_START_ADDRESS, FIRMWARE_IMAGE_SIZE, &blank_check_result);
            if (FSP_SUCCESS != fsp_err)
            {
                COMMS_STR_PRINT(tx_str, "ERROR: R_FLASH_HP_BlankCheck API failed with error 0x%x\r\n", fsp_err);
                ERROR_INDICATER
                __BKPT(0);
            }

            if (FLASH_RESULT_BLANK == blank_check_result)
            {
                COMMS_STR_PRINT(tx_str, "Blank\r\n");
            }
            else
            {
                COMMS_STR_PRINT(tx_str, "Not blank\r\n");

                COMMS_STR_PRINT(tx_str, "Erasing the secondary slot... ");
                ThreadsAndInterrupts(DISABLE);
                FLASH_CTRL_INDICATER
                fsp_err = R_FLASH_HP_Erase(&g_flash0_ctrl, (uint32_t) SECONDARY_IMAGE_START_ADDRESS, SECONDARY_IMAGE_NUM_BLOCKS);
                FLASH_CTRL_INDICATER_CLEAR
                ThreadsAndInterrupts(RE_ENABLE);

                if(FSP_SUCCESS == fsp_err)
                {
                    COMMS_STR_PRINT(tx_str, "Erased\r\n");

                    COMMS_STR_PRINT(tx_str, "Blank checking the secondary slot again... ");
                    fsp_err = R_FLASH_HP_BlankCheck(&g_flash0_ctrl, SECONDARY_IMAGE_START_ADDRESS, FIRMWARE_IMAGE_SIZE, &blank_check_result);
                    if (FSP_SUCCESS != fsp_err)
                    {
                        COMMS_STR_PRINT(tx_str, "ERROR: R_FLASH_HP_BlankCheck API failed with error 0x%x\r\n", fsp_err);
                        ERROR_INDICATER
                        __BKPT(0);
                    }

                    if (FLASH_RESULT_BLANK == blank_check_result)
                    {
                        COMMS_STR_PRINT(tx_str, "Blank\r\n");
                    }
                    else
                    {
                        COMMS_STR_PRINT(tx_str, "Not Blank\r\n");
                        break;
                    }
                }
                else
                {
                    COMMS_STR_PRINT(tx_str, "ERROR: R_FLASH_HP_Erase API failed with error 0x%x\r\n", fsp_err);
                    break;
                }
            }

            /* XModem download and flash programming */
            COMMS_STR_PRINT(tx_str, "Start Xmodem download and flash programming... ");

            unsigned char xm_err = XmodemDownloadAndProgramFlash(SECONDARY_IMAGE_START_ADDRESS);
            comms_read_stop();
            if(XM_SUCCESS == xm_err)
            {
                COMMS_STR_PRINT(tx_str, "Done\r\n");
            }
            else
            {
                switch(xm_err)
                {
                    case XM_API_ERROR:
                        COMMS_STR_PRINT(tx_str, "ERROR: API failed\r\n");
                        break;

                    case XM_TIMEOUT:
                        COMMS_STR_PRINT(tx_str, "ERROR: Timeout during Xmodem download\r\n");
                        break;

                    case XM_PROG_FAIL:
                        COMMS_STR_PRINT(tx_str, "ERROR: Flash programming error\r\n");
                        break;

                    case XM_HOST_CANCEL:
                        COMMS_STR_PRINT(tx_str, "Xmodem transfer canceled\r\n");
                        break;

                    default:
                        COMMS_STR_PRINT(tx_str, "ERROR: unknown error (%d)\r\n", xm_err);
                        break;
                }
            }
            break;

#ifdef MCUBOOT_IMAGE_UTILITIES_MCUBOOT_CONFIG_H_
        case '5':
            COMMS_STR_PRINT(tx_str, "Image confirmed flag (image_ok=0x1) writing... ");
            if(0 == boot_set_confirmed())
            {
                COMMS_STR_PRINT(tx_str, "Done\r\n");
            }
            else
            {
                COMMS_STR_PRINT(tx_str, "ERROR: boot_set_confirmed() API failed\r\n");
            }
            break;
#endif

        case '9':
            COMMS_STR_PRINT(tx_str, "Resetting the system\r\n");
            system_delay_ms(100);
            fsp_err = comms_close();
            if(fsp_err != FSP_SUCCESS)
            {
                ERROR_INDICATER
                __BKPT(0);
            }

            ThreadsAndInterrupts(DISABLE);
            NVIC_SystemReset();
            break;

        default:
            COMMS_STR_PRINT(tx_str, "ERROR: Invalid input\r\n");
            comms_read_stop();
            break;
    }
}
