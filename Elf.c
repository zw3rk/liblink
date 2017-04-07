#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <assert.h>

#include "Linker.h"
#include "Elf.h"
#include "elf/plt.h"
#include "elf/got.h"
#include "elf/reloc.h"
#include "debug.h"

/*
 * If the number of sections is greater than or equal to SHN_LORESERVE (0xff00),
 * e_shnum has the value SHN_UNDEF (0). The actual number of section header
 * table entries is contained in the sh_size field of the section header at
 * index 0. Otherwise, the sh_size member of the initial entry contains the
 * value zero.
 */
static ElfWord elf_shnum(ElfEhdr* ehdr)
{
    ElfShdr* shdr = (ElfShdr*) ((char*)ehdr + ehdr->e_shoff);
    ElfHalf shnum = ehdr->e_shnum;
    assert(shnum != SHN_UNDEF || shdr[0].sh_size != 0);
    return shnum != SHN_UNDEF ? shnum : shdr[0].sh_size;
}

//static ElfWord elf_shstrndx(ElfEhdr* ehdr)
//{
////   Elf_Shdr* shdr = (Elf_Shdr*) ((char*)ehdr + ehdr->e_shoff);
//    ElfHalf shstrndx = ehdr->e_shstrndx;
//#if defined(SHN_XINDEX)
//    return shstrndx != SHN_XINDEX ? shstrndx : shdr[0].sh_link;
//#else
//    // some OSes do not support SHN_XINDEX yet, let's revert to
//    // old way
//    return shstrndx;
//#endif
//}


#if defined(SHN_XINDEX)
static Elf_Word*
get_shndx_table(Elf_Ehdr* ehdr)
{
   Elf_Word  i;
   char*     ehdrC    = (char*)ehdr;
   Elf_Shdr* shdr     = (Elf_Shdr*) (ehdrC + ehdr->e_shoff);
   Elf_Word shnum = elf_shnum(ehdr);

   for (i = 0; i < shnum; i++) {
     if (shdr[i].sh_type == SHT_SYMTAB_SHNDX) {
       return (Elf32_Word*)(ehdrC + shdr[i].sh_offset);
     }
   }
   return NULL;
}
#endif

/*
 * Note, the ghc linker does the following:
 * - Linker/LoadObj(path)
 *  - Linker/LoadObj_(path) (mutex guarded)
 *   - Check if alreadyLoaded
 *   - Linker/preloadObjectFile
 *    - mmap or malloc file into memory
 *    - mkOc
 *    - verifyImage
 *    - ocInit_<arch>
 *   - Linker/loadOc
 *    - verifyImage
 *    - allocateSymbolExtras
 *    - getNames
 *    - setInitialStatus
 *   - prepend oc to global objects list.
 */

static void setOcInitialStatus(ObjectCode* oc) {
    if (oc->archiveMemberName == NULL) {
        oc->status = OBJECT_NEEDED;
    } else {
        oc->status = OBJECT_LOADED;
    }
}

ObjectCode *
mkOc(char *path, uint8_t *image, long imageSize,
     bool mapped, char *archiveMemberName, unsigned misalignment ) {
    ObjectCode* oc;

    oc = calloc(1, sizeof(ObjectCode));
    assert(oc != NULL);

    oc->formatName = "ELF";

    oc->image = image;
    oc->fileName = strdup(path);

    if (archiveMemberName != NULL) oc->archiveMemberName = strdup(archiveMemberName);

    setOcInitialStatus( oc );

    oc->fileSize          = imageSize;
    oc->symbols           = NULL;
    oc->n_sections        = 0;
    oc->sections          = NULL;
    oc->proddables        = NULL;

    oc->imageMapped       = mapped;

    oc->misalignment      = misalignment;

    /* chain it onto the list of objects */
    oc->next              = NULL;

    return oc;
}

