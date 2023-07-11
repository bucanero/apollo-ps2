/*
* original .MAX, .CBS, .PSU file decoding from Cheat Device PS2 by root670
* https://github.com/root670/CheatDevicePS2
*/

#include <stdio.h>
#include <sys/time.h>
#include <zlib.h>
#include <libmc.h>
#include <polarssl/arc4.h>

#include "utils.h"
#include "lzari.h"
#include "ps2mc.h"

#define  MAX_HEADER_MAGIC   "Ps2PowerSave"
#define  CBS_HEADER_MAGIC   "CFU\0"
#define  XPS_HEADER_MAGIC   "SharkPortSave\0\0\0"

#define  xps_mode_swap(M)   ((M & 0x00FF) << 8) + ((M & 0xFF00) >> 8)

// This is the initial permutation state ("S") for the RC4 stream cipher
// algorithm used to encrypt and decrypt Codebreaker saves.
// Source: https://github.com/ps2dev/mymc/blob/master/ps2save.py#L36
const uint8_t cbsKey[256] = {
    0x5f, 0x1f, 0x85, 0x6f, 0x31, 0xaa, 0x3b, 0x18,
    0x21, 0xb9, 0xce, 0x1c, 0x07, 0x4c, 0x9c, 0xb4,
    0x81, 0xb8, 0xef, 0x98, 0x59, 0xae, 0xf9, 0x26,
    0xe3, 0x80, 0xa3, 0x29, 0x2d, 0x73, 0x51, 0x62,
    0x7c, 0x64, 0x46, 0xf4, 0x34, 0x1a, 0xf6, 0xe1,
    0xba, 0x3a, 0x0d, 0x82, 0x79, 0x0a, 0x5c, 0x16,
    0x71, 0x49, 0x8e, 0xac, 0x8c, 0x9f, 0x35, 0x19,
    0x45, 0x94, 0x3f, 0x56, 0x0c, 0x91, 0x00, 0x0b,
    0xd7, 0xb0, 0xdd, 0x39, 0x66, 0xa1, 0x76, 0x52,
    0x13, 0x57, 0xf3, 0xbb, 0x4e, 0xe5, 0xdc, 0xf0,
    0x65, 0x84, 0xb2, 0xd6, 0xdf, 0x15, 0x3c, 0x63,
    0x1d, 0x89, 0x14, 0xbd, 0xd2, 0x36, 0xfe, 0xb1,
    0xca, 0x8b, 0xa4, 0xc6, 0x9e, 0x67, 0x47, 0x37,
    0x42, 0x6d, 0x6a, 0x03, 0x92, 0x70, 0x05, 0x7d,
    0x96, 0x2f, 0x40, 0x90, 0xc4, 0xf1, 0x3e, 0x3d,
    0x01, 0xf7, 0x68, 0x1e, 0xc3, 0xfc, 0x72, 0xb5,
    0x54, 0xcf, 0xe7, 0x41, 0xe4, 0x4d, 0x83, 0x55,
    0x12, 0x22, 0x09, 0x78, 0xfa, 0xde, 0xa7, 0x06,
    0x08, 0x23, 0xbf, 0x0f, 0xcc, 0xc1, 0x97, 0x61,
    0xc5, 0x4a, 0xe6, 0xa0, 0x11, 0xc2, 0xea, 0x74,
    0x02, 0x87, 0xd5, 0xd1, 0x9d, 0xb7, 0x7e, 0x38,
    0x60, 0x53, 0x95, 0x8d, 0x25, 0x77, 0x10, 0x5e,
    0x9b, 0x7f, 0xd8, 0x6e, 0xda, 0xa2, 0x2e, 0x20,
    0x4f, 0xcd, 0x8f, 0xcb, 0xbe, 0x5a, 0xe0, 0xed,
    0x2c, 0x9a, 0xd4, 0xe2, 0xaf, 0xd0, 0xa9, 0xe8,
    0xad, 0x7a, 0xbc, 0xa8, 0xf2, 0xee, 0xeb, 0xf5,
    0xa6, 0x99, 0x28, 0x24, 0x6c, 0x2b, 0x75, 0x5d,
    0xf8, 0xd3, 0x86, 0x17, 0xfb, 0xc0, 0x7b, 0xb3,
    0x58, 0xdb, 0xc7, 0x4b, 0xff, 0x04, 0x50, 0xe9,
    0x88, 0x69, 0xc9, 0x2a, 0xab, 0xfd, 0x5b, 0x1b,
    0x8a, 0xd9, 0xec, 0x27, 0x44, 0x0e, 0x33, 0xc8,
    0x6b, 0x93, 0x32, 0x48, 0xb6, 0x30, 0x43, 0xa5
}; 

