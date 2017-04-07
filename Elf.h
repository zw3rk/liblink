#ifndef LINK_ELF_H
#define LINK_ELF_H

#include <unistd.h>
#include <stdbool.h>
#include "Types.h"
#include "BinaryTree.h"

ObjectCode *
loadObject(Linker * l, char * name, char * path);

ObjectCode *
loadArchive(Linker * l, char * path);

ObjectCode *
load_object( char * name, uint8_t * image);

bool
resolveObject(Linker * l, ObjectCode * oc);

/* Prototypes */
bool
load_sections(ObjectCode * oc);

bool
get_names(Linker *l, ObjectCode * oc);

bool
mprotect_object_code(ObjectCode * oc);

struct global_symbol *
read_global_symbols(Linker l, ObjectCode * oc);

#endif //LINK_ELF_H
