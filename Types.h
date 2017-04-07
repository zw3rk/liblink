//
// Created by Moritz Angermann on 3/29/17.
//

#ifndef LINK_TYPES_H
#define LINK_TYPES_H

#include <stdbool.h>

#include "elf/compat.h"
#include "Hash.h"
#include "elf/target.h"

typedef char   SymbolName;
typedef void * SymbolAddr;

typedef enum _OStatus {
    OBJECT_LOADED,
    OBJECT_NEEDED,
    OBJECT_RESOLVED,
    OBJECT_UNLOADED,
    OBJECT_DONT_RESOLVE
} OStatus;

typedef enum _SectionKind {
    SECTIONKIND_TEXT,     /* .text (rx)   */
    SECTIONKIND_RWDATA,   /* .data (rw)   */
    SECTIONKIND_RODATA,   /* .data (ro)   */
    SECTIONKIND_ZEROFILL, /* .bss section */
    SECTIONKIND_INIT_ARRAY,
    SECTIONKIND_OTHER,
    SECTIONKIND_NOINFOAVAIL
} SectionKind;

typedef enum _SectionAlloc {
    SECTION_NOMEM,
    SECTION_M32,
    SECTION_MMAP,
    SECTION_MALLOC,
} SectionAlloc;

typedef struct _ElfSymbol {
    SymbolName * name;  /* the name of the symbol. */
    addr_t addr;  /* the final resting place of the symbol */
    hash_t hash;
    addr_t got_addr;    /* address of the got slot for this symbol, if any */
    ElfSym * elf_sym;  /* the elf symbol entry */
} ElfSymbol;

typedef struct _ElfSymbolTable {
    unsigned  index;               /* the index of the underlying symtab */
    ElfSymbol * symbols;
    size_t n_symbols;
    char * names;                  /* strings table for this symbol table */
    struct _ElfSymbolTable * next; /* there may be multiple symbol tables */
} ElfSymbolTable;

typedef struct _ElfRelocationTable {
    unsigned index;
    unsigned targetSectionIndex;
    ElfShdr *sectionHeader;
    ElfRel  *relocations;
    size_t n_relocations;
    struct _ElfRelocationTable *next;
} ElfRelocationTable;

typedef struct _ElfRelocationATable {
    unsigned index;
    unsigned targetSectionIndex;
    ElfShdr *sectionHeader;
    ElfRela  *relocations;
    size_t n_relocations;
    struct _ElfRelocationATable *next;
} ElfRelocationATable;

typedef struct _Stub {
    addr_t addr;
    addr_t target;
    struct _Stub * next;
} Stub;

typedef struct _SectionFormatInfo {
    /*
     * The following fields are relevant for stubs next to sections only.
     */
    addr_t stub_offset;
    size_t stub_size;
    size_t nstubs;
    Stub * stubs;

    char * name;

    ElfShdr *sectionHeader;
} SectionFormatInfo;

typedef struct _Section {
    addr_t   start;              /* actual start of section in memory */
    size_t   size;               /* actual size of section in memory */
    SectionKind kind;
    SectionAlloc alloc;
    /*
     * The following fields are relevant for SECTION_MMAP sections only
     */
    size_t mapped_offset;      /* offset from the image of mapped_start */
    addr_t mapped_start;       /* start of mmap() block */
    size_t mapped_size;        /* size of mmap() block */

    /* A customizable type to augment the Section type. */
    SectionFormatInfo* info;
} Section;

/*
 * Just a quick ELF recap:
 *
 * .-----------------.
 * | ELF Header      |
 * |-----------------|
 * | Program Header  |
 * |-----------------|   .
 * | Section 1       |   |
 * |-----------------|   | Segment 1
 * | Section 2       |   |
 * |-----------------|   :
 * | ...             |   |
 * |-----------------|   | Segment n
 * | Section n       |   '
 * |-----------------|
 * | Section Header  |
 * '-----------------'
 *
 *
 * The Program Header will inform us about the Segments.  Whereas the Section
 * Header provides Information about the sections.
 *
 */
typedef struct _ObjectCodeFormatInfo {
    ElfEhdr              *elfHeader;
    ElfPhdr              *programHeader;
    ElfShdr              *sectionHeader;
    char                 *sectionHeaderStrtab;

    ElfSymbolTable       *symbolTables;
    ElfRelocationTable   *relTable;
    ElfRelocationATable  *relaTable;


    /* pointer to the global offset table */
    addr_t                got_start;
    size_t                got_size;

} ObjectCodeFormatInfo;

typedef struct _ProddableBlock {
    void* start;
    int   size;
    struct _ProddableBlock* next;
} ProddableBlock;

typedef struct _ObjectCode {
    OStatus    status;
    char      *fileName;
    long       fileSize;     /* also mapped image size when using mmap() */
    char*      formatName;            /* eg "ELF32", "DLL", "COFF", etc. */

    /* If this object is a member of an archive, archiveMemberName is
     * like "libarchive.a(object.o)". Otherwise it's NULL.
     */
    char*      archiveMemberName;

    /* An array containing ptrs to all the symbol names copied from
       this object into the global symbol hash table.  This is so that
       we know which parts of the latter mapping to nuke when this
       object is removed from the system. */
    char**  symbols;
    unsigned n_symbols;

    /* ptr to mem containing the object file image */
    uint8_t *      image;

    /* A customizable type, that formats can use to augment ObjectCode */
    ObjectCodeFormatInfo *info;

    /* non-zero if the object file was mmap'd, otherwise malloc'd */
    bool        imageMapped;

    /* flag used when deciding whether to unload an object file */
    bool        referenced;

    /* record by how much image has been deliberately misaligned
       after allocation, so that we can use realloc */
    unsigned       misalignment;

    /* The section-kind entries for this object module.  Linked
       list. */
    unsigned n_sections;
    Section* sections;

    /* Allow a chain of these things */
    struct _ObjectCode * next;

    /* SANITY CHECK ONLY: a list of the only memory regions which may
       safely be prodded during relocation.  Any attempt to prod
       outside one of these is an error in the linker. */
    ProddableBlock* proddables;

//    ForeignExportStablePtr *stable_ptrs;

} ObjectCode;

#endif //LINK_TYPES_H
