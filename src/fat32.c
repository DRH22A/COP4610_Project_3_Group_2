#include "fat32.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

// Global variables
FILE *image_fp = NULL;
BPB_t bpb;

// These are variable names for the shell prompt to keep track of
static char loaded_image_name[256] = {0};
char cwd_name[256] = "/";
uint32_t current_dir_cluster = 0;

// Global array for tracking open files
static OpenFile open_files[MAX_OPEN_FILES];

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
    
    fat32_init_open_files();

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

//
void fat32_set_fat_entry(uint32_t cluster, uint32_t value) {
	value &= 0x0FFFFFFF;

	uint32_t fat_offset = cluster * 4;
	uint32_t fat1_lba = bpb.BPB_RsvdSecCnt;
	uint32_t fat1_byte_addr = fat1_lba * bpb.BPB_BytsPerSec + fat_offset;

	fseek(image_fp, fat1_byte_addr, SEEK_SET);
	fwrite(&value, sizeof(uint32_t), 1, image_fp);

	uint32_t fat2_lba = bpb.BPB_RsvdSecCnt + bpb.BPB_FATSz32;
	uint32_t fat2_byte_addr = fat2_lba * bpb.BPB_BytsPerSec + fat_offset;
	fseek(image_fp, fat2_byte_addr, SEEK_SET);
	fwrite(&value, sizeof(uint32_t), 1, image_fp);

	fflush(image_fp);
}

// Update FAT entry in both FAT tables
uint32_t fat32_find_free_cluster() {
	uint32_t fat1_lba = bpb.BPB_RsvdSecCnt;
	uint32_t fat_bytes = bpb.BPB_FATSz32 * bpb.BPB_BytsPerSec;

	uint32_t total_entries = fat_bytes / 4;

	for(uint32_t c = 2; c < total_entries; c++) {
		uint32_t fat_offset = c * 4;
		uint32_t fat_byte_addr = fat1_lba * bpb.BPB_BytsPerSec + fat_offset;

		uint32_t value = 0;
		fseek(image_fp, fat_byte_addr, SEEK_SET);
		fread(&value, sizeof(uint32_t), 1, image_fp);

		value &= 0x0FFFFFFF;
		if(value == 0)
			return c;
	}

	return 0;
}

// Set bytes of cluster to zero
void fat32_zero_cluster(uint32_t cluster) {
	uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
	uint32_t lba = fat32_cluster_to_lba(cluster);
	uint32_t byte_addr = lba * bpb.BPB_BytsPerSec;

	uint8_t *zeros = calloc(1, cluster_size);
	if(!zeros) return;

	fseek(image_fp, byte_addr, SEEK_SET);
	fwrite(zeros, 1, cluster_size, image_fp);

	free(zeros);
	fflush(image_fp);
}