void
ocInit(ObjectCode * oc)
{
    oc->info = calloc(1, sizeof(ObjectCodeFormatInfo));
    assert(oc->info != NULL);

    oc->info->elfHeader = (ElfEhdr *)oc->image;
    oc->info->programHeader = (ElfPhdr *) (oc->image
                                            + oc->info->elfHeader->e_phoff);
    oc->info->sectionHeader = (ElfShdr *) (oc->image
                                            + oc->info->elfHeader->e_shoff);
    oc->info->sectionHeaderStrtab = (char*) oc->image +
            oc->info->sectionHeader[oc->info->elfHeader->e_shstrndx].sh_offset;

    oc->n_sections = elf_shnum(oc->info->elfHeader);
    oc->sections = calloc(oc->n_sections, sizeof(Section));
    assert(oc->sections != NULL);

    /* get the symbol and relocation table(s) */
    for(unsigned i=0; i < oc->n_sections; i++) {
        if(SHT_REL  == oc->info->sectionHeader[i].sh_type) {
            ElfRelocationTable *relTab = calloc(
                    1, sizeof(ElfRelocationTable));
            assert(relTab != NULL);
            relTab->index = i;

            relTab->relocations = (ElfRel*) ((uint8_t*)oc->info->elfHeader
                                              + oc->info->sectionHeader[i].sh_offset);
            relTab->n_relocations = oc->info->sectionHeader[i].sh_size
                                    / sizeof(ElfRel);
            relTab->targetSectionIndex = oc->info->sectionHeader[i].sh_info;
            relTab->sectionHeader      = &oc->info->sectionHeader[i];

            if(oc->info->relTable == NULL) {
                oc->info->relTable = relTab;
            } else {
                ElfRelocationTable * tail = oc->info->relTable;
                while(tail->next != NULL) tail = tail->next;
                tail->next = relTab;
            }

        } else if(SHT_RELA == oc->info->sectionHeader[i].sh_type) {
            ElfRelocationATable *relTab = calloc(
                    1, sizeof(ElfRelocationATable));
            assert(relTab != NULL);
            relTab->index = i;

            relTab->relocations = (ElfRela*) ((uint8_t*)oc->info->elfHeader
                                               + oc->info->sectionHeader[i].sh_offset);
            relTab->n_relocations = oc->info->sectionHeader[i].sh_size
                                    / sizeof(ElfRela);
            relTab->targetSectionIndex = oc->info->sectionHeader[i].sh_info;
            relTab->sectionHeader      = &oc->info->sectionHeader[i];

            if(oc->info->relaTable == NULL) {
                oc->info->relaTable = relTab;
            } else {
                ElfRelocationATable * tail = oc->info->relaTable;

                while(tail->next != NULL) tail = tail->next;
                tail->next = relTab;
            }

        } else if(SHT_SYMTAB == oc->info->sectionHeader[i].sh_type) {

            ElfSymbolTable *symTab = calloc(
                    1, sizeof(ElfSymbolTable));
            assert(symTab != NULL);

            symTab->index = i; /* store the original index, so we can later
                                * find or assert that we are dealing with the
                                * correct symbol table */

            ElfSym *stab = (ElfSym*)((uint8_t*)oc->info->elfHeader
                                       + oc->info->sectionHeader[i].sh_offset);
            symTab->n_symbols = oc->info->sectionHeader[i].sh_size
                                / sizeof(ElfSym);
            symTab->symbols = calloc(symTab->n_symbols,
                                                  sizeof(ElfSymbol));
            assert(symTab->symbols != NULL);

            /* get the strings table */
            size_t lnkIdx = oc->info->sectionHeader[i].sh_link;
            symTab->names = (char*)(uint8_t*)oc->info->elfHeader
                            + oc->info->sectionHeader[lnkIdx].sh_offset;

            /* build the ElfSymbols from the symbols */
            for(size_t j=0; j < symTab->n_symbols; j++) {
                ElfSymbol * symbol = &symTab->symbols[j];
                symbol->elf_sym = &stab[j];

                if(symbol->elf_sym->st_name == 0) {
                    symbol->name = "(no name)";
                } else {
                    symbol->name = symTab->names + symbol->elf_sym->st_name;
                    symbol->hash = hash(symbol->name);
                }
                /* we don't have an address for this symbol yet; this will be
                 * populated during ocGetNames. hence addr = NULL.
                 */
                symbol->addr = 0x0;
                symbol->got_addr = 0x0;
            }

            /* append the ElfSymbolTable */
            if(oc->info->symbolTables == NULL) {
                oc->info->symbolTables = symTab;
            } else {
                ElfSymbolTable * tail = oc->info->symbolTables;
                while(tail->next != NULL) tail = tail->next;
                tail->next = symTab;
            }
        }
    }
}

