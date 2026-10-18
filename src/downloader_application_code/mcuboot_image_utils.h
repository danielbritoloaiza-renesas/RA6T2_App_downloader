/***********************************************************************************************************************
 * File Name    : mcuboot_image_utils.h
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

#ifndef __MCUBOOT_IMAGE_UTILS_H__
#define __MCUBOOT_IMAGE_UTILS_H__

#include "hal_data.h"

#define MAGIC_NUMBER_CHECK_GOOD     (0x1)
#define MAGIC_NUMBER_CHECK_ANY      (0x0)

typedef struct _mcuboot_image_header {
    uint32_t ih_magic_number;         /* Image header mugic number */
    uint16_t ih_image_header_size;    /* Size of image header (bytes). */
    uint32_t ih_image_size;           /* Size of image size. Does not include header. */
    uint8_t  ih_version_major;        /* Image version (Major) */
    uint8_t  ih_version_minor;        /* Image version (Minor) */
    uint16_t ih_version_revision;     /* Image version (Revision) */
    uint32_t ih_version_build_number; /* Image version (Build number) */
}mcuboot_image_header_t;

typedef enum _mcuboot_swap_type
{
    SWAP_TYPE_NONE    = 0x1,
    SWAP_TYPE_TEST    = 0x2,
    SWAP_TYPE_PERM    = 0x3,
    SWAP_TYPE_REVERT  = 0x4,
    SWAP_TYPE_FAIL    = 0x5,
    SWAP_TYPE_PANIC   = 0xF,
    SWAP_TYPE_UNESTIMATED = 0xE, /* Extra type definition for this application */
} mcuboot_swap_type_t;

typedef struct _mcuboot_image_trailer {
    uint64_t it_magic_number_upper;     /* Image trailer magic number (upper). Includes BOOT_MAX_ALIGN value if BOOT_MAX_ALIEN != 8 byte. */
    uint64_t it_magic_number_lower;     /* Image trailer magic number (lower) */
    uint8_t  magic_number_check_result; /* Magic number check result. 0x1: Bad, 0x2:Good */
    uint8_t  it_image_ok;               /* Image OK identify byte */
    uint8_t  it_copy_done;              /* Copy Done identify byte */
    uint8_t  it_swap_info_image_number; /* Image number info in Swap info area */
    mcuboot_swap_type_t it_swap_info_swap_type; /* Swap type info in Swap info area */
}mcuboot_image_trailer_t;

void mcuboot_image_header_read(uint32_t image_start_addr, mcuboot_image_header_t* dest);
void mcuboot_image_trailer_read (uint32_t image_end_addr, mcuboot_image_trailer_t* dest);

uint8_t mcuboot_image_header_magic_number_check(uint32_t num);
uint8_t mcuboot_image_trailer_magic_number_check(uint64_t num_upper, uint64_t num_lower);

void display_image_slot_header_info(uint32_t image_slot_start_address);
void display_image_trailer_info(void);
void display_user_app_image_data(uint32_t image_slot_start_address, uint32_t dump_size);

#endif /* __MCUBOOT_IMAGE_UTILS_H__ */