// Allocates new cluster for file or directory usage
uint32_t fat32_allocate_cluster() {
	uint32_t free_cluster = fat32_find_free_cluster();
	if(free_cluster == 0)
		return 0;

	fat32_set_fat_entry(free_cluster, FAT32_EOC);
	fat32_zero_cluster(free_cluster);

	return free_cluster;
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

// Helper to go up one directory in cwd_name
void trim_last_dir(char *cwd) {
    size_t len = strlen(cwd);
    if (len <= 1) return; // already at root

    // Remove trailing slash if present
    if (cwd[len - 1] == '/') cwd[len - 1] = '\0';

    // Find last slash
    char *last_slash = strrchr(cwd, '/');
    if (last_slash) {
        if (last_slash == cwd) {
            // back to root
            cwd[1] = '\0';
        } else {
            *(last_slash + 1) = '\0';
        }
    }
}

bool fat32_cd(const char *dirname)
{
    if (strcmp(dirname, "/") == 0) {
        current_dir_cluster = bpb.BPB_RootClus;
        strcpy(cwd_name, "/");
        return true;
    }

    if (strcmp(dirname, ".") == 0) {
        return true; // stay in current directory
    }

    if (strcmp(dirname, "..") == 0) {
        if (current_dir_cluster == bpb.BPB_RootClus) {
            strcpy(cwd_name, "/");
            return true;
        }

        DirEntry_t *dotdot_entry = fat32_find_entry(current_dir_cluster, "..");
        if (!dotdot_entry) {
            fprintf(stderr, "Error: Parent directory not found\n");
            return false;
        }

        uint32_t parent_cluster = fat32_get_first_cluster(dotdot_entry);
        if (parent_cluster == 0) parent_cluster = bpb.BPB_RootClus;

        current_dir_cluster = parent_cluster;
        trim_last_dir(cwd_name);
        return true;
    }

    // Normal directory
    DirEntry_t *entry = fat32_find_entry(current_dir_cluster, dirname);
    if (!entry) {
        fprintf(stderr, "Error: Directory '%s' not found\n", dirname);
        return false;
    }
    if (!(entry->DIR_Attr & ATTR_DIRECTORY)) {
        fprintf(stderr, "Error: '%s' is not a directory\n", dirname);
        return false;
    }

    current_dir_cluster = fat32_get_first_cluster(entry);

    // Append to cwd_name
    size_t len = strlen(cwd_name);
    if (len == 0 || cwd_name[len - 1] != '/') strcat(cwd_name, "/");
    strcat(cwd_name, dirname);
    strcat(cwd_name, "/");

    return true;
}


// mkdir and creat functions (part3)

void fat32_generate_short_name(const char *input, uint8_t out[11]) {
	memset(out, ' ', 11);

	const char *dot = strchr(input, '.');
	int name_len = dot ? (dot - input) : strlen(input);
	int ext_len = dot ? strlen(dot + 1) : 0;

	for (int i = 0; i < name_len && i < 8; i++) {
		char c = input[i];
		if(c >= 'a' && c <= 'z')
			c -= 32;
		out[i] = c;
	}

	if (dot) {
        const char *ext = dot + 1;
        for(int i = 0; i < ext_len && i < 3; i++) {
            char c = ext[i];
            if(c >= 'a' && c <= 'z')
                c -= 32;
            out[8 + i] = c;
        }
    }
}

bool fat32_write_dir_entry(uint32_t dir_cluster, DirEntry_t *entry){
	uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *cluster_buf = malloc(cluster_size);
    if (!cluster_buf) return false;

    uint32_t current = dir_cluster;

    while(1) {
        // Read directory cluster
        if (!fat32_read_cluster(current, cluster_buf)) {
            free(cluster_buf);
            return false;
        }

        DirEntry_t *entries = (DirEntry_t *)cluster_buf;
        uint32_t entries_per_cluster = cluster_size / sizeof(DirEntry_t);

        // Search for free entry
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].DIR_Name[0] == 0x00 || entries[i].DIR_Name[0] == 0xE5) {
                
                memcpy(&entries[i], entry, sizeof(DirEntry_t));

                // Write buffer back to disk
                uint32_t lba = fat32_cluster_to_lba(current);
                fseek(image_fp, lba * bpb.BPB_BytsPerSec, SEEK_SET);
                fwrite(cluster_buf, 1, cluster_size, image_fp);
                fflush(image_fp);

                free(cluster_buf);
                return true;
            }
        }

        uint32_t next = fat32_get_next_cluster(current);

        if (next >= FAT32_EOC) {
            // Allocate new directory cluster
            uint32_t new_cluster = fat32_allocate_cluster();
            if (new_cluster == 0) {
                free(cluster_buf);
                return false;
            }

            fat32_set_fat_entry(current, new_cluster);
            fat32_set_fat_entry(new_cluster, FAT32_EOC);

            // Write entry into new cluster
            memset(cluster_buf, 0, cluster_size);
            DirEntry_t *new_entries = (DirEntry_t *)cluster_buf;
            memcpy(&new_entries[0], entry, sizeof(DirEntry_t));

            uint32_t new_lba = fat32_cluster_to_lba(new_cluster);
            fseek(image_fp, new_lba * bpb.BPB_BytsPerSec, SEEK_SET);
            fwrite(cluster_buf, 1, cluster_size, image_fp);
            fflush(image_fp);

            free(cluster_buf);
            return true;
        }

        current = next;
    }
}

