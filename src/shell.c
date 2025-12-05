//shell.c
#include "lexer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
		// Read commands
		else if (strcmp(cmd, "open") == 0) {
			if (tokens->size < 3) {
				fprintf(stderr, "Error: open requires FILENAME and FLAGS\n");
			} else {
				if (!fat32_open(tokens->items[1], tokens->items[2])) {
					fprintf(stderr, "open failed\n");
				}
			}
		}
		else if (strcmp(cmd, "close") == 0) {
			if (tokens->size < 2) {
				fprintf(stderr, "Error: close requires FILENAME\n");
			} else {
				if (!fat32_close(tokens->items[1])) {
					fprintf(stderr, "close failed\n");
				}
			}
		}
		else if (strcmp(cmd, "lsof") == 0) {
			fat32_lsof();
		}
		else if (strcmp(cmd, "lseek") == 0) {
			if (tokens->size < 3) {
				fprintf(stderr, "Error: lseek requires FILENAME and OFFSET\n");
			} else {
				// Parse offset - handle potential errors
				char *endptr;
				long offset_long = strtol(tokens->items[2], &endptr, 10);
				if (*endptr != '\0' || offset_long < 0) {
					fprintf(stderr, "Error: Invalid offset '%s'\n", tokens->items[2]);
				} else {
					uint32_t offset = (uint32_t)offset_long;
					if (!fat32_lseek(tokens->items[1], offset)) {
						fprintf(stderr, "lseek failed\n");
					}
				}
			}
		}
		else if (strcmp(cmd, "read") == 0) {
			if (tokens->size < 3) {
				fprintf(stderr, "Error: read requires FILENAME and SIZE\n");
			} else {
				// Parse size - handle potential errors
				char *endptr;
				long size_long = strtol(tokens->items[2], &endptr, 10);
				if (*endptr != '\0' || size_long <= 0) {
					fprintf(stderr, "Error: Invalid size '%s'\n", tokens->items[2]);
				} else {
					uint32_t size = (uint32_t)size_long;
					if (!fat32_read(tokens->items[1], size)) {
						fprintf(stderr, "read failed\n");
					}
				}
			}
		}
        else if (strcmp(cmd, "rm") == 0) {
            if (tokens->size < 2) {
                fprintf(stderr, "Error: must use a file name with command rm\n");
            } else {
                if (!fat32_rm(tokens->items[1])) {
                    fprintf(stderr, "rm failed\n");
                }
            }
        }
        else if (strcmp(cmd, "rmdir") == 0) {
            if (tokens->size < 2) {
                fprintf(stderr, "Error: must use a directory name with command rmdir\n");
            } else {
                if (!fat32_rmdir(tokens->items[1])) {
                    fprintf(stderr, "rmdir failed\n");
                }
            }
        }
        else if (strcmp(cmd, "mv") == 0) {
            if (tokens->size < 3) {
                fprintf(stderr, "Error: usage: mv [FILENAME/DIRNAME] [NEW_FILENAME/DIRECTORY]\n");
            } else {
                if (!fat32_mv(tokens->items[1], tokens->items[2])) {
                    fprintf(stderr, "mv failed\n");
                }
            }
        }
        else if (strcmp(cmd, "write") == 0) {
            if (tokens->size < 3) {
                fprintf(stderr, "Error: usage: write [FILENAME] [STRING]\n");
            } else {
                // The string token is already parsed without quotes by get_tokens in lexer.c
                // But we need to verify if the user provided a quoted string as requested.
                // The lexer handles quotes, so tokens->items[2] should be the string content.
                if (!fat32_write(tokens->items[1], tokens->items[2])) {
                    fprintf(stderr, "write failed\n");
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