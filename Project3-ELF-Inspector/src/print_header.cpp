#include "reports.hpp"
#include <iomanip>
#include <iostream>

void printHeader(const ElfFile& f) {
    const auto& h = f.header();
    std::cout << "ELF Header: " << f.path() << '\n';
    std::cout << "  Magic:   7f 45 4c 46\n";
    std::cout << "  Class:   ELF64\n  Data:    little-endian\n";
    std::cout << "  Type:    " << typeName(h.e_type) << '\n';
    std::cout << "  Machine: " << machineName(h.e_machine) << '\n';
    std::cout << "  Version: " << h.e_version << '\n';
    std::cout << "  Entry:   0x" << std::hex << h.e_entry << std::dec << '\n';
    std::cout << "  Section header offset: " << h.e_shoff << '\n';
    std::cout << "  Section header entry size: " << h.e_shentsize << '\n';
    std::cout << "  Section count: " << f.sections().size() << '\n';
    std::cout << "  Section-name string-table index: " << h.e_shstrndx << '\n';
}
