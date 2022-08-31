#ifndef REGTREE_H_
#define REGTREE_H_

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
    };
} AtomNode;

typedef struct {
    char *annotation;
    AtomNode *atom;
    int min, max;
} PieceNode;

struct BranchNode {
    char *annotation;
    PieceNode **pieces;
    int size;
};

extern void regtree_drop(RegexNode *regex);
extern RegexNode *regtree_from_str(char *str);

#endif
