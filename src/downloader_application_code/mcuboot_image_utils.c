/***********************************************************************************************************************
 * File Name    : mcuboot_image_utils.c
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

#include "comms.h"
#include "mcuboot_image_utils.h"
#include "hal_data.h"
#include "comms.h"
#include "bootloader_image_info.h"

#define MCUBOOT_IMAGE_UTILS_SWAP_TYPE_ESTIMATION         (0u) // 1: Enabled, 0: Disabled

#if (BSP_FEATURE_FLASH_HP_VERSION >= 0)
#define FLASH_CF_WRITE_SIZE BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE
#elif (BSP_FEATURE_FLASH_LP_VERSION >= 0)
#define FLASH_CF_WRITE_SIZE BSP_FEATURE_FLASH_LP_CF_WRITE_SIZE
#else
#error "Unsupported device"
#endif

#define MCUBOOT_DEFAULT_IMAGE_HEADER_MAGIC_NUMBER        (0x96f3b83d)

#if (FLASH_CF_WRITE_SIZE == 8U)
#define MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_UPPER (0x8079b62c0f505235)
#define MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_LOWER (0x7fefd260f395c277)
#else
#define MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_UPPER (0x8a1f0f119c67778d)
#define MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_LOWER (0x0b41295de12d0000)
#endif

struct image_version {
    uint8_t  iv_major;
    uint8_t  iv_minor;
    uint16_t iv_revision;
    uint32_t iv_build_num;
};

typedef struct _image_header {
    uint32_t ih_magic;
    uint32_t ih_load_addr;
    uint16_t ih_hdr_size;           /* Size of image header (bytes). */
    uint16_t ih_protect_tlv_size;   /* Size of protected TLV area (bytes). */
    uint32_t ih_img_size;           /* Does not include header. */
    uint32_t ih_flags;              /* IMAGE_F_[...]. */
    struct image_version ih_ver;
    uint32_t PAD1;
}image_header_t;

typedef struct _image_trailer {
    union
    {
        uint8_t it_swap_info;
        struct
        {
            uint8_t swap_type    : 4;
            uint8_t image_number : 4;
        } it_swap_info_b;
    };
    uint8_t PAD4[BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE-1];
    uint8_t it_copy_done;
    uint8_t PAD3[BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE-1];
    uint8_t it_image_ok;
    uint8_t PAD2[BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE-1];
    uint8_t PAD1[BSP_FEATURE_FLASH_HP_CF_WRITE_SIZE-16];
    uint64_t it_magic_lower;
    uint64_t it_magic_upper;
}image_trailer_t;

static void display_swap_type(mcuboot_swap_type_t type);
#if (MCUBOOT_IMAGE_UTILS_SWAP_TYPE_ESTIMATION == 1u)
static mcuboot_swap_type_t estimate_next_swap_type(mcuboot_image_trailer_t* primary, mcuboot_image_trailer_t* secondary);
#endif

void mcuboot_image_header_read (uint32_t image_start_addr, mcuboot_image_header_t* dest)
{
    image_header_t image_header_data = {0x0};
    image_header_data = *((image_header_t*)image_start_addr);

    dest->ih_magic_number           = image_header_data.ih_magic;
    dest->ih_image_header_size      = image_header_data.ih_hdr_size;
    dest->ih_image_size             = image_header_data.ih_img_size;

    dest->ih_version_major          = image_header_data.ih_ver.iv_major;
    dest->ih_version_minor          = image_header_data.ih_ver.iv_minor;
    dest->ih_version_revision       = image_header_data.ih_ver.iv_revision;
    dest->ih_version_build_number   = image_header_data.ih_ver.iv_build_num;
}

void mcuboot_image_trailer_read (uint32_t image_end_addr, mcuboot_image_trailer_t* dest)
{
    image_trailer_t image_trailer_data = {0x0};
    image_trailer_data = *((image_trailer_t *)((image_end_addr + 1) - sizeof(image_trailer_t)));

    dest->it_magic_number_upper     = image_trailer_data.it_magic_upper;
    dest->it_magic_number_lower     = image_trailer_data.it_magic_lower;
    dest->it_image_ok               = image_trailer_data.it_image_ok;
    dest->it_copy_done              = image_trailer_data.it_copy_done;
    dest->it_swap_info_image_number = image_trailer_data.it_swap_info_b.image_number;
    dest->it_swap_info_swap_type    = (mcuboot_swap_type_t) image_trailer_data.it_swap_info_b.swap_type;
}

