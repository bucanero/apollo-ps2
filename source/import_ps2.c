/*
* original .MAX, .CBS, .PSU file decoding from Cheat Device PS2 by root670
* https://github.com/root670/CheatDevicePS2
*/

#include <stdio.h>
#include <stddef.h>
#include <sys/time.h>
#include <zlib.h>
#include <libmc.h>
#include <polarssl/arc4.h>

#include "mcio.h"
#include "utils.h"
#include "lzari.h"
#include "ps2mc.h"
#include "saves.h"
#include "ps2icon.h"

#define  MAX_HEADER_MAGIC   "Ps2PowerSave"
#define  CBS_HEADER_MAGIC   "CFU\0"
#define  XPS_HEADER_MAGIC   "SharkPortSave\0\0\0"

// flag value used for mcSetFileInfo at MC file restoration
#define  MC_SFI             0xFEED

// This is the initial permutation state ("S") for the RC4 stream cipher
// algorithm used to encrypt and decrypt Codebreaker saves.
// Source: https://github.com/ps2dev/mymc/blob/master/ps2save.py#L36
static const uint8_t cbsKey[256] = {
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


int check_memcard_type(const char *mcpath)
{
    int val, type;

    mcGetInfo(mcpath[2] - '0', 0, &type, &val, &val);
    mcSync(0, NULL, &val);

    return type;
}

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

/*
 * A .max carries a CRC-32 of the whole file, computed with its own four header
 * bytes taken as zero. Nothing else in the container can catch a corrupted
 * byte: the LZARI stream has no integrity check of its own, so a flipped bit
 * silently becomes a flipped bit in the save. A stored zero means the writer
 * left the field alone, which the original Action Replay software does.
 */
static int maxChecksumOk(FILE *f, uint32_t stored)
{
    const long at = (long) offsetof(maxHeader_t, crc);
    uint8_t buf[4096];
    uLong crc = crc32(0L, Z_NULL, 0);
    long pos = 0;
    size_t n, i;

    if(stored == 0)
        return 1;

    if(fseek(f, 0, SEEK_SET) != 0)
        return 0;

    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        for(i = 0; i < n; i++)
            if(pos + (long) i >= at && pos + (long) i < at + 4)
                buf[i] = 0;

        crc = crc32(crc, buf, (uInt) n);
        pos += (long) n;
    }

    return (uint32_t) crc == stored;
}

/*
 * Entries are 16-byte aligned, counting from 8 bytes before the buffer: the
 * next one begins at roundUp(offset + 8, 16) - 8. Kept in uint32_t the whole
 * way - the old roundUp(int, int) helper could overflow and wrap negative.
 */
static uint32_t nextEntryOffset(uint32_t offset)
{
    return offset + (16 - ((offset + 8) % 16)) % 16;
}

/*
 * Walk the entry chain and check every header and every file's data lies
 * inside what unlzari() actually produced. A truncated or corrupt stream still
 * "succeeds" with a short buffer, and the import loops index it by each
 * entry's declared length - so refuse instead of writing a broken save.
 */
static int maxEntriesFit(const uint8_t *buf, uint32_t len, uint32_t numFiles)
{
    const maxEntry_t *e;
    uint32_t offset = 0, i;

    for(i = 0; i < numFiles; i++)
    {
        uint32_t next, length;

        /* offset <= len is an invariant of the loop, so len - offset is safe */
        if(len - offset < sizeof(maxEntry_t))
            return 0;

        e = (const maxEntry_t*) &buf[offset];
        length = e->length;
        offset += sizeof(maxEntry_t);

        if(length > len - offset)
            return 0;

        offset += length;

        /* Padding that runs past the data is only meaningful after the last
         * entry; clamping keeps offset <= len. */
        next = nextEntryOffset(offset);
        offset = (next > len) ? len : next;
    }
    return 1;
}

static int isMAXFile(const char *path)
{
    maxHeader_t header;

    // Verify header
    if(read_file(path, (uint8_t*) &header, sizeof(maxHeader_t)) < 0)
        return 0;

    printMAXHeader(&header);

    return (header.compressedSize > 0) &&
           (header.decompressedSize > 0) &&
           (header.numFiles > 0) &&
           strncmp(header.magic, MAX_HEADER_MAGIC, sizeof(header.magic)) == 0 &&
           strlen(header.dirName) > 0;
}

static int setMcTblEntryInfo(const char* dstName, const McFsEntry *entry)
{
    int val;
    sceMcTblGetDir dinfo;

    memset(&dinfo, 0, sizeof(sceMcTblGetDir));
    dinfo.AttrFile = entry->mode;
    dinfo.FileSizeByte = entry->length;
    dinfo._Create = entry->created;
    dinfo._Modify = entry->modified;
    dinfo.Reserve1 = entry->unused;
    dinfo.Reserve2 = entry->attr;
    dinfo.PdaAplNo = entry->unused2[0];
    memcpy(dinfo.EntryName, entry->name, sizeof(dinfo.EntryName));

    mcGetInfo(dstName[2] - '0', 0, &val, &val, &val);  // Wakeup call
    mcSync(0, NULL, &val);
    val = mcSetFileInfo(dstName[2] - '0', 0, &dstName[4], &dinfo, MC_SFI);  // Fix file stats
    LOG("mcSetInfo(%s): %X", dstName, val);
    val = mcSync(0, NULL, &val);

    return val;
}

