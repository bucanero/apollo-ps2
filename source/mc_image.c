/*
 * Full memory card image dump/restore.
 *
 * Raw page access goes through mcimage.irx (iop/mcimage), a small IOP module
 * that calls the XMCMAN exports directly, loaded on first use. Neither libmc
 * nor MCSERV can be used for this with the homebrew modules SDL2's startup
 * loads: libmc refuses mcReadPage/mcWritePage/mcEraseBlock once it picks the
 * XMCSERV protocol, and MCSERV's own raw page RPCs hang the IOP (XMCSERV
 * imports MCMAN export #17 with the old McEraseBlock() signature).
 *
 * Approach inspired by Memory Card Annihilator by ffgriever (BSD-2-Clause).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <kernel.h>
#include <sifrpc.h>
#include <loadfile.h>
#include <libmc.h>

#include "mcio.h"
#include "saves.h"
#include "mc_image.h"
#include "mcimage_rpc.h"

#define MC_PAGE_SIZE        MCIMG_PAGE_SIZE
#define MC_SPARE_SIZE       16
#define MC_BLOCK_PAGES      MCIMG_MAX_PAGES
#define MC_BLOCK_SIZE       (MC_BLOCK_PAGES * MC_PAGE_SIZE)
#define MC_MAX_PAGES        (128 * 1024 * 1024 / MC_PAGE_SIZE)
#define PS1_FRAME_SIZE      MCIMG_PS1_FRAME
#define PS1_FRAMES          MCIMG_PS1_FRAMES
#define MC_SB_MAGIC         "Sony PS2 Memory Card Format "

extern unsigned char _binary_mcimage_irx_start[];
extern unsigned char _binary_mcimage_irx_size[];

typedef struct
{
	int type;
	uint32_t total_pages;
} mc_card_t;

static SifRpcClientData_t mcimg_cd __attribute__((aligned(64)));
static mcimg_rpc_buf_t mcimg_buf __attribute__((aligned(64)));
static uint8_t block_buf[MC_BLOCK_SIZE] __attribute__((aligned(64)));
static uint8_t spare_buf[MC_SPARE_SIZE];
static int mcimg_id = -1;

static int mcimg_init(void)
{
	int ret;

	if (mcimg_cd.server)
		return 0;

	if (mcimg_id < 0)
	{
		// The embedded blob has no alignment guarantee, and the module is
		// DMA'd to the IOP: load it from an aligned copy
		int size = (int) _binary_mcimage_irx_size;
		void* irx = memalign(64, size);
		if (!irx)
			return -1;

		memcpy(irx, _binary_mcimage_irx_start, size);
		int id = SifExecModuleBuffer(irx, size, 0, NULL, &ret);
		free(irx);

		LOG("mcimage.irx: id=%d ret=%d", id, ret);
		if (id < 0 || ret == 1)  // 1 = MODULE_NO_RESIDENT_END
			return -1;

		mcimg_id = id;
	}

	memset(&mcimg_cd, 0, sizeof(mcimg_cd));
	for (int i = 0; i < 100; i++)
	{
		if (SifBindRpc(&mcimg_cd, MCIMG_RPC_ID, 0) < 0)
			return -1;

		if (mcimg_cd.server)
			return 0;

		nopdelay();
	}

	return -1;
}

// Runs one command; data travels in mcimg_buf (send_len/recv_len payload bytes)
static int mcimg_call(int cmd, int port, uint32_t page, uint32_t count, int send_len, int recv_len)
{
	if (mcimg_init() < 0)
		return -1;

	mcimg_buf.hdr.result = -1;
	mcimg_buf.hdr.port = port;
	mcimg_buf.hdr.page = page;
	mcimg_buf.hdr.count = count;

	if (SifCallRpc(&mcimg_cd, cmd, 0, &mcimg_buf, sizeof(mcimg_rpc_hdr_t) + send_len,
			&mcimg_buf, sizeof(mcimg_rpc_hdr_t) + recv_len, NULL, NULL) < 0)
		return -1;

	return mcimg_buf.hdr.result;
}

static int read_card(int port, uint32_t first, uint32_t count, int unit, uint8_t* dst, uint32_t* ecc_errors);
static int parse_superblock(const uint8_t* sb, uint32_t* total_pages);

static int get_card(int port, mc_card_t* card)
{
	int val, ret;

	// Refresh MCMAN's card detection the normal way (through MCSERV). The
	// first query after a card is (re)detected may only report the change.
	for (int i = 0; i < 2; i++)
	{
		if (mcGetInfo(port, 0, &val, &val, &val) == 0)
			mcSync(0, NULL, &ret);
	}

	if (mcimg_call(MCIMG_CMD_GET_SPEC, port, 0, 0, 0, sizeof(mcimg_buf.spec)) < 0)
		return MC_IMG_ERR_NOCARD;

	card->type = mcimg_buf.spec.type;
	if (card->type == sceMcTypePS1 || card->type == sceMcTypePDA)
	{
		card->total_pages = PS1_FRAMES;
		return MC_IMG_OK;
	}

	if (card->type != sceMcTypePS2)
		return MC_IMG_ERR_NOCARD;

	if (mcimg_buf.spec.spec_result == 0)
	{
		// Only 512 byte pages / 16 page blocks (all Sony and common 3rd party cards)
		if (mcimg_buf.spec.pagesize != MC_PAGE_SIZE || mcimg_buf.spec.blocksize != MC_BLOCK_PAGES ||
			mcimg_buf.spec.cardsize <= 0 || mcimg_buf.spec.cardsize > MC_MAX_PAGES ||
			(mcimg_buf.spec.cardsize % MC_BLOCK_PAGES) != 0)
			return MC_IMG_ERR_UNSUPPORTED;

		card->total_pages = mcimg_buf.spec.cardsize;
		return MC_IMG_OK;
	}

	// The card didn't answer the spec query: take the size from its superblock
	LOG("mc%d: McGetCardSpec failed (%d), using the superblock", port, mcimg_buf.spec.spec_result);
	if (read_card(port, 0, 1, MC_PAGE_SIZE, block_buf, NULL) != MC_IMG_OK)
		return MC_IMG_ERR_READ;

	ret = parse_superblock(block_buf, &card->total_pages);
	return (ret == MC_IMG_ERR_BADIMAGE) ? MC_IMG_ERR_UNFORMATTED : ret;
}

// Reads count pages (PS2) or frames (PS1) into dst; adds PS2 pages with
// uncorrectable ECC errors (returned as read) to *ecc_errors
static int read_card(int port, uint32_t first, uint32_t count, int unit, uint8_t* dst, uint32_t* ecc_errors)
{
	int r = mcimg_call(MCIMG_CMD_READ, port, first, count, 0, count * unit);

	if (r != 0)
	{
		LOG("mc%d: read error %d at %u", port, r, first);
		return MC_IMG_ERR_READ;
	}

	if (mcimg_buf.hdr.ecc_errors)
	{
		LOG("mc%d: %u page(s) with ECC errors at %u", port, mcimg_buf.hdr.ecc_errors, first);
		if (ecc_errors)
			*ecc_errors += mcimg_buf.hdr.ecc_errors;
	}

	memcpy(dst, mcimg_buf.data, count * unit);
	return MC_IMG_OK;
}

/* An image's own size comes from its superblock (page 0) */
static int parse_superblock(const uint8_t* sb, uint32_t* total_pages)
{
	uint16_t page_len, pages_per_cluster, pages_per_block;
	uint32_t clusters;

	if (memcmp(sb, MC_SB_MAGIC, strlen(MC_SB_MAGIC)) != 0)
		return MC_IMG_ERR_BADIMAGE;

	memcpy(&page_len, sb + 0x28, sizeof(uint16_t));
	memcpy(&pages_per_cluster, sb + 0x2A, sizeof(uint16_t));
	memcpy(&pages_per_block, sb + 0x2C, sizeof(uint16_t));
	memcpy(&clusters, sb + 0x30, sizeof(uint32_t));

	if (page_len != MC_PAGE_SIZE || pages_per_block != MC_BLOCK_PAGES || pages_per_cluster == 0)
		return MC_IMG_ERR_UNSUPPORTED;

	*total_pages = clusters * pages_per_cluster;
	if (*total_pages == 0 || *total_pages > MC_MAX_PAGES || (*total_pages % MC_BLOCK_PAGES) != 0)
		return MC_IMG_ERR_UNSUPPORTED;

	return MC_IMG_OK;
}