ObjectCode *
processObject(Linker * l, ObjectCode * oc ) {
    ocInit( oc );

    if(load_sections(oc)) abort();

    // get *all* names.
    if(get_names(l, oc)) abort();

    if(make_got(oc)) abort();

    /* prepend oc to the known objects */
    if(l->objects == NULL) l->objects = oc;
    else {
        ObjectCode * tail = l->objects;
        while(tail->next != NULL) tail = tail->next;
        assert(tail->next == NULL);
        tail->next = oc;
    }
    return oc;
}

ObjectCode *
loadObject(Linker * l, char * name __attribute__((unused)), char * path) {
    struct stat st;
    if(stat(path, &st)) {
        __link_log("Failed to call stat(%s);\n", path); abort();
    }
    long fileSize = st.st_size;
    int fd = open(path, O_RDONLY);

    assert(fileSize > 0);
    uint8_t * image = mmap(NULL, (size_t)fileSize, PROT_READ,
                           MAP_PRIVATE, fd, 0);
    if (image == MAP_FAILED) {
        __link_log("mmap: failed. errno = %d", errno);
        abort();
    }

    close(fd);

    ObjectCode * oc = mkOc(path, image, fileSize, true, NULL, 0);

    processObject(l, oc );

    return oc;
}

ObjectCode *
loadArchive(Linker * l, char * path) {
    __link_log("Loading Archive: %s...\n", path);

    unsigned os = count_objects(l);

    FILE *ar = fopen(path, "rb");
    assert(ar != NULL);
    ObjectCode * fst = NULL;

    Object *objs = read_archive(ar);
    for(Object *o=objs; o != NULL; o = o->next) {
        ObjectCode *oc = mkOc(path, o->image, o->size, false, o->name, 0);

        processObject(l, oc );

        if(fst == NULL) fst = oc;
    }
    fclose(ar);

    unsigned os2 = count_objects(l);

    __link_log("Loaded %d objects.\n", os2 - os);
    return fst;
}

bool
resolveObject(Linker * l, ObjectCode * oc) {
    char ocbuf[256]; memset(ocbuf, 0, sizeof(ocbuf));
    get_oc_info(ocbuf, oc);
    __link_log("%s: Resolving Object(s) ...\n", ocbuf);
    if(fill_got( l, oc ))
        return EXIT_FAILURE;
    if(verify_got( l, oc ))
        return EXIT_FAILURE;
    if(relocate_object_code( oc ))
        return EXIT_FAILURE;
    if(mprotect_object_code( oc ))
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}

#define SHF_RO   SHF_ALLOC
#define SHF_RW   (SHF_ALLOC | SHF_WRITE)
#define SHF_RX   (SHF_ALLOC | SHF_EXECINSTR)
#define SHF_RWX  (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)

/* mask is the same as rwx */
#define SHF_MASK (SHF_ALLOC | SHF_EXECINSTR | SHF_WRITE)

SectionKind
section_kind(ElfShdr *hdr) {
    switch(hdr->sh_type) {
        case SHT_PROGBITS: {
            switch(hdr->sh_flags & SHF_MASK) {
                case SHF_RX: return SECTIONKIND_TEXT;
                case SHF_RW: return SECTIONKIND_RWDATA;
                case SHF_RO: return SECTIONKIND_RODATA;
                case SHF_RWX: abort();
                default:
                    break;
            }
        }
        case SHT_NOBITS: {
            switch(hdr->sh_flags & SHF_MASK) {
                case SHF_RW: return SECTIONKIND_ZEROFILL;
                case SHF_RX:
                case SHF_RO:
                case SHF_RWX: abort();
                default:
                    break;
            }
        }
        default:
            return SECTIONKIND_OTHER;
    }
}

