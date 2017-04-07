#include <stdlib.h>
#include "reloc.h"
#include "target.h"

bool
relocate_object_code(ObjectCode * oc) {
    return ADD_SUFFIX(relocate_object_code)(oc);
}