int mc_dump_ps2_image(int port, const char* path, int ecc, uint32_t* ecc_errors)
{
	FILE* fp;
	mc_card_t card;
	uint32_t block, i;
	int ret;

	if (ecc_errors)
		*ecc_errors = 0;

	if ((ret = get_card(port, &card)) != MC_IMG_OK)
		return ret;

	if (card.type != sceMcTypePS2)
		return MC_IMG_ERR_NOCARD;

	LOG("Dumping mc%d: (%u pages, ecc=%d) to %s", port, card.total_pages, ecc, path);
	if ((fp = fopen(path, "wb")) == NULL)
		return MC_IMG_ERR_FILE;

	init_progress_bar("Dumping Memory Card...");
	for (block = 0; block < card.total_pages / MC_BLOCK_PAGES; block++)
	{
		if ((ret = read_card(port, block * MC_BLOCK_PAGES, MC_BLOCK_PAGES, MC_PAGE_SIZE, block_buf, ecc_errors)) != MC_IMG_OK)
			break;

		for (i = 0; i < MC_BLOCK_PAGES && ret == MC_IMG_OK; i++)
		{
			uint8_t* page = block_buf + i * MC_PAGE_SIZE;

			if (fwrite(page, 1, MC_PAGE_SIZE, fp) != MC_PAGE_SIZE)
				ret = MC_IMG_ERR_FILE;

			if (ecc && ret == MC_IMG_OK)
			{
				mcio_calcPageEcc(page, spare_buf);
				if (fwrite(spare_buf, 1, MC_SPARE_SIZE, fp) != MC_SPARE_SIZE)
					ret = MC_IMG_ERR_FILE;
			}
		}
		if (ret != MC_IMG_OK)
			break;

		update_progress_bar(block, card.total_pages / MC_BLOCK_PAGES, "");
	}
	end_progress_bar();

	if (fclose(fp) != 0 && ret == MC_IMG_OK)
		ret = MC_IMG_ERR_FILE;

	if (ret != MC_IMG_OK)
		remove(path);

	return ret;
}

