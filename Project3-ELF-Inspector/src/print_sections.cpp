#include "reports.hpp"
#include <iomanip>
#include <iostream>

void printSections(const ElfFile& f) {
    const auto& sections = f.sections();
    std::cout << "Section Headers (" << sections.size() << "):\n";
    std::cout << "  [Nr] Name                 Type       Flags Addr             Off      Size     Link Info EntSize\n";
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const auto& s = sections[i];
        std::cout << "  [" << std::setw(2) << i << "] " << std::left << std::setw(20) << f.sectionName(i) << std::setw(11)
                  << sectionTypeName(s.sh_type) << std::setw(6) << flagsName(s.sh_flags) << std::right
                  << " 0x" << std::hex << std::setw(14) << s.sh_addr << std::dec
                  << ' ' << std::setw(8) << s.sh_offset << ' ' << std::setw(8) << s.sh_size
                  << ' ' << std::setw(4) << s.sh_link << ' ' << std::setw(4) << s.sh_info << ' ' << s.sh_entsize << '\n';
    }
}