int psv_resign(const char *src_psv);
void write_psvheader(FILE *fp, uint32_t type);
void get_psv_filename(char* psvName, const char* path, const char* dirName);

static void printMAXHeader(const maxHeader_t *header)
{
    if(!header)
        return;

    LOG("Magic            : %.*s", (int)sizeof(header->magic), header->magic);
    LOG("CRC              : %08X", header->crc);
    LOG("dirName          : %.*s", (int)sizeof(header->dirName), header->dirName);
    LOG("iconSysName      : %.*s", (int)sizeof(header->iconSysName), header->iconSysName);
    LOG("compressedSize   : %u", header->compressedSize);
    LOG("numFiles         : %u", header->numFiles);
    LOG("decompressedSize : %u", header->decompressedSize);
}

static int roundUp(int i, int j)
{
    return (i + j - 1) / j * j;
}

static int isMAXFile(const char *path)
{
    if(!path)
        return 0;

    FILE *f = fopen(path, "rb");
    if(!f)
        return 0;

    // Verify file size
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len < sizeof(maxHeader_t))
    {
        fclose(f);
        return 0;
    }

    // Verify header
    maxHeader_t header;
    fread(&header, 1, sizeof(maxHeader_t), f);
    fclose(f);

    printMAXHeader(&header);

    return (header.compressedSize > 0) &&
           (header.decompressedSize > 0) &&
           (header.numFiles > 0) &&
           strncmp(header.magic, MAX_HEADER_MAGIC, sizeof(header.magic)) == 0 &&
           strlen(header.dirName) > 0 &&
           strlen(header.iconSysName) > 0;
}

static void set_ps2header_values(ps2_header_t *ps2h, const ps2_FileInfo_t *ps2fi, const mcIcon *ps2sys)
{
    if (strcmp(ps2fi->filename, ps2sys->view) == 0)
    {
        ps2h->icon1Size = ps2fi->filesize;
        ps2h->icon1Pos = ps2fi->positionInFile;
    }

    if (strcmp(ps2fi->filename, ps2sys->copy) == 0)
    {
        ps2h->icon2Size = ps2fi->filesize;
        ps2h->icon2Pos = ps2fi->positionInFile;
    }

    if (strcmp(ps2fi->filename, ps2sys->del) == 0)
    {
        ps2h->icon3Size = ps2fi->filesize;
        ps2h->icon3Pos = ps2fi->positionInFile;
    }

    if(strcmp(ps2fi->filename, "icon.sys") == 0)
    {
        ps2h->sysSize = ps2fi->filesize;
        ps2h->sysPos = ps2fi->positionInFile;
    }
}

