#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "token.h"
#include "xutils.h"
#include "regtree.h"

static RegexNode *parse_regex(void);

static AtomNode *parse_atom(void) {
    AtomNode *result = xmalloc(sizeof(AtomNode));

    Token lookahead = get_token();

    if (lookahead.type == T_META) {
        if (lookahead.metachar == '(') {
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

static PieceNode *parse_piece(void) {
    PieceNode *result = xmalloc(sizeof(PieceNode));
    result->atom = parse_atom();
    result->min = 1;
    result->max = 1;
    result->annotation = xstrdup(result->atom->annotation);

    Token lookahead = get_token();
    if (lookahead.type == T_META) {
        if (lookahead.metachar == '*') {
            result->min = 0;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.metachar == '+') {
            result->min = 1;
            result->max = -1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else if (lookahead.metachar == '?') {
            result->min = 0;
            result->max = 1;
            result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
        } else {
            unget_token(lookahead);
        }
    } else if (lookahead.type == T_BOUND){
        result->min = lookahead.bound[0];
        result->max = lookahead.bound[1];
        result->annotation = xstrcat(result->annotation, get_token_annotation(lookahead));
    } else {
        unget_token(lookahead);
    }

    assert(result->annotation != NULL);
    return result;
}

static BranchNode *parse_branch(void) {
    BranchNode *result = xmalloc(sizeof(BranchNode));
    result->size = 0;
    result->pieces = NULL;
    result->annotation = NULL;

    for (;;) {
        Token lookahead = get_token();

        if (lookahead.type == T_END
                || (lookahead.type == T_META && lookahead.metachar == ')')
                || (lookahead.type == T_META && lookahead.metachar == '|')) {
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

static RegexNode *parse_regex(void) {
    RegexNode *result = xmalloc(sizeof(RegexNode));
    int first = true;

    result->size = 0;
    result->branches = NULL;
    result->annotation = NULL;

    for (;;) {
        Token lookahead = get_token();

        if (lookahead.type == T_END
                || (lookahead.type == T_META && lookahead.metachar == ')')) {
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
            if (lookahead.type == T_META && lookahead.metachar == '|') {
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

extern RegexNode *regtree_from_str(char *str) {
    set_pattern_str(str);
    return parse_regex();
}

extern void regtree_drop(RegexNode *regex) {
    for (int i = 0; i < regex->size; ++i) {
        BranchNode *branch = regex->branches[i];
        for (int j = 0; j < branch->size; ++j) {
            PieceNode *piece = branch->pieces[j];

            if (!piece->atom->is_simple_atom) {
                regtree_drop(piece->atom->u.regex);
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
