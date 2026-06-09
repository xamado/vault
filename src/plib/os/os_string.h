#ifndef OS_STRING_H
#define OS_STRING_H

#include <stddef.h>
#include "plib/math.h"

int os_stricmp(const char* s1, const char* s2);
int os_strnicmp(const char* s1, const char* s2, size_t n);

char* os_strupr(char* s);
char* os_strlwr(char* s);

#endif // OS_STRING_H