/*
 * Validate and decompress a .max save. Returns the decompressed entry data
 * (caller frees) with every entry checked to fit inside it, or NULL.
 */
static uint8_t* loadMAX(const char *save, maxHeader_t *header)
{
    const long streamStart = sizeof(maxHeader_t) - 4;
    long fileLen;
    uint32_t avail, ret;
    uint8_t *compressed, *decompressed;

    if (!isMAXFile(save))
    {
        LOG("Invalid MAX file: %s", save);
        return NULL;
    }

    FILE *f = fopen(save, "rb");
    if(!f)
        return NULL;

    if(fread(header, 1, sizeof(maxHeader_t), f) != sizeof(maxHeader_t))
    {
        fclose(f);
        return NULL;
    }

    if(!maxChecksumOk(f, header->crc))
    {
        LOG("ERROR! Damaged save: CRC mismatch, refusing to import %s", save);
        fclose(f);
        return NULL;
    }

    // compressedSize cannot be trusted: real saves under-report it
    // (BASLUS-20963FF1200.max claims 17529 for a 17533 byte stream), which
    // starves the decoder and silently truncates the last file. Read from the
    // start of the stream to the end of the file instead; the stream carries
    // its own length in its first word, so extra bytes are harmless.
    fseek(f, 0, SEEK_END);
    fileLen = ftell(f);
    if(fileLen < streamStart)
    {
        fclose(f);
        return NULL;
    }

    avail = (uint32_t)(fileLen - streamStart);
    compressed = malloc(avail);
    if(!compressed)
    {
        fclose(f);
        return NULL;
    }

    fseek(f, streamStart, SEEK_SET); // Seek to beginning of LZARI stream.
    ret = fread(compressed, 1, avail, f);
    fclose(f);
    if(ret != avail)
    {
        LOG("Compressed size: actual=%u, expected=%u", ret, avail);
        avail = ret;
    }

    // calloc: a short stream leaves the tail untouched, it must read as zeros
    decompressed = calloc(1, header->decompressedSize);
    if(!decompressed)
    {
        free(compressed);
        return NULL;
    }

    ret = unlzari(compressed, avail, decompressed, header->decompressedSize);
    free(compressed);

    // As with other save formats, decompressedSize isn't acccurate.
    if(ret == 0)
    {
        LOG("Decompression failed.");
        free(decompressed);
        return NULL;
    }

    if(!maxEntriesFit(decompressed, ret, header->numFiles))
    {
        LOG("ERROR! Truncated or corrupt save: %u decompressed bytes do not "
            "cover the %u files the header declares.", ret, header->numFiles);
        free(decompressed);
        return NULL;
    }

    return decompressed;
}

int importMAX(const char *save, const char* mc_path)
{
    FILE* out;
    maxEntry_t *entry;
    maxHeader_t header;
    char dstName[256];
    uint8_t *decompressed;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    if ((decompressed = loadMAX(save, &header)) == NULL)
        return 0;

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, header.dirName);
    mkdir(dstName, 0777);

    LOG("Save contents:\n");
    // Write the file's data
    for(uint32_t offset = 0; header.numFiles > 0; header.numFiles--)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);
        LOG(" %8d bytes  : %s", entry->length, entry->name);
        update_progress_bar(offset, header.decompressedSize, entry->name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, header.dirName, entry->name);
        if (!(out = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(&decompressed[offset], 1, entry->length, out);
            fclose(out);
        }

        offset = nextEntryOffset(offset + entry->length);
    }

    free(decompressed);

    return 1;
}

int importPSU(const char *save, const char* mc_path)
{
    FILE *psuFile, *outFile;
    char dstName[128];
    uint8_t *data;
    McFsEntry entry, dirEntry;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    psuFile = fopen(save, "rb");
    if(!psuFile)
        return 0;

    // Read main directory entry
    fread(&dirEntry, 1, sizeof(McFsEntry), psuFile);

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, dirEntry.name);
    mkdir(dstName, 0777);

    // Skip "." and ".."
    fseek(psuFile, sizeof(McFsEntry)*2, SEEK_CUR);

    LOG("Save contents:\n");
    // Copy each file entry
    for(int next, numFiles = dirEntry.length - 2; numFiles > 0; numFiles--)
    {
        fread(&entry, 1, sizeof(McFsEntry), psuFile);
        data = malloc(entry.length);
        fread(data, 1, entry.length, psuFile);

        LOG(" %8d bytes  : %s", entry.length, entry.name);
        update_progress_bar(dirEntry.length - numFiles, dirEntry.length, entry.name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, dirEntry.name, entry.name);
        if(!(outFile = fopen(dstName, "wb")))
            LOG("[!] Error writing %s", dstName);
        else
        {
            fwrite(data, 1, entry.length, outFile);
            fclose(outFile);

            setMcTblEntryInfo(dstName, &entry);
        }
        free(data);

        next = 1024 - (entry.length % 1024);
        if(next < 1024)
            fseek(psuFile, next, SEEK_CUR);
    }

    fclose(psuFile);

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, dirEntry.name);
    setMcTblEntryInfo(dstName, &dirEntry);

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
    cbsHeader_t hdr;

    if(read_file(path, (uint8_t*) &hdr, sizeof(cbsHeader_t)) < 0)
        return 0;

    if(memcmp(hdr.magic, CBS_HEADER_MAGIC, 4) != 0 || hdr.dataOffset != sizeof(cbsHeader_t))
        return 0;

    return 1;
}

