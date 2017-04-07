#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include "util.h"
#include "arm.h"
#include "../compat.h"
#include "../../Types.h"
#include "../plt.h"

bool
is_blx(ElfWord w) {
    return (w >> 28) == 0xf;
}

int32_t
decodeAddend_arm(Section * section, ElfRel * rel) {
    addr_t *P = (addr_t*)(section->start + rel->r_offset);

    switch(ELF32_R_TYPE(rel->r_info)) {
        case ARM_CALL:
        case ARM_JUMP24: {
            // bl<c> <label> (Encoding A1) looks like:
            // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
            // [   cond  ]  1  0  1  1  [       imm24      ...
            //
            // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            // ...                 imm24                     ]
            //
            // imm32 = SignExtend(imm24:00,32)

            // blx <label> (Encoding A2) looks like:
            // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
            //  1  1  1  1  1  0  1  H  [       imm24      ...
            //
            // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            // ...                 imm24                     ]
            //
            // imm32 = SignExtend(imm24:H:0,32)
            //
            // assert we do not hit the 'H' case.
            assert(!(is_blx(*P) && (*P & (1<< 24))));
            uint32_t imm24 = (*P & 0x00ffffff) << 2;
            return sign_extend32(26,imm24);
        }
            /* static data relocation */
        case ARM_ABS32:
        case ARM_REL32:
        case ARM_SBREL32:
        case ARM_GOTOFF32:
        case ARM_BASE_PREL:
        case ARM_GOT_BREL:
        case ARM_BASE_ABS:
            // on linux this is static data, I believe.
        case ARM_TARGET1:

        case ARM_ABS32_NOI:
        case ARM_REL32_NOI:
        case ARM_PLT32_ABS:
        case ARM_GOT_ABS:
        case ARM_GOT_PREL:
        case ARM_TLS_GD32:
        case ARM_TLS_LDM32:
        case ARM_TLS_LDO32:
        case ARM_TLS_IE32:
        case ARM_TLS_LE32:
        case ARM_TLS_LDO12:
        case ARM_TLS_LE12:
            return (int32_t)(*P);

            /* static data relocations with non-standard size or processing */
        case ARM_ABS16:
            // TODO: size: 2, sign_extend(*P[16:0]); overflow: -32768 ≤ X ≤ 65535
        case ARM_ABS8:
            // TODO: size: 1, sign_extend(*P[8:0]); overflow: -128 ≤ X ≤ 255
        case ARM_PREL31:
            // TODO: size: 4, sign_extend(*P[30:0]); overflow: 31bit 2's complement.
            return (int32_t)(*P);
        default:
            abort();
    }
}