void
debug_print_section(Section * s) {
    char * kind;
    char range[256]; memset(range, 0, sizeof range);
    char stubs[256]; memset(stubs, 0, sizeof stubs);

    switch(s->kind) {
        case SECTIONKIND_TEXT:     kind = "text";      break;
        case SECTIONKIND_RODATA:   kind = "data (ro)"; break;
        case SECTIONKIND_RWDATA:   kind = "data (rw)"; break;
        case SECTIONKIND_ZEROFILL: kind = "zerofill";  break;
        default:                   kind = "unknown";   break;
    }
    sprintf(range, "%p - %p (%5lu)", (void*)s->start, (void*)
                    (s->start+s->size), (unsigned long)s->size);
    if(s->info->stub_size > 0)
        sprintf(stubs, "%p - %p (%5lu)", (void*)s->info->stub_offset,
                (void*)(s->info->stub_offset + s->info->stub_size),
                (unsigned long)s->info->stub_size);
    else
        sprintf(stubs, "None");

    __link_log("\tName: %15s; Kind: %10s; Range: %40s, Stubs: %s\n",
               s->info->name,
               kind,
               range, stubs);
}

#if defined(SHN_XINDEX)
# error "NOT IMPLEMENTED"
#endif

bool
load_sections(ObjectCode * oc) {
    __link_log("Loading sections for %s (%s)\n",
               oc->fileName,
               oc->archiveMemberName == NULL ? "" : oc->archiveMemberName);
    for(unsigned i=0; i < oc->n_sections; i++) {
        ElfShdr * sectionHeader = &oc->info->sectionHeader[i];
        SectionKind kind = section_kind(sectionHeader);
        switch(kind) {
            case SECTIONKIND_TEXT:
            case SECTIONKIND_RODATA:
            case SECTIONKIND_RWDATA: {
                unsigned nstubs = numberOfStubsForSection(oc, i);
                unsigned stub_space = STUB_SIZE * nstubs;

                /* function stub relocation makes only sense in text sections */
                assert(nstubs == 0 || kind == SECTIONKIND_TEXT);

                if(sectionHeader->sh_size+stub_space == 0) {
                    addSection(&oc->sections[i], kind, SECTION_NOMEM,
                               0x0 /* mem */, 0 /* size */, 0 /* mapped off */,
                               0x0 /* mapped start */, 0 /* mapped size */);

                    oc->sections[i].info->name        = oc->info->sectionHeaderStrtab + sectionHeader->sh_name;
                    oc->sections[i].info->nstubs      = 0;
                    oc->sections[i].info->stub_offset = 0x0;
                    oc->sections[i].info->stub_size   = 0;
                    oc->sections[i].info->stubs       = 0x0;
                } else {
                    /* ensure we are word aligned for the stub space */
                    assert(   0 == stub_space
                           || 0 == (sectionHeader->sh_size & 0x3));
                    void *mem = mmap(NULL, sectionHeader->sh_size + stub_space,
                                     PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE,
                                     -1, 0);

                    addr_t stub_offset = (addr_t) mem + sectionHeader->sh_size;

                    if (mem == MAP_FAILED) {
                        __link_log(
                                "failed to mmap allocated memory to load section %d. errno = %d",
                                i, errno);
                        abort();
                    }

                    /* copy the data from the image into the section memory */
                    memcpy(mem,
                           oc->image + sectionHeader->sh_offset,
                           sectionHeader->sh_size);

                    addSection(&oc->sections[i], kind, SECTION_MMAP,
                               (addr_t)mem, (unsigned)sectionHeader->sh_size, 0,
                               (addr_t)mem, (unsigned)sectionHeader->sh_size + stub_space);

                    oc->sections[i].info->name        = oc->info->sectionHeaderStrtab + sectionHeader->sh_name;
                    oc->sections[i].info->nstubs      = 0;
                    oc->sections[i].info->stub_offset = stub_offset;
                    oc->sections[i].info->stub_size   = stub_space;
                    oc->sections[i].info->stubs       = NULL;
                }

                oc->sections[i].info->sectionHeader = sectionHeader;

                break;
            }
            case SECTIONKIND_ZEROFILL: {
                if(sectionHeader->sh_size > 0) {
                    void *mem = mmap(NULL, sectionHeader->sh_size,
                                     PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE,
                                     -1, 0);

                    if (mem == MAP_FAILED) {
                        __link_log(
                                "failed to mmap allocated memory to load section %d. errno = %d",
                                i, errno);
                        abort();
                    }

                    addSection(&oc->sections[i], kind, SECTION_MMAP,
                               (addr_t)mem, (unsigned)sectionHeader->sh_size, 0,
                               0x0, 0);
                } else {
                    // a zero sized section. calloc nothing, this will usually
                    // result in a single allocated byte.
                    void *mem = mmap(NULL, 1,
                                     PROT_READ | PROT_WRITE,
                                     MAP_ANON | MAP_PRIVATE,
                                     -1, 0);

                    if (mem == MAP_FAILED) {
                        __link_log(
                                "failed to mmap allocated memory to load section %d. errno = %d",
                                i, errno);
                        abort();
                    }

//                    void * mem = calloc(1, sectionHeader->sh_size);
//                    assert(mem != NULL);
                    addSection(&oc->sections[i], kind, SECTION_MALLOC,
                               (addr_t)mem, 1/*sectionHeader->sh_size */, 0,
                               0x0, 0);
                }

                oc->sections[i].info->name        = oc->info->sectionHeaderStrtab + sectionHeader->sh_name;
                oc->sections[i].info->nstubs      = 0;
                oc->sections[i].info->stub_offset = 0x0;
                oc->sections[i].info->stub_size   = 0;
                oc->sections[i].info->stubs       = NULL;

                oc->sections[i].info->sectionHeader = sectionHeader;

                break;
            }
            default: {
                addSection(&oc->sections[i], kind, SECTION_NOMEM,
                           (addr_t)oc->image+sectionHeader->sh_offset,
                           (unsigned)sectionHeader->sh_size,
                           0, 0x0, 0);

                oc->sections[i].info->name        = oc->info->sectionHeaderStrtab + sectionHeader->sh_name;
                oc->sections[i].info->nstubs = 0;
                oc->sections[i].info->stub_offset = 0x0;
                oc->sections[i].info->stub_size = 0;
                oc->sections[i].info->stubs = NULL;

                oc->sections[i].info->sectionHeader = sectionHeader;

//                char *info = elf_lookup_section_type(sectionHeader->sh_type);
//                if (info) {
//                   log("Ignoning section: %s\n", info);
//                } else {
//
//                   log("Unknown section: 0x%0x (%s)\n",
//                           sectionHeader->sh_type,
//                           oc->info->sectionHeaderStrtab
//                           + sectionHeader->sh_name);
//
//                }
                break;
            }
        }
        /* debug */
        switch(kind) {
            case SECTIONKIND_TEXT:
            case SECTIONKIND_RODATA:
            case SECTIONKIND_RWDATA:
            case SECTIONKIND_ZEROFILL:
                debug_print_section(&oc->sections[i]);
                break;
            default:
                break;
        }
    }
    return EXIT_SUCCESS;
}

