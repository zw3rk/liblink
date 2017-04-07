#ifndef LINK_RELOC_H
#define LINK_RELOC_H

#include "../Types.h"
#include "../BinaryTree.h"
#include "reloc/arm.h"
#include "reloc/arm64.h"

bool
relocate_object_code(ObjectCode * oc);

#endif //LINK_RELOC_H
