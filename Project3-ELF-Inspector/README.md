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

The core tests and malformed-input tests passed both in Windows Developer PowerShell and in Google Colab. In Colab, the generated fixture comparison against GNU `readelf` also passed, and `make sanitize` passed with AddressSanitizer and UndefinedBehaviorSanitizer enabled.

Additional checks used actual compiler output:

```sh
gcc -c -O0 tests/samples/a.c -o tests/generated/real_a.o
gcc -c -O0 tests/samples/b.c -o tests/generated/real_b.o
./elfinspect tests/generated/real_a.o --all
./elfinspect --resolve tests/generated/real_a.o tests/generated/real_b.o
readelf -h -SW -sW -rW tests/generated/real_a.o
```

The inspector and GNU `readelf` agreed on the real object's section names, symbols and bindings, and relocation offsets and types. The resolver reported that `real_a.o` defines `counter` and `main` and needs `helper` from `real_b.o`; `real_b.o` defines `helper` and needs `counter` from `real_a.o`; no symbols remained unresolved and there were no duplicate strong definitions.

A CUDA cubin was also produced from Project 2's `vadd.ptx` with `ptxas -arch=sm_75`. The inspector's `--all` report completed and identified `Machine: NVIDIA CUDA (190)`, the ELF header, sections, and symbols. It explicitly reported that REL relocations in `.rel.debug_frame` are not supported; it did not decode that relocation section.

## Limitations

- ELF64 and little-endian only; ELF32 and big-endian inputs are rejected.
- Relocatable objects (`ET_REL`) are the main target. Executables and shared objects can have headers and sections printed, but program segments are not decoded.
- `SHT_RELA` relocations are decoded; `SHT_REL` is identified but not decoded.
- Relocation names cover common x86-64 entries. Other machine-specific relocation numbers are shown numerically; CUDA machine type is recognized, but NVIDIA-specific relocation semantics are not interpreted.
- Extended section numbering, compressed sections, and DWARF line programs are not implemented.
- The symbol resolver reports matches, unresolved names, and duplicate strong definitions. It does not create a linked executable or apply relocations.
- NVIDIA-specific cubin relocation semantics are not interpreted. The tested cubin contains a `.rel.debug_frame` section, whose REL entries are recognized but not decoded.