bool fat32_mkdir(const char *dirname) {
    // Ensure no duplicates
    if (fat32_find_entry(current_dir_cluster, dirname) != NULL) {
        fprintf(stderr, "Error: '%s' already exists\n", dirname);
        return false;
    }

    // Allocate cluster for new directory
    uint32_t new_cluster = fat32_allocate_cluster();
    if (new_cluster == 0) {
        fprintf(stderr, "Error: could not allocate cluster\n");
        return false;
    }

    // Create "." and ".." entries inside the new directory
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *buffer = calloc(1, cluster_size);
    if (!buffer) return false;

    DirEntry_t *entries = (DirEntry_t *)buffer;

    memset(&entries[0], 0, sizeof(DirEntry_t));
    memcpy(entries[0].DIR_Name, ".          ", 11);
    entries[0].DIR_Attr = ATTR_DIRECTORY;
    entries[0].DIR_FstClusLO = new_cluster & 0xFFFF;
    entries[0].DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    memset(&entries[1], 0, sizeof(DirEntry_t));
    memcpy(entries[1].DIR_Name, "..         ", 11);
    entries[1].DIR_Attr = ATTR_DIRECTORY;
    uint32_t parent_cluster = current_dir_cluster;
    entries[1].DIR_FstClusLO = parent_cluster & 0xFFFF;
    entries[1].DIR_FstClusHI = (parent_cluster >> 16) & 0xFFFF;

    // Write the new directory contents
    uint32_t lba = fat32_cluster_to_lba(new_cluster);
    fseek(image_fp, lba * bpb.BPB_BytsPerSec, SEEK_SET);
    fwrite(buffer, 1, cluster_size, image_fp);
    fflush(image_fp);
    free(buffer);

    // Create directory entry in parent directory
    DirEntry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));

    fat32_generate_short_name(dirname, new_entry.DIR_Name);

    new_entry.DIR_Attr = ATTR_DIRECTORY;
    new_entry.DIR_FstClusLO = new_cluster & 0xFFFF;
    new_entry.DIR_FstClusHI = (new_cluster >> 16) & 0xFFFF;
    new_entry.DIR_FileSize = 0;

    return fat32_write_dir_entry(current_dir_cluster, &new_entry);
}

bool fat32_creat(const char *filename) {
    if (fat32_find_entry(current_dir_cluster, filename) != NULL) {
        fprintf(stderr, "Error: '%s' already exists\n", filename);
        return false;
    }

    DirEntry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));

    fat32_generate_short_name(filename, new_entry.DIR_Name);

    new_entry.DIR_Attr = ATTR_ARCHIVE;
    new_entry.DIR_FstClusLO = 0;
    new_entry.DIR_FstClusHI = 0;
    new_entry.DIR_FileSize = 0;

    return fat32_write_dir_entry(current_dir_cluster, &new_entry);
}

// Initialize open files array
void fat32_init_open_files(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        open_files[i].in_use = 0;
        memset(open_files[i].filename, 0, sizeof(open_files[i].filename));
        memset(open_files[i].path, 0, sizeof(open_files[i].path));
    }
}

// Helper: Check if file is already open
static bool is_file_open(const char *filename) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcasecmp(open_files[i].filename, filename) == 0) {
            return true;
        }
    }
    return false;
}

// Helper: Find open file by name
static OpenFile* find_open_file(const char *filename) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcasecmp(open_files[i].filename, filename) == 0) {
            return &open_files[i];
        }
    }
    return NULL;
}

// Helper: Parse mode string
static FileMode parse_mode(const char *mode_str) {
    if (strcmp(mode_str, "-r") == 0) {
        return MODE_READ;
    } else if (strcmp(mode_str, "-w") == 0) {
        return MODE_WRITE;
    } else if (strcmp(mode_str, "-rw") == 0 || strcmp(mode_str, "-wr") == 0) {
        return MODE_READ_WRITE;
    }
    return 0;
}

