#include "downloader_thread.h"
#include "downloader_application_code/common_utils.h"
#include "downloader_application_code/comms.h"
#include "downloader_application_code/menu.h"
/* DownThread entry function */
/* pvParameters contains TaskHandle_t */
/* Downloader Thread entry function */
void downloader_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);
    fsp_err_t fsp_err = FSP_SUCCESS;
    /* Open the comms driver */
    fsp_err = comms_open();
    if(FSP_SUCCESS != fsp_err)
    {
    ERROR_INDICATER
    __BKPT(0);
    }
    /* Display project info */
    project_info();
    /* Open Flash driver */
    fsp_err = R_FLASH_HP_Open(&g_flash0_ctrl, &g_flash0_cfg);
    if(FSP_SUCCESS != fsp_err)
    {
    ERROR_INDICATER
    __BKPT(0);
    }
    while (1)
    {
    /* Display and handle project menu */
    menu();
    /* Delay 1 system tick (=1ms) */
    vTaskDelay(1);
    }
}