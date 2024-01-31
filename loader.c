/*
 * PROJECT:     FreeLoader wrapper for Apple TV
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Main loader functions for FreeLoader on the original Apple TV
 * COPYRIGHT:   Copyright 2023-2024 DistroHopper39B (distrohopper39b.business@gmail.com)
 */

/* INCLUDES *******************************************************************/

#include <linuxloader.h>

/* GLOBALS ********************************************************************/

PMACH_BOOTARGS BootArgs;

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 0

void *relocated_kernel_start = (void *) 0x00100000; // kernel will always be loaded to 1MB

// Descriptor table base addresses & limits for Linux startup.
dt_addr_t gdt_addr = { 0x800, 0x94000 };
dt_addr_t idt_addr = { 0, 0 };

// Initial GDT layout for Linux startup.
uint16_t init_gdt[] = {
        /* gdt[0]: (0x00) dummy */
        0, 0, 0, 0,

        /* gdt[1]: (0x08) unused */
        0, 0, 0, 0,

        /* Documented linux kernel segments */
        /* gdt[2]: (0x10) flat code segment */
        0xFFFF,		/* 4Gb - (0x100000*0x1000 = 4Gb) */
        0x0000,		/* base address=0 */
        0x9A00,		/* code read/exec */
        0x00CF,		/* granularity=4096, 386 (+5th nibble of limit) */
        /* gdt[3]: (0x18) flat data segment */
        0xFFFF,		/* 4Gb - (0x100000*0x1000 = 4Gb) */
        0x0000,		/* base address=0 */
        0x9200,		/* data read/write */
        0x00CF,		/* granularity=4096, 386 (+5th nibble of limit) */

        /* gdt[4]: (0x20) unused */
        0, 0, 0, 0,
        /* gdt[5]: (0x28) unused */
        0, 0, 0, 0,

        /* gdt[6]: (0x30) unused */
        0, 0, 0, 0,
        /* gdt[7]: (0x38) unused */
        0, 0, 0, 0,

        /* gdt[8]: (0x40) unused */
        0, 0, 0, 0,
        /* gdt[9]: (0x48) unused */
        0, 0, 0, 0,

        /* gdt[10]:(0x50) unused */
        0, 0, 0, 0,
        /* gdt[11]:(0x58) unused */
        0, 0, 0, 0,

        /* Segments used by the 2.5.x kernel */
        /* gdt[12]:(0x60) flat code segment */
        0xFFFF,		/* 4Gb - (0x100000*0x1000 = 4Gb) */
        0x0000,		/* base address=0 */
        0x9A00,		/* code read/exec */
        0x00CF,		/* granularity=4096, 386 (+5th nibble of limit) */
        /* gdt[13]:(0x68) flat data segment */
        0xFFFF,		/* 4Gb - (0x100000*0x1000 = 4Gb) */
        0x0000,		/* base address=0 */
        0x9200,		/* data read/write */
        0x00CF,		/* granularity=4096, 386 (+5th nibble of limit) */
};

uint32_t init_gdt_size = sizeof(init_gdt);


/* FUNCTIONS ******************************************************************/

/* Set up command line and enable verbose mode */
static
void SetupCmdline() {
    /* Check if we should enable verbose mode */
    if (strstr(BootArgs->CmdLine, "-v")) {
        /* Enable verbose printing in freeldr-wrapper-appletv */
        ClearScreen(TRUE);
        debug_printf("Booting in Verbose Mode. ");
    }
}