// Helper: Convert mode to string for display
static const char* mode_to_string(FileMode mode) {
    switch (mode) {
        case MODE_READ: return "r";
        case MODE_WRITE: return "w";
        case MODE_READ_WRITE: return "rw";
        default: return "?";
    }
}

// Helper: Get display name from FAT32 name (removes trailing spaces)
static void get_display_name(const uint8_t *fat_name, char *out) {
    memcpy(out, fat_name, 11);
    out[11] = '\0';
    
    // Remove trailing spaces
    for (int i = 10; i >= 0; i--) {
        if (out[i] == ' ')
            out[i] = '\0';
        else
            break;
    }
}

// open [FILENAME] [FLAGS]
// Opens a file for reading/writing
bool fat32_open(const char *filename, const char *flags) {
    // Parse mode
    FileMode mode = parse_mode(flags);
    if (mode == 0) {
        fprintf(stderr, "Error: Invalid mode '%s'. Use -r, -w, -rw, or -wr\n", flags);
        return false;
    }
    
    // Check if already open
    if (is_file_open(filename)) {
        fprintf(stderr, "Error: File '%s' is already open\n", filename);
        return false;
    }
    
    // Find file in current directory
    DirEntry_t *entry = fat32_find_entry(current_dir_cluster, filename);
    if (!entry) {
        fprintf(stderr, "Error: File '%s' does not exist\n", filename);
        return false;
    }
    
    // Check if it's a directory
    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        fprintf(stderr, "Error: '%s' is a directory\n", filename);
        return false;
    }
    
    // Find empty slot in open files array
    int slot = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) {
        fprintf(stderr, "Error: Maximum number of open files reached\n");
        return false;
    }
    
    // Store file information in the open file slot
    open_files[slot].in_use = 1;
    get_display_name(entry->DIR_Name, open_files[slot].filename);
    open_files[slot].mode = mode;
    open_files[slot].offset = 0;  // Initialize offset at 0
    open_files[slot].size = entry->DIR_FileSize;
    open_files[slot].first_cluster = fat32_get_first_cluster(entry);
    strncpy(open_files[slot].path, cwd_name, sizeof(open_files[slot].path) - 1);
    
    return true;
}

// close [FILENAME]
// Closes an opened file
bool fat32_close(const char *filename) {
    // Find file in open files array
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && strcasecmp(open_files[i].filename, filename) == 0) {
            // Close the file by marking slot as unused
            open_files[i].in_use = 0;
            memset(&open_files[i], 0, sizeof(OpenFile));
            return true;
        }
    }
    
    fprintf(stderr, "Error: File '%s' is not open\n", filename);
    return false;
}

// Helper: Find entry and also return directory cluster and offset
bool fat32_find_entry_info(uint32_t dir_cluster, const char *name, 
                           DirEntry_t *out_entry, uint32_t *out_dir_clus, 
                           uint32_t *out_offset) {
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *cluster_buffer = malloc(cluster_size);
    if (!cluster_buffer) return false;
    
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_EOC) {
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            free(cluster_buffer);
            return false;
        }
        
        uint32_t entries_per_cluster = cluster_size / sizeof(DirEntry_t);
        DirEntry_t *entries = (DirEntry_t *)cluster_buffer;
        
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            // 0x00 means end of directory
            if (entries[i].DIR_Name[0] == 0x00) {
                free(cluster_buffer);
                return false;
            }
            
            // 0xE5 means deleted entry
            if (entries[i].DIR_Name[0] == 0xE5)
                continue;
            
            // Skip long name entries
            if ((entries[i].DIR_Attr & ATTR_LONG_NAME) == ATTR_LONG_NAME)
                continue;
            
            // Skip volume labels
            if (entries[i].DIR_Attr & ATTR_VOLUME_ID)
                continue;
            
            // Check if the name matches
            if (compare_fat32_name(entries[i].DIR_Name, name)) {
                memcpy(out_entry, &entries[i], sizeof(DirEntry_t));
                *out_dir_clus = current_cluster;
                *out_offset = i * sizeof(DirEntry_t);
                
                free(cluster_buffer);
                return true;
            }
        }
        // Move to the next cluster
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    free(cluster_buffer);
    return false;
}

