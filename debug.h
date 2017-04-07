//
// Created by Moritz Angermann on 4/1/17.
//

#ifndef LINK_DEBUG_H
#define LINK_DEBUG_H

#include "Linker.h"

void __link_log(const char *fmt, ...);

typedef struct _OcInfo { ObjectCode * oc; Section * section; } OcInfo;

ObjectCode *
find_oc_for_GOT_addr(Linker *l, addr_t got_addr);

OcInfo
find_section(Linker * l, addr_t addr );

ElfSymbol *
find_near_symbol(Linker * l, addr_t addr);

ElfSymbol *
find_symbol_by_GOT_addr(Linker * l, addr_t got_addr);

unsigned
count_objects(Linker * l);

void
get_oc_info(char * buf, ObjectCode *oc);

ElfSymbol *
findS(addr_t addr);

void
install_sigsegv_handler();

#endif //LINK_DEBUG_H
