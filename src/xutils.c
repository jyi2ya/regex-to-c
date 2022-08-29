#include <stdlib.h>
#include <string.h>

#include "xutils.h"

extern void *xrealloc(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        panic("insufficent memory");
    }
    return ptr;
}

extern void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        panic("insufficent memory");
    }
    return ptr;
}

extern char *xstrndup(const char *str, int len) {
    char *result = xmalloc(sizeof(char) * (len + 1));
    strncpy(result, str, len);
    result[len] = '\0';
    return result;
}

extern char *xstrdup(const char *str) {
    char *result = NULL;

    if (str == NULL) {
        result = NULL;
    } else {
        result = xmalloc((strlen(str) + 1) * sizeof(char));
        strcpy(result, str);
    }

    return result;
}

extern char *xstrcat(char *str, char *append) {
    if (str == NULL) {
        return append;
    } else {
        if (append == NULL) {
            return str;
        } else {
            str = xrealloc(str, strlen(str) + strlen(append) + 1);
            strcat(str, append);
            free(append);
        }
    }
    return str;
}