// Helper: Free a cluster chain in the FAT
void fat32_free_chain(uint32_t start_cluster) {
    uint32_t current = start_cluster;
    
    while (current < FAT32_EOC && current != 0) {
        uint32_t next = fat32_get_next_cluster(current);
        fat32_set_fat_entry(current, 0);  // Mark as free
        current = next;
    }
}


// Move/Rename a file or directory
bool fat32_mv(const char *src, const char *dest) {
    // 1. Check if source exists
    uint32_t src_dir_clus, src_offset;
    DirEntry_t src_entry;
    if(!fat32_find_entry_info(current_dir_cluster, src, &src_entry, &src_dir_clus, &src_offset)) {
        fprintf(stderr, "Error: Source file/directory not found\n");
        return false;
    }
    
    // 2. Check if destination is a directory (Move into)
    uint32_t dest_dir_clus = 0, dest_offset = 0;
    DirEntry_t dest_entry;
    bool dest_exists = fat32_find_entry_info(current_dir_cluster, dest, &dest_entry, &dest_dir_clus, &dest_offset);
    
    if(dest_exists) {
        if(dest_entry.DIR_Attr & ATTR_DIRECTORY) {
            // Move 'src' INTO 'dest' directory
            uint32_t target_dir_cluster = fat32_get_first_cluster(&dest_entry);
            
            // Check if file with same name exists in target dir
            if(fat32_find_entry(target_dir_cluster, src) != NULL) {
                fprintf(stderr, "Error: File with same name exists in destination\n");
                return false;
            }
            
            // Write entry to new directory
            if(!fat32_write_dir_entry(target_dir_cluster, &src_entry)) {
                fprintf(stderr, "Error: Failed to write to destination directory\n");
                return false;
            }
            
            // If it was a directory, we need to update its ".." entry to point to the new parent
            if (src_entry.DIR_Attr & ATTR_DIRECTORY) {
                uint32_t src_first_cluster = fat32_get_first_cluster(&src_entry);
                uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
                uint8_t *buf = malloc(cluster_size);
                if (buf && fat32_read_cluster(src_first_cluster, buf)) {
                    DirEntry_t *entries = (DirEntry_t*)buf;
                    // Entry 1 is ".."
                    entries[1].DIR_FstClusLO = target_dir_cluster & 0xFFFF;
                    entries[1].DIR_FstClusHI = (target_dir_cluster >> 16) & 0xFFFF;
                    
                    // Write back
                    uint32_t lba = fat32_cluster_to_lba(src_first_cluster);
                    fseek(image_fp, lba * bpb.BPB_BytsPerSec, SEEK_SET);
                    fwrite(buf, 1, cluster_size, image_fp);
                    fflush(image_fp);
                }
                if(buf) free(buf);
            }
            
            // Delete original entry (mark 0xE5)
            uint8_t byte = 0xE5;
            uint32_t lba = fat32_cluster_to_lba(src_dir_clus);
            fseek(image_fp, lba * bpb.BPB_BytsPerSec + src_offset, SEEK_SET);
            fwrite(&byte, 1, 1, image_fp);
            fflush(image_fp);
            
            return true;
        } else {
            fprintf(stderr, "Error: Destination already exists and is a file\n");
            return false;
        }
    } else {
        // 3. Rename (Same directory, new name)
        // Update name in entry structure
        fat32_generate_short_name(dest, src_entry.DIR_Name);
        
        // Write new entry (in current dir)
        if(!fat32_write_dir_entry(current_dir_cluster, &src_entry)) {
             fprintf(stderr, "Error: Failed to write new entry\n");
             return false;
        }
        
        // Delete old entry
        uint8_t byte = 0xE5;
        uint32_t lba = fat32_cluster_to_lba(src_dir_clus);
        fseek(image_fp, lba * bpb.BPB_BytsPerSec + src_offset, SEEK_SET);
        fwrite(&byte, 1, 1, image_fp);
        fflush(image_fp);
        
        return true;
    }
}

