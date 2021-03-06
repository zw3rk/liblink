#include <stdlib.h>
#include <assert.h>
#include "arm64.h"
#include "util.h"
#include "../plt.h"
#include "../../debug.h"

#define Page(x) ((x) & ~0xFFF)

bool isBranch(addr_t p) {
    return (*(addr_t*)p & 0xFC000000) == 0x14000000;
}

bool isBranchLink(addr_t p) {
    return (*(addr_t*)p & 0xFC000000) == 0x94000000;
}

bool isAdrp(addr_t p) {
    return (*(addr_t*)p & 0x9F000000) == 0x90000000;
}

bool isLoadStore(addr_t p) {
    return (*(addr_t*)p & 0x3B000000) == 0x39000000;
}
bool isAddSub(addr_t p) {
    return (*(addr_t*)p & 0x11C00000) == 0x11000000;
}
bool isVectorOp(addr_t p) {
    return (*(addr_t*)p & 0x04800000) == 0x04800000;
}

/* instructions are 32bit */
typedef uint32_t inst_t;

int64_t
decodeAddend_arm64(Section * section __attribute__((unused)),
                   ElfRel * rel __attribute__((unused)))
{
    abort(/* we don't support Rel locations yet. */);
}

bool
encodeAddend_arm64(Section * section, ElfRel * rel, int64_t addend) {
    /* instructions are 32bit! */
    addr_t P = section->start + rel->r_offset;
    int exp_shift = -1;
    switch(ELF64_R_TYPE(rel->r_info)) {
        /* static misc relocations */
        /* static data relocations */
        case AARCH64_ABS64:
        case AARCH64_PREL64:
            *(uint64_t*)P = (uint64_t)addend;
            break;
        case AARCH64_ABS32:
            assert(is_int64(32, addend));
        case AARCH64_PREL32:
            assert(is_int64(32, addend));
            *(uint32_t*)P = (uint32_t)addend;
            break;
        case AARCH64_ABS16:
            assert(is_int64(16, addend));
        case AARCH64_PREL16:
            assert(is_int64(16, addend));
            *(uint16_t*)P = (uint16_t)addend;
            break;
        /* static aarch64 relocations */
        /* - pc relative relocations */
        case AARCH64_ADR_PREL_PG_HI21: {
            // adrp <Xd>, <label> looks like:
            // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
            //  1 [ lo]  1  0  0  0 [            hi        ...
            //
            // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            // ...              hi            ] [     Rd     ]
            //
            // imm64 = SignExtend(hi:lo:0x000,64)
            assert(is_int64(32, addend));
            assert((addend & 0xfff) == 0); /* page relative */

            *(inst_t *)P = (*(inst_t *)P & 0x9f00001f)
                           | (inst_t) (((uint64_t) addend << 17) & 0x60000000)
                           | (inst_t) (((uint64_t) addend >> 9) & 0x00ffffe0);
            break;
        }
        /* - control flow relocations */
        case AARCH64_JUMP26:   /* relocate b ... */
        case AARCH64_CALL26: { /* relocate bl ... */
            assert(is_int64(26+2, addend)); /* X in range */
            *(inst_t *)P = (*(inst_t *)P & 0xfc000000) /* keep upper 6 (32-6)
 * bits */
                         | ((uint32_t)(addend >> 2) & 0x03ffffff);
            break;
        }
        case AARCH64_ADR_GOT_PAGE: {

            assert(is_int64(32, addend)); /* X in range */
            assert((addend & 0xfff) == 0); /* page relative */

            *(inst_t *)P = (*(inst_t *)P & 0x9f00001f)
               | (inst_t)(((uint64_t)addend << 17) & 0x60000000)  // lo
               | (inst_t)(((uint64_t)addend >> 9)  & 0x00ffffe0); // hi
            break;
        }
        case AARCH64_ADD_ABS_LO12_NC: {
            // add <Xd>, <Xn>, #imm looks like:
            // 31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16
            // sf  0  0  1  0  0  0  1 [ sh] [    imm12    ...
            //
            // 15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0
            // ...   imm12     ] [     Rn     ] [    Rd      ]

            /* fall through */
        }
        case AARCH64_LDST8_ABS_LO12_NC:
            if(exp_shift == -1) exp_shift = 0;
        case AARCH64_LDST16_ABS_LO12_NC:
            if(exp_shift == -1) exp_shift = 1;
        case AARCH64_LDST32_ABS_LO12_NC:
            if(exp_shift == -1) exp_shift = 2;
        case AARCH64_LDST64_ABS_LO12_NC:
            if(exp_shift == -1) exp_shift = 3;
        case AARCH64_LDST128_ABS_LO12_NC:
            if(exp_shift == -1) exp_shift = 4;
        case AARCH64_LD64_GOT_LO12_NC: {
            if(exp_shift == -1) {
                assert( (addend & 7) == 0 );
                exp_shift = 3;
            }
            assert((addend & 0xfff) == addend);
            int shift = 0;
            if(isLoadStore(P)) {
                /* bits 31, 30 encode the size. */
                shift = (*(inst_t*)P >> 30) & 0x3;
                if(0 == shift && isVectorOp(P)) {
                    shift = 4;
                }
            }
            assert(addend == 0 || exp_shift == shift);
            *(inst_t *)P = (*(inst_t *)P & 0xffc003ff)
               | ((inst_t)(addend >> shift << 10) & 0x003ffc00);
            break;
        }
        default:
            abort();
    }
    return EXIT_SUCCESS;
}


