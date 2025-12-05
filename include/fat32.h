#ifndef FAT32_H
#define FAT32_H
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// BIOS Parameter Block (BPB)
#pragma pack(push, 1)
typedef struct {
    uint8_t  BS_jmpBoot[3];
    uint8_t  BS_OEMName[8];
    uint16_t BPB_BytsPerSec;
    uint8_t  BPB_SecPerClus;
    uint16_t BPB_RsvdSecCnt;
    uint8_t  BPB_NumFATs;
    uint16_t BPB_RootEntCnt;
    uint16_t BPB_TotSec16;
    uint8_t  BPB_Media;
    uint16_t BPB_FATSz16;
    uint16_t BPB_SecPerTrk;
    uint16_t BPB_NumHeads;
    uint32_t BPB_HiddSec;
    uint32_t BPB_TotSec32;
    uint32_t BPB_FATSz32;
    uint16_t BPB_ExtFlags;
    uint16_t BPB_FSVer;
    uint32_t BPB_RootClus;
    uint16_t BPB_FSInfo;
    uint16_t BPB_BkBootSec;
    uint8_t  BPB_Reserved[12];
    uint8_t  BS_DrvNum;
    uint8_t  BS_Reserved1;
    uint8_t  BS_BootSig;
    uint32_t BS_VolID;
    uint8_t  BS_VolLab[11];
    uint8_t  BS_FilSysType[8];
} BPB_t;
#pragma pack(pop)

// FAT32 Directory Entry
#pragma pack(push, 1)
typedef struct {
    uint8_t  DIR_Name[11];
    uint8_t  DIR_Attr;
    uint8_t  DIR_NTRes;
    uint8_t  DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint16_t DIR_FstClusHI;
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint16_t DIR_FstClusLO;
    uint32_t DIR_FileSize;
} DirEntry_t;
#pragma pack(pop)

// File attribute bits
#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LONG_NAME  (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)

// Specific cluster values
#define FAT32_EOC       0x0FFFFFF8  // End of cluster chain
#define FAT32_BAD       0x0FFFFFF7  // Bad cluster

// Globals
extern FILE *image_fp;
extern BPB_t bpb;

// Main API functions
bool fat32_mount(const char *filename);
void fat32_unmount();
void fat32_print_info();
const char *fat32_get_image_name();
const char *fat32_get_cwd_name();

// Navigation functions
bool fat32_cd(const char *dirname);
void fat32_ls();

// Create functions
bool fat32_mkdir(const char *dirname);
bool fat32_creat(const char *filename);

// Delete functions
bool fat32_rm(const char *filename);
bool fat32_rmdir(const char *dirname);

// Update functions
bool fat32_mv(const char *src, const char *dest);

// Helper functions
uint32_t fat32_get_first_cluster(DirEntry_t *entry);
uint32_t fat32_get_next_cluster(uint32_t cluster);
uint32_t fat32_cluster_to_lba(uint32_t cluster);
bool fat32_read_cluster(uint32_t cluster, void *buffer);
DirEntry_t* fat32_find_entry(uint32_t dir_cluster, const char *name);

uint32_t fat32_find_free_cluster(void);
void fat32_set_fat_entry(uint32_t cluser, uint32_t value);
uint32_t fat32_allocate_cluster(void);
bool fat32_write_dir_entry(uint32_t dir_cluser, DirEntry_t *entry);
void fat32_generate_short_name(const char *input, uint8_t out[11]);
void fat32_zero_cluster(uint32_t cluster);
bool compare_fat32_name(const uint8_t *fat_name, const char *regular_name);

#endif
