#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#include "xutils.h"
#include "token.h"

static char *pattern_read_pos = NULL;
static Token token_unget;
static bool has_unget_token = false;

static Token Token_new(void) {
    Token result;
    memset(&result, 0, sizeof(result));
    result.type = T_UNKNOWN;
    return result;
}

static void fill_by_range(int begin, int end, bool *ch, bool fill) {
    for (int i = begin; i <= end; ++i) {
        ch[i] = fill;
    }
}

static void fill_by_string(char *s, bool *ch, bool fill) {
    while (*s) {
        ch[(int)*s] = fill;
        s++;
    }
}

static void fill_by_char(int c, bool *ch, bool fill) {
    ch[c] = fill;
}

static Token get_token_escaped(void) {
    Token result = Token_new();

    pattern_read_pos += 1;
    result.type = T_CHARSET;
    char c = *pattern_read_pos;
    switch (c) {
    case '\0':
        panic("regex expression should not end with '\\'");
        break;
    case 'd':
        fill_by_range('0', '9', result.allowed, true);
        break;
    case 'D':
        fill_by_range(1, 255, result.allowed, true);
        fill_by_range('0', '9', result.allowed, false);
        break;
    case 'f':
        fill_by_char('\x0c', result.allowed, true);
        break;
    case 'n':
        fill_by_char('\x0a', result.allowed, true);
        break;
    case 'r':
        fill_by_char('\x0d', result.allowed, true);
        break;
    case 's':
        fill_by_string(" \f\n\r\t\v", result.allowed, true);
        break;
    case 'S':
        fill_by_range(1, 255, result.allowed, true);
        fill_by_string(" \f\n\r\t\v", result.allowed, false);
        break;
    case 't':
        fill_by_char('\x09', result.allowed, true);
        break;
    case 'v':
        fill_by_char('\x0b', result.allowed, true);
        break;
    case 'w':
        fill_by_range('a', 'z', result.allowed, true);
        fill_by_range('A', 'Z', result.allowed, true);
        fill_by_range('0', '9', result.allowed, true);
        fill_by_char('-', result.allowed, true);
        break;
    case 'W':
        fill_by_range(1, 255, result.allowed, true);
        fill_by_range('a', 'z', result.allowed, false);
        fill_by_range('A', 'Z', result.allowed, false);
        fill_by_range('0', '9', result.allowed, false);
        fill_by_char('-', result.allowed, false);
        break;
    case 'x':
        if (pattern_read_pos[1] == '\0' || pattern_read_pos[2] == '\0') {
            panic("'\\xnn' needs two more xdigits");
        }
        if (isxdigit(pattern_read_pos[1]) && isxdigit(pattern_read_pos[2])) {
            int xd;
            sscanf(pattern_read_pos + 1, "%x", &xd);
            fill_by_char(xd, result.allowed, true);
            pattern_read_pos += 2;
        } else {
            panic("'\\xnn' needs two xdigits");
        }
        break;
    default:
        result.allowed[(int)*pattern_read_pos] = true;
        break;
    }
    pattern_read_pos += 1;

    return result;
}

#define cmp_class(s, class_name, shift) \
    (!strncmp(s, class_name, strlen(class_name)) && (shift = strlen(class_name)) > 0)

static Token get_token_charset(void) {
    Token result = Token_new();
    result.type = T_CHARSET;

    bool fill = true;

    pattern_read_pos += 1;
    // for the first character in the bracket
    char first = *pattern_read_pos;
    if (first == ']' || first == '-') {
        result.allowed[(int)first] = fill;
        pattern_read_pos += 1;
    } else if (first == '^') {
        fill_by_range(1, 255, result.allowed, fill);
        fill = false;
        pattern_read_pos += 1;
        first = *pattern_read_pos;
        if (first == ']' || first == '-') {
            result.allowed[(int)first] = fill;
            pattern_read_pos += 1;
        }
    }

    while (*pattern_read_pos != ']') {
        switch (*pattern_read_pos) {
        case '\0':
            panic("bracket should end with ']'");
            break;
        case '-':
            if (pattern_read_pos[1] != ']') {
                fill_by_range((int)pattern_read_pos[-1], \
                        (int)pattern_read_pos[1], result.allowed, fill);
                pattern_read_pos += 1;
            } else { // ']' can be the last character in bracket
                result.allowed[(int)*pattern_read_pos] = fill;
            }
            pattern_read_pos += 1;
            break;
        case '[':
            if (pattern_read_pos[1] != ':') {
                goto not_special;
            }

            // character class
            // support: ascii, alnum, alpha, blank, cntrl, digit, graph, lower,
            // print, punct, space, upper, word, xdigit
            pattern_read_pos += 2;
            int shift = 0;
            if (cmp_class(pattern_read_pos, "ascii", shift)) {
                fill_by_range(0, 255, result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "alnum", shift)) {
                fill_by_range('a', 'z', result.allowed, fill);
                fill_by_range('A', 'Z', result.allowed, fill);
                fill_by_range('0', '9', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "alpha", shift)) {
                fill_by_range('a', 'z', result.allowed, fill);
                fill_by_range('A', 'Z', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "blank", shift)) {
                fill_by_string(" \t", result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "cntrl", shift)) {
                fill_by_range('\x01', '\x1F', result.allowed, fill);
                fill_by_char('\x7F', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "digit", shift)) {
                fill_by_range('0', '9', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "graph", shift)) {
                fill_by_range('\x21', '\x7E', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "lower", shift)) {
                fill_by_range('a', 'z', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "print", shift)) {
                fill_by_range('\x20', '\x7E', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "punct", shift)) {
                fill_by_string("][!\"#$%&'()*+,./:;<=>?@\\^_`{|}~-", result.allowed, \
                        fill);
            } else if (cmp_class(pattern_read_pos, "space", shift)) {
                fill_by_string(" \t\r\n\v\f", result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "upper", shift)) {
                fill_by_range('A', 'Z', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "word", shift)) {
                fill_by_range('a', 'z', result.allowed, fill);
                fill_by_range('A', 'Z', result.allowed, fill);
                fill_by_range('0', '9', result.allowed, fill);
                fill_by_char('-', result.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "xdigit", shift)) {
                fill_by_range('a', 'f', result.allowed, fill);
                fill_by_range('A', 'F', result.allowed, fill);
                fill_by_range('0', '9', result.allowed, fill);
            } else {
                panic("invalid character class name");
            }
            pattern_read_pos += shift;
            if (strncmp(pattern_read_pos, ":]", 2) != 0) {
                panic("character class should end with \":]\"");
            }
            pattern_read_pos += 2;
            break;
        default:
not_special:
            result.allowed[(int)*pattern_read_pos] = fill;
            pattern_read_pos += 1;
            break;
        }
    }

    pattern_read_pos += 1;

    return result;
}

