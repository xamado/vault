#ifndef FALLOUT_PLIB_OS_WINDOW_H_
#define FALLOUT_PLIB_OS_WINDOW_H_

#include <stdbool.h>
#include "plib/math.h"

void os_window_messagebox(const char* title, const char* message);
void os_window_set_title(const char* title);

bool os_window_create(const char* title, int width, int height);
void os_window_destroy(void);

void os_window_set_palette(int start, int count, const unsigned char* palette);
void os_window_lock(void** pixels, int* pitch);
void os_window_unlock(void);
void os_window_present(void);

#endif /* FALLOUT_PLIB_OS_WINDOW_H_ */
