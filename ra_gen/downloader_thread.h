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
FSP_HEADER
FSP_FOOTER
#endif /* DOWNLOADER_THREAD_H_ */
