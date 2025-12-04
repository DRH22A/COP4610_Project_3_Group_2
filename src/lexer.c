#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *get_input(void) {
    char *buffer = NULL;     // Dynamic buffer that will end up holding the input line
    int bufsize = 0;         // Current number of characters in the buffer
    char line[5];            // Temporary buffer
    
    // Reads blocks up to 4 characters until a new line is found
    while (fgets(line, 5, stdin) != NULL)
    {
        int addby = 0;
        // Looks for a newline character in the block
        char *newln = strchr(line, '\n');
        if (newln != NULL)
            addby = newln - line;  // Copies characters before '\n'
        else
            addby = 5 - 1;         // Copies all 4 characters if no newline

        // Expanding the main buffer
        buffer = (char *)realloc(buffer, bufsize + addby);

        // Copying the block into the growing buffer
        memcpy(&buffer[bufsize], line, addby);
        bufsize += addby;

        // Stops once a newline is found
        if (newln != NULL)
            break;
    }

    buffer = (char *)realloc(buffer, bufsize + 1);
    buffer[bufsize] = 0;

    return buffer;
}

tokenlist *new_tokenlist(void) {
    // Allocate the tokenlist structure
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    tokens->size = 0;

    // Allocate space for the token array
    tokens->items = (char **)malloc(sizeof(char *));
    tokens->items[0] = NULL;

    return tokens;
}

void add_token(tokenlist *tokens, char *item) {
    int i = tokens->size;

    // Expand the array to hold one more token + the NULL at the end
    tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *));

    // Allocate space for the token itself
    tokens->items[i] = (char *)malloc(strlen(item) + 1);
    tokens->items[i + 1] = NULL;

    // Copy token text
    strcpy(tokens->items[i], item);

    tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
    char *buf = (char *)malloc(strlen(input) + 1);
    strcpy(buf, input);

    tokenlist *tokens = new_tokenlist();
    char *tok = strtok(buf, " ");
    while (tok != NULL)
    {
        add_token(tokens, tok);
        tok = strtok(NULL, " ");
    }

    free(buf);
    return tokens;
}

void free_tokens(tokenlist *tokens) {
    // Free each token string
    for (int i = 0; i < tokens->size; i++)
        free(tokens->items[i]);

    free(tokens->items);
    free(tokens);
}
