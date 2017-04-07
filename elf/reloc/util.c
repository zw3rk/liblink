#include "util.h"

int32_t
sign_extend32(uint32_t bits, uint32_t x) {
    return ((int32_t) (x << (32 - bits))) >> (32 - bits);
}

int32_t
sign_extend64(uint32_t bits, uint64_t x) {
    return ((int64_t) (x << (64 - bits))) >> (64 - bits);
}

bool
is_int32(uint32_t bits, int32_t x) {
    return bits > 32 || (-((int32_t)1 << (bits-1)) <= x
                         && x < ((int32_t)1 << (bits-1)));
}

bool
is_int64(uint32_t bits, int64_t x) {
    return bits > 64 || (-((int64_t)1 << (bits-1)) <= x
                         && x < ((int64_t)1 << (bits-1)));
}

ElfSymbolTable *
find_symbol_table(ObjectCode * oc, unsigned symolTableIndex) {
    for(ElfSymbolTable * t=oc->info->symbolTables; t != NULL; t = t->next)
        if(t->index == symolTableIndex)
            return t;
    return NULL;
}

ElfSymbol *
find_symbol(ObjectCode * oc, unsigned symbolTableIndex, unsigned long
symbolIndex) {
    ElfSymbolTable * t = find_symbol_table(oc, symbolTableIndex);
    if(NULL != t && symbolIndex < t->n_symbols) {
        return &t->symbols[symbolIndex];
    }
    return NULL;
}