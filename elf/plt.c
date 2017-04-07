#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "plt.h"
#include "compat.h"
#include "plt/arm.h"
#include "plt/arm64.h"
#include "target.h"

#define _make_stub         ADD_SUFFIX(make_stub)
#define need_stub_for_rel  ADD_SUFFIX(need_stub_for_rel)
#define need_stub_for_rela ADD_SUFFIX(need_stub_for_rela)

unsigned
numberOfStubsForSection( ObjectCode *oc, unsigned sectionIndex) {
    unsigned n = 0;
    for(ElfRelocationTable *t = oc->info->relTable; t != NULL; t = t->next)
        if(t->targetSectionIndex == sectionIndex)
            for(size_t i=0; i < t->n_relocations; i++)
                if(need_stub_for_rel(&t->relocations[i]))
                    n += 1;

    for(ElfRelocationATable *t = oc->info->relaTable; t != NULL; t = t->next)
        if(t->targetSectionIndex == sectionIndex)
            for(size_t i=0; i < t->n_relocations; i++)
                if(need_stub_for_rela(&t->relocations[i]))
                    n += 1;
    return n;
}

bool
find_stub(Section * section, ElfSymbol * symbol __attribute__((unused)),
          addr_t *
addr) {
    for(Stub * s = section->info->stubs; s != NULL; s = s->next) {
        if(s->target == *addr) {
            *addr = s->addr;
            return EXIT_SUCCESS;
        }
    }
    return EXIT_FAILURE;
}

bool
make_stub(Section * section, ElfSymbol * symbol __attribute__((unused)), addr_t * addr) {

    Stub * s = calloc(1, sizeof(Stub));
    assert(s != NULL);
    s->target = *addr;
    s->next = NULL;
    s->addr = section->info->stub_offset + 8
            + STUB_SIZE * section->info->nstubs;

    if((*_make_stub)(s))
        return EXIT_FAILURE;

    if(section->info->stubs == NULL) {
        assert(section->info->nstubs == 0);
        /* no stubs yet, let's just create this one */
        section->info->stubs = s;
    } else {
        Stub * tail = section->info->stubs;
        while(tail->next != NULL) tail = tail->next;
        tail->next = s;
    }
    section->info->nstubs += 1;
    *addr = s->addr;
    return EXIT_SUCCESS;
}

void
free_stubs(Section * section) {
    if(section->info->nstubs == 0)
        return;
    Stub * last = section->info->stubs;
    while(last->next != NULL) {
        Stub * t = last;
        last = last->next;
        free(t);
    }
    section->info->stubs = NULL;
    section->info->nstubs = 0;
}
