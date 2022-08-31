#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#include "xutils.h"
#include "regtree.h"

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
            if (atom->allowed[i])
                printf("        case %d: return 1;\n", i);
        printf("\
    }\n\
    return -1;\n\
}\n");
    } else {
        int id = translate_regex(atom->regex);

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
    RegexNode *result = regtree_from_str(argv[1]);
    do_you_like_c(result);
    regtree_drop(result);
    return 0;
}
