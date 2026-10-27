#!/usr/bin/env python3
"""CLI regression tests for elfinspect; generated fixtures are local test data."""
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
GEN = pathlib.Path(__file__).resolve().parent / "generated"
EXE = pathlib.Path(os.environ.get("ELFINSPECT", ROOT / ("elfinspect.exe" if os.name == "nt" else "elfinspect")))


def run(*args, ok=True):
    result = subprocess.run([str(EXE), *map(str, args)], cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if ok and result.returncode != 0:
        raise AssertionError(f"command failed: {args}\n{result.stdout}\n{result.stderr}")
    if not ok and result.returncode == 0:
        raise AssertionError(f"command unexpectedly succeeded: {args}\n{result.stdout}")
    return result


def require(text, *needles):
    for needle in needles:
        if needle not in text:
            raise AssertionError(f"expected output to contain {needle!r}\n{text}")


def main():
    subprocess.run([sys.executable, str(ROOT / "tests" / "make_fixtures.py")], check=True)
    a, b, c = (GEN / n for n in ("a.o", "b.o", "c.o"))
    weak = GEN / "weak.o"
    header = run(a, "-h").stdout
    require(header, "ELF64", "little-endian", "REL (Relocatable file)", "X86-64 (62)")
    sections = run(a, "-S").stdout
    require(sections, ".text", ".rela.text", ".symtab", ".strtab", ".bss", ".shstrtab", "NOBITS", "AX", "WA")
    symbols = run(a, "-s").stdout
    require(symbols, "main", "helper", "GLOBAL", "UND")
    relocs = run(a, "-r").stdout
    require(relocs, "offset=0x1", "R_X86_64_PLT32", "symbol=helper", "addend=-4")
    all_output = run(a, "--all").stdout
    require(all_output, "ELF Header", "Section Headers", "Symbol table", "Relocation section")
    resolved = run("--resolve", a, b).stdout
    require(resolved, "helper ->", "b.o", "Unresolved symbols:\n  (none)", "Duplicate strong definitions:\n  (none)")
    unresolved = run("--resolve", a).stdout
    require(unresolved, "helper -> UNRESOLVED", "Unresolved symbols:", "helper needed by")
    duplicate = run("--resolve", a, b, c).stdout
    require(duplicate, "Duplicate strong definitions:", "helper defined in")
    strong_wins = run("--resolve", a, weak, b).stdout
    require(strong_wins, "helper ->", "b.o")
    if "weak.o (weak provider)" in strong_wins:
        raise AssertionError("strong definition did not override weak provider")
    require(run(GEN / "cuda-machine.o", "-h").stdout, "NVIDIA CUDA (190)")
    for bad in ("empty.bin", "text.o", "truncated.o", "elf32.o", "bad-offset.o"):
        result = run(GEN / bad, "-h", ok=False)
        if "elfinspect:" not in result.stderr:
            raise AssertionError(f"missing clear diagnostic for {bad}: {result.stderr}")
    print("PASS: header, section, symbol, relocation, --all, and four resolver scenarios")
    print("PASS: empty, non-ELF, truncated, ELF32, and out-of-range section-table errors")
    readelf = shutil.which("readelf")
    if readelf:
        def readelf_output(flag):
            return subprocess.run([readelf, flag, str(a)], text=True, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, check=True).stdout

        other_sections = readelf_output("-SW")
        our_section_names = []
        for line in sections.splitlines():
            m = re.match(r"^\s*\[\s*(\d+)\]\s+(.*)$", line)
            if m and int(m.group(1)) != 0:
                our_section_names.append(m.group(2).split()[0])
        ref_section_names = []
        for line in other_sections.splitlines():
            m = re.match(r"^\s*\[\s*(\d+)\]\s+(\S+)", line)
            if m and int(m.group(1)) != 0:
                ref_section_names.append(m.group(2))
        if our_section_names != ref_section_names:
            raise AssertionError(f"section-name mismatch: {our_section_names} != {ref_section_names}")

        def ours_symbols(output):
            result = {}
            for line in output.splitlines():
                parts = line.split()
                if len(parts) >= 7 and parts[0].isdigit() and parts[4] in ("LOCAL", "GLOBAL", "WEAK"):
                    if parts[-1]: result[parts[-1]] = (parts[4], parts[-2])
            return result

        def readelf_symbols(output):
            result = {}
            for line in output.splitlines():
                parts = line.split()
                if len(parts) >= 8 and parts[0].endswith(":") and parts[4] in ("LOCAL", "GLOBAL", "WEAK"):
                    result[parts[-1]] = (parts[4], parts[-2])
            return result

        ref_symbols = readelf_symbols(readelf_output("-sW"))
        for name, binding in ours_symbols(symbols).items():
            if ref_symbols.get(name) != binding:
                raise AssertionError(f"symbol mismatch for {name}: {binding} != {ref_symbols.get(name)}")

        def ours_relocations(output):
            return [(int(off, 16), typ, sym) for off, typ, sym in
                    re.findall(r"offset=0x([0-9a-fA-F]+) type=(\S+) symbol=(\S+)", output)]

        def readelf_relocations(output):
            result = []
            for line in output.splitlines():
                parts = line.split()
                if len(parts) < 4 or not re.fullmatch(r"[0-9a-fA-F]+", parts[0]):
                    continue
                type_index = next((i for i, part in enumerate(parts) if part.startswith("R_X86_64_")), None)
                if type_index is None:
                    continue
                symbol_index = type_index + 1
                if symbol_index < len(parts) and re.fullmatch(r"[0-9a-fA-F]+", parts[symbol_index]):
                    symbol_index += 1  # readelf prints the symbol value before its name
                if symbol_index < len(parts):
                    result.append((int(parts[0], 16), parts[type_index], parts[symbol_index]))
            return result

        ref_relocations = readelf_relocations(readelf_output("-rW"))
        if ours_relocations(relocs) != ref_relocations:
            raise AssertionError(f"relocation mismatch: {ours_relocations(relocs)} != {ref_relocations}")
        print("PASS: section names, symbol names/bindings, and relocation offsets/types match GNU readelf")
    else:
        print("SKIP: GNU readelf comparison (readelf is not installed on this system)")


if __name__ == "__main__":
    main()
