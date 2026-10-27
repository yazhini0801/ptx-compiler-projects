#include "reports.hpp"
#include <iostream>
#include <map>
#include <set>

namespace {
struct Definition { std::string file; bool weak; };
struct Reference { std::string file; };
struct FileSymbols { std::string path; std::vector<std::string> definitions, references; };
}

void resolveSymbols(const std::vector<std::string>& paths) {
    std::vector<ElfFile> files;
    files.reserve(paths.size());
    for (const auto& path : paths) files.emplace_back(path);

    std::map<std::string, std::vector<Definition>> definitions;
    std::map<std::string, std::vector<Reference>> references;
    std::vector<FileSymbols> perFile(files.size());
    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        const auto& f = files[fi]; perFile[fi].path = f.path();
        for (const auto ti : f.symbolTables()) {
            const auto& table = f.sections()[ti];
            const auto syms = f.symbols(ti);
            for (std::size_t si = 1; si < syms.size(); ++si) {
                const auto& s = syms[si];
                const auto bind = ELF64_ST_BIND(s.st_info);
                if (bind != STB_GLOBAL && bind != STB_WEAK) continue;
                const auto name = f.stringAt(table.sh_link, s.st_name);
                if (name.empty()) continue;
                if (s.st_shndx == SHN_UNDEF) {
                    references[name].push_back({f.path()});
                    perFile[fi].references.push_back(name);
                } else {
                    const bool weak = bind == STB_WEAK;
                    definitions[name].push_back({f.path(), weak});
                    perFile[fi].definitions.push_back(name + (weak ? " (weak)" : ""));
                }
            }
        }
    }

    std::map<std::string, Definition> provider;
    std::set<std::string> duplicateNames;
    for (const auto& [name, defs] : definitions) {
        const Definition* best = nullptr;
        std::set<std::string> strongFiles;
        for (const auto& d : defs) {
            if (!d.weak) strongFiles.insert(d.file);
            if (!best || (best->weak && !d.weak)) best = &d;
        }
        if (strongFiles.size() > 1) duplicateNames.insert(name);
        if (best) provider[name] = *best;
    }

    for (const auto& f : perFile) {
        std::cout << "File " << f.path << ":\n  Defines: ";
        if (f.definitions.empty()) std::cout << "(none)";
        for (std::size_t i = 0; i < f.definitions.size(); ++i) std::cout << (i ? ", " : "") << f.definitions[i];
        std::cout << "\n  Needs:\n";
        if (f.references.empty()) std::cout << "    (none)\n";
        for (const auto& name : f.references) {
            const auto found = provider.find(name);
            if (found == provider.end()) std::cout << "    " << name << " -> UNRESOLVED\n";
            else std::cout << "    " << name << " -> " << found->second.file << (found->second.weak ? " (weak provider)" : "") << '\n';
        }
    }
    std::cout << "Unresolved symbols:\n";
    bool unresolvedAny = false;
    for (const auto& [name, refs] : references) if (provider.find(name) == provider.end()) { unresolvedAny = true; std::cout << "  " << name << " needed by"; for (const auto& r : refs) std::cout << " " << r.file; std::cout << '\n'; }
    if (!unresolvedAny) std::cout << "  (none)\n";
    std::cout << "Duplicate strong definitions:\n";
    if (duplicateNames.empty()) std::cout << "  (none)\n";
    for (const auto& name : duplicateNames) {
        std::cout << "  " << name << " defined in";
        for (const auto& d : definitions[name]) if (!d.weak) std::cout << " " << d.file;
        std::cout << '\n';
    }
}
