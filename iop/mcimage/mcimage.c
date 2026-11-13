/*
 * mcimage.irx - raw memory card page access for Apollo's full card dump/restore.
 *
 * Calls the XMCMAN exports directly (as Memory Card Annihilator by ffgriever
 * does, BSD-2-Clause) instead of MCSERV's raw page RPCs: in the XMCSERV build
 * those import export #17 with the old McEraseBlock() signature, while XMCMAN
 * exports McEraseBlock2() there, and the page RPCs hang the IOP.
 *
 * Page data travels inside the RPC send/receive buffers, up to one 16 page
 * block per call.
 */

#include "irx_imports.h"
#include "../../include/mcimage_rpc.h"

#define MODNAME "mcimage"
IRX_ID(MODNAME, 1, 0);

// Same port numbering as XMCSERV (and libmc) use for MCMAN, so this module
// shares their per-port card state instead of keeping a second, competing one
#define MCMAN_PORT(p)   ((p) & 1)
#define MCMAN_SLOT      0

static mcimg_rpc_buf_t rpc_buf __attribute__((aligned(64)));
static SifRpcDataQueue_t rpc_qd;
static SifRpcServerData_t rpc_sd;
static int rpc_tid = -1;

static void get_spec(mcimg_rpc_buf_t *b)
{
	int port = MCMAN_PORT(b->hdr.port);
	s16 pagesize = 0;
	u16 blocksize = 0;
	int cardsize = 0;
	u8 flags = 0;

	// The EE refreshes detection beforehand through libmc's mcGetInfo()
	// (XMCSERV runs McDetectCard2 for it); the card type is read from that.
	b->spec.type = McGetMcType(port, MCMAN_SLOT);

	if (b->spec.type == sceMcTypePS2)
	{
		// The card type is known even if the spec query fails: report that
		// separately, so the EE can fall back to the card's superblock
		b->hdr.result = 0;
		b->spec.spec_result = McGetCardSpec(port, MCMAN_SLOT, &pagesize, &blocksize, &cardsize, &flags);
	}
	else if (b->spec.type == sceMcTypePS1 || b->spec.type == sceMcTypePDA)
	{
		pagesize = MCIMG_PS1_FRAME;
		blocksize = 64;
		cardsize = MCIMG_PS1_FRAMES;
		b->hdr.result = 0;
	}
	else
		b->hdr.result = -1;

	b->spec.pagesize = pagesize;
	b->spec.blocksize = blocksize;
	b->spec.cardsize = cardsize;
	b->spec.flags = flags;
}

static void read_pages(mcimg_rpc_buf_t *b)
{
	int port = MCMAN_PORT(b->hdr.port);
	int type = McGetMcType(port, MCMAN_SLOT);
	int unit = (type == sceMcTypePS2) ? MCIMG_PAGE_SIZE : MCIMG_PS1_FRAME;
	u32 i;

	// both limits fill the same 8 KB buffer: 16 PS2 pages or 64 PS1 frames
	b->hdr.result = 0;
	if (b->hdr.count > ((type == sceMcTypePS2) ? MCIMG_MAX_PAGES : MCIMG_MAX_PS1_FRAMES))
	{
		b->hdr.result = -1;
		return;
	}

	for (i = 0; i < b->hdr.count && b->hdr.result == 0; i++)
	{
		void *dst = &b->data[i * unit];

		if (type == sceMcTypePS2)
			b->hdr.result = McReadPage(port, MCMAN_SLOT, b->hdr.page + i, dst);
		else
			b->hdr.result = McReadPS1PDACard(port, MCMAN_SLOT, b->hdr.page + i, dst);
	}
}

static void write_block(mcimg_rpc_buf_t *b)
{
	int port = MCMAN_PORT(b->hdr.port);
	u8 ecc[16];
	u32 i, j;

	if (McGetMcType(port, MCMAN_SLOT) != sceMcTypePS2)
	{
		b->hdr.result = -1;
		return;
	}

	b->hdr.result = McEraseBlock2(port, MCMAN_SLOT, b->hdr.page, NULL, NULL);

	for (i = 0; i < MCIMG_MAX_PAGES && b->hdr.result == 0; i++)
	{
		u8 *page = &b->data[i * MCIMG_PAGE_SIZE];

		memset(ecc, 0, sizeof(ecc));
		for (j = 0; j < 4; j++)
			McDataChecksum(page + j * 128, ecc + j * 3);

		b->hdr.result = McWritePage(port, MCMAN_SLOT, b->hdr.page * MCIMG_MAX_PAGES + i, page, ecc);
	}
}

static void write_ps1(mcimg_rpc_buf_t *b)
{
	int port = MCMAN_PORT(b->hdr.port);
	int type = McGetMcType(port, MCMAN_SLOT);
	u32 i;

	if ((type != sceMcTypePS1 && type != sceMcTypePDA) || b->hdr.count > MCIMG_MAX_PS1_FRAMES)
	{
		b->hdr.result = -1;
		return;
	}

	b->hdr.result = 0;
	for (i = 0; i < b->hdr.count && b->hdr.result == 0; i++)
		b->hdr.result = McWritePS1PDACard(port, MCMAN_SLOT, b->hdr.page + i, &b->data[i * MCIMG_PS1_FRAME]);
}

static void *rpc_handler(int fno, void *data, int size)
{
	mcimg_rpc_buf_t *b = (mcimg_rpc_buf_t *)data;
	(void)size;

	switch (fno)
	{
	case MCIMG_CMD_GET_SPEC:
		get_spec(b);
		break;
	case MCIMG_CMD_READ:
		read_pages(b);
		break;
	case MCIMG_CMD_WRITE_BLOCK:
		write_block(b);
		break;
	case MCIMG_CMD_WRITE_PS1:
		write_ps1(b);
		break;
	default:
		b->hdr.result = -1;
		break;
	}

	return data;
}

static void rpc_thread(void *arg)
{
	(void)arg;

	sceSifInitRpc(0);
	sceSifSetRpcQueue(&rpc_qd, GetThreadId());
	sceSifRegisterRpc(&rpc_sd, MCIMG_RPC_ID, (void *)rpc_handler, &rpc_buf, NULL, NULL, &rpc_qd);
	sceSifRpcLoop(&rpc_qd);
}

int _start(int argc, char *argv[])
{
	iop_thread_t thread;

	(void)argc;
	(void)argv;

	thread.attr = TH_C;
	thread.thread = rpc_thread;
	thread.priority = 40;
	thread.stacksize = 0x1000;
	thread.option = 0;

	if ((rpc_tid = CreateThread(&thread)) <= 0)
		return MODULE_NO_RESIDENT_END;

	StartThread(rpc_tid, NULL);
	return MODULE_RESIDENT_END;
}
