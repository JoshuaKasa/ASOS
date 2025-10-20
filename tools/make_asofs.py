import os, struct

ASOFS_MAGIC = 0x41534F46 # "ASOF" in ASCII
SECTOR_SIZE = 512
SUPERBLOCK_LBA = 50
APP_START_LBA = 60 # First sector after superblock 
APP_DIR = "app"
DISK_IMG = "disk.img"


def align_up(x, to):
    return ((x + to - 1) // to) * to


def main():
    entries = []
    current_lba = APP_START_LBA

    with open(DISK_IMG, "r+b") as disk:
        # Look for all .bin apps
        for fname in sorted(os.listdir(APP_DIR)):
            if not fname.endswith(".bin"):
                continue

            path = os.path.join(APP_DIR, fname)
            with open(path, "rb") as f:
                data = f.read()

            size = len(data)
            sectors = align_up(size, SECTOR_SIZE) // SECTOR_SIZE

            print(f"[+] Adding {fname} ({size} bytes, {sectors} sectors @ LBA {current_lba})")

            # Write app on disk
            disk.seek(current_lba * SECTOR_SIZE)
            disk.write(data.ljust(sectors * SECTOR_SIZE, b"\x00"))

            # Register entry
            name = fname.encode()[:16]
            name = name + b"\x00" * (16 - len(name))
            entries.append((name, current_lba, size))

            # Update position for next file
            current_lba += sectors

        # Build superblock
        next_free_lba = current_lba # First free sector after last file

        # Structure: magic (4) | file_count (4) | next_free_lba (4)
        sb = struct.pack("<III", ASOFS_MAGIC, len(entries), next_free_lba)

        # Add entry files
        for name, lba, size in entries:
            sb += struct.pack("<16sII", name, lba, size)

        # Padding until 512 bytes
        sb = sb.ljust(SECTOR_SIZE, b"\x00")

        # Write superblock
        disk.seek(SUPERBLOCK_LBA * SECTOR_SIZE)
        disk.write(sb)

    print(f"[D] Superblock written with {len(entries)} apps")
    print(f"[D] Next free LBA = {next_free_lba}")


if __name__ == "__main__":
    main()
