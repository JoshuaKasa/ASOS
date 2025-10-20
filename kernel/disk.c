#include "disk.h"
#include "io.h"

#define ATA_IO_BASE       0x1F0
#define ATA_REG_DATA      (ATA_IO_BASE + 0) // 16-bit
#define ATA_REG_SECCNT    (ATA_IO_BASE + 2)
#define ATA_REG_LBA0      (ATA_IO_BASE + 3)
#define ATA_REG_LBA1      (ATA_IO_BASE + 4)
#define ATA_REG_LBA2      (ATA_IO_BASE + 5)
#define ATA_REG_DRIVE     (ATA_IO_BASE + 6)
#define ATA_REG_STATUS    (ATA_IO_BASE + 7) // Read
#define ATA_REG_COMMAND   (ATA_IO_BASE + 7) // Write

#define ATA_CTRL_BASE     0x3F6
#define ATA_REG_ALTSTAT   (ATA_CTRL_BASE + 0) // Read (non-clearing)
#define ATA_REG_DEVCTRL   (ATA_CTRL_BASE + 0) // Write

// Commands 
#define ATA_CMD_READ_SECT   0x20
#define ATA_CMD_WRITE_SECT  0x30

// STATUS bit 
#define ATA_SR_BSY  0x80
#define ATA_SR_DRQ  0x08
#define ATA_SR_DF   0x20
#define ATA_SR_ERR  0x01

// Simple timeout to not get infinite looops
#define ATA_TIMEOUT 1000000

// Small delay ~400ms (4 reads on the alt status)
static inline void ata_400ns_delay(void) {
    (void)inb(ATA_REG_ALTSTAT);
    (void)inb(ATA_REG_ALTSTAT);
    (void)inb(ATA_REG_ALTSTAT);
    (void)inb(ATA_REG_ALTSTAT);
}

static int ata_wait_not_busy(void) {
    int t = ATA_TIMEOUT;

    while (t--) {
        uint8_t st = inb(ATA_REG_STATUS);

        if (!(st & ATA_SR_BSY)) return 0;
    }

    return -1;
}

static int ata_wait_drq_ok(void) {
    int t = ATA_TIMEOUT;

    while (t--) {
        uint8_t st = inb(ATA_REG_STATUS);

        if (st & (ATA_SR_ERR | ATA_SR_DF)) return -2; //  Error
        if ((st & (ATA_SR_BSY | ATA_SR_DRQ)) == ATA_SR_DRQ) return 0; // Ready with data
    }

    return -3; // Rimeout
}

static int ata_wait_write_done(void) {
    int t = ATA_TIMEOUT;

    while (t--) {
        uint8_t st = inb(ATA_REG_STATUS);
        if (!(st & ATA_SR_BSY) && !(st & ATA_SR_DRQ)) {
            if (st & (ATA_SR_ERR | ATA_SR_DF)) return -12;
            return 0;
        }
    }

    return -13;
}

int ata_read_sector(uint32_t lba, void* buffer) {
    if (ata_wait_not_busy() != 0) return -10;

    // Select drive master + 4 high bits of LBA (LBA mode)
    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_400ns_delay();

    // Set how many sectors to read
    outb(ATA_REG_SECCNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    // Semnd READ SECTORS command
    outb(ATA_REG_COMMAND, ATA_CMD_READ_SECT);
    ata_400ns_delay();

    // Wait for data to be ready
    if (ata_wait_drq_ok() != 0) return -11;

    // Read 512 byte = 256 word of 16 bits each
    uint16_t* p = (uint16_t*)buffer;

    for (int i = 0; i < (SECTOR_SIZE / 2); i++) {
        p[i] = inw(ATA_REG_DATA);
    }

    return 0; // OK
}

int ata_write_sector(uint32_t lba, const void* buffer) {
    if (ata_wait_not_busy() != 0) return -10;

    outb(ATA_REG_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ata_400ns_delay();

    outb(ATA_REG_SECCNT, 1);
    outb(ATA_REG_LBA0, (uint8_t)(lba & 0xFF));
    outb(ATA_REG_LBA1, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_REG_LBA2, (uint8_t)((lba >> 16) & 0xFF));

    outb(ATA_REG_COMMAND, ATA_CMD_WRITE_SECT);
    ata_400ns_delay();

    if (ata_wait_drq_ok() != 0) return -11;

    const uint16_t* p = (const uint16_t*)buffer;
    for (int i = 0; i < (SECTOR_SIZE / 2); i++) {
        outw(ATA_REG_DATA, p[i]);
    }

    if (ata_wait_write_done() != 0) return -14;

    outb(ATA_REG_COMMAND, 0xE7);
    if (ata_wait_not_busy() != 0) return -15;

    return 0;
}