uint8_t mcuboot_image_header_magic_number_check(uint32_t num)
{
    if(num != MCUBOOT_DEFAULT_IMAGE_HEADER_MAGIC_NUMBER)
    {
        return MAGIC_NUMBER_CHECK_ANY;
    }
    return MAGIC_NUMBER_CHECK_GOOD;
}

uint8_t mcuboot_image_trailer_magic_number_check(uint64_t num_upper, uint64_t num_lower)
{
    if(num_upper != MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_UPPER)
    {
        return MAGIC_NUMBER_CHECK_ANY;
    }
#if (FLASH_CF_WRITE_SIZE == 8U)
    /* If BOOT_MAX_ALIGN is 8 bytes, then MAGIC contains the 16 bytes value */
    if(num_lower != MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_LOWER)
#else
    /* In case BOOT_MAX_ALIGN is defined to any value different than 8, */
    /* then the maximum supported write alignment value is encoded */
    /* in the MAGIC field, followed by a fixed 14-byte pattern */
    if((num_lower & 0xFFFFFFFFFFFFFF00) != MCUBOOT_DEFAULT_IMAGE_TRAILER_MAGIC_NUMBER_LOWER)
#endif
    {
        return MAGIC_NUMBER_CHECK_ANY;
    }
    return MAGIC_NUMBER_CHECK_GOOD;
}

void display_image_slot_header_info(uint32_t image_slot_start_address)
{
    uint8_t magic_number_check_result  = 0x0;
    mcuboot_image_header_t header_data = {0x0};
    uint8_t print_data[100] = {0x0};

    /* Primary slot */
    if(PRIMARY_IMAGE_START_ADDRESS == image_slot_start_address)
    {
        COMMS_STR_PRINT(print_data, "\r\n--- Primary Slot - Header Info ---\r\n");
    }
    else if(SECONDARY_IMAGE_START_ADDRESS == image_slot_start_address)
    {
        COMMS_STR_PRINT(print_data, "\r\n--- Secondary Slot - Header Info ---\r\n");
    }
    else
    {
        COMMS_STR_PRINT(print_data, "\r\nInvalied input parameter\r\n");
        return;
    }

    /* Read header of image slot */
    mcuboot_image_header_read(image_slot_start_address, &header_data);

    /* Check the magic number */
    magic_number_check_result = mcuboot_image_header_magic_number_check(header_data.ih_magic_number);
    if(magic_number_check_result == 0x1)
    {
        COMMS_STR_PRINT(print_data, "  Image start address: \t0x%08X\r\n", (unsigned int)image_slot_start_address);
        COMMS_STR_PRINT(print_data, "  Image version: \t%02d.%02d (Rev: %d, Build: %ld)\r\n", header_data.ih_version_major, header_data.ih_version_minor, header_data.ih_version_revision, header_data.ih_version_build_number);
        COMMS_STR_PRINT(print_data, "  App Image size: \t0x%08X (%ld bytes)\r\n", (unsigned int)header_data.ih_image_size, header_data.ih_image_size);
        COMMS_STR_PRINT(print_data, "  Header size: \t\t0x%04X (%d bytes)\r\n", (unsigned int)header_data.ih_image_header_size, header_data.ih_image_header_size);
    }
    else
    {
        COMMS_STR_PRINT(print_data, "  Invalid image magic number. No active image found.\r\n");
    }
    return;
}

