/*
*  PSV file format information from:
*  - https://github.com/PMStanley/PSV-Exporter
*
*  PSU, MAX, CBS file format information from:
*  - https://github.com/root670/CheatDevicePS2
*/

#include <inttypes.h>

#define PSV_MAGIC       "\x00VSP"
#define PSV_SALT        "www.bucanero.com.ar"


typedef struct
{
	char magic[4];
	uint32_t padding1;               //always 0x00000000
	uint8_t salt[20];
	uint8_t signature[20];           //digital sig
	uint32_t padding2;               //always 0x00000000
	uint32_t padding3;               //always 0x00000000
	uint32_t headerSize;             //always 0x0000002C in PS2, 0x00000014 in PS1. 
	uint32_t saveType;               //0x00000002 PS2, 0x00000001 PS1
} psv_header_t;

typedef struct
{
    uint32_t saveSize;          // e.g. 0x00200000
    uint32_t startOfSaveData;   // always 0x84000000 (132)
    uint32_t blockSize;         // always 0x00020000 (512). Block size?
    uint32_t padding1;          // always 0x00000000?
    uint32_t padding2;          // always 0x00000000?
    uint32_t padding3;          // always 0x00000000?
    uint32_t padding4;          // always 0x00000000?
    uint32_t dataSize;          // save size repeated?
    uint32_t unknown1;          // always 0x03900000 (36867)?
    char prodCode[20];          // 20 bytes, 0x00 filled & terminated
    uint32_t padding6;          // always 0x00000000?
    uint32_t padding7;          // always 0x00000000?
    uint32_t padding8;          // always 0x00000000?
} ps1_header_t;

typedef struct
{
	uint32_t displaySize;            //PS3 will just round this up to the neaest 1024 boundary so just make it as good as possible
	uint32_t sysPos;                 //location in file of icon.sys
	uint32_t sysSize;                //icon.sys size
	uint32_t icon1Pos;               //position of 1st icon
	uint32_t icon1Size;              //size of 1st icon
	uint32_t icon2Pos;               //position of 2nd icon
	uint32_t icon2Size;              //size of 2nd icon
	uint32_t icon3Pos;               //position of 3rd icon
	uint32_t icon3Size;              //size of 3rd icon
	uint32_t numberOfFiles;
} ps2_header_t;

typedef struct
{
	sceMcStDateTime created;
	sceMcStDateTime modified;
	uint32_t numberOfFilesInDir;     // this is likely to be number of files in dir + 2 ("." and "..")
	uint32_t attribute;              // (0x00008427 dir)
	char filename[32];
} ps2_MainDirInfo_t;

typedef struct
{
	sceMcStDateTime created;
	sceMcStDateTime modified;
	uint32_t filesize;
	uint32_t attribute;             // (0x00008497 file)
	char filename[32];              // 'Real' PSV files have junk in this after text.
	uint32_t positionInFile;
} ps2_FileInfo_t;

typedef struct maxHeader
{
    char     magic[12];
    uint32_t crc;
    char     dirName[32];
    char     iconSysName[32];
    uint32_t compressedSize;
    uint32_t numFiles;
    uint32_t decompressedSize; // This is actually the start of the LZARI stream, but we need it to  allocate the buffer.
} maxHeader_t;

typedef struct maxEntry
{
    uint32_t length;
    char     name[32];
} maxEntry_t;

typedef struct cbsHeader
{
    char magic[4];
    uint32_t unk1;
    uint32_t dataOffset;
    uint32_t decompressedSize;
    uint32_t compressedSize;
    char name[32];
    sceMcStDateTime created;
    sceMcStDateTime modified;
    uint32_t unk2;
    uint32_t mode;
    char unk3[16];
    char title[72];
    char description[132];
} cbsHeader_t;

typedef struct cbsEntry
{
    sceMcStDateTime created;
    sceMcStDateTime modified;
    uint32_t length;
    uint32_t mode;
    char unk1[8];
    char name[32];
} cbsEntry_t;

typedef struct __attribute__((__packed__)) xpsEntry
{
    uint16_t entry_sz;
    char name[64];
    uint32_t length;
    uint32_t start;
    uint32_t end;
    uint32_t mode;
    sceMcStDateTime created;
    sceMcStDateTime modified;
    char unk1[4];
    char padding[12];
    char title_ascii[64];
    char title_sjis[64];
    char unk2[8];
} xpsEntry_t;