int importMAX(const char *save, const char* mc_path)
{
    FILE* out;
    maxEntry_t *entry;
    maxHeader_t header;
    char dstName[256];
    struct stat st;

    if (!isMAXFile(save))
        return 0;
    
    FILE *f = fopen(save, "rb");
    if(!f)
        return 0;

    fstat(fileno(f), &st);

    fread(&header, 1, sizeof(maxHeader_t), f);
    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, header.dirName);
    mkdir(dstName, 0777);

    // Get compressed file entries
    uint8_t *compressed = malloc(header.compressedSize);

    fseek(f, sizeof(maxHeader_t) - 4, SEEK_SET); // Seek to beginning of LZARI stream.
    uint32_t ret = fread(compressed, 1, header.compressedSize, f);
    if(ret != header.compressedSize)
    {
        LOG("Compressed size: actual=%d, expected=%d\n", ret, header.compressedSize);
        free(compressed);
        return 0;
    }

    fclose(f);
    uint8_t *decompressed = malloc(header.decompressedSize);

    ret = unlzari(compressed, header.compressedSize, decompressed, header.decompressedSize);
    // As with other save formats, decompressedSize isn't acccurate.
    if(ret == 0)
    {
        LOG("Decompression failed.\n");
        free(decompressed);
        free(compressed);
        return 0;
    }

    free(compressed);

    LOG("Save contents:\n");
    // Write the file's data
    for(uint32_t offset = 0; header.numFiles > 0; header.numFiles--)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);
        LOG(" %8d bytes  : %s\n", entry->length, entry->name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, header.dirName, entry->name);
        if (!(out = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(&decompressed[offset], 1, entry->length, out);
            fclose(out);
        }

        offset = roundUp(offset + entry->length + 8, 16) - 8;
    }

    free(decompressed);

    return 1;
}

int importPSU(const char *save, const char* mc_path)
{
    FILE *psuFile, *outFile;
    char dstName[128], dirName[32];
    uint8_t *data;
    McFsEntry entry;

    psuFile = fopen(save, "rb");
    if(!psuFile)
        return 0;
    
    // Read main directory entry
    fread(&entry, 1, sizeof(McFsEntry), psuFile);
    memcpy(dirName, entry.name, sizeof(dirName));

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, dirName);
    mkdir(dstName, 0777);

    // Skip "." and ".."
    fseek(psuFile, sizeof(McFsEntry)*2, SEEK_CUR);

    LOG("Save contents:\n");
    // Copy each file entry
    for(int next, numFiles = entry.length - 2; numFiles > 0; numFiles--)
    {
        fread(&entry, 1, sizeof(McFsEntry), psuFile);
        data = malloc(entry.length);
        fread(data, 1, entry.length, psuFile);

        LOG(" %8d bytes  : %s", entry.length, entry.name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, dirName, entry.name);
        if(!(outFile = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(data, 1, entry.length, outFile);
            fclose(outFile);
        }
        free(data);

        next = 1024 - (entry.length % 1024);
        if(next < 1024)
            fseek(psuFile, next, SEEK_CUR);
    }

    fclose(psuFile);
    
    return 1;
}

static void cbsCrypt(uint8_t *buf, size_t bufLen)
{
    arc4_context ctx;

    memset(&ctx, 0, sizeof(arc4_context));
    memcpy(ctx.m, cbsKey, sizeof(cbsKey));
    arc4_crypt(&ctx, bufLen, buf, buf);
}

static int isCBSFile(const char *path)
{
    if(!path)
        return 0;
    
    FILE *f = fopen(path, "rb");
    if(!f)
        return 0;

    // Verify file size
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len < sizeof(cbsHeader_t))
    {
        fclose(f);
        return 0;
    }

    // Verify header magic
    char magic[4];
    fread(magic, 1, 4, f);
    fclose(f);

    if(memcmp(magic, CBS_HEADER_MAGIC, 4) != 0)
        return 0;

    return 1;
}

