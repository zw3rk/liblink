#include <asm/siginfo.h>
#include <signal.h>
#include <android/log.h>

#include "debug.h"

void __link_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
#if defined(__ANDROID__)
    __android_log_vprint(ANDROID_LOG_FATAL, "SERV", fmt, ap);
#else
    vprintf(fmt, ap);
#endif
    va_end(ap);
}

ObjectCode *
find_oc_for_GOT_addr(Linker *l, addr_t got_addr) {
    for(ObjectCode * oc = l->objects; oc != NULL; oc = oc->next)
        if (oc->info->got_size > 0
            && oc->info->got_start <=  got_addr
            && got_addr < oc->info->got_start + oc->info->got_size)
            return oc;
    return NULL;
}

OcInfo
find_section(Linker * l, addr_t addr ) {
    OcInfo r = { .oc = NULL, .section = NULL };
    for(ObjectCode * oc = l->objects; oc != NULL; oc = oc->next) {
        for(size_t i=0; i < oc->n_sections; i++) {
            if(oc->sections[i].start <= addr &&
               addr < oc->sections[i].start + oc->sections[i].size) {
                r.oc = oc; r.section = &oc->sections[i];
                return r;
            }
        }
    }
    return r;
}

ElfSymbol *
find_near_symbol(Linker * l, addr_t addr) {
    OcInfo info = find_section(l, addr);

    ObjectCode * oc = info.oc;

    if(oc == NULL)
        return NULL;

    for(ElfSymbolTable * stab=oc->info->symbolTables;
        stab != NULL; stab = stab->next) {
        for(unsigned i = 0; i < stab->n_symbols; i++) {
            ElfSymbol * s = &stab->symbols[i];
            if(s->elf_sym->st_size == 0) continue;

            if(s->addr <= addr
                && (addr < s->addr + s->elf_sym->st_size))
                return s;
        }
    }
    return NULL;
}

ElfSymbol *
find_symbol_by_GOT_addr(Linker * l, addr_t got_addr) {
    for(ObjectCode *oc=l->objects; oc != NULL; oc = oc->next) {
        for (ElfSymbolTable *stab = oc->info->symbolTables;
             stab != NULL; stab = stab->next) {
            for (unsigned i = 0; i < stab->n_symbols; i++) {
                ElfSymbol *s = &stab->symbols[i];
                if (s->elf_sym->st_size == 0) continue;
                if (s->got_addr == got_addr)
                    return s;
            }
        }
    }
    return NULL;
}

unsigned
count_objects(Linker * l) {
    unsigned i = 0;
    for(ObjectCode * oc = l->objects; oc != NULL; oc = oc->next)
        i++;
    return i;
}

void
get_oc_info(char * buf, ObjectCode *oc) {
    if(oc->archiveMemberName) {
        sprintf(buf, "%s (%s)", oc->fileName, oc->archiveMemberName);
    } else {
        sprintf(buf, "%s", oc->fileName);
    }
}

ElfSymbol *
findS(addr_t addr) {
    return find_near_symbol(&LINKER, addr);
}