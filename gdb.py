import gdb
import hashlib
from dataclasses import dataclass

@dataclass
class TableDescriptor:
    next_tbl_addr: int

    def __str__(self):
        return f"TableDescriptor({hex(self.next_tbl_addr)})"

@dataclass
class BlockDescriptor:
    output_block_addr: int

    def __str__(self):
        return f"BlockDescriptor({hex(self.output_block_addr)})"

@dataclass
class PageDescriptor:
    output_page_addr: int

    def __str__(self):
        return f"PageDescriptor({hex(self.output_page_addr)})"

@dataclass
class InvalidEntry:
    val: int

    def __str__(self):
        return f"InvalidEntry({hex(self.val)})"

Entry = TableDescriptor | BlockDescriptor | PageDescriptor | InvalidEntry

ARM64_PTES_PER_TABLE = 512

def get_bit(val, n):
    return (val >> n) & 1

# all valid entries have a 1 as their validity bit
def is_valid_entry(val):
    return get_bit(val, 0) == 1

def swap64(x):
    return int.from_bytes(x.to_bytes(8, byteorder='little'), byteorder='big', signed=False)

def parse_entry(val, current_level):
    if not is_valid_entry(val):
        return InvalidEntry(int(val))

    match current_level:
        case 0 | 1 | 2:
            match get_bit(val, 1):
                case 0:
                    return BlockDescriptor(val & 0xFFFFFFFFF000) # this may be wrong but linux dosen't use them
                case 1:
                    return TableDescriptor(val & 0xFFFFFFFFF000)
            raise Exception()
        case 3:
            match get_bit(val, 1):
                case 0:
                    return InvalidEntry(int(val))
                case 1:
                    return PageDescriptor(val & 0xFFFFFFFFF000)
            raise Exception()

def print_entries(table, level):
    for i in range(ARM64_PTES_PER_TABLE):
        val = gdb.parse_and_eval(f'*(unsigned long long *){table + (i * 8)}')
        if val != 0:
            entry = parse_entry(val, level)
            print(f'{" " * 4 * level}{i}: {entry}')
            match entry:
                case TableDescriptor(addr):
                    print_entries(addr, level + 1)
                case BlockDescriptor(addr) | PageDescriptor(addr) | InvalidEntry(addr):
                    pass

def dump_entries(table, level):
    if level == 3:
        print("table:")
        for i in range(ARM64_PTES_PER_TABLE):
            print(hex(gdb.parse_and_eval(f'*(unsigned long long *){table + (i * 8)}')))
        print("end!")
        return
    for i in range(ARM64_PTES_PER_TABLE):
        val = gdb.parse_and_eval(f'*(unsigned long long *){table + (i * 8)}')
        if val != 0:
            entry = parse_entry(val, level)
            print(f'{" " * 4 * level}{i}: {entry}')
            match entry:
                case TableDescriptor(addr):
                    dump_entries(addr, level + 1)
                case BlockDescriptor(addr) | PageDescriptor(addr) | InvalidEntry(addr):
                    pass

def digest_entries(table, level):
    global digest_count
    digest_count = 0
    m = hashlib.sha256()
    do_digest_entries(table, level, m)
    print()
    print(m.hexdigest())

def do_digest_entries(table, level, m):
    global digest_count
    for i in range(ARM64_PTES_PER_TABLE):
        val = gdb.parse_and_eval(f'*(unsigned long long *){table + (i * 8)}')
        if val != 0:
            if not is_valid_entry(val):
                print("warning: found non-zero invalid entry")
            entry = parse_entry(val, level)
            m.update(int(val).to_bytes(8, 'little'))
            match entry:
                case TableDescriptor(addr):
                    do_digest_entries(addr, level + 1, m)
                case BlockDescriptor(addr) | PageDescriptor(addr) | InvalidEntry(addr):
                    pass
    digest_count += 1
    print('\r' + f'processed tables: {digest_count}', end='', flush=True)

class MxDumpPages(gdb.Command):
    """dump aarch64 page tables"""

    def __init__(self):
        super(MxDumpPages, self).__init__("mx-dump-pages", gdb.COMMAND_USER)

    def invoke(self, arg, from_tty):
        argv = gdb.string_to_argv(arg)
        if len(argv) != 1:
            raise Exception("Usage: mx-dump-pages <pgd>")
        pgd = int(gdb.parse_and_eval(argv[0]))

        print_entries((pgd & 0xffffffffffc0) | ((pgd & 0x3c) << 46), 0)
        # dump_entries((pgd & 0xffffffffffc0) | ((pgd & 0x3c) << 46), 0)
        # digest_entries((pgd & 0xffffffffffc0) | ((pgd & 0x3c) << 46), 0)

        print("done!")

# run "maintenance packet Qqemu.PhyMemMode:1" in qemu!
MxDumpPages()
