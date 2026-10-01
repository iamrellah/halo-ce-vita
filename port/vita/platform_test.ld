/* vita-elf-create appends SCE module/import metadata to the executable segment.
 * Reserve a full additional page before writable data, independent of how close
 * the code/rodata end falls to the next page boundary. */
SECTIONS { . = ALIGN(0x1000) + 0x1000; }
INSERT BEFORE .init_array;