int mc_dump_ps1_image(int port, const char* path)
{
	FILE* fp;
	mc_card_t card;
	const uint32_t step = MCIMG_MAX_PS1_FRAMES;
	int ret;

	if ((ret = get_card(port, &card)) != MC_IMG_OK)
		return ret;

	// PocketStation (PDA) cards use the same 128 KB PS1 card format
	if (card.type != sceMcTypePS1 && card.type != sceMcTypePDA)
		return MC_IMG_ERR_NOCARD;

	LOG("Dumping PS1 mc%d: to %s", port, path);
	if ((fp = fopen(path, "wb")) == NULL)
		return MC_IMG_ERR_FILE;

	init_progress_bar("Dumping PS1 Memory Card...");
	for (uint32_t frame = 0; frame < PS1_FRAMES; frame += step)
	{
		if ((ret = read_card(port, frame, step, PS1_FRAME_SIZE, block_buf, NULL)) != MC_IMG_OK)
			break;

		/*
		 * McReadPS1PDACard() remaps frames listed in MCMAN's copy of the card's
		 * broken sector table. If MCMAN never loaded that table (it does so when
		 * it detects a formatted PS1 card) it is all zeros, and frame 0 comes
		 * back as replacement frame 36. A formatted card always starts with "MC",
		 * so refuse rather than write an image with a wrong header frame.
		 */
		if (frame == 0 && (block_buf[0] != 'M' || block_buf[1] != 'C'))
		{
			LOG("PS1 mc%d: frame 0 is not an 'MC' header", port);
			ret = MC_IMG_ERR_UNFORMATTED;
			break;
		}

		if (fwrite(block_buf, 1, step * PS1_FRAME_SIZE, fp) != step * PS1_FRAME_SIZE)
		{
			ret = MC_IMG_ERR_FILE;
			break;
		}

		update_progress_bar(frame, PS1_FRAMES, "");
	}
	end_progress_bar();

	if (fclose(fp) != 0 && ret == MC_IMG_OK)
		ret = MC_IMG_ERR_FILE;

	if (ret != MC_IMG_OK)
		remove(path);

	return ret;
}

int mc_check_ps2_restore(int port, const char* path, mc_image_info_t* info)
{
	FILE* fp;
	long size;
	mc_card_t card;
	uint32_t image_pages;
	int ret;

	if ((ret = get_card(port, &card)) != MC_IMG_OK)
		return ret;

	if (card.type != sceMcTypePS2)
		return MC_IMG_ERR_NOCARD;

	if ((fp = fopen(path, "rb")) == NULL)
		return MC_IMG_ERR_FILE;

	fseek(fp, 0, SEEK_END);
	size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (size <= 0 || fread(block_buf, 1, MC_PAGE_SIZE, fp) != MC_PAGE_SIZE)
	{
		fclose(fp);
		return MC_IMG_ERR_FILE;
	}
	fclose(fp);

	if ((ret = parse_superblock(block_buf, &image_pages)) != MC_IMG_OK)
		return ret;

	if (image_pages != card.total_pages)
		return MC_IMG_ERR_SIZE;

	if ((uint64_t) size == (uint64_t) card.total_pages * MC_PAGE_SIZE)
		info->ecc = 0;
	else if ((uint64_t) size == (uint64_t) card.total_pages * (MC_PAGE_SIZE + MC_SPARE_SIZE))
		info->ecc = 1;
	else
		return MC_IMG_ERR_SIZE;

	info->total_pages = card.total_pages;
	return MC_IMG_OK;
}

