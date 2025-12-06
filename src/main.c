#include <stdio.h>
#include <stdlib.h>
#include "fat32.h"
#include "shell.h"

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Please use: %s fat32.img\n", argv[0]);
        return 1;
    }
    
    // Mount image
    if (!fat32_mount(argv[1])) {
        return 1;
    }
    
    shell_start();
    fat32_unmount();
    
    return 0;
}