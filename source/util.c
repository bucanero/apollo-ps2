#include "utils.h"
#include "types.h"

void dump_data(const u8 *data, u64 size) {
	u64 i;
	for (i = 0; i < size; i++)
		dbglogger_printf("%02X", data[i]);
	dbglogger_printf("\n");
}

int get_file_size(const char *file_path, u64 *size) {
	struct stat stat_buf;

	if (!file_path || !size)
		return -1;

	if (stat(file_path, &stat_buf) < 0)
		return -1;

	*size = stat_buf.st_size;

	return 0;
}

int read_file(const char *file_path, u8 *data, u64 size) {
	FILE *fp;

	if (!file_path || !data)
		return -1;

	fp = fopen(file_path, "rb");
	if (!fp)
		return -1;

	memset(data, 0, size);

	if (fread(data, 1, size, fp) != size) {
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

int write_file(const char *file_path, u8 *data, u64 size) {
	FILE *fp;

	if (!file_path || !data)
		return -1;

	fp = fopen(file_path, "wb");
	if (!fp)
		return -1;

	if (fwrite(data, 1, size, fp) != size) {
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

void memrcpy(void *dst, void *src, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		((uint8_t *)dst)[i] = ((uint8_t *)src)[len-1-i];
	}
}

/*
 * memxor: perform exclusive-or of memory buffers.
 */
void memxor(const void *a, const void *b, void *Result, size_t Length)
{
	size_t i;
	for (i = 0; i < Length; i++) {
		((uint8_t *)Result)[i] = ((uint8_t *)a)[i] ^ ((uint8_t *)b)[i];
	}
}

/*
 * append_le_uint16: append an unsigned 16 bits Little Endian
 * value to a buffer
 */
void append_le_uint16(uint8_t *buf, uint16_t val)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
}

/*
 * append_le_uint32: append an unsigned 32 bits Little Endian
 * value to a buffer
 */
void append_le_uint32(uint8_t *buf, uint32_t val)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
	buf[2] = (uint8_t)(val >> 16);
	buf[3] = (uint8_t)(val >> 24);
}

/*
 * append_le_uint64: append an unsigned 64 bits Little Endian
 * value to a buffer
 */
void append_le_uint64(uint8_t *buf, uint64_t val)
{
	int i;
	for (i = 7; i >= 0; i--, val >>= 8) {
		buf[i] = (uint8_t)val;
	}
}

/*
 * read_le_uint16: read an unsigned 16 bits Little Endian
 * value from a buffer
 */
uint16_t read_le_uint16(const uint8_t *buf)
{
	register uint16_t val;

	val = buf[0];
	val |= (buf[1] << 8);

	return val;
}

/*
 * read_le_uint32: read an unsigned 32 bits Little Endian
 * value from a buffer
 */
uint32_t read_le_uint32(const uint8_t *buf)
{
	register uint32_t val;

	val = buf[0];
	val |= (buf[1] << 8);
	val |= (buf[2] << 16);
	val |= (buf[3] << 24);

	return val;
}

/*
 * read_le_uint64: read an unsigned 64 bits Little Endian
 * value from a buffer
 */
uint64_t read_le_uint64(const uint8_t *buf)
{
	uint64_t val = 0;
	int i;
	for (i = 0; i < 8; i++) {
		val = (val << 8) | buf[i];
	}

	return val;
}

u64 align_to_pow2(u64 offset, u64 alignment) {
	return (offset + alignment - 1) & ~(alignment - 1);
}
