#include "elf_reader.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

template<class T> T ElfFile::readStruct(std::uint64_t offset, const std::string& what) const {
    requireRange(offset, sizeof(T), what);
    T result{};
    std::memcpy(&result, bytes_.data() + static_cast<std::size_t>(offset), sizeof(T));
    return result;
}

ElfFile::ElfFile(const std::string& path) : path_(path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("cannot open file: " + path);
    in.seekg(0, std::ios::end);
    const auto end = in.tellg();
    if (end < 0) throw std::runtime_error("cannot determine file size: " + path);
    bytes_.resize(static_cast<std::size_t>(end));
    in.seekg(0, std::ios::beg);
    if (!bytes_.empty() && !in.read(reinterpret_cast<char*>(bytes_.data()), static_cast<std::streamsize>(bytes_.size())))
        throw std::runtime_error("failed to read file: " + path);
    if (bytes_.size() < sizeof(Elf64_Ehdr)) throw std::runtime_error("file is too small to contain an ELF64 header: " + path);
    header_ = readStruct<Elf64_Ehdr>(0, "ELF header");
    if (header_.e_ident[0] != ELFMAG0 || header_.e_ident[1] != ELFMAG1 ||
        header_.e_ident[2] != ELFMAG2 || header_.e_ident[3] != ELFMAG3)
        throw std::runtime_error("not an ELF file: " + path);
    if (header_.e_ident[EI_CLASS] != ELFCLASS64) throw std::runtime_error("unsupported ELF class (only ELF64 is supported): " + path);
    if (header_.e_ident[EI_DATA] != ELFDATA2LSB) throw std::runtime_error("unsupported ELF byte order (only little-endian is supported): " + path);
    if (header_.e_ident[6] != 1 || header_.e_version != 1) throw std::runtime_error("unsupported or invalid ELF version: " + path);
    if (header_.e_ehsize < sizeof(Elf64_Ehdr)) throw std::runtime_error("invalid ELF header size: " + path);
    if (header_.e_shoff == 0 || header_.e_shnum == 0) return; // valid sectionless ELF
    if (header_.e_shentsize < sizeof(Elf64_Shdr)) throw std::runtime_error("section-header entries are too small: " + path);
    if (header_.e_shnum > std::numeric_limits<std::size_t>::max() / sizeof(Elf64_Shdr))
        throw std::runtime_error("section table is too large: " + path);
    requireRange(header_.e_shoff, static_cast<std::uint64_t>(header_.e_shentsize) * header_.e_shnum, "section-header table");
    sections_.reserve(header_.e_shnum);
    for (std::uint16_t i = 0; i < header_.e_shnum; ++i)
        sections_.push_back(readStruct<Elf64_Shdr>(header_.e_shoff + static_cast<std::uint64_t>(i) * header_.e_shentsize, "section header"));
    for (std::size_t i = 0; i < sections_.size(); ++i) {
        const auto& s = sections_[i];
        if (s.sh_type != SHT_NOBITS && s.sh_type != SHT_NULL) requireRange(s.sh_offset, s.sh_size, "section contents");
    }
    if (header_.e_shstrndx != 0 && header_.e_shstrndx >= sections_.size())
        throw std::runtime_error("section-name string-table index is out of range: " + path);
}

void ElfFile::requireRange(std::uint64_t offset, std::uint64_t size, const std::string& what) const {
    if (offset > bytes_.size() || size > bytes_.size() - static_cast<std::size_t>(offset))
        throw std::runtime_error(what + " extends beyond end of file: " + path_);
}

std::string ElfFile::stringAt(std::size_t tableIndex, std::uint32_t offset) const {
    if (tableIndex >= sections_.size()) throw std::runtime_error("string-table index is out of range: " + path_);
    const auto& table = sections_[tableIndex];
    if (table.sh_type != SHT_STRTAB) throw std::runtime_error("linked section is not a string table: " + path_);
    if (offset >= table.sh_size) {
        if (offset == 0 && table.sh_size == 0) return {};
        throw std::runtime_error("string offset is outside its string table: " + path_);
    }
    const auto start = static_cast<std::size_t>(table.sh_offset + offset);
    const auto end = static_cast<std::size_t>(table.sh_offset + table.sh_size);
    auto it = std::find(bytes_.begin() + start, bytes_.begin() + end, std::uint8_t{0});
    if (it == bytes_.begin() + end) throw std::runtime_error("unterminated string in string table: " + path_);
    return std::string(reinterpret_cast<const char*>(bytes_.data() + start), static_cast<std::size_t>(it - (bytes_.begin() + start)));
}

std::string ElfFile::sectionName(std::size_t index) const {
    if (index >= sections_.size()) throw std::runtime_error("section index is out of range: " + path_);
    if (header_.e_shstrndx == 0) return "<no-name-table>";
    return stringAt(header_.e_shstrndx, sections_[index].sh_name);
}

