#ifndef _UTIL_H_
#define _UTIL_H_

#include <apollo.h>
#include "types.h"

#define ALIGN(_value, _alignment) (((_value) + (_alignment) - 1) & ~((_alignment) - 1))

void dump_data(const u8 *data, u64 size);

int get_file_size(const char *file_path, u64 *size);
int read_file(const char *file_path, u8 *data, u64 size);
int write_file(const char *file_path, u8 *data, u64 size);

u64 align_to_pow2(u64 offset, u64 alignment);

void memrcpy(void *dst, void *src, size_t len);
void memxor(const void *a, const void *b, void *Result, size_t Length);
void append_le_uint16(uint8_t *buf, uint16_t val);
void append_le_uint32(uint8_t *buf, uint32_t val);
void append_le_uint64(uint8_t *buf, uint64_t val);
uint16_t read_le_uint16(const uint8_t *buf);
uint32_t read_le_uint32(const uint8_t *buf);
uint64_t read_le_uint64(const uint8_t *buf);

// todo: why asprintf is not defined?
int asprintf(char **ret, const char *format, ...);

#endif /* !_UTIL_H_ */
