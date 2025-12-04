#pragma once

#include <stdlib.h>
#include <stdbool.h>

// A dynamic list of string tokens.
typedef struct {
    char **items; // Array of C-strings
    size_t size; // Number of tokens stored
} tokenlist;

char *get_input(void);
tokenlist *get_tokens(char *input);
tokenlist *new_tokenlist(void);
void add_token(tokenlist *tokens, char *item);
void free_tokens(tokenlist *tokens);
