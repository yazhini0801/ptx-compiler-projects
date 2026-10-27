#pragma once
#include "elf_reader.hpp"
#include <string>
#include <vector>
void printHeader(const ElfFile& file);
void printSections(const ElfFile& file);
void printSymbols(const ElfFile& file);
void printRelocations(const ElfFile& file);
void resolveSymbols(const std::vector<std::string>& paths);
