#include "plib/os/os_string.h"

#if defined(_WIN32)
#include <string.h>
#else
#include <strings.h>
#endif

#include <ctype.h>

int os_stricmp(const char* s1, const char* s2)
{
#if defined(_WIN32)
    return _stricmp(s1, s2);
#else
    return strcasecmp(s1, s2);
#endif
}

int os_strnicmp(const char* s1, const char* s2, size_t n)
{
#if defined(_WIN32)
    return _strnicmp(s1, s2, n);
#else
    return strncasecmp(s1, s2, n);
#endif
}

char* os_strupr(char* s)
{
    char* tmp = s;
    while (*tmp) {
        *tmp = toupper((unsigned char)*tmp);
        tmp++;
    }
    return s;
}

char* os_strlwr(char* s)
{
    char* tmp = s;
    while (*tmp) {
        *tmp = tolower((unsigned char)*tmp);
        tmp++;
    }
    return s;
}
