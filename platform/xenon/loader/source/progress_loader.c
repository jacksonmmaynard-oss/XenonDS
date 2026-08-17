// SPDX-License-Identifier: GPL-2.0-only
// Derived from LibXenon's GPL-2.0 ELF loader by Hector Martin.
#include "progress_loader.h"

#include <elf/elf_abi.h>
#include <fcntl.h>
#include <ppc/cache.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time/time.h>
#include <unistd.h>
#include <xenon_soc/xenon_power.h>

#define XENONDS_CODE_RELOC ((void*)0x87FF0000)
#define XENONDS_TEMP_BEGIN ((void*)0x87F80000)
#define XENONDS_DATA_RELOC ((void*)0x88000000)
#define XENONDS_CHUNK_SIZE (1024u * 1024u)
#define XENONDS_CACHE_LINE 128u
#define XENONDS_GET_RELOCATED(symbol) \
    (XENONDS_CODE_RELOC + ((unsigned long)(symbol) - (unsigned long)elfldr_start))

extern void shutdown_drivers(void);
extern unsigned char elfldr_start[];
extern unsigned char elfldr_end[];
extern unsigned char pagetable_end[];
extern void xenonds_elf_run(unsigned long entry, unsigned long devtree);
extern void xenonds_elf_hold_thread(void);
extern volatile unsigned long xenonds_secondary_hold_addr;

struct xenonds_ati_info {
    uint32_t unknown1[4];
    uint32_t base;
    uint32_t unknown2[8];
    uint32_t width;
    uint32_t height;
} __attribute__((packed));

static inline __attribute__((always_inline)) unsigned long xenonds_min_ul(
    unsigned long left, unsigned long right) {
    return left < right ? left : right;
}

static inline __attribute__((always_inline)) unsigned long xenonds_tiled_index(
    unsigned int x, unsigned int y, unsigned int width) {
    return (((y >> 5) * 32 * width + ((x >> 5) << 10) + (x & 3) +
             ((y & 1) << 2) + (((x & 31) >> 2) << 3) +
             (((y & 31) >> 1) << 6)) ^ ((y & 8) << 2));
}

static inline __attribute__((always_inline)) void xenonds_flush_data(
    void* address, unsigned long length) {
    unsigned char* line = (unsigned char*)((unsigned long)address &
                                           ~(XENONDS_CACHE_LINE - 1));
    unsigned char* end = (unsigned char*)address + length;
    while (line < end) {
        asm volatile("dcbst 0, %0" : : "b"(line));
        line += XENONDS_CACHE_LINE;
    }
    asm volatile("sync");
}

static inline __attribute__((always_inline)) void xenonds_flush_executable(
    void* address, unsigned long length) {
    unsigned char* line = (unsigned char*)((unsigned long)address &
                                           ~(XENONDS_CACHE_LINE - 1));
    unsigned char* end = (unsigned char*)address + length;
    while (line < end) {
        asm volatile("dcbst 0, %0" : : "b"(line));
        asm volatile("icbi 0, %0" : : "b"(line));
        line += XENONDS_CACHE_LINE;
    }
    asm volatile("sync");
    asm volatile("isync");
}

static inline __attribute__((always_inline)) void xenonds_fast_zero(
    void* address, unsigned long length) {
    uint32_t* words = (uint32_t*)address;
    while (length >= sizeof(uint32_t)) {
        *words++ = 0;
        length -= sizeof(uint32_t);
    }
    unsigned char* bytes = (unsigned char*)words;
    while (length-- != 0) {
        *bytes++ = 0;
    }
}

static inline __attribute__((always_inline)) void xenonds_fast_copy(
    void* destination, const void* source, unsigned long length) {
    unsigned char* output = (unsigned char*)destination;
    const unsigned char* input = (const unsigned char*)source;
    while (length-- != 0) {
        *output++ = *input++;
    }
}

static inline __attribute__((always_inline)) void xenonds_draw_progress(
    unsigned int percent) {
    const struct xenonds_ati_info* info =
        (const struct xenonds_ati_info*)0xec806100ULL;
    const unsigned int width = info->width;
    const unsigned int height = info->height;
    if (width < 160 || height < 100 || info->base == 0) {
        return;
    }

    volatile uint32_t* framebuffer =
        (volatile uint32_t*)(unsigned long)(info->base | 0x80000000u);
    const unsigned int padded_width = (width + 31u) & ~31u;
    const unsigned int padded_height = (height + 31u) & ~31u;
    const unsigned int left = 40;
    const unsigned int right = width - 40;
    const unsigned int top = height - 54;
    const unsigned int bottom = height - 30;
    const unsigned int fill_right = left + ((right - left) * percent) / 100u;

    for (unsigned int y = top; y < bottom; ++y) {
        for (unsigned int x = left; x < right; ++x) {
            const int border = y < top + 3 || y >= bottom - 3 ||
                               x < left + 3 || x >= right - 3;
            uint32_t color = border ? 0xFFFFFF00u : 0x15182000u;
            if (!border && x < fill_right) {
                color = 0x00E8A800u;
            }
            framebuffer[xenonds_tiled_index(x, y, padded_width)] = color;
        }
    }

    xenonds_flush_data((void*)framebuffer,
                       (unsigned long)padded_width * padded_height *
                           sizeof(uint32_t));
}

