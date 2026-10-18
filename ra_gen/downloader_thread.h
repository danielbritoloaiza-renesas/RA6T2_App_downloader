/* generated thread header file - do not edit */
#ifndef DOWNLOADER_THREAD_H_
#define DOWNLOADER_THREAD_H_
#include "bsp_api.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "hal_data.h"
#ifdef __cplusplus
                extern "C" void downloader_thread_entry(void * pvParameters);
                #else
extern void downloader_thread_entry(void *pvParameters);
#endif
#include "r_flash_hp.h"
#include "r_flash_api.h"
FSP_HEADER
/* Flash on Flash HP Instance */
extern const flash_instance_t g_flash0;

/** Access the Flash HP instance using these structures when calling API functions directly (::p_api is not used). */
extern flash_hp_instance_ctrl_t g_flash0_ctrl;
extern const flash_cfg_t g_flash0_cfg;

#ifndef NULL
void NULL(flash_callback_args_t *p_args);
#endif
FSP_FOOTER
#endif /* DOWNLOADER_THREAD_H_ */
