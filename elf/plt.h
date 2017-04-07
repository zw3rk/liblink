#include "../Types.h"
#include "plt/arm.h"
#include "plt/arm64.h"

#ifndef LINK_PLT_H
#define LINK_PLT_H
unsigned  numberOfStubsForSection( ObjectCode *oc, unsigned sectionIndex);

#define STUB_SIZE          ADD_SUFFIX(stub_size)

bool find_stub(Section * section, ElfSymbol * symbol, addr_t * addr);
bool make_stub(Section * section, ElfSymbol * symbol, addr_t * addr);

void free_stubs(Section * section);

#endif //LINK_PLT_H