static void __attribute__((section(".elfldr"), noreturn, flatten, optimize("O2")))
xenonds_prepare_payload(void* address) {
    Elf32_Ehdr* header = (Elf32_Ehdr*)address;
    Elf32_Shdr* headers = (Elf32_Shdr*)((unsigned char*)address + header->e_shoff);
    unsigned long total = 0;
    unsigned long processed = 0;
    unsigned long protected_size = (unsigned long)pagetable_end & 0x7fffffff;
    int preserved_beginning = 0;

    for (int index = 0; index < header->e_shnum; ++index) {
        if ((headers[index].sh_flags & SHF_ALLOC) && headers[index].sh_size != 0) {
            total += headers[index].sh_size;
        }
    }

    xenonds_draw_progress(10);
    for (int index = 0; index < header->e_shnum; ++index) {
        Elf32_Shdr* section = &headers[index];
        if (!(section->sh_flags & SHF_ALLOC) || section->sh_size == 0) {
            continue;
        }

        unsigned char* target =
            (unsigned char*)((unsigned long)0x80000000u | section->sh_addr);
        unsigned char* source = (unsigned char*)address + section->sh_offset;
        unsigned long remaining = section->sh_size;

        if (section->sh_type != SHT_NOBITS &&
            (unsigned long)target < (unsigned long)pagetable_end) {
            if (preserved_beginning || remaining < protected_size) {
                for (;;) {}
            }
            preserved_beginning = 1;
            xenonds_fast_copy(XENONDS_TEMP_BEGIN, source, protected_size);
            target = pagetable_end;
            source += protected_size;
            remaining -= protected_size;
            processed += protected_size;
        }

        while (remaining != 0) {
            const unsigned long chunk =
                xenonds_min_ul(remaining, XENONDS_CHUNK_SIZE);
            if (section->sh_type == SHT_NOBITS) {
                xenonds_fast_zero(target, chunk);
                xenonds_flush_data(target, chunk);
            } else {
                xenonds_fast_copy(target, source, chunk);
                xenonds_flush_executable(target, chunk);
                source += chunk;
            }
            target += chunk;
            remaining -= chunk;
            processed += chunk;

            unsigned int percent = 10;
            if (total != 0) {
                const unsigned long step = (total + 89u) / 90u;
                percent += (unsigned int)(processed / step);
            }
            if (percent > 99) {
                percent = 99;
            }
            xenonds_draw_progress(percent);
        }
    }

    if (preserved_beginning) {
        xenonds_fast_copy((void*)0x80000000, XENONDS_TEMP_BEGIN, protected_size);
        xenonds_flush_executable((void*)0x80000000, protected_size);
    }

    xenonds_draw_progress(100);
    *(volatile unsigned long*)XENONDS_GET_RELOCATED(
        &xenonds_secondary_hold_addr) = header->e_entry + 0x60;

    void (*run)(unsigned long, unsigned long) =
        XENONDS_GET_RELOCATED(xenonds_elf_run);
    run(header->e_entry, 0x07FE0000u);
    for (;;) {}
}

static int xenonds_validate_payload(const unsigned char* data, unsigned long size) {
    if (size < sizeof(Elf32_Ehdr)) {
        return -10;
    }

    const Elf32_Ehdr* header = (const Elf32_Ehdr*)data;
    if (!IS_ELF(*header) || header->e_ident[EI_CLASS] != ELFCLASS32 ||
        header->e_type != ET_EXEC || header->e_entry == 0) {
        return -11;
    }

    const unsigned long table_end = header->e_shoff +
        (unsigned long)header->e_shnum * sizeof(Elf32_Shdr);
    if (header->e_shentsize != sizeof(Elf32_Shdr) || table_end > size) {
        return -12;
    }

    const Elf32_Shdr* sections = (const Elf32_Shdr*)(data + header->e_shoff);
    for (int index = 0; index < header->e_shnum; ++index) {
        if ((sections[index].sh_flags & SHF_ALLOC) &&
            sections[index].sh_type != SHT_NOBITS &&
            sections[index].sh_offset + sections[index].sh_size > size) {
            return -13;
        }
    }
    return 0;
}

int xenonds_run_payload(const char* path) {
    const int file = open(path, O_RDONLY);
    if (file < 0) {
        return -1;
    }

    struct stat info;
    if (fstat(file, &info) != 0 || info.st_size <= 0) {
        close(file);
        return -2;
    }

    const unsigned long size = (unsigned long)info.st_size;
    unsigned char* data = (unsigned char*)malloc(size);
    if (data == NULL) {
        close(file);
        return -3;
    }

    unsigned long complete = 0;
    int last_percent = -1;
    while (complete < size) {
        const unsigned long request = xenonds_min_ul(size - complete, 256u * 1024u);
        const int amount = read(file, data + complete, request);
        if (amount <= 0) {
            close(file);
            free(data);
            return -4;
        }
        complete += (unsigned long)amount;
        const int percent = (int)((complete * 100u) / size);
        if (percent / 10 != last_percent / 10) {
            printf("  Core read: %d%%\n", percent);
            last_percent = percent;
        }
    }
    close(file);

    const int validation = xenonds_validate_payload(data, size);
    if (validation != 0) {
        free(data);
        return validation;
    }

    printf("Core ELF validation: PASS\n");
    printf("Stage 3/3: preparing 99 MB DeSmuME runtime memory.\n");
    printf("The progress bar at the bottom should continue moving.\n");
    printf("Do not turn off the Xbox during this stage.\n");

    shutdown_drivers();
    memcpy(XENONDS_CODE_RELOC, elfldr_start, elfldr_end - elfldr_start);
    memicbi(XENONDS_CODE_RELOC, elfldr_end - elfldr_start);
    memcpy(XENONDS_DATA_RELOC, data, size);

    xenon_thread_startup();
    for (int thread = 1; thread < 6; ++thread) {
        while (xenon_run_thread_task(
            thread, NULL, XENONDS_GET_RELOCATED(xenonds_elf_hold_thread))) {}
    }
    mdelay(200);

    void (*prepare)(void*) = XENONDS_GET_RELOCATED(xenonds_prepare_payload);
    prepare(XENONDS_DATA_RELOC);
    return -20;
}
