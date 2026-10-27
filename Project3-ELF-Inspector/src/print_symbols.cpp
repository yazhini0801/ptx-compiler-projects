#include "reports.hpp"
#include <iomanip>
#include <iostream>

void printSymbols(const ElfFile& f) {
    const auto& sections = f.sections();
    bool any = false;
    for (const auto ti : f.symbolTables()) {
        any = true;
        const auto& table = sections[ti];
        const auto syms = f.symbols(ti);
        std::cout << "Symbol table '" << f.sectionName(ti) << "' contains " << syms.size() << " entries:\n";
        std::cout << "  Idx Value              Size Type   Bind  Ndx Name\n";
        for (std::size_t i = 0; i < syms.size(); ++i) {
            const auto& s = syms[i];
            std::cout << "  " << std::setw(3) << i << ' ' << std::hex << std::setw(16) << std::setfill('0') << s.st_value
                      << std::dec << std::setfill(' ') << ' ' << std::setw(5) << s.st_size << ' '
                      << std::setw(6) << symbolTypeName(ELF64_ST_TYPE(s.st_info)) << ' '
                      << std::setw(6) << symbolBindName(ELF64_ST_BIND(s.st_info)) << ' '
                      << std::setw(4) << symbolSectionName(s.st_shndx) << ' '
                      << f.stringAt(table.sh_link, s.st_name) << '\n';
        }
    }
    if (!any) std::cout << "No symbol table found.\n";
}
