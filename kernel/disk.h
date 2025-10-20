#pragma once
#include <stdint.h>

#define SECTOR_SIZE 512

int ata_read_sector(uint32_t lba, void* buffer);
int ata_write_sector(uint32_t lba, const void* buffer);