/* Get RSDP */
void *AcpiGetRsdp() {
    efi_system_table_t *system_table;
    efi_config_table_t *config_tables;
    u32 i, num_config_tables;
    u32 acpi_table = 0, acpi_20_table = 0;

    system_table = (efi_system_table_t *) BootArgs->EfiSystemTable;
    num_config_tables = system_table->nr_tables;
    config_tables = (efi_config_table_t *) system_table->tables;

    // scan system table
    for (i = 0; i < num_config_tables; i++) {
        if (efi_guidcmp(config_tables[i].guid, ACPI_20_TABLE_GUID) == 0) {
            acpi_20_table = config_tables[i].table;
        }
        if (efi_guidcmp(config_tables[i].guid, ACPI_TABLE_GUID) == 0) {
            acpi_table = config_tables[i].table;
        }

    }

    // use ACPI 2.0 first, if that is not found use ACPI 1.0
    if (acpi_20_table) {
        trace("Using ACPI 2.0 found at 0x%X.\n", acpi_20_table);
        return (void *) acpi_20_table;
    } else if (acpi_table) {
        trace("Using ACPI 1.0 found at 0x%X.\n", config_tables[i].table);
        return (void *) acpi_table;
    }

    fatal("No ACPI table found!\n");
    return NULL;
}

/* Load Linux kernel */
static
void LoadLinux(struct boot_params *boot_params, const u8 *kernel_ptr, u32 kernel_len, const u8 *initrd_ptr, u32 initrd_len) {
    // find actual linux kernel length
    u32 real_kernel_len = kernel_len - ((kernel_ptr[0x1F1] + 1) * 512);
    // copy the linux kernel to the relocated location
    trace("Copying Linux kernel to 0x%X...\n", relocated_kernel_start);
    memcpy(relocated_kernel_start, &kernel_ptr[(kernel_ptr[0x1F1] + 1) * 512], real_kernel_len);
    trace("done.\n");
    // zero boot parameters
    memset(boot_params, 0, sizeof(struct boot_params)); // 4096
    // set up the linux setup_header
    struct setup_header *setup_header = &boot_params->hdr;
    u32 setup_header_end = kernel_ptr[0x201] + 0x202;
    memcpy(setup_header, (kernel_ptr + 0x1f1), setup_header_end - 0x1f1);

    trace("Loading Linux with boot protocol %u.%u\n", setup_header->version >> 8, setup_header->version & 0xff);
    // FIXME: check to make sure we are loading kernel with a modern protocol (how low can we go for working video etc)

    // print out linux kernel version information
    char *kernel_version[128];
    memcpy(kernel_version, kernel_ptr + (setup_header->kernel_version + 0x200), 128);
    debug_printf("Linux kernel version %s\n", kernel_version);

    // configure the setup_header
    setup_header->cmd_line_ptr = (u32) (u8 *) BootArgs->CmdLine;
    setup_header->vid_mode = 0xffff; // "normal"

    setup_header->type_of_loader = 0xff; // unassigned

    if(!(setup_header->loadflags & (1 << 0))) {
        fatal("Linux kernels that load at 0x10000 are unsupported!\n");
    }

    setup_header->loadflags &= ~(1 << 5); // print early messages

    // set up initial ramdisk
    if(initrd_len != 0) {
        trace("Setting up initial ramdisk.\n");
        setup_header->ramdisk_image = (u32) initrd_ptr;
        setup_header->ramdisk_size  = initrd_len;
    }

    // set up video
    struct screen_info *screen_info = &boot_params->screen_info;

    screen_info->capabilities   = VIDEO_CAPABILITY_64BIT_BASE | VIDEO_CAPABILITY_SKIP_QUIRKS;
    screen_info->flags          = VIDEO_FLAGS_NOCURSOR;
    screen_info->lfb_base       = BootArgs->Video.BaseAddress;
    screen_info->lfb_size       = (BootArgs->Video.Pitch * BootArgs->Video.Height);
    screen_info->lfb_width      = (BootArgs->Video.Pitch / 4); // Video.Width is not always correct
    screen_info->lfb_height     = BootArgs->Video.Height;
    screen_info->lfb_depth      = BootArgs->Video.Depth;
    screen_info->lfb_linelength = BootArgs->Video.Pitch;
    screen_info->red_size       = 8;
    screen_info->red_pos        = 16;
    screen_info->green_size     = 8;
    screen_info->green_pos      = 8;
    screen_info->blue_size      = 8;
    screen_info->blue_pos       = 0;

    screen_info->orig_video_isVGA = VIDEO_TYPE_EFI;

    boot_params->acpi_rsdp_addr = (u32) AcpiGetRsdp();

    // sign off!
    memcpy(&boot_params->efi_info.efi_loader_signature, "EL32", 4);

    // setup EFI memory map
    boot_params->efi_info.efi_systab = BootArgs->EfiSystemTable;
    boot_params->efi_info.efi_memmap = BootArgs->EfiMemoryMap;
    boot_params->efi_info.efi_memmap_size = BootArgs->EfiMemoryMapSize;
    boot_params->efi_info.efi_memdesc_size = BootArgs->EfiMemoryDescriptorSize;
    boot_params->efi_info.efi_memdesc_version = BootArgs->EfiMemoryDescriptorVersion;

    // setup e820 memory map
    fill_e820map(boot_params);
    print_e820_memory_map(boot_params);

    // GO!!
    // Initialize Linux GDT.
    memset((void *) gdt_addr.base, 0x00, gdt_addr.limit);
    memcpy((void *) gdt_addr.base, init_gdt, init_gdt_size);

    // Load descriptor table pointers.
    asm volatile ( "lidt %0" : : "m" (idt_addr) );
    asm volatile ( "lgdt %0" : : "m" (gdt_addr) );

    // ebx := 0  (%%TBD - do not know why, yet)
    // ecx := kernel entry point
    // esi := address of boot sector and setup data
    asm volatile ( "movl %0, %%esi" : : "m" (boot_params) );
    asm volatile ( "movl %0, %%ecx" : : "m" (relocated_kernel_start) );
    asm volatile ( "xorl %%ebx, %%ebx" : : );

    // Jump to kernel entry point.
    asm volatile ( "jmp *%%ecx" : : );
}