// TODO:
/*
 * We need to fix st_shndx if SHN_XINDEX is given, and
 * shndx *is* SHN_XINDEX, to lookup the section index in
 * the shndxTable.
 */
bool
is_local(ElfSymbol * symbol) {
    return ELF_ST_BIND(symbol->elf_sym->st_info) == STB_LOCAL;
}
bool
is_global(ElfSymbol * symbol) {
    return ELF_ST_BIND(symbol->elf_sym->st_info) == STB_GLOBAL;
}
bool
is_weak(ElfSymbol * symbol) {
    return ELF_ST_BIND(symbol->elf_sym->st_info) == STB_WEAK;
}
bool
is_bound(ElfSymbol * symbol) {
    return is_local(symbol) || is_global(symbol) || is_weak(symbol);
}

bool
is_defined(ElfSymbol * symbol) {
    return !(symbol->elf_sym->st_shndx == SHN_UNDEF);
}

bool
is_in_sepcial_section(ElfSymbol * symbol) {
#if defined(SHN_XINDEX)
# error "NOT IMPLEMENTED"
#else
    return symbol->elf_sym->st_shndx >= SHN_LORESERVE;
#endif
}


bool
is_regular_type(ElfSymbol * symbol) {
    switch(ELF_ST_TYPE(symbol->elf_sym->st_info)) {
        case STT_NOTYPE:
        case STT_OBJECT:
        case STT_FUNC:
            return true;
        default:
            return false;
    }
}
bool
is_section_symbol(ElfSymbol * symbol) {
    return ELF_ST_TYPE(symbol->elf_sym->st_info) == STT_SECTION;
}