/*
 * MCMAN caches directory and FAT clusters (and the superblock data) and keeps
 * them while it believes the same card is inserted, so after the card is
 * rewritten underneath it that cache is stale - and any later write through
 * it would corrupt the restored card. Reloading MCMAN isn't possible (the
 * retail IOP can't unload modules), but McUnformat() drops the cache and marks
 * the card unformatted, which makes the next card detection (any mcGetInfo)
 * run a full re-detect that re-reads the restored superblock.
 */
static int mc_invalidate_card(int port)
{
	int ret;

	if (mcUnformat(port, 0) != 0)
		return -1;

	mcSync(0, NULL, &ret);
	return ret;
}

int mc_restore_ps2_image(int port, const char* path)
{
	FILE* fp;
	mc_image_info_t info;
	uint32_t block, i;
	int ret;

	// Validate everything before the first erase
	if ((ret = mc_check_ps2_restore(port, path, &info)) != MC_IMG_OK)
		return ret;

	if ((fp = fopen(path, "rb")) == NULL)
		return MC_IMG_ERR_FILE;

	LOG("Restoring %s (%u pages, ecc=%d) to mc%d:", path, info.total_pages, info.ecc, port);
	init_progress_bar("Erasing Memory Card...");
	ret = mc_invalidate_card(port);
	end_progress_bar();
	if (ret < 0)
	{
		LOG("mc%d: unformat failed (%d)", port, ret);
		fclose(fp);
		return MC_IMG_ERR_ERASE;
	}

	ret = MC_IMG_OK;
	init_progress_bar("Writing Memory Card image...");
	for (block = 0; block < info.total_pages / MC_BLOCK_PAGES; block++)
	{
		for (i = 0; i < MC_BLOCK_PAGES; i++)
		{
			if (fread(block_buf + i * MC_PAGE_SIZE, 1, MC_PAGE_SIZE, fp) != MC_PAGE_SIZE ||
				(info.ecc && fseek(fp, MC_SPARE_SIZE, SEEK_CUR) != 0))
			{
				ret = MC_IMG_ERR_FILE;
				break;
			}
		}
		if (ret != MC_IMG_OK)
			break;

		// erase the block, then write its 16 pages (ECC computed on the IOP)
		memcpy(mcimg_buf.data, block_buf, MC_BLOCK_SIZE);
		if (mcimg_call(MCIMG_CMD_WRITE_BLOCK, port, block, MC_BLOCK_PAGES, MC_BLOCK_SIZE, 0) != 0)
		{
			ret = MC_IMG_ERR_WRITE;
			break;
		}

		// read it back: catches bad blocks and silent write failures
		if (mcimg_call(MCIMG_CMD_READ, port, block * MC_BLOCK_PAGES, MC_BLOCK_PAGES, 0, MC_BLOCK_SIZE) != 0 ||
			mcimg_buf.hdr.ecc_errors != 0 || memcmp(mcimg_buf.data, block_buf, MC_BLOCK_SIZE) != 0)
		{
			LOG("Verify failed at block %u", block);
			ret = MC_IMG_ERR_VERIFY;
			break;
		}

		update_progress_bar(block, info.total_pages / MC_BLOCK_PAGES, "");
	}
	end_progress_bar();
	fclose(fp);

	return ret;
}

const char* mc_image_error(int err)
{
	switch (err)
	{
	case MC_IMG_OK:              return "OK";
	case MC_IMG_ERR_NOCARD:      return "No Memory Card of the right type in the slot";
	case MC_IMG_ERR_UNFORMATTED: return "Memory Card is not formatted (or unreadable)";
	case MC_IMG_ERR_UNSUPPORTED: return "Unsupported Memory Card geometry";
	case MC_IMG_ERR_FILE:        return "Cannot read or write the image file";
	case MC_IMG_ERR_BADIMAGE:    return "File is not a formatted PS2 Memory Card image";
	case MC_IMG_ERR_SIZE:        return "Image size does not match the Memory Card";
	case MC_IMG_ERR_READ:        return "Memory Card read error";
	case MC_IMG_ERR_ERASE:       return "Memory Card erase error";
	case MC_IMG_ERR_WRITE:       return "Memory Card write error";
	case MC_IMG_ERR_VERIFY:      return "Verification failed (bad block?)";
	default:                     return "Unknown error";
	}
}