std::vector<std::size_t> ElfFile::symbolTables() const {
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < sections_.size(); ++i) if (sections_[i].sh_type == SHT_SYMTAB) out.push_back(i);
    return out;
}

std::vector<Elf64_Sym> ElfFile::symbols(std::size_t symtabIndex) const {
    if (symtabIndex >= sections_.size() || sections_[symtabIndex].sh_type != SHT_SYMTAB)
        throw std::runtime_error("invalid symbol-table section: " + path_);
    const auto& s = sections_[symtabIndex];
    if (s.sh_link >= sections_.size()) throw std::runtime_error("symbol table links to an invalid string table: " + path_);
    if (s.sh_entsize < sizeof(Elf64_Sym) || s.sh_entsize == 0 || s.sh_size % s.sh_entsize != 0)
        throw std::runtime_error("invalid symbol-table entry size: " + path_);
    std::vector<Elf64_Sym> out;
    const auto count = s.sh_size / s.sh_entsize;
    if (count > std::numeric_limits<std::size_t>::max()) throw std::runtime_error("symbol table is too large: " + path_);
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i)
        out.push_back(readStruct<Elf64_Sym>(s.sh_offset + i * s.sh_entsize, "symbol-table entry"));
    return out;
}

std::vector<Elf64_Rela> ElfFile::relocations(std::size_t relaIndex) const {
    if (relaIndex >= sections_.size() || sections_[relaIndex].sh_type != SHT_RELA)
        throw std::runtime_error("invalid RELA section: " + path_);
    const auto& s = sections_[relaIndex];
    if (s.sh_entsize < sizeof(Elf64_Rela) || s.sh_entsize == 0 || s.sh_size % s.sh_entsize != 0)
        throw std::runtime_error("invalid RELA entry size: " + path_);
    std::vector<Elf64_Rela> out;
    const auto count = s.sh_size / s.sh_entsize;
    out.reserve(static_cast<std::size_t>(count));
    for (std::uint64_t i = 0; i < count; ++i)
        out.push_back(readStruct<Elf64_Rela>(s.sh_offset + i * s.sh_entsize, "RELA entry"));
    return out;
}

std::string ElfFile::bytesAsHex(std::size_t offset, std::size_t count) const {
    requireRange(offset, count, "section dump");
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < count; ++i) { if (i) out << ' '; out << std::setw(2) << unsigned(bytes_[offset + i]); }
    return out.str();
}

std::string typeName(std::uint32_t t) {
    switch (t) { case ET_REL: return "REL (Relocatable file)"; case ET_EXEC: return "EXEC (Executable file)"; case ET_DYN: return "DYN (Shared object)"; default: return "unknown (" + std::to_string(t) + ")"; }
}
std::string machineName(std::uint16_t m) {
    if (m == EM_X86_64) return "Advanced Micro Devices X86-64 (62)";
    if (m == 190) return "NVIDIA CUDA (190)";
    return "unknown (" + std::to_string(m) + ")";
}
std::string sectionTypeName(std::uint32_t t) {
    switch (t) {
        case SHT_NULL: return "NULL"; case SHT_PROGBITS: return "PROGBITS"; case SHT_SYMTAB: return "SYMTAB";
        case SHT_STRTAB: return "STRTAB"; case SHT_RELA: return "RELA"; case SHT_REL: return "REL"; case SHT_NOBITS: return "NOBITS";
        default: { std::ostringstream o; o << "0x" << std::hex << t; return o.str(); }
    }
}
std::string symbolTypeName(unsigned char t) {
    switch (t) { case STT_NOTYPE: return "NOTYPE"; case STT_OBJECT: return "OBJECT"; case STT_FUNC: return "FUNC"; case STT_SECTION: return "SECTION"; case STT_FILE: return "FILE"; default: return "type-" + std::to_string(t); }
}
std::string symbolBindName(unsigned char b) {
    switch (b) { case STB_LOCAL: return "LOCAL"; case STB_GLOBAL: return "GLOBAL"; case STB_WEAK: return "WEAK"; default: return "bind-" + std::to_string(b); }
}
std::string symbolSectionName(std::uint16_t s) {
    if (s == SHN_UNDEF) return "UND";
    if (s == SHN_ABS) return "ABS";
    if (s == SHN_COMMON) return "COM";
    return std::to_string(s);
}
std::string flagsName(std::uint64_t f) {
    std::string out; if (f & SHF_WRITE) out += 'W'; if (f & SHF_ALLOC) out += 'A'; if (f & SHF_EXECINSTR) out += 'X'; return out.empty() ? "-" : out;
}
std::string relocationName(std::uint16_t machine, std::uint32_t t) {
    if (machine == EM_X86_64) switch (t) {
        case 1: return "R_X86_64_64"; case 2: return "R_X86_64_PC32"; case 4: return "R_X86_64_PLT32";
        case 9: return "R_X86_64_GOTPCREL"; case 10: return "R_X86_64_32"; case 11: return "R_X86_64_32S";
    }
    return "type-" + std::to_string(t);
}
