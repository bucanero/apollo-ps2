#ifndef MC_IMAGE_H
#define MC_IMAGE_H

#include <stdint.h>

/* Full memory card image dump/restore, through mcimage.irx (iop/mcimage) */

enum mc_image_result
{
    MC_IMG_OK = 0,
    MC_IMG_ERR_NOCARD = -1,      // no card, or wrong card type in the slot
    MC_IMG_ERR_UNFORMATTED = -2, // card has no valid PS2 superblock (size unknown)
    MC_IMG_ERR_UNSUPPORTED = -3, // page/block geometry this code cannot handle
    MC_IMG_ERR_FILE = -4,        // cannot open/read/write the image file
    MC_IMG_ERR_BADIMAGE = -5,    // image is not a PS2 memory card image
    MC_IMG_ERR_SIZE = -6,        // image and card sizes differ
    MC_IMG_ERR_READ = -7,        // card read failed
    MC_IMG_ERR_ERASE = -8,       // card block erase failed
    MC_IMG_ERR_WRITE = -9,       // card page write failed
    MC_IMG_ERR_VERIFY = -10,     // page read back differs from what was written
};

typedef struct
{
    uint32_t total_pages;   // pages on the card (512 bytes each)
    int ecc;                // image carries 16 byte spare (ECC) per page
} mc_image_info_t;

// Dump a PS2 card (port 0/1) to an image file, raw (.VMC) or with ECC (.PS2).
// Pages with ECC errors are dumped as read; `ecc_errors` (optional) gets how
// many there were. A .PS2 image gets freshly computed ECC for every page.
int mc_dump_ps2_image(int port, const char* path, int ecc, uint32_t* ecc_errors);

// Dump a PS1 card (port 0/1) to a raw 128 KB .MCR image
int mc_dump_ps1_image(int port, const char* path);

// Check that an image can be written to the PS2 card in port. Touches nothing.
int mc_check_ps2_restore(int port, const char* path, mc_image_info_t* info);

// Write an image to the PS2 card in port (erase + write + verify every block)
int mc_restore_ps2_image(int port, const char* path);

const char* mc_image_error(int err);

#endif