int importCBS(const char *save, const char *mc_path)
{
    FILE *dstFile;
    uint8_t *cbsData;
    uint8_t *compressed;
    uint8_t *decompressed;
    cbsHeader_t *header;
    cbsEntry_t entryHeader;
    unsigned long decompressedSize;
    size_t cbsLen;
    char dstName[256];

    if(!isCBSFile(save))
        return 0;

    if(read_buffer(save, &cbsData, &cbsLen) < 0)
        return 0;

    header = (cbsHeader_t *)cbsData;
    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, header->name);
    mkdir(dstName, 0777);

    // Get data for file entries
    compressed = cbsData + sizeof(cbsHeader_t);
    // Some tools create .CBS saves with an incorrect compressed size in the header.
    // It can't be trusted!
    cbsCrypt(compressed, cbsLen - sizeof(cbsHeader_t));
    decompressedSize = header->decompressedSize;
    decompressed = malloc(decompressedSize);
    int z_ret = uncompress(decompressed, &decompressedSize, compressed, cbsLen - sizeof(cbsHeader_t));

    if(z_ret != Z_OK)
    {
        // Compression failed.
        LOG("Decompression failed! (Z_ERR = %d)", z_ret);
        free(cbsData);
        free(decompressed);
        return 0;
    }

    LOG("Save contents:\n");

    // Write the file's data
    for(uint32_t offset = 0; offset < (decompressedSize - sizeof(cbsEntry_t)); offset += entryHeader.length)
    {
        /* Entry header can't be read directly because it might not be 32-bit aligned.
        GCC will likely emit an lw instruction for reading the 32-bit variables in the
        struct which will halt the processor if it tries to load from an address
        that's misaligned. */
        memcpy(&entryHeader, &decompressed[offset], sizeof(cbsEntry_t));
        offset += sizeof(cbsEntry_t);
        LOG(" %8d bytes  : %s", entryHeader.length, entryHeader.name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, header->name, entryHeader.name);
        if (!(dstFile = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(&decompressed[offset], 1, entryHeader.length, dstFile);
            fclose(dstFile);
        }
    }

    free(decompressed);
    free(cbsData);

    return 1;
}

int importXPS(const char *save, const char *mc_path)
{
    uint32_t len;
    FILE *xpsFile, *outFile;
    char dstName[128];
    char tmp[100];
    uint8_t *data;
    xpsEntry_t entry;

    xpsFile = fopen(save, "rb");
    if(!xpsFile)
        return 0;

    fread(&tmp, 1, 0x15, xpsFile);
    if (memcmp(&tmp[4], XPS_HEADER_MAGIC, 16) != 0)
    {
        fclose(xpsFile);
        return 0;
    }

    // Skip the variable size header
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&tmp, 1, len, xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&tmp, 1, len, xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);

    // Read main directory entry
    fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);
    // Keep the file position (start of file entries)
    len = ftell(xpsFile);

    snprintf(tmp, sizeof(tmp), "%s%s", mc_path, entry.name);
    mkdir(tmp, 0777);

    // Rewind
    fseek(xpsFile, len, SEEK_SET);

    LOG("Save contents:\n");
    // Copy each file entry
    for(int numFiles = entry.length - 2; numFiles > 0; numFiles--)
    {
        fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);
        data = malloc(entry.length);
        fread(data, 1, entry.length, xpsFile);

        LOG(" %8d bytes  : %s\n", entry.length, entry.name);

        snprintf(dstName, sizeof(dstName), "%s/%s", tmp, entry.name);
        if(!(outFile = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(data, 1, entry.length, outFile);
            fclose(outFile);
        }

        free(data);
    }

    fclose(xpsFile);

    return 1;
}

