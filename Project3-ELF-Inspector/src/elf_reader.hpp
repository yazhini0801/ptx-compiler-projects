#pragma once
#include "elf_types.hpp"
#include <cstdint>
#include <string>
#include <vector>

class ElfFile {
public:
    explicit ElfFile(const std::string& path);
    const std::string& path() const { return path_; }
    const Elf64_Ehdr& header() const { return header_; }
    const std::vector<Elf64_Shdr>& sections() const { return sections_; }
    std::string sectionName(std::size_t index) const;
    std::string stringAt(std::size_t tableIndex, std::uint32_t offset) const;
    std::vector<Elf64_Sym> symbols(std::size_t symtabIndex) const;
    std::vector<Elf64_Rela> relocations(std::size_t relaIndex) const;
    std::vector<std::size_t> symbolTables() const;
    std::string bytesAsHex(std::size_t offset, std::size_t count) const;
private:
    std::string path_;
    std::vector<std::uint8_t> bytes_;
    Elf64_Ehdr header_{};
    std::vector<Elf64_Shdr> sections_;
    void requireRange(std::uint64_t offset, std::uint64_t size, const std::string& what) const;
    template<class T> T readStruct(std::uint64_t offset, const std::string& what) const;
};

std::string typeName(std::uint32_t type);
std::string machineName(std::uint16_t machine);
std::string sectionTypeName(std::uint32_t type);
std::string symbolTypeName(unsigned char type);
std::string symbolBindName(unsigned char bind);
std::string relocationName(std::uint16_t machine, std::uint32_t type);
std::string symbolSectionName(std::uint16_t index);
std::string flagsName(std::uint64_t flags);