static Token get_token_bound() {
    Token result = Token_new();
    result.type = T_BOUND;

    enum {
        S_START, S_READLEFT, S_READLEFT_BEGIN, S_READRIGHT, S_END
    } state = S_START;

    while (state != S_END) {
        switch (state) {
        case S_START:
            if (*pattern_read_pos == '{') {
                result.bound[0] = 0;
                pattern_read_pos += 1;
                state = S_READLEFT_BEGIN;
            } else {
                panic("illegal bound");
            }
            break;

        case S_READLEFT_BEGIN:
            if (isdigit(*pattern_read_pos)) {
                result.bound[0] = *pattern_read_pos - '0';
                pattern_read_pos += 1;
                state = S_READLEFT;
            } else {
                panic("illegal bound");
            }
            break;

        case S_READLEFT:
            if (*pattern_read_pos == ',') {
                result.bound[1] = -1;
                pattern_read_pos += 1;
                state = S_READRIGHT;
            } else if (*pattern_read_pos == '}') {
                result.bound[1] = result.bound[0];
                pattern_read_pos += 1;
                state = S_END;
            } else if (isdigit(*pattern_read_pos)) {
                result.bound[0] *= 10;
                result.bound[0] += *pattern_read_pos - '0';
                pattern_read_pos += 1;
                state = S_READLEFT;
            } else {
                panic("illegal bound");
            }
            break;

        case S_READRIGHT:
            if (*pattern_read_pos == '}') {
                pattern_read_pos += 1;
                state = S_END;
            } else if (isdigit(*pattern_read_pos)) {
                if (result.bound[1] == -1) {
                    result.bound[1] = *pattern_read_pos - '0';
                } else {
                    result.bound[1] *= 10;
                    result.bound[1] += *pattern_read_pos - '0';
                }
                pattern_read_pos += 1;
                state = S_READRIGHT;
            } else {
                panic("illegal bound");
            }
            break;
        default:
            panic("illegal bound");
        }
    }

    if (result.bound[1] >= 0 && result.bound[0] > result.bound[1]) {
        panic("illegal bound");
    }

    return result;
}

extern Token get_token(void) {
    const char *pattern_read_pos_old = pattern_read_pos;

    Token result = Token_new();

    if (has_unget_token) {
        result = token_unget;
        has_unget_token = false;
    } else if (*pattern_read_pos == '\0') {
        result.type = T_END;
    } else if (*pattern_read_pos == '\\') {
        result = get_token_escaped();
    } else if (*pattern_read_pos == '{') {
        result = get_token_bound();
    } else if (strchr("^$*+?{}()|", *pattern_read_pos) != NULL) {
        result.type = T_META;
        result.metachar = *pattern_read_pos;
        pattern_read_pos += 1;
    } else if (*pattern_read_pos == '.') {
        result.type = T_CHARSET;
        // `.` should not match '\0'
        for (int i = 1; i < 256; ++i)
            result.allowed[i] = true;
        pattern_read_pos += 1;
    } else if (*pattern_read_pos == '[') {
        result = get_token_charset();
    } else {
        result.type = T_CHARSET;
        result.allowed[(int)*pattern_read_pos] = true;
        pattern_read_pos += 1;
    }

    if (result.anno_start == NULL) {
        result.anno_start = pattern_read_pos_old;
        result.anno_len = pattern_read_pos - pattern_read_pos_old;
    }

    return result;
}

extern char *get_token_annotation(Token t) {
    char *result = xstrndup(t.anno_start, t.anno_len);
    return result;
}

extern void unget_token(Token t) {
    token_unget = t;
    has_unget_token = true;
}

extern void set_pattern_str(char *str) {
    pattern_read_pos = str;
}
