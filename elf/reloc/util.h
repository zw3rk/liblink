#ifndef LINK_UTIL_H
#define LINK_UTIL_H

#include <stdint.h>
#include <stdbool.h>
#include "../../Types.h"

int32_t sign_extend64(uint32_t bits, uint64_t x);

/**
 *
 * @param bits the values number of bits.
 * @param x the value to be extended.
 * @return 32bit int with the same value as x.
 */
int32_t sign_extend32(uint32_t bits, uint32_t x);

bool is_int64(uint32_t bits, int64_t x);

/**
 *
 * @param bits the number of bits to fit the value into.
 * @param x the vlaue to be tested.
 * @return true if the value can fit into bits, otherwise false.
 */
bool is_int32(uint32_t bits, int32_t x);

ElfSymbolTable * find_symbol_table(ObjectCode * oc,
                                   unsigned symbolTableIndex);

ElfSymbol * find_symbol(ObjectCode * oc,
                        unsigned symbolTableIndex,
                        unsigned long symbolIndex);
#endif //LINK_UTIL_H