// Delete a file
bool fat32_rm(const char *filename) {
    // Note: 'open' command not implemented yet, so skipping check if file is open
    
    uint32_t dir_clus, offset;
    DirEntry_t entry;
    if(!fat32_find_entry_info(current_dir_cluster, filename, &entry, &dir_clus, &offset)) {
        fprintf(stderr, "Error: File not found\n");
        return false;
    }
    
    if(entry.DIR_Attr & ATTR_DIRECTORY) {
        fprintf(stderr, "Error: Is a directory\n");
        return false;
    }
    
    // Mark as deleted
    uint8_t byte = 0xE5;
    uint32_t lba = fat32_cluster_to_lba(dir_clus);
    fseek(image_fp, lba * bpb.BPB_BytsPerSec + offset, SEEK_SET);
    fwrite(&byte, 1, 1, image_fp);
    fflush(image_fp);
    
    // Free chain
    uint32_t first_cluster = fat32_get_first_cluster(&entry);
    if(first_cluster != 0) {
        fat32_free_chain(first_cluster);
    }
    
    return true;
}

// Remove a directory
bool fat32_rmdir(const char *dirname) {
    uint32_t dir_clus, offset;
    DirEntry_t entry;
    if(!fat32_find_entry_info(current_dir_cluster, dirname, &entry, &dir_clus, &offset)) {
        fprintf(stderr, "Error: Directory not found\n");
        return false;
    }
    
    if(!(entry.DIR_Attr & ATTR_DIRECTORY)) {
        fprintf(stderr, "Error: Not a directory\n");
        return false;
    }
    
    uint32_t target_dir_first_cluster = fat32_get_first_cluster(&entry);
    
    // Check if empty
    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint8_t *buffer = malloc(cluster_size);
    uint32_t current = target_dir_first_cluster;
    bool is_empty = true;
    
    while(current < FAT32_EOC && is_empty) {
        if(!fat32_read_cluster(current, buffer)) {
             free(buffer); return false;
        }
        DirEntry_t *ents = (DirEntry_t*)buffer;
        int ent_count = cluster_size / sizeof(DirEntry_t);
        for(int j=0; j<ent_count; j++) {
            if(ents[j].DIR_Name[0] == 0x00) break;
            if(ents[j].DIR_Name[0] == 0xE5) continue;
             
             // Ignore "." and ".."
             if(strncmp((char*)ents[j].DIR_Name, ".          ", 11) == 0) continue;
             if(strncmp((char*)ents[j].DIR_Name, "..         ", 11) == 0) continue;
             
             is_empty = false;
             break;
        }
        current = fat32_get_next_cluster(current);
    }
    free(buffer);
    
    if(!is_empty) {
        fprintf(stderr, "Error: Directory not empty\n");
        return false;
    }
    
    // Delete entry
    uint8_t byte = 0xE5;
    uint32_t lba = fat32_cluster_to_lba(dir_clus);
    fseek(image_fp, lba * bpb.BPB_BytsPerSec + offset, SEEK_SET);
    fwrite(&byte, 1, 1, image_fp);
    fflush(image_fp);
    
    // Free chain
    fat32_free_chain(target_dir_first_cluster);
    
    return true;
}

// lsof
// Lists all opened files
void fat32_lsof(void) {
    int count = 0;
    
    // Count open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use) {
            count++;
        }
    }
    
    if (count == 0) {
        printf("No files are currently open\n");
        return;
    }
    
    // Print header
    printf("%-5s %-12s %-6s %-10s %s\n", 
           "Index", "Filename", "Mode", "Offset", "Path");
    printf("-------------------------------------------------------------\n");
    
    // List all open files
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use) {
            printf("%-5d %-12s %-6s %-10u %s\n",
                   i,
                   open_files[i].filename,
                   mode_to_string(open_files[i].mode),
                   open_files[i].offset,
                   open_files[i].path);
        }
    }
}