bool
encodeAddend_arm(Section * section, ElfRel * rel, int32_t addened) {
    addr_t P = section->start + rel->r_offset;

    switch(ELF32_R_TYPE(rel->r_info)) {
        case ARM_CALL:
        case ARM_JUMP24: {
            assert(is_int32(26, addened));
            uint32_t imm24 = (uint32_t) ((addened & 0x03fffffc) >> 2);
            *(uint32_t *)P = (   *(uint32_t *)P & 0xff000000)
                           | (            imm24 & 0x00ffffff);
            break;
        }
            /* static data relocation */
        case ARM_ABS32:
        case ARM_REL32:
        case ARM_SBREL32:
        case ARM_GOTOFF32:
        case ARM_BASE_PREL:
        case ARM_GOT_BREL:
        case ARM_BASE_ABS:
            // on linux this is static data, I believe.
        case ARM_TARGET1:

        case ARM_ABS32_NOI:
        case ARM_REL32_NOI:
        case ARM_PLT32_ABS:
        case ARM_GOT_ABS:
        case ARM_GOT_PREL:
        case ARM_TLS_GD32:
        case ARM_TLS_LDM32:
        case ARM_TLS_LDO32:
        case ARM_TLS_IE32:
        case ARM_TLS_LE32:
        case ARM_TLS_LDO12:
        case ARM_TLS_LE12:
            *(uint32_t *)P = (uint32_t) addened;
            break;
            /* static data relocations with non-standard size or processing */
        case ARM_ABS16:
            // TODO: size: 2, sign_extend(*P[16:0]); overflow: -32768 ≤ X ≤ 65535
        case ARM_ABS8:
            // TODO: size: 1, sign_extend(*P[8:0]); overflow: -128 ≤ X ≤ 255
        case ARM_PREL31:
            // TODO: size: 4, sign_extend(*P[30:0]); overflow: 31bit 2's complement.
            *(uint32_t *)P = (uint32_t) addened;
            break;
        default:
            return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

bool
relocate_object_code_arm(ObjectCode * oc) {
    // do REL relocations first. Then RelA

    for(ElfRelocationTable *relTab = oc->info->relTable;
        relTab != NULL; relTab = relTab->next) {
        /* only relocate interesting sections */
        if (SECTIONKIND_OTHER == oc->sections[relTab->targetSectionIndex].kind)
            continue;

        Section *targetSection = &oc->sections[relTab->targetSectionIndex];

        for(unsigned i=0; i < relTab->n_relocations; i++) {
            ElfRel * rel = &relTab->relocations[i];

            ElfSymbol * symbol =
                    find_symbol(oc,
                                relTab->sectionHeader->sh_link,
                                ELF32_R_SYM(rel->r_info));

            assert(symbol != NULL);

            /* Position where something is relocated */
            addr_t P     = targetSection->start + rel->r_offset;
            /* Address of the symbol */
            addr_t S     =  symbol->addr;
            /* GOT slot for the symbol */
            addr_t GOT_S = symbol->got_addr;

            assert(S);
            assert(P);

            /* thumb mode? */
            /* TODO: read thumb mode. We currently set it to off */
            uint32_t T = 0;

            unsigned type = ELF32_R_TYPE(rel->r_info);

            switch(type) {
                case ARM_NONE: {
                    abort(/* not supported */);
                }
                case ARM_PC24: {
                    /* type: deprecated, class: ARM, op: ((S + A) | T) - P */
                    abort();
                }
                case ARM_ABS32: {
                    /* type: static, class: data, op: (S + A) | T */
                    int32_t A = decodeAddend_arm(targetSection, rel);
                    int32_t V = (S + A) | T;
                    encodeAddend_arm(targetSection, rel, V);
                    break;
                }
                case ARM_REL32: {
                    /* type: static, class: data, op: ((S + A) | T) - P */
                    int32_t A = decodeAddend_arm(targetSection, rel);
                    int32_t V = ((S + A) | T) - P;
                    encodeAddend_arm(targetSection, rel, V);
                    break;
                }
                    // case ARM_LDR_PC_G0:
                    // case ARM_ABS16:
                    // case ARM_ABS12:
                    // case ARM_PLT32: /* deprecated */
                case ARM_CALL:
                case ARM_JUMP24: {
                    /* type: static, class: arm, op: ((S + A) | T) - P */
                    /* CALL *may* change to BLX when targeting Thumb        */
                    /* JUMP24 *must* use stub/veneer to transition to Thumb */

                    /* Stubs *may* be used for PC24, CALL and JUMP24
                     *             or THM_CALL, THM_JUMP24, and THM_JUMP19
                     *       and the target is STT_FUNC
                     *                      or in a different section
                     */
                    int32_t A = decodeAddend_arm(targetSection, rel);
                    S = S + A; A = 0;
                    int32_t V = ((S + A) | T) - P;
                    if(!is_int32(26, V)) { /* overflow */
                        // [Note PC bias]
                        // From the ELF for the ARM Architecture documentation:
                        // > 4.6.1.1 Addends and PC-bias compensation
                        // > A binary file may use REL or RELA relocations or a mixture of the two
                        // > (but multiple relocations for the same address must use only one type).
                        // > If the relocation is pc-relative then compensation for the PC bias (the
                        // > PC value is 8 bytes ahead of the executing instruction in ARM state and
                        // > 4 bytes in Thumb state) must be encoded in the relocation by the object
                        // > producer.
                        int32_t bias = 8;

                        S += bias;
                        /* try to locate an existing stub for this target */
                        if(find_stub(targetSection, symbol, &S)) {
                            /* didn't find any. Need to create one */
                            if(make_stub(targetSection, symbol, &S)) {
                                abort(/* failed to create stub */);
                            }
                        }
                        S -= bias;

                        V = ((S + A) | T) - P;
                        assert(is_int32(26, V)); /* X in range */
                    }
                    encodeAddend_arm(targetSection, rel, V);
                    break;
                }
                case ARM_PREL31: {
                    int32_t A = decodeAddend_arm(targetSection, rel);
                    int32_t V = ((S + A) | T) - P;
                    encodeAddend_arm(targetSection, rel, V);
                    break;
                }
                case ARM_GOT_PREL: {
                    assert(GOT_S);
                    int32_t A = decodeAddend_arm(targetSection, rel);
                    int32_t V = GOT_S + A - P;
                    encodeAddend_arm(targetSection, rel, V);
                    break;
                }
                default: {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    for(ElfRelocationATable *relaTab = oc->info->relaTable;
        relaTab != NULL; relaTab = relaTab->next) {
        /* only relocate interesting sections */
        if (SECTIONKIND_OTHER == oc->sections[relaTab->targetSectionIndex].kind)
            continue;

//        Section *targetSection = &oc->sections[relaTab->targetSectionIndex];

        for(unsigned i=0; i < relaTab->n_relocations; i++) {

            ElfRela *rel = &relaTab->relocations[i];

//            ElfSymbol *symbol =
//                    find_symbol(oc,
//                                relaTab->sectionHeader->sh_link,
//                                ELF32_R_SYM(rel->r_info));
//
//            assert(symbol != NULL);
//
//            /* Position where something is relocated */
//            addr_t P     = targetSection->start + rel->r_offset;
//            /* Address of the symbol */
//            addr_t S     = symbol->addr;
//            /* GOT slot for the symbol */
//            addr_t GOT_S = symbol->got_addr;

            switch(ELF32_R_TYPE(rel->r_info)) {
                default:
                    return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}