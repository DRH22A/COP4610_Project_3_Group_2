#include "lexer.h"
#include <stdio.h>
#include <string.h>
#include "fat32.h"
#include "shell.h"

void shell_start()
{
    while (1) {

        // Prints the shell prompt using the loaded FAT32 image filename and the current working directory name
        printf("%s/%s> ", fat32_get_image_name(), fat32_get_cwd_name());
        fflush(stdout);
        
        // Reads the input from the user
        char *input = get_input();

        // Tokenizes the input
        tokenlist *tokens = get_tokens(input);
        
        // If the user pressed enter with no input then continue
        if (tokens->size == 0) {
            free(input);
            free_tokens(tokens);
            continue;
        }
        
        // First token is always the command name
        char *cmd = tokens->items[0];
        
        // Exit command
        if (strcmp(cmd, "exit") == 0) {
            free(input);
            free_tokens(tokens);
            return;
        }
        else if (strcmp(cmd, "info") == 0) {
            fat32_print_info(); // Prints FAT32 filesystem info
        }
        else if (strcmp(cmd, "ls") == 0) { // ls command
            fat32_ls();
        }
        else if (strcmp(cmd, "cd") == 0) {
            // Check to make sure that cd requires a second argument
            if (tokens->size < 2) {
                fprintf(stderr, "Error: must use a directory name with command cd\n");
            } else {
                fat32_cd(tokens->items[1]);
            }
        }
		else if (strcmp(cmd, "mkdir") == 0){
			if (tokens->size < 2) {
				fprintf(stderr, "Error: must use a directory name with command mkdir\n");
			} else {
				if (!fat32_mkdir(tokens->items[1])) {
					fprintf(stderr, "mkdir failed\n");
				}
			}
		}
		else if (strcmp(cmd, "creat") == 0) {
			if (tokens->size < 2) {
				fprintf(stderr, "Error: must use a file name with command creat\n");
			} else {
				if(!fat32_creat(tokens->items[1])) {
					fprintf(stderr, "creat failed\n");
				} 
			}
		}
        else {
            // Unknown commands
            printf("Unknown command: %s\n", cmd);
        }
        
        free(input);
        free_tokens(tokens);
    }
}
