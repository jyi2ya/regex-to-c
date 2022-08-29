#ifndef TOKEN_H_
#define TOKEN_H_

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

extern void set_pattern_str(char *str);
extern Token get_token(void);
extern char *get_token_annotation(Token t);
extern void unget_token(Token t);

#endif
