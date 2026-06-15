#ifndef FALLOUT_GAME_SFXCACHE_H_
#define FALLOUT_GAME_SFXCACHE_H_
#include <stdint.h>

// The maximum number of sound effects that can be loaded and played
// simultaneously.
#define SOUND_EFFECTS_MAX_COUNT 4

int sfxc_init(int cache_size, const char* effectsPath);
void sfxc_exit();
int sfxc_is_initialized();
void sfxc_flush();
intptr_t sfxc_cached_open(const char* fname, int mode, ...);
int sfxc_cached_close(intptr_t handle);
int sfxc_cached_read(intptr_t handle, void* buf, unsigned int size);
int sfxc_cached_write(intptr_t handle, const void* buf, unsigned int size);
long sfxc_cached_seek(intptr_t handle, long offset, int origin);
long sfxc_cached_tell(intptr_t handle);
long sfxc_cached_file_size(intptr_t handle);

#endif /* FALLOUT_GAME_SFXCACHE_H_ */
