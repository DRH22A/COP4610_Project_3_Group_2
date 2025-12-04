#include "fat32.h"
#include <string.h>
#include <stdlib.h>

// Global variables
FILE *image_fp = NULL;
BPB_t bpb;

// These are variable names for the shell prompt to keep track of
static char loaded_image_name[256] = {0};
static char cwd_name[256] = "/";
static uint32_t current_dir_cluster = 0;

// Mount image
bool fat32_mount(const char *filename)
{
    image_fp = fopen(filename, "rb+");
    if (!image_fp) {
        perror("There was an error opening the FAT32 image!");
        return false;
    }

    // Saves the image name for shell prompt
    strncpy(loaded_image_name, filename, sizeof(loaded_image_name)-1);

    // Reads the entire BPB
    fseek(image_fp, 0, SEEK_SET);
    size_t bytes_read = fread(&bpb, sizeof(BPB_t), 1, image_fp);
    
    if (bytes_read != 1) {
        fprintf(stderr, "Error: Failed trying to read boot sector\n");
        fclose(image_fp);
        image_fp = NULL;
        return false;
    }
    
    // Initializes current directory to root using /
    current_dir_cluster = bpb.BPB_RootClus;
    strcpy(cwd_name, "/");
    
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
void fat32_print_info() {
    printf("Root Cluster: %u\n", bpb.BPB_RootClus);
    printf("Bytes Per Sector: %u\n", bpb.BPB_BytsPerSec);
    printf("Sectors Per Cluster: %u\n", bpb.BPB_SecPerClus);
    
    // Calculates total sectors used by FATs
    uint32_t fat_sectors = bpb.BPB_NumFATs * bpb.BPB_FATSz32;
    
    // Calculates data region sectors
    uint32_t data_sectors = bpb.BPB_TotSec32 - 
        (bpb.BPB_RsvdSecCnt + fat_sectors);
    
    // Calculates total clusters in data region
    uint32_t total_clusters = data_sectors / bpb.BPB_SecPerClus;
    
    // Calculates entries per FAT
    uint32_t entries_per_fat = 
        (bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec) / 4;
    
    printf("Total clusters in data region: %u\n", total_clusters);
    printf("Entries in one FAT: %u\n", entries_per_fat);
    
    // Size of the image in bytes
    fseek(image_fp, 0, SEEK_END);
    long size = ftell(image_fp);
    printf("Size of Image (bytes): %ld\n", size);
}

// Constant character variables for the shell prompt
const char *fat32_get_image_name() { 
    return loaded_image_name; 
}
const char *fat32_get_cwd_name() {
    return cwd_name; 
}

// Helper Functions
// Converts cluster number to logical block address
uint32_t fat32_cluster_to_lba(uint32_t cluster) {
    uint32_t fat_sectors = bpb.BPB_NumFATs * bpb.BPB_FATSz32;
    uint32_t first_data_sector = bpb.BPB_RsvdSecCnt + fat_sectors;
    return first_data_sector + (cluster - 2) * bpb.BPB_SecPerClus;
}

// Reads a  cluster into the buffer
bool fat32_read_cluster(uint32_t cluster, void *buffer) {
    uint32_t lba = fat32_cluster_to_lba(cluster);
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    
    fseek(image_fp, lba * bpb.BPB_BytsPerSec, SEEK_SET);
    size_t bytes_read = fread(buffer, 1, cluster_size, image_fp);
    
    return bytes_read == cluster_size;
}

// Get the next cluster in the FAT chain
uint32_t fat32_get_next_cluster(uint32_t cluster) {
    // Calculate FAT entry offset
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = bpb.BPB_RsvdSecCnt + (fat_offset / bpb.BPB_BytsPerSec);
    uint32_t entry_offset = fat_offset % bpb.BPB_BytsPerSec;
    
    // Read the FAT entry
    fseek(image_fp, fat_sector * bpb.BPB_BytsPerSec + entry_offset, SEEK_SET);
    uint32_t next_cluster;
    fread(&next_cluster, sizeof(uint32_t), 1, image_fp);
    
    next_cluster &= 0x0FFFFFFF;
    
    return next_cluster;
}

// Extracts the first cluster number from a directory entry
uint32_t fat32_get_first_cluster(DirEntry_t *entry) {
    return ((uint32_t)entry->DIR_FstClusHI << 16) | entry->DIR_FstClusLO;
}

// Compare FAT32 short name (11 bytes, space-padded) with a normal string
bool compare_fat32_name(const uint8_t *fat_name, const char *regular_name) {
    char formatted[12];
    memcpy(formatted, fat_name, 11);
    formatted[11] = '\0';
    
    // Removes trailing spaces
    for (int i = 10; i >= 0; i--) {
        if (formatted[i] == ' ')
            formatted[i] = '\0';
        else
            break;
    }
    
    // Special cases for . and ..
    if (strcmp(regular_name, ".") == 0) {
        return (fat_name[0] == '.' && fat_name[1] == ' ');
    }
    if (strcmp(regular_name, "..") == 0) {
        return (fat_name[0] == '.' && fat_name[1] == '.');
    }
    
    // Converts to uppercase for comparison
    char upper_regular[256];
    strncpy(upper_regular, regular_name, sizeof(upper_regular) - 1);
    upper_regular[sizeof(upper_regular) - 1] = '\0';
    for (int i = 0; upper_regular[i]; i++) {
        if (upper_regular[i] >= 'a' && upper_regular[i] <= 'z')
            upper_regular[i] = upper_regular[i] - 'a' + 'A';
    }
    
    return strcasecmp(formatted, upper_regular) == 0;
}

// Finds a directory entry by name in the given directory cluster
DirEntry_t* fat32_find_entry(uint32_t dir_cluster, const char *name) {
    static DirEntry_t found_entry;
    
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *cluster_buffer = malloc(cluster_size);
    if (!cluster_buffer) return NULL;
    
    uint32_t current_cluster = dir_cluster;
    
    // Moving along the cluster chain
    while (current_cluster < FAT32_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            free(cluster_buffer);
            return NULL;
        }
        
        uint32_t entries_per_cluster = cluster_size / sizeof(DirEntry_t);
        DirEntry_t *entries = (DirEntry_t *)cluster_buffer;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            // 0x00 means end of directory
            if (entries[i].DIR_Name[0] == 0x00)
                break;
            
            // 0xE5 means deleted entry
            if (entries[i].DIR_Name[0] == 0xE5)
                continue;
            
            // Skips long name entries
            if ((entries[i].DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
                continue;
            
            // Skips volume labels
            if (entries[i].DIR_Attr & ATTR_VOLUME_ID)
                continue;
            
            // Checks to see if the name matches
            if (compare_fat32_name(entries[i].DIR_Name, name)) {
                memcpy(&found_entry, &entries[i], sizeof(DirEntry_t));
                free(cluster_buffer);
                return &found_entry;
            }
        }
        
        // Moves to the next cluster
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    free(cluster_buffer);
    return NULL;
}

// Navigation Commands
// ls command
void fat32_ls()
{
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *cluster_buffer = malloc(cluster_size);
    if (!cluster_buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return;
    }
    
    uint32_t current_cluster = current_dir_cluster;
    // Moves along the cluster chain
    while (current_cluster < FAT32_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            fprintf(stderr, "Error: There was an issue reading the cluster\n");
            free(cluster_buffer);
            return;
        }
        
        // Parse directory entries
        uint32_t entries_per_cluster = cluster_size / sizeof(DirEntry_t);
        DirEntry_t *entries = (DirEntry_t *)cluster_buffer;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            // 0x00 means end of directory
            if (entries[i].DIR_Name[0] == 0x00) {
                free(cluster_buffer);
                return;
            }
            
            // 0xE5 means deleted entry
            if (entries[i].DIR_Name[0] == 0xE5)
                continue;
            
            // Skips long name entries
            if ((entries[i].DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
                continue;
            
            // Skips volume labels
            if (entries[i].DIR_Attr & ATTR_VOLUME_ID)
                continue;
            
            // Prints the name
            char name[12];
            memcpy(name, entries[i].DIR_Name, 11);
            name[11] = '\0';
            
            // Removes trailing spaces
            for (int j = 10; j >= 0; j--) {
                if (name[j] == ' ')
                    name[j] = '\0';
                else
                    break;
            }
            
            printf("%s\n", name);
        }
        
        // Move to next cluster
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    free(cluster_buffer);
}

// cd command
bool fat32_cd(const char *dirname)
{
    // Handles the root directory
    if (strcmp(dirname, "/") == 0) {
        current_dir_cluster = bpb.BPB_RootClus;
        strcpy(cwd_name, "/");
        return true;
    }
    
    // Finds the directory entry
    DirEntry_t *entry = fat32_find_entry(current_dir_cluster, dirname);
    
    if (!entry) {
        fprintf(stderr, "Error: Directory '%s' not found\n", dirname);
        return false;
    }
    
    // Checks to see if it is a directory
    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", dirname);
        return false;
    }
    
    // Updates current directory
    uint32_t new_cluster = fat32_get_first_cluster(entry);
    
    // Handles special case of ".." from root
    if (new_cluster == 0) {
        new_cluster = bpb.BPB_RootClus;
    }
    
    current_dir_cluster = new_cluster;
    
    // Update path string
    if (strcmp(dirname, "..") == 0) {
        // Go up one directory
        char *last_slash = strrchr(cwd_name, '/');
        if (last_slash && last_slash != cwd_name) {
            *last_slash = '\0';
        } else {
            strcpy(cwd_name, "/");
        }
    } else if (strcmp(dirname, ".") != 0) {
        // Go into subdirectory (skip ".")
        if (strcmp(cwd_name, "/") != 0) {
            strcat(cwd_name, "/");
        }
        strcat(cwd_name, dirname);
    }
    
    return true;
}