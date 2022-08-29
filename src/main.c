#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#include "xutils.h"
#include "token.h"

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
            result->annotation = xstrcat(result->annotation, xstrdup(result->u.regex->annotation));
            result->annotation = xstrcat(result->annotation, xstrdup(")"));
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
        result->annotation = xstrcat(result->annotation, xstrdup(result->pieces[result->size]->annotation));

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
            result->annotation = xstrcat(result->annotation, xstrdup(result->branches[result->size]->annotation));
            result->size += 1;
            first = false;
        } else {
            if (lookahead.type == T_META && lookahead.u.metachar == '|') {
                result->branches = xrealloc(result->branches, (result->size + 1) * sizeof(BranchNode*));
                result->branches[result->size] = parse_branch();
                result->annotation = xstrcat(result->annotation, xstrdup("|"));
                result->annotation = xstrcat(result->annotation, xstrdup(result->branches[result->size]->annotation));
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

void free_regex_tree(RegexNode *regex) {
    for (int i = 0; i < regex->size; ++i) {
        BranchNode *branch = regex->branches[i];
        for (int j = 0; j < branch->size; ++j) {
            PieceNode *piece = branch->pieces[j];

            if (!piece->atom->is_simple_atom) {
                free_regex_tree(piece->atom->u.regex);
            }

            free(piece->atom->annotation);
            free(piece->atom);

            free(piece->annotation);
            free(piece);
        }
        free(branch->annotation);
        free(branch->pieces);
        free(branch);
    }
    free(regex->annotation);
    free(regex->branches);
    free(regex);
}

int main(int argc, char *argv[]) {
    if (argc < 2)
        help();
    set_pattern_str(argv[1]);
    RegexNode *result = parse_regex();
    do_you_like_c(result);
    free_regex_tree(result);
    return 0;
}
