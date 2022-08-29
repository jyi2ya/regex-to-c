#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#define unimplemented(msg) \
    do { \
        fprintf(stderr, "unimplemented@%d: %s\n", __LINE__, msg); \
        abort(); \
    } while (0)

#define panic(msg) \
    do { \
        fprintf(stderr, "panic@%d: %s\n", __LINE__, msg); \
        abort(); \
    } while (0)

void *xrealloc(void *ptr, size_t size) {
    ptr = realloc(ptr, size);
    if (ptr == NULL) {
        panic("insufficent memory");
    }
    return ptr;
}

void *xmalloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr == NULL) {
        panic("insufficent memory");
    }
    return ptr;
}

char *xstrndup(const char *str, int len) {
    char *result = xmalloc(sizeof(char) * (len + 1));
    strncpy(result, str, len);
    result[len] = '\0';
    return result;
}

char *xstrdup(const char *str) {
    char *result = NULL;

    if (str == NULL) {
        result = NULL;
    } else {
        result = xmalloc((strlen(str) + 1) * sizeof(char));
        strcpy(result, str);
    }

    return result;
}

char *xstrcat(char *str, const char *append) {
    if (str == NULL) {
        return xstrdup(append);
    } else {
        if (append == NULL) {
            return str;
        } else {
            str = xrealloc(str, strlen(str) + strlen(append) + 1);
            strcat(str, append);
        }
    }
    return str;
}

typedef enum {
    T_CHARSET, T_META, T_UNKNOWN, T_END, T_BOUND
} TokenTypeTag;

typedef struct {
    TokenTypeTag type;
    union {
        bool allowed[256];
        int bound[2];
        int metachar;
    } u;
    const char *anno_start;
    int anno_len;
} Token;

char *pattern_read_pos = NULL;
Token token_unget;
bool has_unget_token = false;

Token Token_new(void) {
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

Token get_token_escaped(void) {
    Token result = Token_new();

    pattern_read_pos += 1;
    result.type = T_CHARSET;
    char c = *pattern_read_pos;
    switch (c) {
    case '\0':
        panic("regex expression should not end with '\\'");
        break;
    case 'd':
        fill_by_range('0', '9', result.u.allowed, true);
        break;
    case 'D':
        fill_by_range(1, 255, result.u.allowed, true);
        fill_by_range('0', '9', result.u.allowed, false);
        break;
    case 'f':
        fill_by_char('\x0c', result.u.allowed, true);
        break;
    case 'n':
        fill_by_char('\x0a', result.u.allowed, true);
        break;
    case 'r':
        fill_by_char('\x0d', result.u.allowed, true);
        break;
    case 's':
        fill_by_string(" \f\n\r\t\v", result.u.allowed, true);
        break;
    case 'S':
        fill_by_range(1, 255, result.u.allowed, true);
        fill_by_string(" \f\n\r\t\v", result.u.allowed, false);
        break;
    case 't':
        fill_by_char('\x09', result.u.allowed, true);
        break;
    case 'v':
        fill_by_char('\x0b', result.u.allowed, true);
        break;
    case 'w':
        fill_by_range('a', 'z', result.u.allowed, true);
        fill_by_range('A', 'Z', result.u.allowed, true);
        fill_by_range('0', '9', result.u.allowed, true);
        fill_by_char('-', result.u.allowed, true);
        break;
    case 'W':
        fill_by_range(1, 255, result.u.allowed, true);
        fill_by_range('a', 'z', result.u.allowed, false);
        fill_by_range('A', 'Z', result.u.allowed, false);
        fill_by_range('0', '9', result.u.allowed, false);
        fill_by_char('-', result.u.allowed, false);
        break;
    case 'x':
        if (pattern_read_pos[1] == '\0' || pattern_read_pos[2] == '\0') {
            panic("'\\xnn' needs two more xdigits");
        }
        if (isxdigit(pattern_read_pos[1]) && isxdigit(pattern_read_pos[2])) {
            int xd;
            sscanf(pattern_read_pos + 1, "%x", &xd);
            fill_by_char(xd, result.u.allowed, true);
            pattern_read_pos += 2;
        } else {
            panic("'\\xnn' needs two xdigits");
        }
        break;
    default:
        result.u.allowed[(int)*pattern_read_pos] = true;
        break;
    }
    pattern_read_pos += 1;

    return result;
}

#define cmp_class(s, class_name, shift) \
    (!strncmp(s, class_name, strlen(class_name)) && (shift = strlen(class_name)) > 0)

Token get_token_charset(void) {
    Token result = Token_new();
    result.type = T_CHARSET;

    bool fill = true;

    pattern_read_pos += 1;
    // for the first character in the bracket
    char first = *pattern_read_pos;
    if (first == ']' || first == '-') {
        result.u.allowed[(int)first] = fill;
        pattern_read_pos += 1;
    } else if (first == '^') {
        fill_by_range(1, 255, result.u.allowed, fill);
        fill = false;
        pattern_read_pos += 1;
        first = *pattern_read_pos;
        if (first == ']' || first == '-') {
            result.u.allowed[(int)first] = fill;
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
                        (int)pattern_read_pos[1], result.u.allowed, fill);
                pattern_read_pos += 1;
            } else { // ']' can be the last character in bracket
                result.u.allowed[(int)*pattern_read_pos] = fill;
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
                fill_by_range(0, 255, result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "alnum", shift)) {
                fill_by_range('a', 'z', result.u.allowed, fill);
                fill_by_range('A', 'Z', result.u.allowed, fill);
                fill_by_range('0', '9', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "alpha", shift)) {
                fill_by_range('a', 'z', result.u.allowed, fill);
                fill_by_range('A', 'Z', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "blank", shift)) {
                fill_by_string(" \t", result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "cntrl", shift)) {
                fill_by_range('\x01', '\x1F', result.u.allowed, fill);
                fill_by_char('\x7F', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "digit", shift)) {
                fill_by_range('0', '9', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "graph", shift)) {
                fill_by_range('\x21', '\x7E', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "lower", shift)) {
                fill_by_range('a', 'z', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "print", shift)) {
                fill_by_range('\x20', '\x7E', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "punct", shift)) {
                fill_by_string("][!\"#$%&'()*+,./:;<=>?@\\^_`{|}~-", result.u.allowed, \
                        fill);
            } else if (cmp_class(pattern_read_pos, "space", shift)) {
                fill_by_string(" \t\r\n\v\f", result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "upper", shift)) {
                fill_by_range('A', 'Z', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "word", shift)) {
                fill_by_range('a', 'z', result.u.allowed, fill);
                fill_by_range('A', 'Z', result.u.allowed, fill);
                fill_by_range('0', '9', result.u.allowed, fill);
                fill_by_char('-', result.u.allowed, fill);
            } else if (cmp_class(pattern_read_pos, "xdigit", shift)) {
                fill_by_range('a', 'f', result.u.allowed, fill);
                fill_by_range('A', 'F', result.u.allowed, fill);
                fill_by_range('0', '9', result.u.allowed, fill);
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
            result.u.allowed[(int)*pattern_read_pos] = fill;
            pattern_read_pos += 1;
            break;
        }
    }

    pattern_read_pos += 1;

    return result;
}

