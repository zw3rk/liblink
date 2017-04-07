//
// Created by Moritz Angermann on 3/30/17.
//

#ifndef LINK_GOT_H
#define LINK_GOT_H

#include "../BinaryTree.h"
#include "../Types.h"
#include "../Linker.h"

bool need_got_slot(ElfSym * symbol);
bool make_got(ObjectCode * oc);
bool fill_got(Linker * l, ObjectCode * oc);
bool verify_got(Linker * l, ObjectCode * oc);
void free_got(ObjectCode * oc);

#endif //LINK_GOT_H
