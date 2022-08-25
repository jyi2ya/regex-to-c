#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

void unimplemented(const char *msg) {
    fprintf(stderr, "unimplemented: %s\n", msg);
    abort();
}

void panic(const char *msg) {
    fprintf(stderr, "panic: %s", msg);
    abort();
}

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
    T_CHARSET, T_META, T_UNKNOWN, T_END
} TokenTypeTag;

typedef struct {
    TokenTypeTag type;
    bool allowed[256];
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

Token get_token_escaped(void) {
    // TODO 处理正则表达式中的转义字符
    Token result = Token_new();

    pattern_read_pos += 1;
    result.type = T_CHARSET;
    result.allowed[(int)*pattern_read_pos] = true;
    pattern_read_pos += 1;

    return result;
}

Token get_token_charset(void) {
    // TODO 更好地解析字符集
    Token result = Token_new();

    pattern_read_pos += 1;
    result.type = T_CHARSET;

    while (*pattern_read_pos != ']') {
        switch (*pattern_read_pos) {
            case '-' :
                if (pattern_read_pos[1] != ']') {
                    for (int i = pattern_read_pos[-1]; i <= pattern_read_pos[1]; ++i)
                        result.allowed[i] = true;
                    pattern_read_pos += 2;
                } else {
                    result.allowed[(int)*pattern_read_pos] = true;
                    pattern_read_pos += 1;
                }
                break;

            default:
                result.allowed[(int)*pattern_read_pos] = true;
                pattern_read_pos += 1;
                break;
        }
    }

    pattern_read_pos += 1;

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
    } else if (strchr("^$*+?{}()|", *pattern_read_pos) != NULL) {
        result.type = T_META;
        result.allowed[(int)*pattern_read_pos] = true;
        pattern_read_pos += 1;
    } else if (*pattern_read_pos == '.') {
        result.type = T_CHARSET;
        for (int i = 0; i < 256; ++i)
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

char *get_token_annotation(Token t) {
    char *result = strndup(t.anno_start, t.anno_len);
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
        if (lookahead.allowed['(']) {
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
        memset(result->u.allowed, 0, sizeof(bool) * 256);
        for (int i = 0; i < 256; ++i) {
            if (lookahead.allowed[i]) {
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
        if (lookahead.allowed['*']) {
            result->min = 0;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.allowed['+']) {
            result->min = 1;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.allowed['?']) {
            result->min = 0;
            result->max = 1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.allowed['{']) {
            unimplemented("bound");
        } else {
            unget_token(lookahead);
        }
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
                || (lookahead.type == T_META && lookahead.allowed[')'])
                || (lookahead.type == T_META && lookahead.allowed['|'])) {
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
                || (lookahead.type == T_META && lookahead.allowed[')'])) {
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
            if (lookahead.type == T_META && lookahead.allowed['|']) {
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
void atom%03d(int *len, int *success, char *str) { // %s\n\
    *len = 0, *success = 0;\n\
", cnt, atom->annotation);

        printf("    switch (*str) {\n");
        for (int i = 0; i < 256; ++i)
            if (atom->u.allowed[i])
                printf("        case %d: { *len = 1; *success = 1; break; }\n", i);
        printf("        default: { *len = 0; *success = 0; break; }\n");
        printf("    }\n");
        printf("}\n");
    } else {
        int id = translate_regex(atom->u.regex);

        printf("\
void atom%03d(int *len, int *success, char *str) { // %s\n\
    *len = 0, *success = 0;\n\
    regex%03d(len, success, str);\n\
}\n\
", cnt, atom->annotation, id);
    }

    return cnt++;
}

int translate_piece(PieceNode *piece) {
    static int cnt = 0;
    int id = translate_atom(piece->atom);
    printf("\n\
void piece%03d(int *len, int *success, char *str) { // %s\n\
    *len = 0, *success = 0;\n\
    for (int i = 0; i < %d; ++i) {\n\
        int s, l;\n\
        atom%03d(&l, &s, str);\n\
        if (!s) {\n\
            *success = 0;\n\
            return;\n\
        } else {\n\
            *len += l, str += l;\n\
        }\n\
    }\n\
    *success = 1;\n\
", cnt, piece->annotation, piece->min, id);

    if (piece->max == -1) {
        printf("\n\
    for (;;) {\n\
        int s, l;\n\
        atom%03d(&l, &s, str);\n\
        if (!s) {\n\
            return;\n\
        } else {\n\
            *len += l, str += l;\n\
        }\n\
    }\n\
", id);
    } else {
        printf("\n\
    for (int i = %d; i < %d; ++i) {\n\
        int s, l;\n\
        atom%03d(&l, &s, str);\n\
        if (!s) {\n\
            return;\n\
        } else {\n\
            *len += l, str += l;\n\
        }\n\
    }\n\
", piece->min, piece->max, id);
    }
    printf("}\n");

    return cnt++;
}

int translate_branch(BranchNode *branch) {
    static int cnt = 0;
    int *pieces = malloc(sizeof(int) * branch->size);
    for (int i = 0; i < branch->size; ++i)
        pieces[i] = translate_piece(branch->pieces[i]);
    printf("\n\
void branch%03d(int *len, int *success, char *str) { // %s\n\
    int l, s;\n\
    *success = 0, *len = 0; \n\
", cnt, branch->annotation);
    for (int i = 0; i < branch->size; ++i)
        printf("\n\
    piece%03d(&l, &s, str);\n\
    if (!s) {\n\
        *success = 0; *len = 0;\n\
        return;\n\
    } else {\n\
        *success = 1; *len += l; str += l;\n\
    }\n\
", pieces[i]);

    free(pieces);

    printf("}\n");
    return cnt++;
}

int translate_regex(RegexNode *regex) {
    static int cnt = 0;
    int *branches = malloc(sizeof(int) * regex->size);
    for (int i = 0; i < regex->size; ++i)
        branches[i] = translate_branch(regex->branches[i]);
    printf("\n\
void regex%03d(int *len, int *success, char *str) { // %s\n\
    int l, s;\n\
    *success = 0, *len = 0; \n\
", cnt, regex->annotation);
    for (int i = 0; i < regex->size; ++i)
        printf("\n\
    branch%03d(&l, &s, str);\n\
    if (s) {\n\
        *success = 1; *len = l;\n\
        return;\n\
    } else {\n\
        *success = 0; *len = 0;\n\
    }\n\
", branches[i]);

    printf("}\n");

    free(branches);
    return cnt++;
}

void do_you_like_c(RegexNode *regex) {
    int id = translate_regex(regex);
    printf("\n\
int match(int *len, int *success, char *str) {\n\
    if (len == NULL || success == NULL) {\n\
        int l, s;\n\
        regex%03d(len, success, str);\n\
    } else {\n\
        regex%03d(len, success, str);\n\
    }\n\
    return *success;\n\
}\n", id, id);
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        help();
    pattern_read_pos = argv[1];

    RegexNode *result = parse_regex();
    do_you_like_c(result);
    return 0;
}
