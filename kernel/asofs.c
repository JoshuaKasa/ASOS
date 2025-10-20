#include "../lib/stdlib.h"
#include "../lib/string.h"
#include "asofs.h"
#include "disk.h"
#include "console.h"

#define SUPERBLOCK_LBA 50
static asofs_superblock_t sb;

static int asofs_read_data(uint32_t start_lba, uint8_t* dest, uint32_t size) {
    uint32_t bytes_left = size;
    uint32_t lba = start_lba;
    uint8_t sector_buf[SECTOR_SIZE];

    while (bytes_left > 0) {
        int rc = ata_read_sector(lba, sector_buf);

        if (rc != 0)
            return -1;

        uint32_t to_copy =
            (bytes_left >= SECTOR_SIZE) ? SECTOR_SIZE : bytes_left;
        memcpy(dest, sector_buf, to_copy);

        bytes_left -= to_copy;
        dest += to_copy;
        lba++;
    }

    return 0;
}

static int asofs_write_data(uint32_t start_lba,
                            const uint8_t* src,
                            uint32_t size) {
    uint32_t bytes_left = size;
    uint32_t lba = start_lba;
    uint8_t sector_buf[SECTOR_SIZE];

    while (bytes_left > 0) {
        memset(sector_buf, 0, SECTOR_SIZE);
        uint32_t to_copy =
            (bytes_left >= SECTOR_SIZE) ? SECTOR_SIZE : bytes_left;
        memcpy(sector_buf, src, to_copy);

        if (ata_write_sector(lba, sector_buf) != 0)  // Error checking
            return -1;

        bytes_left -= to_copy;
        src += to_copy;
        lba++;
    }
    return 0;
}

static asofs_file_entry_t* asofs_create_file_entry(const char* name,
                                                   uint32_t size) {
    if (sb.file_count >= ASOFS_MAX_FILES)
        return NULL;

    asofs_file_entry_t* f = &sb.files[sb.file_count++];

    strcpy(f->name, name);
    f->start_lba = sb.next_free_lba;
    f->size = size;

    return f;
}

static int asofs_write_superblock(void) {
    uint8_t buf[SECTOR_SIZE];

    memset(buf, 0, sizeof buf);
    memcpy(buf, &sb, sizeof sb);

    return ata_write_sector(SUPERBLOCK_LBA, buf);
}

int asofs_load_superblock(void) {
    uint8_t buf[SECTOR_SIZE];

    if (ata_read_sector(SUPERBLOCK_LBA, buf) != 0) {
        console_write("[ASOFS] Error during superblock reading!\n");
        return -1;
    }
    memcpy(&sb, buf, sizeof sb);

    if (sb.magic != ASOFS_MAGIC) {
        console_write("[ASOFS] Wrong magic number, FS not valid!\n");
        return -2;
    }

    console_write("[ASOFS] Correctly read superblock!\n");
    return 0;
}

void asofs_list_files(void) {
    console_write("[ASOFS] Disk files:\n");
    for (uint32_t i = 0; i < sb.file_count; i++) {
        console_write(" - ");
        console_write(sb.files[i].name);
        console_write(" (");

        char tmp[16];

        itoa(sb.files[i].size, tmp, 10);
        console_write(tmp);
        console_write(" bytes)\n");
    }
}

asofs_file_entry_t* asofs_find_file(const char* name) {
    for (uint32_t i = 0; i < sb.file_count; i++) {
        if (strcmp(sb.files[i].name, name) == 0)
            return &sb.files[i];
    }

    return 0;
}

int asofs_load_file(const asofs_file_entry_t* file, uint8_t* dest) {
    if (!file || !dest)
        return -1;
    if (file->size == 0)
        return 0;

    return asofs_read_data(file->start_lba, dest, file->size);
}

int asofs_write_file(const char* name, const char* data, uint32_t size) {
    if (!name || !data || size == 0)
        return -1;

    asofs_file_entry_t* f = asofs_find_file(name);
    int is_new = 0;

    if (!f) {
        f = asofs_create_file_entry(name, size);
        if (!f) {
            console_write("[ASOFS] File table full!\n");
            return -2;
        }
        is_new = 1;
    } 
    else {
        console_write("[ASOFS] Overwriting file: ");
        console_write(name);
        console_write("\n");
    }

    // Write real data
    if (asofs_write_data(f->start_lba, (const uint8_t*)data, size) != 0)
        return -3;

    f->size = size;

    if (is_new)
        sb.next_free_lba =
            f->start_lba + (size + SECTOR_SIZE - 1) / SECTOR_SIZE;

    asofs_write_superblock();

    console_write("[ASOFS] ");
    console_write(is_new ? "Created: " : "Updated: ");
    console_write(name);
    console_write("\n");

    return 0;
}

void asofs_run_app(const char* name) {
    const asofs_file_entry_t* f = asofs_find_file(name);

    if (!f) {
        console_write("[ASOFS] App not found: ");
        console_write(name);
        console_write("\n");

        return;
    }

    #define APP_BASE ((uint8_t*)0x00300000)

    if (asofs_load_file(f, APP_BASE) != 0) {
        console_write("[ASOFS] Error during app loading!\n");

        return;
    }

    console_write("[ASOFS] App loaded in memory. Starting...\n");
    console_clear();  // We doin't want trash from other apps
    void (*entry)(void) = (void (*)(void))APP_BASE;  // 0x00300000
    entry();
}

int asofs_enum_files(char* out, int max_entries, int name_max) {
    if (!out || max_entries <= 0 || name_max <= 1) 
        return -1;

    int count = (int)sb.file_count;

    if (count > max_entries) count = max_entries;

    for (int i = 0; i < count; ++i) {
        const char* src = sb.files[i].name;
        char* dst = out + i * name_max;
        int n = 16; 

        if (n > name_max - 1) 
            n = name_max - 1;

        int k = 0;

        for (; k < n && src[k]; ++k) dst[k] = src[k];
        for (; k < n; ++k) dst[k] = src[k];

        dst[n] = 0;
    }
    return count;
}

void asofs_return_to_kernel(void) {
    asm volatile("jmp kernel_main");
}
