#include "fat32.h"
#include <string.h>

// Global variables
FILE *image_fp = NULL;
BPB_t bpb;

// Keeps track of varibale names for the shell prompt
static char loaded_image_name[256] = {0};
static char cwd_name[4] = "/";   // Part 1 → root only

// Mount image
bool fat32_mount(const char *filename)
{
    image_fp = fopen(filename, "rb+");
    if (!image_fp) {
        perror("Error opening FAT32 image");
        return false;
    }

    // Saves the image name for shell prompt
    strncpy(loaded_image_name, filename, sizeof(loaded_image_name)-1);

    // Reads the entire BPB at once
    fseek(image_fp, 0, SEEK_SET);
    fread(&bpb, sizeof(BPB_t), 1, image_fp);

    return true;
}

// Unmount image
void fat32_unmount()
{
    if (image_fp) {
        fclose(image_fp);
        image_fp = NULL;
    }
}

// Info command
void fat32_print_info()
{
    printf("Root Cluster: %u\n", bpb.BPB_RootClus);
    printf("Bytes Per Sector: %u\n", bpb.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", bpb.BPB_SecPerClus);

    // Total number of data sectors and fat sectors
    uint32_t data_sectors =
        bpb.BPB_TotSec32 -
        (bpb.BPB_RsvdSecCnt + fat_sectors);
        
    uint32_t fat_sectors = bpb.BPB_NumFATs * bpb.BPB_FATSz32;

    uint32_t total_clusters = data_sectors / bpb.BPB_SecPerClus;

    // Total number of FAT entries
    uint32_t entries_per_fat =
        (bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec) / 4;

    printf("Total Clusters in Data Region: %u\n", total_clusters);
    printf("Entries in one FAT: %u\n", entries_per_fat);

    // Size of the image in bytes
    fseek(image_fp, 0, SEEK_END);
    long size = ftell(image_fp);
    printf("Size of Image (bytes): %ld\n", size);
}

// For shell prompt
const char *fat32_get_image_name() { return loaded_image_name; }
const char *fat32_get_cwd_name()   { return cwd_name; }
