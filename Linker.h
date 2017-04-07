#ifndef Linker_h
#define Linker_h

#include <stdio.h>
#include <stdbool.h>
#include "Ar.h"
//#include "MachO.h"
#include "BinaryTree.h"
#include "Types.h"

typedef struct _global_symbol {
    ElfSymbol * symbol;
    ObjectCode * oc;
    bool    is_weak;
    struct _global_symbol *next;
} GlobalSymbol;

typedef struct _linker {
    /* all the known global symbols in the current linker session */
    GlobalSymbol * symbols;

    /* global symbol table */
    binary_tree_node * gsyms;
    /* all the objects loaded */
    ObjectCode * objects;
} Linker;

void
addSection (Section *s, SectionKind kind, SectionAlloc alloc,
            addr_t start, unsigned size, unsigned mapped_offset,
            addr_t mapped_start, unsigned mapped_size);

/* global, yuck! */
extern Linker LINKER;

bool
insert_global_symbol(Linker * l, GlobalSymbol * symbol);

void
list_global_symbols();

addr_t
lookupSymbol_(Linker *l, char * name);

addr_t
lookupGlobalSymbol_(char * name);

bool
lookup_system_symbols(const char * name, addr_t * addr);

bool
lookup_global_symbol(binary_tree_node * gsyms, ElfSymbol * needle, addr_t *
addr);

bool
lookup_global_symbol_(binary_tree_node * gsyms, char * name, addr_t * addr);


struct object_code *
find_object_code(ObjectCode * oc, addr_t addr);

struct object_code *
find_object_code_for_name(ObjectCode * oc, char * name);
//void
//load_object(const char * path);

#endif /* Linker_h */
