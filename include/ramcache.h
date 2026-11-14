#ifndef __RAMCACHE_H__
#define __RAMCACHE_H__

// Keeps APOLLO_LOCAL_CACHE files in EE memory (no USB storage). See source/ramcache.c
void ramcache_enable(void);
int ramcache_enabled(void);
int ramcache_is_path(const char* path);
int ramcache_clear(void);

#endif