/**
 * Compute the *new* addend for a relocation, given a pre-existing addend.
 * @param section The section the relocation is in.
 * @param rel     The Relocation struct.
 * @param symbol  The target symbol.
 * @param addend  The existing addend. Either explicit or implicit.
 * @return The new computed addend.
 */
static int64_t
compute_addend(Section * section, ElfRel * rel,
               ElfSymbol * symbol, int64_t addend) {

    /* Position where something is relocated */
    addr_t P = (section->start +
                rel->r_offset);

    assert(0x0 != P);
    assert((uint64_t)section->start <= P);
    assert(P <= (uint64_t)section->start + section->size);
    /* Address of the symbol */
    addr_t S = (addr_t) symbol->addr;
    assert(0x0 != S);
    /* GOT slot for the symbol */
    addr_t GOT_S = (addr_t) symbol->got_addr;

    int64_t A = addend;

    switch(ELF64_R_TYPE(rel->r_info)) {
        case AARCH64_ABS64: /* type: static, class: data, op: S + A; overflow: none */
        case AARCH64_ABS32: /* type: static, class: data, op: S + A; overflow: int32 */
        case AARCH64_ABS16: /* type: static, class: data, op: S + A; overflow: int16 */
            return S + A;
        case AARCH64_PREL64: /* type: static, class: data, op: S + A - P; overflow: none */
        case AARCH64_PREL32: /* type: static, class: data, op: S + A - P; overflow: int32 */
        case AARCH64_PREL16: /* type: static, class: data, op: S + A - P; overflow: int16 */
            return S + A - P;
        case AARCH64_ADR_PREL_PG_HI21:
            /* type: static, class: aarch64, op: Page(S + A) - Page(P);
             * overflow: int32 */
            return Page(S + A) - Page(P);
        case AARCH64_ADD_ABS_LO12_NC: /* type: static, class: aarch64, op: S + A */
            return (S + A) & 0xfff;
        case AARCH64_JUMP26:
        case AARCH64_CALL26: {
            // S+A-P
            int64_t V = S + A - P;
            /* note: we are encoding bits [27:2] */
            if(!is_int64(26+2, V)) {
                // [Note PC bias arm64]
                // There is no PC bias to accommodate in the
                // relocation of a place containing an instruction
                // that formulates a PC-relative address. The program
                // counter reflects the address of the currently
                // executing instruction.

                /* need a stub */
                /* check if we already have that stub */
                if(find_stub(section, symbol, &S)) {
                    /* did not find it. Crete a new stub. */
                    if(make_stub(section, symbol, &S)) {
                        abort(/* could not find or make stub */);
                    }
                }
                __link_log("\tPLT Needed to relocate %s (%p) via stub; "
                                   "new address: %p!\n",
                           symbol->name, symbol->addr, (void *) S);

                assert(0 == (0xffff000000000000 & S));
                V = S + A - P;
                assert(is_int64(26+2, V)); /* X in range */
            }
            return V;
        }
        case AARCH64_LDST128_ABS_LO12_NC: assert(0 == ((S+A) & 0x0f));
        case AARCH64_LDST64_ABS_LO12_NC:  assert(0 == ((S+A) & 0x07));
        case AARCH64_LDST32_ABS_LO12_NC:  assert(0 == ((S+A) & 0x03));
        case AARCH64_LDST16_ABS_LO12_NC:  assert(0 == ((S+A) & 0x01));
        case AARCH64_LDST8_ABS_LO12_NC: /* type: static, class: aarch64, op: S + A */
            return (S + A) & 0xfff;

        case AARCH64_ADR_GOT_PAGE: {
            // Page(G(GDAT(S+A))) - Page(P)
            // Set the immediate value of an ADRP to bits [32:12] of X;
            // check that –2^32 ≤ X < 2^32
            // NOTE: we'll do what seemingly everyone else does, and
            //       reduce this to Page(GOT(S)+A) - Page(P)
            // TODO: fix this story proper, so that the transformation
            //       makes sense without resorting to, everyone else
            //       does it like this as well.
            assert(0x0 != GOT_S);
            return Page(GOT_S+A) - Page(P);
        }
        case AARCH64_LD64_GOT_LO12_NC: {
            // G(GDAT(S+A))
            assert(0x0 != GOT_S);
            return (GOT_S + A) & 0xfff;
        }
        default:
            abort(/* unhandled rel */);
    }
}

