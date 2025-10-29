#include <stdint.h>

#define ATA_PRIMARY_IO       0x1F0
#define ATA_REG_DATA         0x00
#define ATA_REG_ERROR        0x01
#define ATA_REG_SECCOUNT0    0x02
#define ATA_REG_LBA0         0x03
#define ATA_REG_LBA1         0x04
#define ATA_REG_LBA2         0x05
#define ATA_REG_HDDEVSEL     0x06
#define ATA_REG_COMMAND      0x07
#define ATA_REG_STATUS       0x07  // Same as COMMAND for reads

#define ATA_CMD_READ_PIO     0x20

// Status bits
#define ATA_SR_ERR  0x01  // Error
#define ATA_SR_DRQ  0x08  // Data request ready
#define ATA_SR_SRV  0x10  // Overlapped Mode Service Request
#define ATA_SR_DF   0x20  // Device Fault
#define ATA_SR_RDY  0x40  // Drive ready
#define ATA_SR_BSY  0x80  // Busy

// I/O helpers
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void insw(uint16_t port, void *addr, uint32_t count) {
    __asm__ volatile ("rep insw" : "+D"(addr), "+c"(count) : "d"(port) : "memory");
}

// Wait for BSY=0 and optionally DRQ=1, return 0 if ready, -1 on error/timeout
static int ata_wait_ready(int check_drq) {
    for (int i = 0; i < 100000; i++) { // small timeout loop
        uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY)) {
            if (check_drq) {
                if (status & ATA_SR_DRQ)
                    return 0;  // ready for data transfer
            } else {
                return 0;  // just not busy
            }
        }
    }
    return -1; // timeout or no DRQ
}

// --- Safe, read-only 512-byte ATA sector read ---
int ata_read_sector(uint32_t lba, void *buffer) {
    // Select drive + LBA bits
    outb(ATA_PRIMARY_IO + ATA_REG_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));

    // Set up sector count and address
    outb(ATA_PRIMARY_IO + ATA_REG_SECCOUNT0, 1);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA0, (uint8_t)lba);
    outb(ATA_PRIMARY_IO + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(ATA_PRIMARY_IO + ATA_REG_LBA2, (uint8_t)(lba >> 16));

    // Issue READ SECTORS command
    outb(ATA_PRIMARY_IO + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    // Wait for drive to be ready and request data
    if (ata_wait_ready(1) < 0)
        return -1; // timeout or not ready

    uint8_t status = inb(ATA_PRIMARY_IO + ATA_REG_STATUS);
    if (status & (ATA_SR_ERR | ATA_SR_DF))
        return -1; // hardware error or device fault

    // Transfer 256 words (512 bytes)
    insw(ATA_PRIMARY_IO + ATA_REG_DATA, buffer, 256);

    // Final wait until BSY clears
    if (ata_wait_ready(0) < 0)
        return -1; // timeout

    return 0; // success
}