void display_image_trailer_info(void)
{
    mcuboot_image_trailer_t trailer_data_primary   = {0x0};
    mcuboot_image_trailer_t trailer_data_secondary = {0x0};
    uint8_t print_data[100] = {0x0};

    /* Read trailer in image slot */
    mcuboot_image_trailer_read(PRIMARY_IMAGE_END_ADDRESS, &trailer_data_primary);
    mcuboot_image_trailer_read(SECONDARY_IMAGE_END_ADDRESS, &trailer_data_secondary);

    COMMS_STR_PRINT(print_data, "\r\n--- Image Trailer Info ---\r\n");

    COMMS_STR_PRINT(print_data, "Swap type used in last boot: ");
    display_swap_type(trailer_data_primary.it_swap_info_swap_type);
    COMMS_STR_PRINT(print_data, "\r\n\r\n");

    COMMS_STR_PRINT(print_data, "Current image trailer info: \r\n");

    /* Check the magic number */
    trailer_data_primary.magic_number_check_result = mcuboot_image_trailer_magic_number_check(trailer_data_primary.it_magic_number_upper, trailer_data_primary.it_magic_number_lower);
    trailer_data_secondary.magic_number_check_result = mcuboot_image_trailer_magic_number_check(trailer_data_secondary.it_magic_number_upper, trailer_data_secondary.it_magic_number_lower);

    /* Display trailer info in secondary and primary slot */
    COMMS_STR_PRINT(print_data, " Primary slot   - Magic: \t\t%s\r\n", (trailer_data_primary.magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD) ? "Good" : "Unset or Any");
    COMMS_STR_PRINT(print_data, " Primary slot   - Image_ok byte: \t0x%02X\r\n", trailer_data_primary.it_image_ok);
    COMMS_STR_PRINT(print_data, " Primary slot   - Copy_done byte: \t0x%02X\r\n", trailer_data_primary.it_copy_done);
    COMMS_STR_PRINT(print_data, " Secondary slot - Magic: \t\t%s\r\n", (trailer_data_secondary.magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD) ? "Good" : "Unset or Any");
    COMMS_STR_PRINT(print_data, " Secondary slot - Image_ok byte: \t0x%02X\r\n", trailer_data_secondary.it_image_ok);
    COMMS_STR_PRINT(print_data, " Secondary slot - Copy_done byte: \t0x%02X\r\n", trailer_data_secondary.it_copy_done);
    COMMS_STR_PRINT(print_data, "\r\n");

#if (MCUBOOT_IMAGE_UTILS_SWAP_TYPE_ESTIMATION == 1u)
    /* Determine next swap type from current image trailer info and display it */
    COMMS_STR_PRINT(print_data, "*Estimated swap type for next boot: ");
    mcuboot_swap_type_t estimate_swap_type = estimate_next_swap_type(&trailer_data_primary, &trailer_data_secondary);
    display_swap_type(estimate_swap_type);
    COMMS_STR_PRINT(print_data, "\r\n\r\n");
#endif
}

static void display_swap_type(mcuboot_swap_type_t type)
{
    uint8_t print_data[100] = {0x0};

    switch(type)
    {
        case SWAP_TYPE_NONE:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_NONE");
            break;
        case SWAP_TYPE_TEST:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_TEST");
            break;
        case SWAP_TYPE_PERM:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_PERM");
            break;
        case SWAP_TYPE_REVERT:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_REVERT");
            break;
        case SWAP_TYPE_FAIL:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_FAIL");
            break;
        case SWAP_TYPE_PANIC:
            COMMS_STR_PRINT(print_data, "BOOT_SWAP_TYPE_PANIC (or Swap type not recorded)");
            break;
        case SWAP_TYPE_UNESTIMATED:
            COMMS_STR_PRINT(print_data, "(Not attempt to swap images)");
            break;
        default:
            COMMS_STR_PRINT(print_data, "(Unknown swap type)");
            break;
    }
}

