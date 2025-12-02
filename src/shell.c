#include "lexer.h"
#include <stdio.h>
#include <string.h>
#include "fat32.h"
#include "shell.h"

void shell_run()
{
    while (1) {

        // Print shell prompt
        printf("%s/%s> ", fat32_get_image_name(), fat32_get_cwd_name());
        fflush(stdout);

        // Get input using the  lexer’s functions
        char *input = get_input();
        tokenlist *tokens = get_tokens(input);

        if (tokens->size == 0) {
            free(input);
            free_tokens(tokens);
            continue;
        }

        char *cmd = tokens->items[0];

        if (strcmp(cmd, "exit") == 0) {
            free(input);
            free_tokens(tokens);
            return;
        }

        else if (strcmp(cmd, "info") == 0) {
            fat32_print_info();
        }

        else {
            printf("Unknown command: %s\n", cmd);
        }

        free(input);
        free_tokens(tokens);
    }
}
