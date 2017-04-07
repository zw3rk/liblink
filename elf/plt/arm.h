#ifndef LINK_ARM_H
#define LINK_ARM_H

#include "../../Types.h"

extern const size_t stub_size_arm;
bool need_stub_for_rel_arm(ElfRel * rel);
bool need_stub_for_rela_arm(ElfRela * rel);
bool make_stub_arm(Stub * s);

#endif //LINK_ARM_H