int importPSV(const char *save, const char* mc_path)
{
    uint32_t dataPos = 0;
    FILE *outFile, *psvFile;
    char dstName[256];
    uint8_t *data;
    McFsEntry entry;
    ps2_MainDirInfo_t ps2md;
    ps2_FileInfo_t ps2fi;

    psvFile = fopen(save, "rb");
    if(!psvFile)
        return 0;

    // Read main directory entry
    fseek(psvFile, 0x68, SEEK_SET);
    fread(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psvFile);

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, ps2md.filename);
    mkdir(dstName, 0777);

    LOG("Save contents:\n");

    for (int numFiles = (ps2md.numberOfFilesInDir - 2); numFiles > 0; numFiles--)
    {
        fread(&ps2fi, sizeof(ps2_FileInfo_t), 1, psvFile);
        dataPos = ftell(psvFile);
        data = malloc(ps2fi.filesize);
        fseek(psvFile, ps2fi.positionInFile, SEEK_SET);
        fread(data, 1, ps2fi.filesize, psvFile);

        LOG(" %8d bytes  : %s", ps2fi.filesize, ps2fi.filename);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, ps2md.filename, ps2fi.filename);
        if(!(outFile = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(data, 1, ps2fi.filesize, outFile);
            fclose(outFile);
        }

        free(data);
        fseek(psvFile, dataPos, SEEK_SET);
    }

    fclose(psvFile);

    return 1;
}

int ps2_psv2psu(const char *save, const char* psu_path)
{
    uint32_t dataPos = 0;
    FILE *psuFile, *psvFile;
    int numFiles, next;
    char dstName[256];
    uint8_t *data;
    McFsEntry entry;
    ps2_MainDirInfo_t ps2md;
    ps2_FileInfo_t ps2fi;
    
    psvFile = fopen(save, "rb");
    if(!psvFile)
        return 0;

    snprintf(dstName, sizeof(dstName), "%s%s.psu", psu_path, strrchr(save, '/')+1);
    psuFile = fopen(dstName, "wb");
    
    if(!psuFile)
    {
        fclose(psvFile);
        return 0;
    }

    // Read main directory entry
    fseek(psvFile, 0x68, SEEK_SET);
    fread(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psvFile);
    numFiles = ES32(ps2md.numberOfFilesInDir);

    memset(&entry, 0, sizeof(entry));
    memcpy(&entry.created, &ps2md.created, sizeof(sceMcStDateTime));
    memcpy(&entry.modified, &ps2md.modified, sizeof(sceMcStDateTime));
    memcpy(entry.name, ps2md.filename, sizeof(entry.name));
    entry.mode = ps2md.attribute;
    entry.length = ps2md.numberOfFilesInDir;
    fwrite(&entry, sizeof(entry), 1, psuFile);

    // "."
    memset(entry.name, 0, sizeof(entry.name));
    strcpy(entry.name, ".");
    entry.length = 0;
    fwrite(&entry, sizeof(entry), 1, psuFile);
    numFiles--;

    // ".."
    strcpy(entry.name, "..");
    fwrite(&entry, sizeof(entry), 1, psuFile);
    numFiles--;

    while (numFiles > 0)
    {
        fread(&ps2fi, sizeof(ps2_FileInfo_t), 1, psvFile);
        dataPos = ftell(psvFile);

        memset(&entry, 0, sizeof(entry));
        memcpy(&entry.created, &ps2fi.created, sizeof(sceMcStDateTime));
        memcpy(&entry.modified, &ps2fi.modified, sizeof(sceMcStDateTime));
        memcpy(entry.name, ps2fi.filename, sizeof(entry.name));
        entry.mode = ps2fi.attribute;
        entry.length = ps2fi.filesize;
        fwrite(&entry, sizeof(entry), 1, psuFile);

        ps2fi.positionInFile = ES32(ps2fi.positionInFile);
        ps2fi.filesize = ES32(ps2fi.filesize);
        data = malloc(ps2fi.filesize);

        fseek(psvFile, ps2fi.positionInFile, SEEK_SET);
        fread(data, 1, ps2fi.filesize, psvFile);
        fwrite(data, 1, ps2fi.filesize, psuFile);
        free(data);

        next = 1024 - (ps2fi.filesize % 1024);
        if(next < 1024)
        {
            data = malloc(next);
            memset(data, 0xFF, next);
            fwrite(data, 1, next, psuFile);
            free(data);
        }

        fseek(psvFile, dataPos, SEEK_SET);
        numFiles--;
    }

    fclose(psvFile);
    fclose(psuFile);
    
    return 1;
}