/* C entry point. */
void WrapperInit(u32 BootArgPtr) {
    /* set up bootArgs */
    BootArgs = (PMACH_BOOTARGS) BootArgPtr;
    /* set up screen */
    SetupScreen();
    /* set up command line */
    SetupCmdline();
    debug_printf("Linux loader for Apple TV version %d.%d.%d (built with %s on %s %s) [%s@%s]\n",
                 VERSION_MAJOR,
                 VERSION_MINOR,
                 VERSION_PATCH,
                 __VERSION__,
                 __DATE__,
                 __TIME__,
                 __BUILD_USER__,
                 __BUILD_HOST__
    );
    debug_printf("Command line arguments: %s\n", BootArgs->CmdLine);

    debug_printf("Starting Linux...\n");
    /* Initialize boot parameters */
    u8 boot_sector[sizeof(struct boot_params)];
    struct boot_params *boot_params = (struct boot_params *) boot_sector;

    /* Find Linux kernel */
    u32 kernel_len = 0;
    u8 *kernel_ptr = GetSectionDataFromHeader(&_mh_execute_header, "__TEXT", "__vmlinuz", &kernel_len);
    if (!kernel_len) {
        fatal("Linux kernel not found!\n");
    }
    /* Find initial ramdisk */
    u32 initrd_len = 0;
    u8 *initrd_ptr = GetSectionDataFromHeader(&_mh_execute_header, "__TEXT", "__initrd", &initrd_len);
    if (!initrd_len) {
        warn("No initial ramdisk found! Linux may kernel panic.\n");
    }
    u32 *signature = (u32 *)(kernel_ptr + 0x202);
    if (*signature != 'SrdH') {
        fatal("This is not a Linux kernel! Signature is 0x%08X\n", signature);
    }
    LoadLinux(boot_params, kernel_ptr, kernel_len, initrd_ptr, initrd_len);

    fail();
}
