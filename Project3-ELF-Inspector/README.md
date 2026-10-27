# Project 3: ELF Inspector and Mini Symbol Resolver

`elfinspect` is a C++17 command-line tool that reads ELF64 files without libelf. It prints headers, section tables, symbols, and RELA relocations, and provides a small report showing how global and weak symbols in several object files match up. It inspects files; it does not link them or execute them.

```text
ELF bytes -> checked ELF header -> sections -> symbols -> relocations -> reports
                                               \-> symbol resolution across files
```

## Build

### Linux or WSL

The normal build uses the system `<elf.h>` definitions, GNU Make, and Python 3:

```sh
make
make test
```

To run the address and undefined-behavior sanitizer suite:

```sh
make sanitize
```

### Windows with Visual Studio 2022

Open **Developer PowerShell for VS 2022** in this folder and run:

```powershell
cl /std:c++17 /EHsc /W4 /Isrc src\elf_reader.cpp src\print_header.cpp src\print_sections.cpp src\print_symbols.cpp src\print_relocs.cpp src\resolve.cpp src\main.cpp /Fe:elfinspect.exe
python tests\run_tests.py
```

Visual Studio does not provide `<elf.h>`, so `src/elf_types.hpp` supplies the standard ELF64 record layouts on Windows. Linux builds use the platform's `<elf.h>` types.

## Run it

Options can be combined only by selecting the reports you want for one input file:

```sh
./elfinspect tests/generated/a.o -h
./elfinspect tests/generated/a.o -S
./elfinspect tests/generated/a.o -s
./elfinspect tests/generated/a.o -r
./elfinspect tests/generated/a.o --all
./elfinspect --resolve tests/generated/a.o tests/generated/b.o
```

`-h` prints the ELF header, `-S` the section table, `-s` symbols, `-r` RELA relocations, and `--all` all four reports. `--resolve` is a separate mode that compares symbol requirements and definitions among its input files.

The test fixture models a call from `a.o` to `helper`, with `helper` defined by `b.o`. The key relocation report is similar to:

```text
Relocation section '.rela.text' applies to '.text':
  offset=0x1 type=R_X86_64_PLT32 symbol=helper addend=-4
```

`--resolve a.o b.o` reports that `b.o` provides `helper`; `--resolve a.o` reports it unresolved; adding `c.o` reports duplicate strong definitions of `helper`.

## How the reader works

ELF is a binary file format. The reader loads the file into a byte vector and validates the magic, 64-bit class, and little-endian encoding. Before copying a header, table entry, or string, it checks that the requested byte range is within the file. This prevents malformed offsets from causing out-of-bounds reads.

- The **ELF header** gives the file type, machine, and location of its section table.
- A **section** is a named file area, such as `.text` (code), `.data` (initialized data), `.rodata` (constants), `.bss` (zero-initialized data), or symbol/string tables. Section names are offsets into `.shstrtab`.
- A **symbol** names a function or variable. `UND` means this object needs a definition from elsewhere; `GLOBAL` and `WEAK` are binding types.
- A **relocation** tells a linker where a reference needs fixing after final addresses are known. For x86-64 `PC32` and `PLT32`, the common formula is `S + A - P`: symbol address plus addend minus the patch location.
- A **linker** combines object files, resolves symbols, assigns addresses, and applies relocations. A loader later maps an executable into memory. This inspector reports some of the linker's inputs but does not perform those jobs.

Unknown section and relocation types are printed numerically instead of causing a failure. Machine number 190 is identified as NVIDIA CUDA for CUDA ELF headers such as cubins. The NVIDIA CUDA ELF version value `0x81` is accepted for machine 190; standard ELF inputs still require version 1.

## Tests and verification

`make test` generates small ELF64 object fixtures and exercises header, section, symbol, relocation, combined output, and symbol resolution. It also checks empty, non-ELF, truncated, ELF32, and out-of-range section-table inputs; malformed files must exit with a diagnostic rather than crash. When GNU `readelf` is installed, the script compares section-name order, symbol names/bindings/section indices, and relocation offsets/types/symbols against it. Without `readelf`, that comparison is reported as skipped.

The fixtures are constructed by `tests/make_fixtures.py`, so the core test suite does not need a CUDA GPU or a C compiler. They are test objects, not actual NVIDIA cubins. If `readelf` is available, you can additionally compile real samples with `gcc -c -O0 tests/samples/a.c -o a.o` and inspect the resulting objects.

## Limitations

- ELF64 and little-endian only; ELF32 and big-endian inputs are rejected.
- Relocatable objects (`ET_REL`) are the main target. Executables and shared objects can have headers and sections printed, but program segments are not decoded.
- `SHT_RELA` relocations are decoded; `SHT_REL` is identified but not decoded.
- Relocation names cover common x86-64 entries. Other machine-specific relocation numbers are shown numerically; CUDA machine type is recognized, but NVIDIA-specific relocation semantics are not interpreted.
- Extended section numbering, compressed sections, and DWARF line programs are not implemented.
- The symbol resolver reports matches, unresolved names, and duplicate strong definitions. It does not create a linked executable or apply relocations.
- No actual `.cubin` was available in this Windows workspace during development; the CUDA machine-number display is covered using a synthetic ELF header fixture. A real cubin inspection is a stretch validation.
