#ifndef LINK_ARM64_H
#define LINK_ARM64_H

#include "../../Types.h"

extern const size_t stub_size_arm64;
bool need_stub_for_rel_arm64(ElfRel * rel);
bool need_stub_for_rela_arm64(ElfRela * rel);
bool make_stub_arm64(Stub * s);

#endif //LINK_ARM64_H