/*
 * Validate, decrypt and decompress a .cbs save. Returns the decompressed entry
 * data (caller frees) with every entry checked to fit inside it, or NULL.
 */
static uint8_t* loadCBS(const char *save, cbsHeader_t *header, uLong *decompressedSize)
{
    uint8_t *cbsData;
    uint8_t *decompressed;
    cbsEntry_t entryHeader;
    size_t cbsLen;
    uint32_t offset;

    if(!isCBSFile(save))
    {
        LOG("Invalid CBS file: %s", save);
        return NULL;
    }

    if(read_buffer(save, &cbsData, &cbsLen) < 0)
        return NULL;

    memcpy(header, cbsData, sizeof(cbsHeader_t));

    // Some tools create .CBS saves with an incorrect compressed size in the header.
    // It can't be trusted!
    cbsCrypt(cbsData + sizeof(cbsHeader_t), cbsLen - sizeof(cbsHeader_t));
    *decompressedSize = header->decompressedSize;
    decompressed = malloc(*decompressedSize);
    if(!decompressed)
    {
        free(cbsData);
        return NULL;
    }

    int z_ret = uncompress(decompressed, decompressedSize, cbsData + sizeof(cbsHeader_t), cbsLen - sizeof(cbsHeader_t));
    free(cbsData);

    if(z_ret != Z_OK)
    {
        LOG("Decompression failed! (Z_ERR = %d)", z_ret);
        free(decompressed);
        return NULL;
    }

    // decompressedSize is unsigned: the old loop condition wrapped to a huge
    // value for a save smaller than one entry header. Such a save has no
    // files at all, so refuse it rather than create an empty directory.
    if(*decompressedSize <= sizeof(cbsEntry_t))
    {
        LOG("ERROR! Corrupt save: no file entries (%u bytes)", (uint32_t) *decompressedSize);
        free(decompressed);
        return NULL;
    }

    // Entry lengths come from the file; check them all before anything is written.
    for(offset = 0; offset < (*decompressedSize - sizeof(cbsEntry_t)); offset += entryHeader.length)
    {
        memcpy(&entryHeader, &decompressed[offset], sizeof(cbsEntry_t));
        offset += sizeof(cbsEntry_t);

        if(entryHeader.length > *decompressedSize - offset)
        {
            LOG("ERROR! Corrupt save: '%.32s' claims %u bytes, %u remain.",
                entryHeader.name, entryHeader.length, (uint32_t)(*decompressedSize - offset));
            free(decompressed);
            return NULL;
        }
    }

    return decompressed;
}

int importCBS(const char *save, const char *mc_path)
{
    FILE *dstFile;
    uint8_t *decompressed;
    cbsHeader_t header;
    cbsEntry_t entryHeader;
    uLong decompressedSize;
    char dstName[256];
    McFsEntry mcEntry;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    if((decompressed = loadCBS(save, &header, &decompressedSize)) == NULL)
        return 0;

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, header.name);
    mkdir(dstName, 0777);

    LOG("Save contents:\n");

    // Write the file's data
    for(uint32_t offset = 0; decompressedSize > sizeof(cbsEntry_t) &&
                             offset < (decompressedSize - sizeof(cbsEntry_t)); offset += entryHeader.length)
    {
        /* Entry header can't be read directly because it might not be 32-bit aligned.
        GCC will likely emit an lw instruction for reading the 32-bit variables in the
        struct which will halt the processor if it tries to load from an address
        that's misaligned. */
        memcpy(&entryHeader, &decompressed[offset], sizeof(cbsEntry_t));
        offset += sizeof(cbsEntry_t);
        LOG(" %8d bytes  : %s", entryHeader.length, entryHeader.name);
        update_progress_bar(offset + sizeof(cbsEntry_t), decompressedSize, entryHeader.name);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, header.name, entryHeader.name);
        if (!(dstFile = fopen(dstName, "wb")))
        {
            LOG("[!] Error writing %s", dstName);
            free(decompressed);
            return 0;
        }

        fwrite(&decompressed[offset], 1, entryHeader.length, dstFile);
        fclose(dstFile);

        memset(&mcEntry, 0, sizeof(McFsEntry));
        mcEntry.mode = entryHeader.mode;
        mcEntry.length = entryHeader.length;
        mcEntry.created = entryHeader.created;
        mcEntry.modified = entryHeader.modified;
        memcpy(mcEntry.name, entryHeader.name, sizeof(mcEntry.name));

        setMcTblEntryInfo(dstName, &mcEntry);
    }
    free(decompressed);

    memset(&mcEntry, 0, sizeof(McFsEntry));
    mcEntry.mode = header.mode;
    mcEntry.created = header.created;
    mcEntry.modified = header.modified;
    memcpy(mcEntry.name, header.name, sizeof(mcEntry.name));

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, header.name);
    setMcTblEntryInfo(dstName, &mcEntry);

    return 1;
}

/*
 * The four bytes after the body are a checksum of it. Not every writer appends
 * them - a file that stops at the last byte of data is accepted - but when
 * they are there they have to agree, because nothing else in an .xps can catch
 * a corrupted byte. A stored zero means the writer left the field alone.
 */
