#pragma once
#include <stdint.h>

#define ASOFS_MAGIC 0x41534F46 // ASOFS in ASCII
#define ASOFS_MAX_FILES 16

typedef struct {
    char name[16];
    uint32_t start_lba;
    uint32_t size;
} __attribute__((packed)) asofs_file_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t file_count;
    uint32_t next_free_lba;
    asofs_file_entry_t files[ASOFS_MAX_FILES];
} __attribute__((packed)) asofs_superblock_t;

int asofs_load_superblock(void);
void asofs_list_files(void);
asofs_file_entry_t* asofs_find_file(const char* name);
int asofs_load_file(const asofs_file_entry_t* file, uint8_t* dest);
int asofs_write_file(const char* name, const char* data, uint32_t size);
void asofs_run_app(const char* name);
void asofs_return_to_kernel(void);
int asofs_enum_files(char* out, int max_entries, int name_max);