Token get_token_bound() {
    Token result = Token_new();
    result.type = T_BOUND;

    enum {
        S_START, S_READLEFT, S_READLEFT_BEGIN, S_READRIGHT, S_END
    } state = S_START;

    while (state != S_END) {
        switch (state) {
        case S_START:
            if (*pattern_read_pos == '{') {
                result.u.bound[0] = 0;
                pattern_read_pos += 1;
                state = S_READLEFT_BEGIN;
            } else {
                panic("illegal bound");
            }
            break;

        case S_READLEFT_BEGIN:
            if (isdigit(*pattern_read_pos)) {
                result.u.bound[0] = *pattern_read_pos - '0';
                pattern_read_pos += 1;
                state = S_READLEFT;
            } else {
                panic("illegal bound");
            }
            break;

        case S_READLEFT:
            if (*pattern_read_pos == ',') {
                result.u.bound[1] = -1;
                pattern_read_pos += 1;
                state = S_READRIGHT;
            } else if (*pattern_read_pos == '}') {
                result.u.bound[1] = result.u.bound[0];
                pattern_read_pos += 1;
                state = S_END;
            } else if (isdigit(*pattern_read_pos)) {
                result.u.bound[0] *= 10;
                result.u.bound[0] += *pattern_read_pos - '0';
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
                if (result.u.bound[1] == -1) {
                    result.u.bound[1] = *pattern_read_pos - '0';
                } else {
                    result.u.bound[1] *= 10;
                    result.u.bound[1] += *pattern_read_pos - '0';
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

    if (result.u.bound[1] >= 0 && result.u.bound[0] > result.u.bound[1]) {
        panic("illegal bound");
    }

    return result;
}

Token get_token(void) {
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
        result.u.metachar = *pattern_read_pos;
        pattern_read_pos += 1;
    } else if (*pattern_read_pos == '.') {
        result.type = T_CHARSET;
        // `.` should not match '\0'
        for (int i = 1; i < 256; ++i)
            result.u.allowed[i] = true;
        pattern_read_pos += 1;
    } else if (*pattern_read_pos == '[') {
        result = get_token_charset();
    } else {
        result.type = T_CHARSET;
        result.u.allowed[(int)*pattern_read_pos] = true;
        pattern_read_pos += 1;
    }

    if (result.anno_start == NULL) {
        result.anno_start = pattern_read_pos_old;
        result.anno_len = pattern_read_pos - pattern_read_pos_old;
    }

    return result;
}

char *get_token_annotation(Token t) {
    char *result = xstrndup(t.anno_start, t.anno_len);
    return result;
}

void unget_token(Token t) {
    token_unget = t;
    has_unget_token = true;
}

typedef struct BranchNode BranchNode;

typedef struct RegexNode {
    char *annotation;
    BranchNode **branches;
    int size;
} RegexNode;

typedef struct {
    char *annotation;
    bool is_simple_atom;
    union {
        RegexNode *regex;
        bool allowed[256];
    } u;
} AtomNode;

AtomNode *parse_atom(void) {
    AtomNode *result = xmalloc(sizeof(AtomNode));

    RegexNode *parse_regex(void);

    Token lookahead = get_token();

    if (lookahead.type == T_META) {
        if (lookahead.u.metachar == '(') {
            result->is_simple_atom = false;
            result->u.regex = parse_regex();
            result->annotation = xstrdup("(");
            result->annotation = xstrcat(result->annotation, result->u.regex->annotation);
            result->annotation = xstrcat(result->annotation, ")");
        } else {
            panic("illegal regex");
        }
    } else if (lookahead.type == T_CHARSET) {
        result->is_simple_atom = true;
        memset(&result->u, 0, sizeof(result->u));
        for (int i = 0; i < 256; ++i) {
            if (lookahead.u.allowed[i]) {
                result->u.allowed[i] = true;
            }
        }
        result->annotation = get_token_annotation(lookahead);
    } else {
        panic("illegal regex");
    }

    assert(result->annotation != NULL);
    return result;
}

typedef struct {
    char *annotation;
    AtomNode *atom;
    int min, max;
} PieceNode;

PieceNode *parse_piece(void) {
    PieceNode *result = xmalloc(sizeof(PieceNode));
    result->atom = parse_atom();
    result->min = 1;
    result->max = 1;
    result->annotation = xstrdup(result->atom->annotation);

    Token lookahead = get_token();
    if (lookahead.type == T_META) {
        if (lookahead.u.metachar == '*') {
            result->min = 0;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.u.metachar == '+') {
            result->min = 1;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.u.metachar == '?') {
            result->min = 0;
            result->max = 1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else {
            unget_token(lookahead);
        }
    } else if (lookahead.type == T_BOUND){
        result->min = lookahead.u.bound[0];
        result->max = lookahead.u.bound[1];
        result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
    } else {
        unget_token(lookahead);
    }

    assert(result->annotation != NULL);
    return result;
}

struct BranchNode {
    char *annotation;
    PieceNode **pieces;
    int size;
};

BranchNode *parse_branch(void) {
    BranchNode *result = xmalloc(sizeof(BranchNode));
    result->size = 0;
    result->pieces = NULL;
    result->annotation = NULL;

    for (;;) {
        Token lookahead = get_token();

        if (lookahead.type == T_END
                || (lookahead.type == T_META && lookahead.u.metachar == ')')
                || (lookahead.type == T_META && lookahead.u.metachar == '|')) {
            unget_token(lookahead);
            break;
        }

        unget_token(lookahead);
        result->pieces = xrealloc(result->pieces, (result->size + 1) * sizeof(PieceNode*));
        result->pieces[result->size] = parse_piece();
        result->annotation = xstrcat(result->annotation, result->pieces[result->size]->annotation);

        result->size += 1;
    }

    assert(result->annotation != NULL);
    return result;
}

RegexNode *parse_regex(void) {
    RegexNode *result = xmalloc(sizeof(RegexNode));
    int first = true;

    result->size = 0;
    result->branches = NULL;
    result->annotation = NULL;

    for (;;) {
        Token lookahead = get_token();

        if (lookahead.type == T_END
                || (lookahead.type == T_META && lookahead.u.metachar == ')')) {
            break;
        }

        if (first) {
            unget_token(lookahead);
            result->branches = xrealloc(result->branches, (result->size + 1) * sizeof(BranchNode*));
            result->branches[result->size] = parse_branch();
            result->annotation = xstrcat(result->annotation, result->branches[result->size]->annotation);
            result->size += 1;
            first = false;
        } else {
            if (lookahead.type == T_META && lookahead.u.metachar == '|') {
                result->branches = xrealloc(result->branches, (result->size + 1) * sizeof(BranchNode*));
                result->branches[result->size] = parse_branch();
                result->annotation = xstrcat(result->annotation, "|");
                result->annotation = xstrcat(result->annotation, result->branches[result->size]->annotation);
                result->size += 1;
            } else {
                panic("illegal regex");
            }
        }
    }

    assert(result->annotation != NULL);
    return result;
}

void help(void) {
    unimplemented("help");
}

int translate_atom(AtomNode *atom) {
    static int cnt = 0;
    int translate_regex(RegexNode *regex);

    if (atom->is_simple_atom) {
        printf("\
int atom%03d(char *str) { // %s\n\
    switch (((int)*str + 256) %% 256) {\n\
", cnt, atom->annotation);
        for (int i = 0; i < 256; ++i)
            if (atom->u.allowed[i])
                printf("        case %d: return 1;\n", i);
        printf("\
    }\n\
    return -1;\n\
}\n");
    } else {
        int id = translate_regex(atom->u.regex);

        printf("\
int atom%03d(char *str) { // %s\n\
    return regex%03d(str);\n\
}\n\
", cnt, atom->annotation, id);
    }

    return cnt++;
}

int translate_piece(PieceNode *piece) {
    static int cnt = 0;
    int id = translate_atom(piece->atom);
    printf("\n\
int piece%03d(char *str) { // %s\n\
    char *str_old = str;\n\
    for (int i = 0; i < %d; ++i) {\n\
        int len = atom%03d(str);\n\
        if (len == -1) {\n\
            return -1;\n\
        } else {\n\
            str += len;\n\
        }\n\
    }\n\
", cnt, piece->annotation, piece->min, id);

    if (piece->max == -1) {
        printf("\n\
    int allow_empty_match = 1;\n\
    for (;;) {\n\
        int len = atom%03d(str);\n\
        if (len == -1) {\n\
            break;\n\
        } else if (len == 0) {\n\
            if (allow_empty_match) {\n\
                allow_empty_match = 0;\n\
            } else {\n\
                break;\n\
            }\n\
        } else {\n\
            str += len;\n\
        }\n\
    }\n\
", id);
    } else {
        printf("\n\
    for (int i = %d; i < %d; ++i) {\n\
        int len = atom%03d(str);\n\
        if (len == -1) {\n\
            break;\n\
        } else {\n\
            str += len;\n\
        }\n\
    }\n\
", piece->min, piece->max, id);
    }

    printf("\
    return str - str_old;\n\
}\n");

    return cnt++;
}

int translate_branch(BranchNode *branch) {
    static int cnt = 0;
    int *pieces = malloc(sizeof(int) * branch->size);
    for (int i = 0; i < branch->size; ++i)
        pieces[i] = translate_piece(branch->pieces[i]);
    printf("\n\
int branch%03d(char *str) { // %s\n\
    char *str_old = str;\n\
    int len = 0;\n\
", cnt, branch->annotation);
    for (int i = 0; i < branch->size; ++i)
        printf("\n\
    len = piece%03d(str);\n\
    if (len == -1) {\n\
        return -1;\n\
    } else {\n\
        str += len;\n\
    }\n\
", pieces[i]);

    free(pieces);

    printf("\
    return str - str_old;\n\
}\n");
    return cnt++;
}

int translate_regex(RegexNode *regex) {
    static int cnt = 0;
    int *branches = malloc(sizeof(int) * regex->size);
    for (int i = 0; i < regex->size; ++i)
        branches[i] = translate_branch(regex->branches[i]);
    printf("\n\
int regex%03d(char *str) { // %s\n\
    int len = 0;\n\
    int max = -1;\n\
", cnt, regex->annotation);
    for (int i = 0; i < regex->size; ++i)
        printf("\n\
    len = branch%03d(str);\n\
    if (len > max) {\n\
        max = len;\n\
    }\n\
", branches[i]);

    printf("\
    return max;\n\
}\n");

    free(branches);
    return cnt++;
}

void do_you_like_c(RegexNode *regex) {
    int id = translate_regex(regex);
    printf("\n\
int match(char *str) {\n\
    return regex%03d(str);\n\
}\n", id);
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        help();
    pattern_read_pos = argv[1];

    RegexNode *result = parse_regex();
    do_you_like_c(result);
    return 0;
}
