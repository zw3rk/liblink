//
// Created by Moritz Angermann on 3/30/17.
//

#include <malloc.h>
#include <stdio.h>
#include <memory.h>
#include "luts.h"

const char * elf_section_types[0x12] = {
        "NULL (Section header table entry unused)",
        "PROGBITS (Program data)",
        "SYMTAB (Symbol table)",
        "STRTAB (String table)",
        "RELA (Relocation entries with addends)",
        "HASH (Symbol hash table)",
        "DYNAMIC (Dynamic linking information)",
        "NOTE (Notes)",
        "NOBITS (Program space with no data (bss))",
        "REL (Relocation entries, no addends)",
        "SHLIB (Reserved)",
        "DYNSYM (Dynamic linker symbol table)",
        "INIT_ARRAY (Array of constructors)",
        "FINI_ARRAY (Array of destructors)",
        "PREINIT_ARRAY (Array of pre-constructors)",
        "GROUP (Section group)",
        "SYMTAB_SHNDX (Extended section indeces)",
        "NUM (Number of defined types.)"
};

const char* elf_lookup_section_type(unsigned n) {
    if(n > 0x11) {
        return NULL;
    }
    return elf_section_types[n];
}
