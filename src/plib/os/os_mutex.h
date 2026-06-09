#ifndef FALLOUT_PLIB_OS_MUTEX_H_
#define FALLOUT_PLIB_OS_MUTEX_H_

#include <stdbool.h>

typedef struct os_mutex os_mutex;

os_mutex* os_mutex_create(const char* name);
bool os_mutex_try_lock(os_mutex* mutex);
void os_mutex_destroy(os_mutex* mutex);

#endif /* FALLOUT_PLIB_OS_MUTEX_H_ */
