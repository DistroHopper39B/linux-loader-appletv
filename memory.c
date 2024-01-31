/*
 * PROJECT:     FreeLoader wrapper for Apple TV
 * LICENSE:     GPL-2.0-only (https://spdx.org/licenses/GPL-2.0-only)
 * PURPOSE:     Main loader functions for FreeLoader on the original Apple TV
 * COPYRIGHT:   Copyright 2023-2024 DistroHopper39B (distrohopper39b.business@gmail.com)
 */

/* INCLUDES *******************************************************************/

#include <linuxloader.h>

/* FUNCTIONS ******************************************************************/

/* Modified from atv-bootloader linux_code.c */

static void add_memory_region(struct boot_e820_entry *e820_map,
                              u32 *e820_nr_map,
                              UINT64 start,
                              UINT64 size,
                              UINT32 type)
{
    u32 x = *e820_nr_map;

    if (x == E820_MAX_ENTRIES_ZEROPAGE) {
        fatal("Too many entries in the memory map!\n");
        return;
    }

    if ((x > 0) && e820_map[x-1].addr + e820_map[x-1].size == start
        && e820_map[x-1].type == type)
        e820_map[x-1].size += size;
    else {
        e820_map[x].addr = start;
        e820_map[x].size = size;
        e820_map[x].type = type;
        (*e820_nr_map)++;
    }
}


void fill_e820map(struct boot_params *boot_params)
{
    u32               nr_map, e820_nr_map = 0, i;
    UINT64            start, end, size;
    efi_memory_desc_t *md, *p;
    struct boot_e820_entry  *e820_map;

    nr_map = boot_params->efi_info.efi_memmap_size / boot_params->efi_info.efi_memdesc_size;
    e820_map = (struct boot_e820_entry *) boot_params->e820_table;

    for (i = 0, p = (efi_memory_desc_t *) boot_params->efi_info.efi_memmap; i < nr_map; i++) {
        md = p;
        switch (md->type) {
            // ACPI tables -- to be preserved by loader/OS until ACPI is enable
            // once enabled, can be treated as conventional memory
            case EFI_ACPI_RECLAIM_MEMORY:
                add_memory_region(e820_map, &e820_nr_map,
                                  md->phys_addr,
                                  md->num_pages << EFI_PAGE_SHIFT,
                                  E820_ACPI);
                break;
                // must be preserved by loader/OS in working an ACPI S1-S3 states
            case EFI_RUNTIME_SERVICES_CODE:
            case EFI_RUNTIME_SERVICES_DATA:
            case EFI_RESERVED_TYPE:
            case EFI_MEMORY_MAPPED_IO:
            case EFI_MEMORY_MAPPED_IO_PORT_SPACE:
            case EFI_UNUSABLE_MEMORY:
            case EFI_PAL_CODE:
                add_memory_region(e820_map, &e820_nr_map,
                                  md->phys_addr,
                                  md->num_pages << EFI_PAGE_SHIFT,
                                  E820_RESERVED);
                break;
                // can be treaded as conventional memory by loader/OS
            case EFI_LOADER_CODE:
            case EFI_LOADER_DATA:
            case EFI_BOOT_SERVICES_CODE:
            case EFI_BOOT_SERVICES_DATA:
            case EFI_CONVENTIONAL_MEMORY:
                start = md->phys_addr;
                size  = md->num_pages << EFI_PAGE_SHIFT;
                end   = start + size;
                /* Fix up for BIOS that claims RAM in 640K-1MB region */
                if (start < 0x100000ULL && end > 0xA0000ULL) {
                    if (start < 0xA0000ULL) {
                        /* start < 640K
                         * set memory map from start to 640K
                         */
                        add_memory_region(e820_map,
                                          &e820_nr_map,
                                          start,
                                          0xA0000ULL-start,
                                          E820_RAM);
                    }
                    if (end <= 0x100000ULL) {
                        continue;
                    }
                    // end > 1MB, set memory map avoiding 640K to 1MB hole
                    start = 0x100000ULL;
                    size = end - start;
                }
                add_memory_region(e820_map, &e820_nr_map,
                                  start, size, E820_RAM);
                break;
                // ACPI working memory --- should be preserved by loader/OS in the working
                //  and ACPI S1-S3 states
            case EFI_ACPI_MEMORY_NVS:
                add_memory_region(e820_map, &e820_nr_map,
                                  md->phys_addr,
                                  md->num_pages << EFI_PAGE_SHIFT,
                                  E820_NVS);
                break;
            default:
                /* We should not hit this case */
                warn("default add_memory_region, should not see this\n");
                add_memory_region(e820_map, &e820_nr_map,
                                  md->phys_addr,
                                  md->num_pages << EFI_PAGE_SHIFT,
                                  E820_RESERVED);
                break;
        }
        p = (efi_memory_desc_t *) NextEFIMemoryDescriptor(p, boot_params->efi_info.efi_memdesc_size);
    }
    boot_params->e820_entries = e820_nr_map;
}

void print_e820_memory_map(struct boot_params *boot_params)
{
    int              i;
    struct boot_e820_entry *e820_map;

    e820_map = (struct boot_e820_entry *) boot_params->e820_table;

    for (i = 0; i < boot_params->e820_entries; i++) {
        debug_printf("%s: 0x%08X%08X - 0x%08X%08X ", "E820 Map",
               hi32( e820_map[i].addr ),
               lo32( e820_map[i].addr ),
               hi32( e820_map[i].addr + e820_map[i].size),
               lo32( e820_map[i].addr + e820_map[i].size) );
        switch (e820_map[i].type) {
            case E820_RAM:
                debug_printf("(usable)\n");
                break;
            case E820_RESERVED:
                debug_printf("(reserved)\n");
                break;
            case E820_ACPI:
                debug_printf("(ACPI data)\n");
                break;
            case E820_NVS:
                debug_printf("(ACPI NVS)\n");
                break;
            default:
                debug_printf("type %u\n", e820_map[i].type);
                break;
        }
    }
}