static int xpsChecksumOk(FILE *f, long bodyStart, uint32_t bodySize)
{
    uint8_t buf[4096];
    uint32_t sum = 0, stored;
    long fileLen, end;
    size_t left = bodySize, n, i;

    if(fseek(f, 0, SEEK_END) != 0)
        return 0;

    fileLen = ftell(f);
    if(fileLen < 0 || bodyStart < 0)
        return 0;

    end = bodyStart + (long) bodySize;
    if(end < bodyStart || end > fileLen)
        return 0;               /* the body overflows, or runs past the file */

    /* No trailer (or not one this format defines): nothing to check against */
    if(fileLen - end != 4)
        return 1;

    if(fseek(f, bodyStart, SEEK_SET) != 0)
        return 0;

    while(left > 0)
    {
        n = fread(buf, 1, (left < sizeof(buf)) ? left : sizeof(buf), f);
        if(n == 0)
            return 0;

        for(i = 0; i < n; i++)
            sum += (uint32_t) buf[i] << (sum % 24);

        left -= n;
    }

    if(fread(&stored, 1, sizeof(stored), f) != sizeof(stored))
        return 0;

    return (stored == 0) || (stored == sum);
}

int importXPS(const char *save, const char *mc_path)
{
    uint32_t len;
    long bodyStart, bodyEnd;
    FILE *xpsFile, *outFile;
    char dstName[128];
    char tmp[100];
    uint8_t *data;
    xpsEntry_t entry;
    McFsEntry mcEntry, dirEntry;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    xpsFile = fopen(save, "rb");
    if(!xpsFile)
        return 0;

    fread(&tmp, 1, 0x15, xpsFile);
    if (memcmp(&tmp[4], XPS_HEADER_MAGIC, 16) != 0)
    {
        LOG("Invalid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }

    // Skip the variable size header: three length-prefixed strings (title,
    // date and the writing tool's comment), then the size of the rest of the
    // file. The comment is empty in most saves, which makes its length look
    // like a spare zero word - but PS2SaveConverter fills it in, and reading
    // only two strings then skipping 8 bytes lands in the middle of it.
    // Seek past them instead of reading: they can be longer than tmp[].
    for (int i = 0; i < 3; i++)
    {
        if (fread(&len, 1, sizeof(uint32_t), xpsFile) != sizeof(uint32_t) ||
            fseek(xpsFile, len, SEEK_CUR) != 0)
        {
            LOG("Invalid XPS file: %s", save);
            fclose(xpsFile);
            return 0;
        }
    }
    if (fread(&len, 1, sizeof(uint32_t), xpsFile) != sizeof(uint32_t) ||
        (bodyStart = ftell(xpsFile)) < 0)
    {
        LOG("Invalid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }

    if (len < sizeof(xpsEntry_t) ||
        !xpsChecksumOk(xpsFile, bodyStart, len))
    {
        LOG("ERROR! Damaged save: checksum mismatch, refusing to import %s", save);
        fclose(xpsFile);
        return 0;
    }
    fseek(xpsFile, bodyStart, SEEK_SET);

    // Read main directory entry. Check the read itself too: a short read would
    // leave entry.length uninitialized, and it drives the loop below.
    if (fread(&entry, 1, sizeof(xpsEntry_t), xpsFile) != sizeof(xpsEntry_t) ||
        entry.length <= 2)
    {
        LOG("Invalid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }
    entry.name[sizeof(entry.name) - 1] = 0;

    bodyEnd = bodyStart + (long) len;

    snprintf(tmp, sizeof(tmp), "%s%s", mc_path, entry.name);
    mkdir(tmp, 0777);

    // Root dir
    memset(&mcEntry, 0, sizeof(McFsEntry));
    memset(&dirEntry, 0, sizeof(McFsEntry));
    dirEntry.mode = ES16(entry.mode);
    dirEntry.created = entry.created;
    dirEntry.modified = entry.modified;
    dirEntry.length = entry.length;
    memcpy(dirEntry.name, entry.name, sizeof(dirEntry.name));

    LOG("Save contents:\n");
    // Copy each file entry
    for(int numFiles = entry.length - 2; numFiles > 0; numFiles--)
    {
        // Check the header read before using any of its fields, and bound the
        // declared length by what is left of the body before allocating.
        long pos;
        if(fread(&entry, 1, sizeof(xpsEntry_t), xpsFile) != sizeof(xpsEntry_t) ||
           (pos = ftell(xpsFile)) < 0 || entry.length > (uint32_t)(bodyEnd - pos))
        {
            LOG("[!] Corrupt save: truncated or oversized entry");
            fclose(xpsFile);
            return 0;
        }
        entry.name[sizeof(entry.name) - 1] = 0;

        data = malloc(entry.length);
        if(!data || fread(data, 1, entry.length, xpsFile) != entry.length)
        {
            LOG("[!] Corrupt save: '%.64s' is truncated", entry.name);
            free(data);
            fclose(xpsFile);
            return 0;
        }

        LOG(" %8d bytes  : %s\n", entry.length, entry.name);
        update_progress_bar(dirEntry.length - numFiles, dirEntry.length, entry.name);

        snprintf(dstName, sizeof(dstName), "%s/%s", tmp, entry.name);
        if(!(outFile = fopen(dstName, "wb")))
        {
            LOG("[!] Error writing %s", dstName);
            free(data);
            fclose(xpsFile);
            return 0;
        }

        fwrite(data, 1, entry.length, outFile);
        fclose(outFile);
        free(data);

        mcEntry.mode = ES16(entry.mode);
        mcEntry.created = entry.created;
        mcEntry.modified = entry.modified;
        mcEntry.length = entry.length;
        memcpy(mcEntry.name, entry.name, sizeof(mcEntry.name));

        setMcTblEntryInfo(dstName, &mcEntry);
    }

    fclose(xpsFile);
    setMcTblEntryInfo(tmp, &dirEntry);

    return 1;
}

int importPSV(const char *save, const char* mc_path)
{
    uint32_t dataPos;
    FILE *outFile, *psvFile;
    char dstName[256];
    uint8_t *data;
    McFsEntry entry;
    ps2_MainDirInfo_t ps2md;
    ps2_FileInfo_t ps2fi;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    psvFile = fopen(save, "rb");
    if(!psvFile)
        return 0;

    fread(dstName, 1, sizeof(dstName), psvFile);
    if ((memcmp(dstName, PSV_MAGIC, 4) != 0) || (dstName[0x3C] != 0x02))
    {
        LOG("Invalid PSV file: %s", save);
        fclose(psvFile);
        return 0;
    }

    // Read main directory entry
    fseek(psvFile, 0x68, SEEK_SET);
    fread(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psvFile);

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, ps2md.filename);
    mkdir(dstName, 0777);

    LOG("Save contents:\n");
    memset(&entry, 0, sizeof(McFsEntry));

    for (int numFiles = (ps2md.numberOfFilesInDir - 2); numFiles > 0; numFiles--)
    {
        fread(&ps2fi, sizeof(ps2_FileInfo_t), 1, psvFile);
        dataPos = ftell(psvFile);
        data = malloc(ps2fi.filesize);
        fseek(psvFile, ps2fi.positionInFile, SEEK_SET);
        fread(data, 1, ps2fi.filesize, psvFile);

        LOG(" %8d bytes  : %s", ps2fi.filesize, ps2fi.filename);
        update_progress_bar(ps2md.numberOfFilesInDir - numFiles, ps2md.numberOfFilesInDir, ps2fi.filename);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, ps2md.filename, ps2fi.filename);
        if(!(outFile = fopen(dstName, "wb")))
        {
            LOG("[!] Error writing %s", dstName);
            free(data);
            fclose(psvFile);
            return 0;
        }

        fwrite(data, 1, ps2fi.filesize, outFile);
        fclose(outFile);
        free(data);

        entry.mode = ps2fi.attribute;
        entry.created = ps2fi.created;
        entry.modified = ps2fi.modified;
        entry.length = ps2fi.filesize;
        memcpy(entry.name, ps2fi.filename, sizeof(entry.name));

        setMcTblEntryInfo(dstName, &entry);

        fseek(psvFile, dataPos, SEEK_SET);
    }

    fclose(psvFile);

    entry.mode = ps2md.attribute;
    entry.created = ps2md.created;
    entry.modified = ps2md.modified;
    entry.length = ps2md.numberOfFilesInDir;
    memcpy(entry.name, ps2md.filename, sizeof(entry.name));

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, ps2md.filename);
    setMcTblEntryInfo(dstName, &entry);

    return 1;
}

int importPSV_buffer(const uint8_t *savebuf, size_t bufsize, const char* mc_path)
{
    FILE *outFile;
    char dstName[256];
    McFsEntry entry;
    ps2_MainDirInfo_t *ps2md;
    ps2_FileInfo_t *ps2fi;

    if(check_memcard_type(mc_path) != sceMcTypePS2)
    {
        LOG("Error: Memory Card is not PS2 type");
        return 0;
    }

    if ((memcmp(savebuf, PSV_MAGIC, 4) != 0) || (savebuf[0x3C] != 0x02))
    {
        LOG("Invalid PSV file data");
        return 0;
    }

    // Read main directory entry
    ps2md = (ps2_MainDirInfo_t *)(savebuf + 0x68);

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, ps2md->filename);
    mkdir(dstName, 0777);

    LOG("Save contents:");
    memset(&entry, 0, sizeof(McFsEntry));

    for (int numFiles = (ps2md->numberOfFilesInDir - 2); numFiles > 0; numFiles--)
    {
        ps2fi = (ps2_FileInfo_t *)(savebuf + 0x2C + sizeof(ps2_MainDirInfo_t) + (numFiles * sizeof(ps2_FileInfo_t)));

        LOG(" %8d bytes  : %s", ps2fi->filesize, ps2fi->filename);
        update_progress_bar(ps2md->numberOfFilesInDir - numFiles, ps2md->numberOfFilesInDir, ps2fi->filename);

        snprintf(dstName, sizeof(dstName), "%s%s/%s", mc_path, ps2md->filename, ps2fi->filename);
        if(!(outFile = fopen(dstName, "wb")))
        {
            LOG("[!] Error writing %s", dstName);
            return 0;
        }

        fwrite(savebuf + ps2fi->positionInFile, 1, ps2fi->filesize, outFile);
        fclose(outFile);

        entry.mode = ps2fi->attribute;
        entry.created = ps2fi->created;
        entry.modified = ps2fi->modified;
        entry.length = ps2fi->filesize;
        memcpy(entry.name, ps2fi->filename, sizeof(entry.name));

        setMcTblEntryInfo(dstName, &entry);
    }

    entry.mode = ps2md->attribute;
    entry.created = ps2md->created;
    entry.modified = ps2md->modified;
    entry.length = ps2md->numberOfFilesInDir;
    memcpy(entry.name, ps2md->filename, sizeof(entry.name));

    snprintf(dstName, sizeof(dstName), "%s%s", mc_path, ps2md->filename);
    setMcTblEntryInfo(dstName, &entry);

    return 1;
}

uint8_t* loadVmcIcon(const char *save, const char* icon)
{
    int r, fd, has_sys = 0;
    char filepath[128];
    struct io_dirent dirent;
    uint8_t iconsys[PS2ICON_SYS_SIZE];

    // The save's icon.sys has the lights the model is shaded with
    snprintf(filepath, sizeof(filepath), "%s/icon.sys", save);
    fd = mcio_mcOpen(filepath, sceMcFileAttrReadable | sceMcFileAttrFile);
    if (fd >= 0)
    {
        has_sys = (mcio_mcRead(fd, iconsys, sizeof(iconsys)) == sizeof(iconsys));
        mcio_mcClose(fd);
    }

    snprintf(filepath, sizeof(filepath), "%s/%s", save, icon);
    LOG("Reading '%s'...", filepath);

    // Read icon entry
    if (mcio_mcStat(filepath, &dirent) < 0 || dirent.stat.size == 0)
        return NULL;

    fd = mcio_mcOpen(filepath, sceMcFileAttrReadable | sceMcFileAttrFile);
    if (fd < 0)
        return NULL;

    uint8_t *p = malloc(dirent.stat.size);
    if (p == NULL)
    {
        mcio_mcClose(fd);
        return NULL;
    }

    r = mcio_mcRead(fd, p, dirent.stat.size);
    mcio_mcClose(fd);

    if (r != (int)dirent.stat.size)
    {
        free(p);
        return NULL;
    }

    uint8_t* tex = ps2IconRender(p, dirent.stat.size, has_sys ? iconsys : NULL);
    free(p);

    return tex;
}

int importVMC(const char *save, const char* mc_path)
{
	int r, fd, dd;
	uint32_t i = 0;
	struct io_dirent dirent;
    char filepath[128];
    FILE *outFile;
    McFsEntry entry, dirEntry;

	LOG("Copying '%s' to %s...", save, mc_path);

	dd = mcio_mcDopen(save);
	if (dd < 0)
		return 0;

    // Read main directory entry
    mcio_mcStat(save, &dirent);

	memset(&dirEntry, 0, sizeof(McFsEntry));
	memcpy(&dirEntry.created, &dirent.stat.ctime, sizeof(struct sceMcStDateTime));
	memcpy(&dirEntry.modified, &dirent.stat.mtime, sizeof(struct sceMcStDateTime));
	memcpy(dirEntry.name, dirent.name, sizeof(entry.name));
	dirEntry.mode = dirent.stat.mode;
	dirEntry.length = dirent.stat.size;

    snprintf(filepath, sizeof(filepath), "%s%s", mc_path, save);
    mkdir(filepath, 0777);

    // Copy each file entry
    do {
        r = mcio_mcDread(dd, &dirent);
        if (r && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
        {
            snprintf(filepath, sizeof(filepath), "%s/%s", save, dirent.name);
            LOG("Copy %-48s | %8d bytes", filepath, dirent.stat.size);
            update_progress_bar(++i, dirEntry.length - 2, dirent.name);

            mcio_mcStat(filepath, &dirent);

            memset(&entry, 0, sizeof(McFsEntry));
            memcpy(&entry.created, &dirent.stat.ctime, sizeof(struct sceMcStDateTime));
            memcpy(&entry.modified, &dirent.stat.mtime, sizeof(struct sceMcStDateTime));
            memcpy(entry.name, dirent.name, sizeof(entry.name));
            entry.mode = dirent.stat.mode;
            entry.length = dirent.stat.size;

            fd = mcio_mcOpen(filepath, sceMcFileAttrReadable | sceMcFileAttrFile);
            if (fd < 0) {
                i = 0;
                break;
            }

            uint8_t *p = malloc(dirent.stat.size);
            if (p == NULL) {
                i = 0;
                break;
            }

            r = mcio_mcRead(fd, p, dirent.stat.size);
            mcio_mcClose(fd);

            if (r != (int)dirent.stat.size) {
                free(p);
                i = 0;
                break;
            }

            snprintf(filepath, sizeof(filepath), "%s%s/%s", mc_path, dirEntry.name, entry.name);
            if(!(outFile = fopen(filepath, "wb")))
            {
                LOG("[!] Error writing %s", filepath);
                i = 0;
                break;
            }
            r = fwrite(p, 1, entry.length, outFile);
            fclose(outFile);

            setMcTblEntryInfo(filepath, &entry);
            free(p);

            if (r != (int)dirent.stat.size) {
                i = 0;
                break;
            }
        }
    } while (r);

    mcio_mcDclose(dd);

    snprintf(filepath, sizeof(filepath), "%s%s", mc_path, dirEntry.name);
    setMcTblEntryInfo(filepath, &dirEntry);

    return (i > 0);
}

static void setMcFsEntryValues(McFsEntry *entry, sceMcTblGetDir *mcDir)
{
    entry->mode = mcDir->AttrFile;
    entry->length = mcDir->FileSizeByte;
    entry->created = mcDir->_Create;
    entry->modified = mcDir->_Modify;

    entry->unused = mcDir->Reserve1;
    entry->attr = mcDir->Reserve2;
    entry->unused2[0] = mcDir->PdaAplNo;

    memcpy(entry->name, mcDir->EntryName, sizeof(entry->name));
}

int exportPSU(const char *save, const char* psu_path)
{
    FILE *psuFile;
    sceMcTblGetDir mcDir[64] __attribute__((aligned(64)));
    McFsEntry mcEntry;
    char mcPath[100];
    char filePath[150];
    char *data;
    int ret;

    LOG("Export %s -> %s ...", save, psu_path);
    psuFile = fopen(psu_path, "wb");
    if(!psuFile)
        return 0;

    strcpy(mcPath, strchr(save, '/')+1);
    *strrchr(mcPath, '/') = 0;
    mcGetDir(save[2] - '0', 0, mcPath, 0, 1, mcDir);
    mcSync(0, NULL, &ret);
    LOG("mcGetDir(%s) %d", mcPath, ret);

    memset(&mcEntry, 0, sizeof(McFsEntry));
    setMcFsEntryValues(&mcEntry, &mcDir[0]);

    snprintf(mcPath, sizeof(mcPath), "%s*", save + 5);
    mcGetDir(save[2] - '0', 0, mcPath, 0, countof(mcDir), mcDir);
    mcSync(0, NULL, &ret);
    LOG("mcGetDir(%s) %d", mcPath, ret);

    // root directory
    mcEntry.length = ret;
    fwrite(&mcEntry, 1, sizeof(McFsEntry), psuFile);

    setMcFsEntryValues(&mcEntry, &mcDir[0]);
    fwrite(&mcEntry, 1, sizeof(McFsEntry), psuFile);
    setMcFsEntryValues(&mcEntry, &mcDir[1]);
    fwrite(&mcEntry, 1, sizeof(McFsEntry), psuFile);

    for(int i = 0, padding; i < ret; i++)
    {
        if(!(mcDir[i].AttrFile & sceMcFileAttrFile))
            continue;

        LOG("(%d/%d) Add '%s'", i+1, ret, mcDir[i].EntryName);
        update_progress_bar(i+1, ret, mcDir[i].EntryName);
        setMcFsEntryValues(&mcEntry, &mcDir[i]);

        snprintf(filePath, sizeof(filePath), "%s%s", save, mcEntry.name);
        data = malloc(mcEntry.length);
        read_file(filePath, data, mcEntry.length);

        fwrite(&mcEntry, 1, sizeof(McFsEntry), psuFile);
        fwrite(data, 1, mcEntry.length, psuFile);
        free(data);

        padding = 1024 - (mcEntry.length % 1024);
        if(padding < 1024)
        {
            while(padding--)
                fputc(0xFF, psuFile);
        }
    }
    fclose(psuFile);

    return 1;
}

int exportCBS(const char *save, const char* cbs_path, const char* title)
{
    FILE *cbsFile, *mcFile;
    sceMcTblGetDir mcDir[64] __attribute__((aligned(64)));
    cbsHeader_t header;
    cbsEntry_t entryHeader;
    uint8_t *dataBuff;
    uint8_t *dataCompressed;
    uLong compressedSize;
    uint32_t dataOffset = 0;
    char mcPath[100];
    char filePath[150];
    int i, ret;

    LOG("Export %s -> %s ...", save, cbs_path);
    cbsFile = fopen(cbs_path, "wb");
    if(!cbsFile)
        return 0;

    // Get root directory entry
    strcpy(mcPath, strchr(save, '/')+1);
    *strrchr(mcPath, '/') = 0;
    mcGetDir(save[2] - '0', 0, mcPath, 0, 1, mcDir);
    mcSync(0, NULL, &ret);
    LOG("mcGetDir (%s) %d", mcPath, ret);

    memset(&header, 0, sizeof(cbsHeader_t));
    memset(&entryHeader, 0, sizeof(cbsEntry_t));
    memcpy(header.magic, "CFU\0", 4);
    header.unk1 = 0x1F40;
    header.dataOffset = sizeof(cbsHeader_t); // 0x128
    header.created = mcDir[0]._Create;
    header.modified = mcDir[0]._Modify;
    header.mode = mcDir[0].AttrFile; // 0x8427
    memcpy(header.name, mcDir[0].EntryName, sizeof(header.name));
    strncpy(header.title, title, sizeof(header.title)-1);

    snprintf(mcPath, sizeof(mcPath), "%s*", save + 5);
    mcGetDir(save[2] - '0', 0, mcPath, 0, countof(mcDir), mcDir);
    mcSync(0, NULL, &ret);
    LOG("mcGetDir (%s) %d", mcPath, ret);

    for(i = 0; i < ret; i++)
    {
        if(mcDir[i].AttrFile & sceMcFileAttrFile)
            header.decompressedSize += mcDir[i].FileSizeByte + sizeof(cbsEntry_t);
    }
    dataBuff = malloc(header.decompressedSize);

    for(i = 0; i < ret; i++)
    {
        if(!(mcDir[i].AttrFile & sceMcFileAttrFile))
            continue;

        LOG("(%d/%d) Add '%s'", i+1, ret, mcDir[i].EntryName);
        update_progress_bar(i+1, ret, mcDir[i].EntryName);

        entryHeader.created = mcDir[i]._Create;
        entryHeader.modified = mcDir[i]._Modify;
        entryHeader.length = mcDir[i].FileSizeByte;
        entryHeader.mode = mcDir[i].AttrFile;
        memcpy(entryHeader.name, mcDir[i].EntryName, sizeof(entryHeader.name));

        memcpy(&dataBuff[dataOffset], &entryHeader, sizeof(cbsEntry_t));
        dataOffset += sizeof(cbsEntry_t);

        snprintf(filePath, sizeof(filePath), "%s%s", save, entryHeader.name);
        mcFile = fopen(filePath, "rb");
        fread(&dataBuff[dataOffset], 1, entryHeader.length, mcFile);
        fclose(mcFile);

        dataOffset += entryHeader.length;
    }

    compressedSize = compressBound(header.decompressedSize);
    dataCompressed = malloc(compressedSize);
    if(!dataCompressed)
    {
        LOG("malloc failed");
        free(dataBuff);
        fclose(cbsFile);
        return 0;
    }

    ret = compress2(dataCompressed, &compressedSize, dataBuff, header.decompressedSize, Z_BEST_COMPRESSION);
    if(ret != Z_OK)
    {
        LOG("compress2 failed");
        free(dataBuff);
        free(dataCompressed);
        fclose(cbsFile);
        return 0;
    }

    header.compressedSize = compressedSize + header.dataOffset; //0x128
    fwrite(&header, 1, sizeof(cbsHeader_t), cbsFile);
    cbsCrypt(dataCompressed, compressedSize);
    fwrite(dataCompressed, 1, compressedSize, cbsFile);
    fclose(cbsFile);

    free(dataBuff);
    free(dataCompressed);

    return 1;
}

int vmc_import_max(const char *save)
{
    int r, fd;
    maxEntry_t *entry;
    maxHeader_t header;
    char dstName[256];

    uint8_t *decompressed = loadMAX(save, &header);
    if (!decompressed)
        return 0;

    r = mcio_mcMkDir(header.dirName);
    if (r < 0)
        LOG("Error: can't create directory '%s'... (%d)", header.dirName, r);
    else
        mcio_mcClose(r);

    LOG("Save contents:\n");
    // Write the file's data
    for(uint32_t offset = 0; header.numFiles > 0; header.numFiles--)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);
        LOG(" %8d bytes  : %s", entry->length, entry->name);
        update_progress_bar(offset, header.decompressedSize, entry->name);

        snprintf(dstName, sizeof(dstName), "%s/%s", header.dirName, entry->name);
        fd = mcio_mcOpen(dstName, sceMcFileCreateFile | sceMcFileAttrWriteable | sceMcFileAttrFile);
        if (fd < 0)
        {
            free(decompressed);
            return 0;
        }

        r = mcio_mcWrite(fd, &decompressed[offset], entry->length);
        mcio_mcClose(fd);

        if (r != (int)entry->length)
        {
            free(decompressed);
            return 0;
        }

        offset = nextEntryOffset(offset + entry->length);
    }

    free(decompressed);

    return 1;
}

int vmc_import_cbs(const char *save)
{
    int r, fd;
    uint8_t *decompressed;
    cbsHeader_t header;
    cbsEntry_t entryHeader;
    uLong decompressedSize;
    char dstName[256];
    struct io_dirent mcEntry;

    if((decompressed = loadCBS(save, &header, &decompressedSize)) == NULL)
        return 0;

    r = mcio_mcMkDir(header.name);
    if (r < 0)
        LOG("Error: can't create directory '%s'... (%d)", header.name, r);
    else
        mcio_mcClose(r);

    LOG("Save contents:\n");

    // Write the file's data
    for(uint32_t offset = 0; decompressedSize > sizeof(cbsEntry_t) &&
                             offset < (decompressedSize - sizeof(cbsEntry_t)); offset += entryHeader.length)
    {
        /* Entry header can't be read directly because it might not be 32-bit aligned.
        GCC will likely emit an lw instruction for reading the 32-bit variables in the
        struct which will halt the processor if it tries to load from an address
        that's misaligned. */
        memcpy(&entryHeader, &decompressed[offset], sizeof(cbsEntry_t));
        offset += sizeof(cbsEntry_t);
        LOG(" %8d bytes  : %s", entryHeader.length, entryHeader.name);
        update_progress_bar(offset + sizeof(cbsEntry_t), decompressedSize, entryHeader.name);

        snprintf(dstName, sizeof(dstName), "%s/%s", header.name, entryHeader.name);
        fd = mcio_mcOpen(dstName, sceMcFileCreateFile | sceMcFileAttrWriteable | sceMcFileAttrFile);
        if (fd < 0)
        {
            free(decompressed);
            return 0;
        }

        r = mcio_mcWrite(fd, &decompressed[offset], entryHeader.length);
        mcio_mcClose(fd);

        if (r != (int)entryHeader.length)
        {
            free(decompressed);
            return 0;
        }

        mcio_mcStat(dstName, &mcEntry);
        memcpy(&mcEntry.stat.ctime, &entryHeader.created, sizeof(struct sceMcStDateTime));
        memcpy(&mcEntry.stat.mtime, &entryHeader.modified, sizeof(struct sceMcStDateTime));
        mcEntry.stat.mode = entryHeader.mode;
        mcio_mcSetStat(dstName, &mcEntry);
    }
    free(decompressed);

    mcio_mcStat(header.name, &mcEntry);
    memcpy(&mcEntry.stat.ctime, &header.created, sizeof(struct sceMcStDateTime));
    memcpy(&mcEntry.stat.mtime, &header.modified, sizeof(struct sceMcStDateTime));
    mcEntry.stat.mode = header.mode;
    mcio_mcSetStat(header.name, &mcEntry);

    return 1;
}
