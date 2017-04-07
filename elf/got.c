#include <assert.h>
#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>
#include "got.h"
#include "../Linker.h"
#include "../debug.h"
/*
 * Check if we need a global offset table slot for a
 * given symbol
 */
bool
need_got_slot(ElfSym * symbol) {
    /* using global here should give an upper bound */
    /* I don't believe we need to relocate STB_LOCAL
     * symbols via the GOT; however I'm unsure about
     * STB_WEAK.
     *
     * Any more restrictive filter here would result
     * in a smaller GOT, which is preferrable.
     */
    return ELF_ST_BIND(symbol->st_info) == STB_GLOBAL
        || ELF_ST_BIND(symbol->st_info) == STB_WEAK;
}

bool
make_got(ObjectCode * oc) {
    size_t got_slots = 0;

    /* we need to find all symbol tables (elf can have multiple)
     * and need to iterate over all symbols, to check how many
     * got slots we need at most
     */
    assert( oc->info != NULL );
    assert( oc->info->sectionHeader != NULL );
    for(unsigned i=0; i < oc->n_sections; i++) {
        if(SHT_SYMTAB == oc->info->sectionHeader[i].sh_type) {
            ElfSym *symTab = (ElfSym*)((uint8_t*)oc->info->elfHeader
                                         + oc->info->sectionHeader[i].sh_offset);
            size_t n_symbols = oc->info->sectionHeader[i].sh_size
                               / sizeof(ElfSym);
            for(size_t j=0; j < n_symbols; j++) {
                if(need_got_slot(&symTab[j])) {
                    got_slots += 1;
                }
            }
        }
    }
    if(got_slots > 0) {
        oc->info->got_size = got_slots * sizeof(void *);
         void * mem = mmap(NULL, oc->info->got_size,
                           PROT_READ | PROT_WRITE,
                           MAP_ANON | MAP_PRIVATE,
                           -1, 0);
        if (mem == MAP_FAILED) {
            __link_log("MAP_FAILED. errno=%d", errno);
            return EXIT_FAILURE;
        }
        oc->info->got_start = (addr_t)mem;
        /* update got_addr */
        size_t slot = 0;
        for(ElfSymbolTable *symTab = oc->info->symbolTables;
            symTab != NULL; symTab = symTab->next)
            for(size_t i=0; i < symTab->n_symbols; i++)
                if(need_got_slot(symTab->symbols[i].elf_sym))
                    symTab->symbols[i].got_addr
                            = oc->info->got_start
                              + (slot++ * sizeof(addr_t));
    }
    return EXIT_SUCCESS;
}

bool
fill_got(Linker * l, ObjectCode * oc) {
    /* fill the GOT table */
    for(ElfSymbolTable *symTab = oc->info->symbolTables;
        symTab != NULL; symTab = symTab->next) {
        for(size_t i=0; i < symTab->n_symbols; i++) {
            ElfSymbol * symbol = &symTab->symbols[i];
            if(need_got_slot(symbol->elf_sym)) {
                /* no type are undefined symbols */
                if(   STT_NOTYPE == ELF_ST_TYPE(symbol->elf_sym->st_info)
                   || STB_WEAK   == ELF_ST_BIND(symbol->elf_sym->st_info)) {
                    if(0x0 == symbol->addr) {
                        symbol->addr = lookupSymbol_(l, symbol->name);
                        if(0x0 == symbol->addr) {
                            __link_log("Failed to lookup symbol: %s\n",
                                       symbol->name);
                            return EXIT_FAILURE;
                        }
                    } else {
                        // we already have the address.
                    }
                } /* else it was defined somewhere in the same object, and
                  * we should have the address already.
                  */
                if(0x0 == symbol->addr) {
                    __link_log(
                            "Something went wrong! Symbol %s has null address.\n",
                            symbol->name);
                    return EXIT_FAILURE;
                }
                if(0x0 == symbol->got_addr) {
                    __link_log("Not good either!");
                    return EXIT_FAILURE;
                }
                *(addr_t*)symbol->got_addr = symbol->addr;
            }
        }
    }
    return EXIT_SUCCESS;
}
bool
verify_got(Linker * l __attribute__((unused)), ObjectCode * oc) {
    for(ElfSymbolTable *symTab = oc->info->symbolTables;
        symTab != NULL; symTab = symTab->next) {
        for(size_t i=0; i < symTab->n_symbols; i++) {
            ElfSymbol * symbol = &symTab->symbols[i];
            if(symbol->got_addr) {
                assert((addr_t)(*(addr_t*)symbol->got_addr)
                       == (addr_t)symbol->addr);
            }
            assert(0 == ((addr_t)symbol->addr & 0xffff000000000000));
        }
    }
    return EXIT_SUCCESS;
}

void
free_got(ObjectCode * oc) {
//    munmap(oc->info->got_start, oc->info->got_size);
    oc->info->got_start = 0x0;
    oc->info->got_size = 0;
}
