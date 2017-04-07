#include <malloc.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <assert.h>

#include "Linker.h"
#include "Elf.h"
#include "debug.h"

Linker LINKER = { .symbols = NULL, .gsyms = NULL, .objects = NULL };

bool
insert_global_symbol(Linker * l, GlobalSymbol * symbol)
{
    if(symbol->is_weak) {
        /* let's see if we can resolve that symbol to a known system symbol */
        assert(0x0 != symbol->symbol->addr);
        if(lookup_system_symbols(symbol->symbol->name,
                                 &symbol->symbol->addr)) {
            /* failed to find it in the global symbols */
            assert(0x0 != symbol->symbol->addr);
            abort(/* forward reference not yet supposed */);
        } else {
            assert(0x0 != symbol->symbol->addr);
            symbol->is_weak = false;
        }
    }
    assert(!symbol->is_weak);
    binary_tree_insert(&l->gsyms, symbol->symbol->hash, symbol);
    return true;
}

bool
lookup_system_symbols(const char * name, addr_t * addr)
{
    *addr = (addr_t)dlsym(RTLD_DEFAULT /*libHandle*/, name);
    if(0x0 == *addr) {
        const char * err = dlerror();
        __link_log("Failed to lookup symbol %s; error: %s\n", name, err);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

bool
lookup_global_symbol(binary_tree_node * gsyms, ElfSymbol * needle, addr_t *addr)
{
    GlobalSymbol * s = NULL;
    if(binary_tree_lookup(gsyms, needle->hash, (void**)&s))
        return EXIT_FAILURE;
    *addr = s->symbol->addr;
    return EXIT_SUCCESS;
}

bool
lookup_global_symbol_(binary_tree_node * gsyms, char * name, addr_t * addr)
{
    ElfSymbol needle = { .name = name, .hash = hash(name) };
    return lookup_global_symbol(gsyms, &needle, addr);
}

addr_t
lookupSymbol_(Linker * l, char * name) {
    addr_t addr = 0x0;

    if(lookup_global_symbol_(l->gsyms, name, &addr)
       && lookup_system_symbols(name, &addr)) {
        __link_log(
                "WARN: failed to find symbol '%s' ins global or system symbols!\n",
                name);
    }
    return addr;
}

addr_t
lookupGlobalSymbol_(char * name) {
    return lookupSymbol_(&LINKER, name);
}


void
walk(binary_tree_node * n) {
    if(n->left != NULL) walk(n->left);
    GlobalSymbol *g = (GlobalSymbol *)n->value;
    __link_log("%p %p %s\n", g->symbol->addr, g->symbol->got_addr,
               g->symbol->name);
    if(n->right != NULL) walk(n->right);
}
void
list_global_symbols() {
    walk(LINKER.gsyms);
}

void
addSection (Section *s, SectionKind kind, SectionAlloc alloc,
            addr_t start, unsigned size, unsigned mapped_offset,
            addr_t mapped_start, unsigned mapped_size)
{
    s->start        = start;     /* actual start of section in memory */
    s->size         = size;      /* actual size of section in memory */
    s->kind         = kind;
    s->alloc        = alloc;
    s->mapped_offset = mapped_offset; /* offset from the image of mapped_start */

    s->mapped_start = mapped_start; /* start of mmap() block */
    s->mapped_size  = mapped_size;  /* size of mmap() block */

    s->info = calloc(1, sizeof(SectionFormatInfo));
    assert(s->info != NULL);
}
