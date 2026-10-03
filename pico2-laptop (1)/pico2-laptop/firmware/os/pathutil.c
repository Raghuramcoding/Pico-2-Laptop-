#include "os.h"
#include <string.h>

/* Turn (cwd, arg) into a clean absolute path: handles ".", ".." and "//". */
int path_resolve(const char *cwd, const char *arg, char *out, size_t outsz)
{
    char tmp[200];
    if (arg[0] == '/') {
        if (strlen(arg) >= sizeof tmp) return -1;
        strcpy(tmp, arg);
    } else {
        if (strlen(cwd) + 1 + strlen(arg) >= sizeof tmp) return -1;
        strcpy(tmp, cwd);
        strcpy(tmp + strlen(tmp), "/");
        strcpy(tmp + strlen(tmp), arg);
    }
    if (outsz < 2) return -1;
    size_t o = 1;
    out[0] = '/'; out[1] = 0;
    const char *p = tmp;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *s = p;
        while (*p && *p != '/') p++;
        size_t len = (size_t)(p - s);
        if (len == 1 && s[0] == '.') continue;
        if (len == 2 && s[0] == '.' && s[1] == '.') {
            size_t i = o;
            while (i > 1 && out[i - 1] != '/') i--;
            o = (i > 1) ? i - 1 : 1;
            out[o] = 0;
            continue;
        }
        if (o + (o > 1 ? 1 : 0) + len + 1 > outsz) return -1;
        if (o > 1) out[o++] = '/';
        memcpy(out + o, s, len);
        o += len;
        out[o] = 0;
    }
    return 0;
}