bool
relocate_object_code_arm64(ObjectCode * oc) {
    for(ElfRelocationTable *relTab = oc->info->relTable;
        relTab != NULL; relTab = relTab->next) {
        /* only relocate interesting sections */
        if (SECTIONKIND_OTHER == oc->sections[relTab->targetSectionIndex].kind)
            continue;

        Section *targetSection = &oc->sections[relTab->targetSectionIndex];

        for (unsigned i = 0; i < relTab->n_relocations; i++) {
            ElfRel *rel = &relTab->relocations[i];

            ElfSymbol *symbol =
                    find_symbol(oc,
                                relTab->sectionHeader->sh_link,
                                ELF64_R_SYM((Elf64_Xword)rel->r_info));

            assert(symbol != NULL);

            /* decode implicit addend */
            int64_t addend = decodeAddend_arm64(targetSection, rel);

            addend = compute_addend(targetSection, rel, symbol, addend);
            encodeAddend_arm64(targetSection, rel, addend);
        }
    }
    for(ElfRelocationATable *relaTab = oc->info->relaTable;
        relaTab != NULL; relaTab = relaTab->next) {
        /* only relocate interesting sections */
        if (SECTIONKIND_OTHER == oc->sections[relaTab->targetSectionIndex].kind)
            continue;

        Section *targetSection = &oc->sections[relaTab->targetSectionIndex];

        char ocbuf[256]; memset(ocbuf, 0, sizeof(ocbuf));
        get_oc_info(ocbuf, oc);
        __link_log("%s: Processing %d relocations for section %s...\n", ocbuf,
                   relaTab->n_relocations, targetSection->info->name);

        for(unsigned i=0; i < relaTab->n_relocations; i++) {

            ElfRela *rel = &relaTab->relocations[i];

            ElfSymbol *symbol =
                    find_symbol(oc,
                                relaTab->sectionHeader->sh_link,
                                ELF64_R_SYM((Elf64_Xword)rel->r_info));

            assert(0x0 != symbol);

            /* take explicit addend */
            int64_t addend = rel->r_addend;

            addend = compute_addend(targetSection, (ElfRel*)rel,
                                    symbol, addend);
            encodeAddend_arm64(targetSection, (ElfRel*)rel, addend);
        }
    }
    return EXIT_SUCCESS;
}