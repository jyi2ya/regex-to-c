#ifndef XUTILS_H_
#define XUTILS_H_

#include <stdio.h>
#include <stdlib.h>

#define panic(msg) \
    do { \
        fprintf(stderr, "panic@%d: %s\n", __LINE__, msg); \
        abort(); \
    } while (0)

#define unimplemented(msg) \
    do { \
        fprintf(stderr, "unimplemented@%d: %s\n", __LINE__, msg); \
        abort(); \
    } while (0)

extern void *xrealloc(void *ptr, size_t size);
extern void *xmalloc(size_t size);
extern char *xstrndup(const char *str, int len);
extern char *xstrdup(const char *str);
extern char *xstrcat(char *str, char *append);

#endif
