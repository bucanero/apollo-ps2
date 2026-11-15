/*
 * RAM cache: APOLLO_LOCAL_CACHE in EE memory when there's no USB storage
 *
 * The cache folder (mass:/APOLLO/CACHE/) holds temporary and cached files:
 * downloaded lists and icons, save zips being imported, update checks.
 * Without USB storage those writes fail and the features using them with it.
 *
 * When ramcache_enable() is called (at startup, if the cache folder can't be
 * created), any path under APOLLO_LOCAL_CACHE is kept in memory instead. The
 * link wraps the libc calls the app and its libraries use on those files
 * (-Wl,--wrap=..., see CMakeLists.txt); every other path goes to the real
 * call. fopencookie() gives a normal FILE*, so curl, libpng, apollo-lib's
 * read_buffer/write_buffer and the app's own helpers need no changes.
 *
 * libzip opens archives with open()/fstat() instead, so zip_util.c reads
 * cached zips into memory (ramcache_is_path) like it does for the disc.
 *
 * The store is capped at RAMCACHE_MAX_SIZE: to make room, the least recently
 * used files that aren't open are dropped (they're only a cache).
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#include "saves.h"
#include "ramcache.h"

#define RAMCACHE_MAX_SIZE	(8 * 1024 * 1024)

typedef struct ram_file
{
	char* name;
	uint8_t* data;
	size_t size;
	size_t alloc;
	time_t mtime;
	uint32_t last_use;
	int open_count;
	int deleted;		// removed while open: freed on the last close
	struct ram_file* next;
} ram_file_t;

typedef struct
{
	ram_file_t* file;
	size_t pos;
	int append;
} ram_handle_t;

FILE* __real_fopen(const char *path, const char *mode);
int __real_stat(const char *path, struct stat *st);
int __real_remove(const char *path);
int __real_unlink(const char *path);
int __real_chmod(const char *path, mode_t mode);
int __real_mkdir(const char *path, mode_t mode);

static int ramcache_active = 0;
static ram_file_t* files = NULL;
static size_t total_size = 0;
static uint32_t use_counter = 0;

void ramcache_enable(void)
{
	ramcache_active = 1;
	LOG("RAM cache enabled for %s (up to %d KB)", APOLLO_LOCAL_CACHE, RAMCACHE_MAX_SIZE / 1024);
}

int ramcache_enabled(void)
{
	return ramcache_active;
}

int ramcache_is_path(const char* path)
{
	return (ramcache_active && path && strncmp(path, APOLLO_LOCAL_CACHE, sizeof(APOLLO_LOCAL_CACHE) - 1) == 0);
}

// The cache folder itself, or one of its parents (as mkdirs() walks them)
static int is_cache_dir(const char* path)
{
	size_t len = strlen(path);

	if (!ramcache_active || !len || len > sizeof(APOLLO_LOCAL_CACHE) - 1)
		return 0;

	if (path[len-1] == '/')
		len--;

	return (strncmp(path, APOLLO_LOCAL_CACHE, len) == 0 &&
		(APOLLO_LOCAL_CACHE[len] == '/' || APOLLO_LOCAL_CACHE[len] == 0));
}

static ram_file_t* find_file(const char* path)
{
	for (ram_file_t* f = files; f; f = f->next)
		if (!f->deleted && strcmp(f->name, path) == 0)
			return f;

	return NULL;
}

static void delete_file(ram_file_t* file)
{
	for (ram_file_t** f = &files; *f; f = &(*f)->next)
		if (*f == file)
		{
			*f = file->next;
			break;
		}

	total_size -= file->alloc;
	free(file->data);
	free(file->name);
	free(file);
}

// Drops least recently used closed files until 'needed' more bytes fit
static int make_room(size_t needed, const ram_file_t* keep)
{
	while (total_size + needed > RAMCACHE_MAX_SIZE)
	{
		ram_file_t* lru = NULL;

		for (ram_file_t* f = files; f; f = f->next)
			if (f != keep && f->open_count == 0 && (!lru || f->last_use < lru->last_use))
				lru = f;

		if (!lru)
			return 0;

		LOG("RAM cache full: dropping %s (%d bytes)", lru->name, (int) lru->size);
		delete_file(lru);
	}

	return 1;
}

static int reserve(ram_file_t* file, size_t size)
{
	size_t alloc;
	uint8_t* data;

	if (size <= file->alloc)
		return 1;

	// grow in 64 KB steps, or doubling, whichever is larger
	alloc = file->alloc * 2;
	if (alloc < size)
		alloc = size;
	alloc = (alloc + 0xFFFF) & ~0xFFFF;

	if (!make_room(alloc - file->alloc, file) && !make_room(size - file->alloc, file))
		return 0;

	if (total_size + (alloc - file->alloc) > RAMCACHE_MAX_SIZE)
		alloc = size;

	if ((data = realloc(file->data, alloc)) == NULL)
		return 0;

	total_size += alloc - file->alloc;
	file->data = data;
	file->alloc = alloc;
	return 1;
}

static ssize_t ram_read(void* cookie, char* buf, size_t n)
{
	ram_handle_t* h = cookie;

	if (h->pos >= h->file->size)
		return 0;

	if (n > h->file->size - h->pos)
		n = h->file->size - h->pos;

	memcpy(buf, h->file->data + h->pos, n);
	h->pos += n;
	return n;
}

static ssize_t ram_write(void* cookie, const char* buf, size_t n)
{
	ram_handle_t* h = cookie;

	if (h->append)
		h->pos = h->file->size;

	if (!reserve(h->file, h->pos + n))
	{
		errno = ENOSPC;
		return -1;
	}

	if (h->pos > h->file->size)
		memset(h->file->data + h->file->size, 0, h->pos - h->file->size);

	memcpy(h->file->data + h->pos, buf, n);
	h->pos += n;
	if (h->pos > h->file->size)
		h->file->size = h->pos;

	h->file->mtime = time(NULL);
	return n;
}

#ifdef __LARGE64_FILES
static int ram_seek(void* cookie, _off64_t* off, int whence)
#else
static int ram_seek(void* cookie, off_t* off, int whence)
#endif
{
	ram_handle_t* h = cookie;
	long long pos;

	switch (whence)
	{
	case SEEK_SET: pos = *off; break;
	case SEEK_CUR: pos = (long long) h->pos + *off; break;
	case SEEK_END: pos = (long long) h->file->size + *off; break;
	default: errno = EINVAL; return -1;
	}

	if (pos < 0)
	{
		errno = EINVAL;
		return -1;
	}

	h->pos = (size_t) pos;
	*off = pos;
	return 0;
}

static int ram_close(void* cookie)
{
	ram_handle_t* h = cookie;

	ram_file_t* file = h->file;

	free(h);
	file->open_count--;
	file->last_use = ++use_counter;

	if (file->deleted && file->open_count == 0)
		delete_file(file);

	return 0;
}

static FILE* ram_fopen(const char* path, const char* mode)
{
	ram_file_t* file = find_file(path);
	ram_handle_t* h;
	FILE* fp;
	cookie_io_functions_t io = { ram_read, ram_write, ram_seek, ram_close };

	if (mode[0] == 'r' && !file)
	{
		errno = ENOENT;
		return NULL;
	}

	if (!file)
	{
		if ((file = calloc(1, sizeof(ram_file_t))) == NULL || (file->name = strdup(path)) == NULL)
		{
			free(file);
			errno = ENOMEM;
			return NULL;
		}
		file->mtime = time(NULL);
		file->next = files;
		files = file;
	}

	// "w": truncate (keeping the buffer for the new contents)
	if (mode[0] == 'w')
	{
		file->size = 0;
		file->mtime = time(NULL);
	}

	if ((h = calloc(1, sizeof(ram_handle_t))) == NULL)
	{
		errno = ENOMEM;
		return NULL;
	}

	h->file = file;
	h->append = (mode[0] == 'a');
	file->open_count++;
	file->last_use = ++use_counter;

	if ((fp = fopencookie(h, mode, io)) == NULL)
	{
		file->open_count--;
		free(h);
	}

	return fp;
}

// Removes every cached file that isn't open; returns how many
int ramcache_clear(void)
{
	int count = 0;
	ram_file_t *f = files, *next;

	for (; f; f = next)
	{
		next = f->next;
		if (f->open_count == 0)
		{
			delete_file(f);
			count++;
		}
	}

	return count;
}

FILE* __wrap_fopen(const char *path, const char *mode)
{
	if (ramcache_is_path(path))
		return ram_fopen(path, mode);

	return __real_fopen(path, mode);
}

int __wrap_stat(const char *path, struct stat *st)
{
	ram_file_t* file;

	if (is_cache_dir(path))
	{
		memset(st, 0, sizeof(struct stat));
		st->st_mode = S_IFDIR | 0777;
		return 0;
	}

	if (!ramcache_is_path(path))
		return __real_stat(path, st);

	if ((file = find_file(path)) == NULL)
	{
		errno = ENOENT;
		return -1;
	}

	memset(st, 0, sizeof(struct stat));
	st->st_mode = S_IFREG | 0777;
	st->st_size = file->size;
	st->st_mtime = file->mtime;
	st->st_atime = file->mtime;
	st->st_ctime = file->mtime;
	return 0;
}

static int ram_remove(const char* path)
{
	ram_file_t* file = find_file(path);

	if (!file)
	{
		errno = ENOENT;
		return -1;
	}

	// an open file is hidden now and freed on its last close
	if (file->open_count)
	{
		file->deleted = 1;
		return 0;
	}

	delete_file(file);
	return 0;
}

int __wrap_remove(const char *path)
{
	return ramcache_is_path(path) ? ram_remove(path) : __real_remove(path);
}

int __wrap_unlink(const char *path)
{
	return ramcache_is_path(path) ? ram_remove(path) : __real_unlink(path);
}

int __wrap_chmod(const char *path, mode_t mode)
{
	if (is_cache_dir(path) || ramcache_is_path(path))
		return 0;

	return __real_chmod(path, mode);
}

int __wrap_mkdir(const char *path, mode_t mode)
{
	if (is_cache_dir(path))
		return 0;

	return __real_mkdir(path, mode);
}