// lseek [FILENAME] [OFFSET]
// Sets the offset for a file
bool fat32_lseek(const char *filename, uint32_t offset) {
    OpenFile *file = find_open_file(filename);
    if (!file) {
        fprintf(stderr, "Error: File '%s' is not open\n", filename);
        return false;
    }
    
    if (offset > file->size) {
        fprintf(stderr, "Error: Offset %u is larger than file size %u\n", 
                offset, file->size);
        return false;
    }
    
    file->offset = offset;
    return true;
}

// Helper function: Read data from file starting at given offset
static bool read_file_data(uint32_t first_cluster, uint32_t offset, 
                          uint8_t *buffer, uint32_t size) {
    if (first_cluster == 0) {
        // Empty file (no clusters allocated)
        return true;
    }
    
    uint32_t bytes_per_cluster = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;
    uint32_t current_cluster = first_cluster;
    uint32_t bytes_read = 0;
    
    // Skip to the cluster containing the offset
    uint32_t skip_bytes = offset;
    while (skip_bytes >= bytes_per_cluster && current_cluster < FAT32_EOC) {
        current_cluster = fat32_get_next_cluster(current_cluster);
        skip_bytes -= bytes_per_cluster;
    }
    
    // Check if we went past end of file
    if (current_cluster >= FAT32_EOC) {
        return true; // Reached EOF while skipping
    }
    
    // Allocate buffer for reading clusters
    uint32_t cluster_buffer_size = bytes_per_cluster;
    uint8_t *cluster_buffer = malloc(cluster_buffer_size);
    if (!cluster_buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return false;
    }
    
    // Read data from clusters
    while (bytes_read < size && current_cluster < FAT32_EOC) {
        // Read the entire cluster
        if (!fat32_read_cluster(current_cluster, cluster_buffer)) {
            free(cluster_buffer);
            return false;
        }
        
        // Calculate how much to copy from this cluster
        uint32_t to_read = bytes_per_cluster - skip_bytes;
        if (bytes_read + to_read > size) {
            to_read = size - bytes_read;
        }
        
        // Copy data to output buffer
        memcpy(buffer + bytes_read, cluster_buffer + skip_bytes, to_read);
        bytes_read += to_read;
        
        // Move to next cluster
        current_cluster = fat32_get_next_cluster(current_cluster);
        skip_bytes = 0; // Only skip bytes in first cluster
    }
    
    free(cluster_buffer);
    return true;
}

// read [FILENAME] [SIZE]
// Reads data from a file
bool fat32_read(const char *filename, uint32_t size) {
    // Check if file exists
    DirEntry_t *entry = fat32_find_entry(current_dir_cluster, filename);
    if (!entry) {
        fprintf(stderr, "Error: File '%s' does not exist\n", filename);
        return false;
    }
    
    // Check if it's a directory
    if (entry->DIR_Attr & ATTR_DIRECTORY) {
        fprintf(stderr, "Error: '%s' is a directory\n", filename);
        return false;
    }
    
    // Find open file
    OpenFile *file = find_open_file(filename);
    if (!file) {
        fprintf(stderr, "Error: File '%s' is not open\n", filename);
        return false;
    }
    
    // Check read permission
    if (file->mode != MODE_READ && file->mode != MODE_READ_WRITE) {
        fprintf(stderr, "Error: File '%s' is not open for reading\n", filename);
        return false;
    }
    
    // Determine actual bytes to read
    uint32_t bytes_to_read = size;
    if (file->offset + bytes_to_read > file->size) {
        bytes_to_read = file->size - file->offset;
    }
    
    if (bytes_to_read == 0) {
        printf("(end of file)\n");
        return true;
    }
    
    // Allocate buffer for reading
    uint8_t *buffer = malloc(bytes_to_read);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        return false;
    }
    
    // Read data from clusters
    if (!read_file_data(file->first_cluster, file->offset, buffer, bytes_to_read)) {
        free(buffer);
        return false;
    }
    
    // Print the data (handle non-printable characters)
    for (uint32_t i = 0; i < bytes_to_read; i++) {
        if (buffer[i] >= 32 && buffer[i] <= 126) {
            putchar(buffer[i]);
        } else if (buffer[i] == '\n') {
            putchar('\n');
        } else if (buffer[i] == '\r') {
            // Skip carriage return
        } else if (buffer[i] == '\t') {
            putchar('\t');
        } else {
            // Non-printable character - print as dot
            putchar('.');
        }
    }
    printf("\n");
    
    free(buffer);
    
    // Update offset
    file->offset += bytes_to_read;
    
    return true;
}

