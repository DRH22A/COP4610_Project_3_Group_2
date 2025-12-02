#include <stdio.h>
#include <stdlib.h>
#include "fat32.h"
#include "shell.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s fat32.img\n", argv[0]);
        return 1;
    }

    // Display arguments
    printf("%s\n", argv[0]);
    printf("%s\n", argv[1]);

    // Mount image
    if (!fat32_mount(argv[1])) {
        return 1;
    }

    shell_run();

    fat32_unmount();

    return 0;
}
