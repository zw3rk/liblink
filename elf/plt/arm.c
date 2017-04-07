//
// Created by Moritz Angermann on 4/3/17.
//

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "arm.h"

/* three 4 byte instructions */
const size_t stub_size_arm = 12;

/*
 * Compute the number of stub (PLT entries) for a given section by iterating
 * over the relocations and relocations with explicit addend and counting those
 * relocations that might require a PLT relocation.
 *
 * This will be an upper bound, and we might not use all stubs.  However by
 * calculating the number of potential stubs beforehand, we can allocate enough
 * space adjacent to the section, such that the PLT is rather close to the
 * section, and the risk of the stubs being out of reach for the instruction to
 * be relocated is minimal.
 */
bool need_stub_for_rel_arm(ElfRel * rel) {
    switch(ELF32_R_TYPE(rel->r_info)) {
        case ARM_PC24:
        case ARM_CALL:
        case ARM_JUMP24:
        case ARM_THM_CALL:
        case ARM_THM_JUMP24:
        case ARM_THM_JUMP19:
            return true;
        default:
            return false;
    }
}
bool need_stub_for_rela_arm(ElfRela * rela) {
    switch(ELF32_R_TYPE(rela->r_info)) {
        case ARM_PC24:
        case ARM_CALL:
        case ARM_JUMP24:
        case ARM_THM_CALL:
        case ARM_THM_JUMP24:
        case ARM_THM_JUMP19:
            return true;
        default:
            return false;
    }
}

bool
make_stub_arm(Stub * s) {

    // We (the linker) may corrupt r12 (ip) according to the "ELF for the ARM
    // Architecture" reference.

    // movw<c> <Rd>, #<imm16> (Encoding A2) looks like:
    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
    // [   cond  ]  0  0  1  1  0  0  0  0 [   imm4  ]
    //
    // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // [    Rd   ] [              imm12              ]
    //
    // movt<c> <Rd>, #<imm16> (Encoding A1) looks like:
    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
    // [   cond  ]  0  0  1  1  0  1  0  0 [   imm4  ]
    //
    // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    // [    Rd   ] [              imm12              ]
    //
    // bx<c> <Rd> (Encoding A1) looks like:
    // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
    // [   cond  ]  0  0  0  1  0  0  1  0  1  1  1  1
    //
    // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
    //  1  1  1  1  1  1  1  1  0  0  0  1  [    Rd   ]
    //
    // The difference for the movw and movt is only bit 22.
    // We'll use 0b1110 for the condition.

    uint32_t movw_r12 = 0xe300c000;
    uint32_t movt_r12 = 0xe340c000;
    uint32_t bx_r12   = 0xe12fff1c;

    *((uint32_t*)s->addr+0) = movw_r12
                              | (((uint32_t )s->target & 0xf000) << 4)
                              |  ((uint32_t )s->target & 0x0fff);
    *((uint32_t*)s->addr+1) = movt_r12
                              | ((((uint32_t )s->target >> 16) & 0xf000) << 4)
                              |  (((uint32_t )s->target >> 16) & 0x0fff);
    *((uint32_t*)s->addr+2) = bx_r12;

    return EXIT_SUCCESS;
}