#if (MCUBOOT_IMAGE_UTILS_SWAP_TYPE_ESTIMATION == 1u)
static mcuboot_swap_type_t estimate_next_swap_type(mcuboot_image_trailer_t* primary, mcuboot_image_trailer_t* secondary)
{
    /* Check if State I (BOOT_SWAP_TYPE_TEST)
     *                  | primary slot | secondary slot |
     * -----------------+--------------+----------------|
     *            magic | Any          | Good           |
     *         image-ok | Any          | Unset          |
     *        copy-done | Any          | Any            |
     * -----------------+--------------+----------------'
     *  result: BOOT_SWAP_TYPE_TEST                     |
     * -------------------------------------------------'
     */
    if((secondary->magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD) && (secondary->it_image_ok != 0x1))
    {
        return SWAP_TYPE_TEST;
    }
    /* Check if State II (BOOT_SWAP_TYPE_PERM)
     *                  | primary slot | secondary slot |
     * -----------------+--------------+----------------|
     *            magic | Any          | Good           |
     *         image-ok | Any          | 0x01           |
     *        copy-done | Any          | Any            |
     * -----------------+--------------+----------------'
     *  result: BOOT_SWAP_TYPE_PERM                     |
     * -------------------------------------------------'
     */
    if((secondary->magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD) && (secondary->it_image_ok == 0x1))
    {
        return SWAP_TYPE_PERM;
    }
    /* Check if State III (BOOT_SWAP_TYPE_REVERT)
     *                   | primary slot | secondary slot |
     *  -----------------+--------------+----------------|
     *             magic | Good         | Unset          |
     *          image-ok | 0xff         | Any            |
     *         copy-done | 0x01         | Any            |
     *  -----------------+--------------+----------------'
     *   result: BOOT_SWAP_TYPE_REVERT                   |
     *  -------------------------------------------------'
     */
    if((primary->magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD) && (primary->it_image_ok == 0xff)
            && (primary->it_copy_done == 0x1) && (secondary->magic_number_check_result == MAGIC_NUMBER_CHECK_ANY))
    {
        return SWAP_TYPE_REVERT;
    }
    /* Check if State IV (BOOT_SWAP_TYPE_NONE, FAIL, or PANIC)
     *                  | primary slot | secondary slot |
     * -----------------+--------------+----------------|
     *            magic | Any          | Any            |
     *         image-ok | Any          | Any            |
     *        copy-done | Any          | Any            |
     * -----------------+--------------+----------------'
     *  result: BOOT_SWAP_TYPE_NONE,                    |
     *          BOOT_SWAP_TYPE_FAIL, or                 |
     *          BOOT_SWAP_TYPE_PANIC                    |
     * -------------------------------------------------'
     */
    return SWAP_TYPE_UNESTIMATED;
}
#endif

void display_user_app_image_data(uint32_t image_slot_start_address, uint32_t dump_size)
{
    uint8_t magic_number_check_result  = 0x0;
    mcuboot_image_header_t header_data = {0x0};
    uint8_t print_data[100] = {0x0};
    uint32_t dump_address = 0x0;
    uint32_t header_size = 0x0;

    if(!(((PRIMARY_IMAGE_START_ADDRESS <= image_slot_start_address) && ((image_slot_start_address + dump_size) <= (PRIMARY_IMAGE_START_ADDRESS + FIRMWARE_IMAGE_SIZE)))
            || (((SECONDARY_IMAGE_START_ADDRESS <= image_slot_start_address) && ((image_slot_start_address + dump_size) <= (SECONDARY_IMAGE_START_ADDRESS + FIRMWARE_IMAGE_SIZE))))))
    {
        COMMS_STR_PRINT(print_data, "\r\nInvalied input parameter\r\n");
        return;
    }

    /* Read header of image slot */
    mcuboot_image_header_read(image_slot_start_address, &header_data);

    if(PRIMARY_IMAGE_START_ADDRESS == image_slot_start_address)
    {
        COMMS_STR_PRINT(print_data, "\r\n--- Primary Slot --- UserApp Image Data ---\r\n");
    }
    else if(SECONDARY_IMAGE_START_ADDRESS == image_slot_start_address)
    {
        COMMS_STR_PRINT(print_data, "\r\n--- Secondary Slot - UserApp Image Data ---\r\n");
    }
    else
    {
        /* Do nothing */
    }

    /* Check the magic number */
    magic_number_check_result = mcuboot_image_header_magic_number_check(header_data.ih_magic_number);
    if(magic_number_check_result == MAGIC_NUMBER_CHECK_GOOD)
    {
        header_size = header_data.ih_image_header_size;
    }
    else
    {
        header_size = 0x200; // Please update depends on your configuration.
        COMMS_STR_PRINT(print_data, "Invalid image magic number. No active image found.\r\n");
    }

    COMMS_STR_PRINT(print_data, "Dump area: 0x%08x - 0x%08x",
                        (unsigned int)(image_slot_start_address + header_size),
                        (unsigned int)(image_slot_start_address + header_size + dump_size));

    dump_address = image_slot_start_address + header_size;
    while(1)
    {
        if(dump_address % 32 == 0)
        {
            COMMS_STR_PRINT(print_data, "\r\n0x%08x  ", (unsigned int)dump_address);
        }
        COMMS_STR_PRINT(print_data, "%08x ", *((unsigned int *)dump_address));
        dump_address += 8;

        if(dump_address >= (uint32_t)(image_slot_start_address + header_size + dump_size))
        {
            break;
        }
    }
    COMMS_STR_PRINT(print_data, "\r\n");

    return;
}
