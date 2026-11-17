#ifndef MCIMAGE_RPC_H
#define MCIMAGE_RPC_H

/* RPC protocol shared by mcimage.irx (IOP) and mc_image.c (EE) */

#define MCIMG_RPC_ID            0x41504D43  // 'APMC'

#define MCIMG_CMD_GET_SPEC      1   // in: port            out: spec
#define MCIMG_CMD_READ          2   // in: port, page, count  out: count pages (PS2) / frames (PS1)
#define MCIMG_CMD_WRITE_BLOCK   3   // in: port, page=block, 16 pages  (PS2: erase + write w/ ECC)
#define MCIMG_CMD_WRITE_PS1     4   // in: port, page=frame, count, frames

#define MCIMG_PAGE_SIZE         512
#define MCIMG_MAX_PAGES         16              // one PS2 erase block per call
#define MCIMG_PS1_FRAME         128
#define MCIMG_PS1_FRAMES        1024
#define MCIMG_MAX_PS1_FRAMES    (MCIMG_MAX_PAGES * MCIMG_PAGE_SIZE / MCIMG_PS1_FRAME)

typedef struct
{
	int result;
	int port;
	unsigned int page;
	unsigned int count;
	unsigned int ecc_errors;  // out (READ): PS2 pages whose ECC could not correct them
} mcimg_rpc_hdr_t;

typedef struct
{
	mcimg_rpc_hdr_t hdr;
	union
	{
		struct
		{
			int type;
			int spec_result;    // McGetCardSpec() result (PS2 only)
			int pagesize;
			int blocksize;
			int cardsize;
			int flags;
		} spec;
		unsigned char data[MCIMG_MAX_PAGES * MCIMG_PAGE_SIZE];
	};
} mcimg_rpc_buf_t;

#endif
