#include <stdio.h>
#include <ps1card.h>
#include <libmc.h>

#include "utils.h"
#include "saves.h"

// flag value used for mcSetFileInfo at MC file restoration
#define  MC_SFI             0xFEED


static int set_ps1_folder(const char* mc, const char* path)
{
    int val;
    sceMcTblGetDir dinfo;

    mcGetDir(mc[2] - '0', 0, path, 0, 1, &dinfo);
    mcSync(0, NULL, &val);
    LOG("mcGetDir(%s) %d", path, val);

    // set PS1 attributes (PS2 memcard)
    dinfo.AttrFile = 0x9027;

    val = mcSetFileInfo(mc[2] - '0', 0, path, &dinfo, MC_SFI);  // Fix file stats
    LOG("mcSetInfo(%s): %X", path, val);
    val = mcSync(0, NULL, &val);

    return val;
}

static int write_ps1_save(const char* mc_path, const char* fname, const uint8_t* data, size_t dataSize)
{
    char dstName[256];

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, fname);

    switch (check_memcard_type(mc_path))
    {
    case sceMcTypePS1:
        LOG("PS1 MC detected");
        break;

    case sceMcTypePS2:
        LOG("PS2 MC detected");
        mkdir(dstName, 0777);
        set_ps1_folder(mc_path, fname);
        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, fname, fname);
        break;

    default:
        return 0;
    }

    LOG("Saving '%s'...", dstName);
    return (write_buffer(dstName, data, dataSize) == 0);
}

int importPS1psv(const char *save, const char* mc_path, const char* fname)
{
    uint8_t *data;
    size_t dataSize;

    if (read_buffer(save, &data, &dataSize) < 0)
        return 0;

    if ((memcmp(data, "\x00VSP", 4) != 0) || (data[0x3C] != 0x01))
    {
        free(data);
        return 0;
    }

    int ret = write_ps1_save(mc_path, fname, data + 0x84, dataSize - 0x84);
    free(data);

    return ret;
}

int importPS1psv_buffer(const uint8_t *data, size_t dataSize, const char* mc_path)
{
    if ((memcmp(data, "\x00VSP", 4) != 0) || (data[0x3C] != 0x01))
        return 0;

    return write_ps1_save(mc_path, &data[0x64], data + 0x84, dataSize - 0x84);
}

int importPS1mcs(const char *save, const char* mc_path, const char* fname)
{
    uint8_t *data;
    size_t dataSize;
    char dstName[256];

    if (read_buffer(save, &data, &dataSize) < 0)
        return 0;

    if (data[0] != 'Q' || data[0x80] != 'S' || data[0x81] != 'C')
    {
        free(data);
        return 0;
    }

    int ret = write_ps1_save(mc_path, fname, data + PS1CARD_HEADER_SIZE, dataSize - PS1CARD_HEADER_SIZE);
    free(data);

    return ret;
}

int importPS1psx(const char *save, const char* mc_path, const char* fname)
{
    uint8_t *data;
    size_t dataSize;
    char dstName[256];

    if (read_buffer(save, &data, &dataSize) < 0)
        return 0;

    if (data[0x36] != 'S' || data[0x37] != 'C')
    {
        free(data);
        return 0;
    }

    int ret = write_ps1_save(mc_path, fname, data + 0x36, dataSize - 0x36);
    free(data);

    return ret;
}

int exportMCS(const char* path, const char* fname, const char* dstName)
{
    char srcName[256];
    uint8_t mcshdr[128];
    size_t sz;
    uint8_t *input;
    FILE *pf;

    snprintf(srcName, sizeof(srcName), "%s%s", path, fname);
    if(read_buffer(srcName, &input, &sz) < 0)
        return 0;

    pf = fopen(dstName, "wb");
    if (!pf) {
        LOG("Failed to open output file");
        free(input);
        return 0;
    }
    
    memset(mcshdr, 0, sizeof(mcshdr));
    memcpy(mcshdr + 4, &sz, 4);
    memcpy(mcshdr + 10, fname, strlen(fname));
    mcshdr[0] = 0x51;
    mcshdr[8] = 0xFF;
    mcshdr[9] = 0xFF;

    for (int x=0; x<127; x++)
        mcshdr[127] ^= mcshdr[x];

    fwrite(mcshdr, sizeof(mcshdr), 1, pf);
    fwrite(input, sz, 1, pf);
    fclose(pf);
    free(input);

    LOG("MCS generated successfully: %s", dstName);
    return 1;
}
