#include "reports.hpp"
#include <iostream>
#include <stdexcept>

void printRelocations(const ElfFile& f) {
    const auto& sections = f.sections();
    bool any = false;
    for (std::size_t ri = 0; ri < sections.size(); ++ri) {
        const auto& relsec = sections[ri];
        if (relsec.sh_type == SHT_REL) {
            any = true; std::cout << "Relocation section '" << f.sectionName(ri) << "': REL entries (without addends) are not supported.\n"; continue;
        }
        if (relsec.sh_type != SHT_RELA) continue;
        any = true;
        if (relsec.sh_link >= sections.size() || sections[relsec.sh_link].sh_type != SHT_SYMTAB)
            throw std::runtime_error("RELA section links to an invalid symbol table: " + f.path());
        if (relsec.sh_info >= sections.size()) throw std::runtime_error("RELA target section index is out of range: " + f.path());
        const auto symtabIndex = relsec.sh_link;
        const auto syms = f.symbols(symtabIndex);
        std::cout << "Relocation section '" << f.sectionName(ri) << "' applies to '" << f.sectionName(relsec.sh_info) << "':\n";
        for (const auto& r : f.relocations(ri)) {
            const auto symIndex = static_cast<std::size_t>(r.r_info >> 32);
            const auto relocType = static_cast<std::uint32_t>(r.r_info & 0xffffffffu);
            if (symIndex >= syms.size()) throw std::runtime_error("relocation refers to an invalid symbol index: " + f.path());
            auto name = f.stringAt(sections[symtabIndex].sh_link, syms[symIndex].st_name);
            // ELF section symbols intentionally have an empty st_name; their
            // section index supplies the meaningful name (as readelf shows).
            if (name.empty() && ELF64_ST_TYPE(syms[symIndex].st_info) == STT_SECTION &&
                syms[symIndex].st_shndx < sections.size())
                name = f.sectionName(syms[symIndex].st_shndx);
            std::cout << "  offset=0x" << std::hex << r.r_offset << std::dec
                      << " type=" << relocationName(f.header().e_machine, relocType)
                      << " symbol=" << (name.empty() ? "<none>" : name) << " addend=" << r.r_addend << '\n';
        }
    }
    if (!any) std::cout << "No relocation sections found.\n";
}
