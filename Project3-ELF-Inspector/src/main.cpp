#include "reports.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void usage() {
    std::cerr << "Usage: elfinspect FILE [-h] [-S] [-s] [-r] [--all]\n"
                 "       elfinspect --resolve FILE...\n"
                 "  -h       print ELF header\n  -S       print section table\n"
                 "  -s       print symbol tables\n  -r       print RELA relocations\n"
                 "  --all    print header, sections, symbols, and relocations\n";
}
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(); return 2; }
    try {
        if (std::string(argv[1]) == "--resolve") {
            if (argc < 3) { usage(); return 2; }
            std::vector<std::string> paths;
            for (int i = 2; i < argc; ++i) paths.emplace_back(argv[i]);
            resolveSymbols(paths);
            return 0;
        }
        const std::string path = argv[1];
        bool header = false, sections = false, symbols = false, relocs = false, all = false;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "-h") header = true;
            else if (arg == "-S") sections = true;
            else if (arg == "-s") symbols = true;
            else if (arg == "-r") relocs = true;
            else if (arg == "--all") all = true;
            else { usage(); throw std::runtime_error("unknown option: " + arg); }
        }
        if (!(header || sections || symbols || relocs || all)) { usage(); return 2; }
        const ElfFile file(path);
        if (all || header) printHeader(file);
        if (all || sections) printSections(file);
        if (all || symbols) printSymbols(file);
        if (all || relocs) printRelocations(file);
    } catch (const std::exception& e) {
        std::cerr << "elfinspect: " << e.what() << '\n';
        return 1;
    }
    return 0;
}
