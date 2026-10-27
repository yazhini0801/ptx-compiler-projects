#!/usr/bin/env python3
"""Create tiny ELF64 relocatable objects for repeatable parser tests."""
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parent / "generated"
EHDR = struct.Struct("<16sHHIQQQIHHHHHH")
SHDR = struct.Struct("<IIQQQQIIQQ")
SYM = struct.Struct("<IBBHQQ")
RELA = struct.Struct("<QQq")


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def make_object(name, defined, undefined, weak_definition=False):
    ROOT.mkdir(parents=True, exist_ok=True)
    # Section indices: 0 null, 1 .text, 2 .rela.text, 3 .eh_frame,
    # 4 .rela.eh_frame, 5 .symtab, 6 .strtab, 7 .bss (NOBITS), 8 .shstrtab.
    shnames = b"\0.text\0.rela.text\0.eh_frame\0.rela.eh_frame\0.symtab\0.strtab\0.bss\0.shstrtab\0"
    shname_offsets = {s.decode(): shnames.index(s) for s in
                      (b".text", b".rela.text", b".eh_frame", b".rela.eh_frame", b".symtab", b".strtab", b".bss", b".shstrtab")}
    symbol_names = ["main"] if defined == "main" else []
    symbol_names += [defined] if defined and defined != "main" else []
    symbol_names += undefined
    strtab = bytearray(b"\0")
    name_offsets = {"": 0}
    for symbol in symbol_names:
        if symbol not in name_offsets:
            name_offsets[symbol] = len(strtab)
            strtab.extend(symbol.encode() + b"\0")
    symbols = [SYM.pack(0, 0, 0, 0, 0, 0), SYM.pack(0, (0 << 4) | 3, 0, 1, 0, 0)]
    if defined:
        bind = 2 if weak_definition else 1
        symbols.append(SYM.pack(name_offsets[defined], (bind << 4) | 2, 0, 1, 0, 8))
    for symbol in undefined:
        symbols.append(SYM.pack(name_offsets[symbol], (1 << 4), 0, 0, 0, 0))
    symbytes = b"".join(symbols)
    # a.o references helper at call offset 1; other objects have no relocations.
    relas = [RELA.pack(1, ((len(symbols) - 1) << 32) | 4, -4)] if "helper" in undefined else []
    rela_eh_frame = RELA.pack(0, (1 << 32) | 2, 0)  # unnamed STT_SECTION symbol resolves to .text
    relabytes = b"".join(relas)

    body = bytearray(b"\0" * EHDR.size)
    section_data = {}
    for secname, data, boundary in ((".text", b"\xe8\0\0\0\0\xc3\x90\x90", 16),
                                    (".rela.text", relabytes, 8),
                                    (".eh_frame", b"\0\0\0\0", 8),
                                    (".rela.eh_frame", rela_eh_frame, 8),
                                    (".symtab", symbytes, 8),
                                    (".strtab", bytes(strtab), 1),
                                    (".shstrtab", shnames, 1)):
        if not data:
            section_data[secname] = (0, 0)
            continue
        offset = align(len(body), boundary)
        body.extend(b"\0" * (offset - len(body)))
        section_data[secname] = (offset, len(data))
        body.extend(data)
    shoff = align(len(body), 8)
    body.extend(b"\0" * (shoff - len(body)))
    text_off, text_size = section_data[".text"]
    rela_off, rela_size = section_data[".rela.text"]
    eh_frame_off, eh_frame_size = section_data[".eh_frame"]
    rela_eh_frame_off, rela_eh_frame_size = section_data[".rela.eh_frame"]
    sym_off, sym_size = section_data[".symtab"]
    str_off, str_size = section_data[".strtab"]
    shstr_off, shstr_size = section_data[".shstrtab"]
    shdrs = [
        SHDR.pack(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        SHDR.pack(shname_offsets[".text"], 1, 0x6, 0, text_off, text_size, 0, 0, 16, 0),
        SHDR.pack(shname_offsets[".rela.text"], 4, 0, 0, rela_off, rela_size, 5, 1, 8, RELA.size),
        SHDR.pack(shname_offsets[".eh_frame"], 1, 0x2, 0, eh_frame_off, eh_frame_size, 0, 0, 8, 0),
        SHDR.pack(shname_offsets[".rela.eh_frame"], 4, 0, 0, rela_eh_frame_off, rela_eh_frame_size, 5, 3, 8, RELA.size),
        SHDR.pack(shname_offsets[".symtab"], 2, 0, 0, sym_off, sym_size, 6, 2, 8, SYM.size),
        SHDR.pack(shname_offsets[".strtab"], 3, 0, 0, str_off, str_size, 0, 0, 1, 0),
        SHDR.pack(shname_offsets[".bss"], 8, 0x3, 0, 0, 8, 0, 0, 8, 0),
        SHDR.pack(shname_offsets[".shstrtab"], 3, 0, 0, shstr_off, shstr_size, 0, 0, 1, 0),
    ]
    body.extend(b"".join(shdrs))
    ident = b"\x7fELF" + bytes((2, 1, 1, 0, 0)) + b"\0" * 7
    body[:EHDR.size] = EHDR.pack(ident, 1, 62, 1, 0, 0, shoff, 0, EHDR.size,
                                 0, 0, SHDR.size, len(shdrs), 8)
    (ROOT / name).write_bytes(body)


def main():
    make_object("a.o", "main", ["helper"])
    make_object("b.o", "helper", [])
    make_object("c.o", "helper", [])
    make_object("weak.o", "helper", [], weak_definition=True)
    make_object("empty.o", None, [])
    (ROOT / "empty.bin").write_bytes(b"")
    (ROOT / "text.o").write_text("this is not an ELF object\n", encoding="ascii")
    (ROOT / "elf32.o").write_bytes(b"\x7fELF\x01\x01\x01" + b"\0" * (EHDR.size - 7))
    a = bytearray((ROOT / "a.o").read_bytes())
    struct.pack_into("<Q", a, 40, len(a) + 4096)  # e_shoff
    (ROOT / "bad-offset.o").write_bytes(a)
    (ROOT / "truncated.o").write_bytes((ROOT / "a.o").read_bytes()[:-12])
    cuda = bytearray((ROOT / "a.o").read_bytes())
    struct.pack_into("<H", cuda, 18, 190)  # e_machine = EM_CUDA
    (ROOT / "cuda-machine.o").write_bytes(cuda)


if __name__ == "__main__":
    main()