// Write to a file
bool fat32_write(const char *filename, const char *data) {
    // 1. Find open file
    OpenFile *file = find_open_file(filename);
    if (!file) {
        fprintf(stderr, "Error: File '%s' is not open\n", filename);
        return false;
    }

    // 2. Check mode
    if (file->mode != MODE_WRITE && file->mode != MODE_READ_WRITE) {
        fprintf(stderr, "Error: File '%s' is not open for writing\n", filename);
        return false;
    }

    uint32_t len = strlen(data);
    if (len == 0) return true;

    uint32_t cluster_size = bpb.BPB_BytsPerSec * bpb.BPB_SecPerClus;

    // Handle empty file (first_cluster == 0)
    if (file->first_cluster == 0) {
        file->first_cluster = fat32_allocate_cluster();
        if (file->first_cluster == 0) {
            fprintf(stderr, "Error: Disk full\n");
            return false;
        }
    }

    uint32_t bytes_written = 0;
    uint32_t current_cluster = file->first_cluster;
    
    uint32_t clusters_to_skip = file->offset / cluster_size;
    uint32_t cluster_offset = file->offset % cluster_size;

    for(uint32_t i=0; i<clusters_to_skip; i++) {
        uint32_t next = fat32_get_next_cluster(current_cluster);
        if (next >= FAT32_EOC) {
            uint32_t new_cluster = fat32_allocate_cluster();
            if (new_cluster == 0) return false;
            fat32_set_fat_entry(current_cluster, new_cluster);
            current_cluster = new_cluster;
        } else {
            current_cluster = next;
        }
    }

    while (bytes_written < len) {
        uint32_t lba = fat32_cluster_to_lba(current_cluster);
        uint32_t write_pos = lba * bpb.BPB_BytsPerSec + cluster_offset;
        
        uint32_t space_in_cluster = cluster_size - cluster_offset;
        uint32_t amount = (len - bytes_written < space_in_cluster) ? (len - bytes_written) : space_in_cluster;
        
        fseek(image_fp, write_pos, SEEK_SET);
        fwrite(data + bytes_written, 1, amount, image_fp);
        fflush(image_fp);
        
        bytes_written += amount;
        cluster_offset += amount;
        
        if (cluster_offset >= cluster_size && bytes_written < len) {
            // Need next cluster
            uint32_t next = fat32_get_next_cluster(current_cluster);
            if (next >= FAT32_EOC) {
                uint32_t new_cluster = fat32_allocate_cluster();
                if (new_cluster == 0) {
                    fprintf(stderr, "Error: Disk full\n");
                    break;
                }
                fat32_set_fat_entry(current_cluster, new_cluster);
                current_cluster = new_cluster;
            } else {
                current_cluster = next;
            }
            cluster_offset = 0;
        }
    }

    // Update offset
    file->offset += bytes_written;
    
    // Update size if extended
    if (file->offset > file->size) {
        file->size = file->offset;
        
        // Update directory entry
        uint32_t dir_clus, dir_offset;
        DirEntry_t entry;
        if (fat32_find_entry_info(current_dir_cluster, filename, &entry, &dir_clus, &dir_offset)) {
            entry.DIR_FileSize = file->size;
            entry.DIR_FstClusHI = (file->first_cluster >> 16) & 0xFFFF;
            entry.DIR_FstClusLO = file->first_cluster & 0xFFFF;
            
            uint32_t ent_lba = fat32_cluster_to_lba(dir_clus);
            fseek(image_fp, ent_lba * bpb.BPB_BytsPerSec + dir_offset, SEEK_SET);
            fwrite(&entry, sizeof(DirEntry_t), 1, image_fp);
            fflush(image_fp);
        }
    }

    return true;
}