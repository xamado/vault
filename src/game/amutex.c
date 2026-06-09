#include "game/amutex.h"

#include "plib/os/os_mutex.h"
#include <stddef.h>

static os_mutex* autorun_mutex = NULL;

bool autorun_mutex_create()
{
    autorun_mutex = os_mutex_create("InterplayGenericAutorunMutex");
    if (autorun_mutex == NULL) {
        return false;
    }
    if (!os_mutex_try_lock(autorun_mutex)) {
        os_mutex_destroy(autorun_mutex);
        autorun_mutex = NULL;
        return false;
    }
    return true;
}

void autorun_mutex_destroy()
{
    if (autorun_mutex != NULL) {
        os_mutex_destroy(autorun_mutex);
        autorun_mutex = NULL;
    }
}
