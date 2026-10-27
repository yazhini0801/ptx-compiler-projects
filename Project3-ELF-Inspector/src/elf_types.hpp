#pragma once

// Linux builds use the platform's ELF definitions. The fallback keeps the
// inspector buildable with Visual Studio, which does not ship <elf.h>.
#if __has_include(<elf.h>)
#include <elf.h>
#else
#include <cstdint>
using Elf64_Half = std::uint16_t;
using Elf64_Word = std::uint32_t;
using Elf64_Sword = std::int32_t;
using Elf64_Xword = std::uint64_t;
using Elf64_Sxword = std::int64_t;
using Elf64_Addr = std::uint64_t;
using Elf64_Off = std::uint64_t;

struct Elf64_Ehdr {
    unsigned char e_ident[16]; Elf64_Half e_type, e_machine; Elf64_Word e_version;
    Elf64_Addr e_entry; Elf64_Off e_phoff, e_shoff; Elf64_Word e_flags;
    Elf64_Half e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
};
struct Elf64_Shdr {
    Elf64_Word sh_name, sh_type; Elf64_Xword sh_flags; Elf64_Addr sh_addr;
    Elf64_Off sh_offset; Elf64_Xword sh_size; Elf64_Word sh_link, sh_info;
    Elf64_Xword sh_addralign, sh_entsize;
};
struct Elf64_Sym {
    Elf64_Word st_name; unsigned char st_info, st_other; Elf64_Half st_shndx;
    Elf64_Addr st_value; Elf64_Xword st_size;
};
struct Elf64_Rela { Elf64_Addr r_offset; Elf64_Xword r_info; Elf64_Sxword r_addend; };

constexpr unsigned char ELFMAG0 = 0x7f, ELFMAG1 = 'E', ELFMAG2 = 'L', ELFMAG3 = 'F';
constexpr int EI_CLASS = 4, EI_DATA = 5, ELFCLASS32 = 1, ELFCLASS64 = 2, ELFDATA2LSB = 1, ELFDATA2MSB = 2;
constexpr int ET_REL = 1, ET_EXEC = 2, ET_DYN = 3, EM_X86_64 = 62;
constexpr Elf64_Word SHT_NULL = 0, SHT_PROGBITS = 1, SHT_SYMTAB = 2, SHT_STRTAB = 3;
constexpr Elf64_Word SHT_RELA = 4, SHT_NOBITS = 8, SHT_REL = 9;
constexpr Elf64_Xword SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4;
constexpr Elf64_Half SHN_UNDEF = 0, SHN_ABS = 0xfff1, SHN_COMMON = 0xfff2;
constexpr unsigned char STB_LOCAL = 0, STB_GLOBAL = 1, STB_WEAK = 2;
constexpr unsigned char STT_NOTYPE = 0, STT_OBJECT = 1, STT_FUNC = 2, STT_SECTION = 3, STT_FILE = 4;
constexpr unsigned char ELF64_ST_BIND(unsigned char i) { return i >> 4; }
constexpr unsigned char ELF64_ST_TYPE(unsigned char i) { return i & 0xf; }
#endif

static_assert(sizeof(Elf64_Ehdr) == 64, "ELF64 header layout must be 64 bytes");
static_assert(sizeof(Elf64_Shdr) == 64, "ELF64 section header layout must be 64 bytes");
static_assert(sizeof(Elf64_Sym) == 24, "ELF64 symbol layout must be 24 bytes");
static_assert(sizeof(Elf64_Rela) == 24, "ELF64 RELA layout must be 24 bytes");