bool
mprotect_loaded_sections(ObjectCode * oc) {
    // mprotect sections
    for(unsigned i=0; i < oc->n_sections; i++) {
        if(oc->sections[i].alloc == SECTION_NOMEM) continue;

        /* we can only mprotect mmaped sections */
        if(!(oc->sections[i].alloc == SECTION_MMAP)) continue;

        unsigned flags = PROT_NONE;
        switch( oc->sections[i].kind ) {
            case SECTIONKIND_ZEROFILL:
            case SECTIONKIND_RWDATA:
                flags = PROT_READ | PROT_WRITE;
                break;
            case SECTIONKIND_TEXT:
                flags = PROT_READ | PROT_EXEC;
                break;
            default:
                flags = PROT_READ;
                break;
        }

        if(0 != mprotect((void*)oc->sections[i].mapped_start,
                                oc->sections[i].mapped_size, flags)) {
            __link_log("mprotect for section %d failed!", i);
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

bool
mprotect_object_code(ObjectCode * oc) {
    if(mprotect_loaded_sections(oc))
        return EXIT_FAILURE;

    // make got also executable.
    if(0x0 != oc->info->got_start) {
        if( mprotect((void*)oc->info->got_start,
                     oc->info->got_size,
                     PROT_READ | PROT_EXEC) ) {
            __link_log("mprotect failed!");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

bool
get_names(Linker * l, ObjectCode * oc) {
    oc->n_symbols = 0;
    for(ElfSymbolTable *symTab = oc->info->symbolTables;
        symTab != NULL; symTab = symTab->next) {
        oc->n_symbols += symTab->n_symbols;
    }

    oc->symbols = calloc(oc->n_symbols, sizeof(SymbolName*));
    assert(oc->symbols != NULL);

    // Note calloc: if we fail partway through initializing symbols, we need
    // to undo the additions to the symbol table so far. We know which ones
    // have been added by whether the entry is NULL or not.

    unsigned curSymbol = 0;

    for(ElfSymbolTable *symTab = oc->info->symbolTables;
        symTab != NULL; symTab = symTab->next) {
        for (size_t j = 0; j < symTab->n_symbols; j++) {
            ElfSymbol * symbol = &symTab->symbols[j];

            unsigned short shndx = symbol->elf_sym->st_shndx;

            /* COMMON means we need to unify this symbol with the one defined
             * elsewhere.  Hence ideally we'd look it up?
             *
             * Supposedly this only happens proper with FORTRAN, or with c/c++
             * if you do not mark globals as `extern` in the header.  E.g.
             * introduce them uninitialized in a different compiliation unit
             * by transitively (or directly) including the header.
             *
             * Thus, I believe it's best to leave this assert in here. The
             * alternative would be to just ignore this symbol, and hope we
             * will have read the proper symbol definition by the time we
             * try to look it up.
             */
            assert(shndx != SHN_COMMON);

            if(is_section_symbol(symbol)) {
                symbol->addr = oc->sections[shndx].start;
            } else if (is_weak(symbol)) {
                /* address will be resolved in insert_global_symbol
                 */
                symbol->addr = 0x0;
            } else if(   is_bound(symbol)
                      && is_defined(symbol)
                      && !is_in_sepcial_section(symbol)
                      && is_regular_type(symbol)) {

                assert(shndx > 0); /* 0 is the undefined section */
                assert(shndx < oc->n_sections); /* in range */

                /* we may end up with zero sized items */

                assert(!is_weak(symbol));
                symbol->addr = oc->sections[shndx].start
                               + symbol->elf_sym->st_value;
            }

            if (0x0 != symbol->addr) {
                assert(symbol->name != NULL);

                /* ignore local symbols */
                if(   is_global(symbol)
                   || is_weak(symbol)) {
                    GlobalSymbol * g = calloc(1, sizeof(GlobalSymbol));
                    assert(g != NULL);
                    g->oc = oc;
                    g->symbol = symbol;
                    g->is_weak = is_weak(symbol);

                    if(!insert_global_symbol(l, g)) {
                        abort();
                    }

                    oc->symbols[curSymbol++] = symbol->name;
                }
            } else {
                /* Skip. */
            }
        }
    }
    return EXIT_SUCCESS;
}