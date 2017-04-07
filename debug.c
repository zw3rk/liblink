//
// Created by Moritz Angermann on 4/1/17.
//

#include <asm/siginfo.h>
#include <signal.h>
#include <stdlib.h>
//#include <unwind.h>
#include <dlfcn.h>
#include <android/log.h>

#include "debug.h"
#include "Types.h"
#include "Linker.h"

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


/* stack trace logic */

/*
struct BacktraceState
{
    void** current;
    void** end;
    void* pc;
};

_Unwind_Reason_Code unwindCallback(struct _Unwind_Context* context, void* arg)
{
    struct BacktraceState* state = (struct BacktraceState*)arg;
    state->pc = (void*)_Unwind_GetIP(context);
    if (state->pc)
    {
        if (state->current == state->end)
            return _URC_END_OF_STACK;
        else
            *state->current++ = (void*)state->pc;
    }
    return _URC_NO_REASON;
}
size_t
android_print_stackTrace(void** addrs, size_t max, void * pc)
{
    struct BacktraceState state = {addrs, addrs + max, pc};
    _Unwind_Backtrace(unwindCallback, &state);

    return state.current - addrs;
}
#define MAX_STACK_FRAMES 64
static void *stack_traces[MAX_STACK_FRAMES];
void posix_print_stack_trace() {
    int i, trace_size = 0;
    char **messages = (char **) NULL;

}

//void show_backtrace (void) {
//    unw_cursor_t cursor; unw_context_t uc;
//    unw_word_t ip, sp;
//
//    unw_getcontext(&uc);
//    unw_init_local(&cursor, &uc);
//    while (unw_step(&cursor) > 0) {
//        unw_get_reg(&cursor, UNW_REG_IP, &ip);
//        unw_get_reg(&cursor, UNW_REG_SP, &sp);
//        printf ("ip = %lx, sp = %lx\n", (long) ip, (long) sp);
//    }
//}

void posix_signal_handler(int sig, siginfo_t *siginfo, void *ctx)
{
    switch(sig)
    {
        case SIGSEGV:
            fputs("Caught SIGSEGV: Segmentation Fault\n", stderr);
            break;
        case SIGINT:
            fputs("Caught SIGINT: Interactive attention signal, (usually ctrl+c)\n",
                  stderr);
            break;
        case SIGFPE:
            switch(siginfo->si_code)
            {
                case FPE_INTDIV:
                    fputs("Caught SIGFPE: (integer divide by zero)\n", stderr);
                    break;
                case FPE_INTOVF:
                    fputs("Caught SIGFPE: (integer overflow)\n", stderr);
                    break;
                case FPE_FLTDIV:
                    fputs("Caught SIGFPE: (floating-point divide by zero)\n", stderr);
                    break;
                case FPE_FLTOVF:
                    fputs("Caught SIGFPE: (floating-point overflow)\n", stderr);
                    break;
                case FPE_FLTUND:
                    fputs("Caught SIGFPE: (floating-point underflow)\n", stderr);
                    break;
                case FPE_FLTRES:
                    fputs("Caught SIGFPE: (floating-point inexact result)\n", stderr);
                    break;
                case FPE_FLTINV:
                    fputs("Caught SIGFPE: (floating-point invalid operation)\n", stderr);
                    break;
                case FPE_FLTSUB:
                    fputs("Caught SIGFPE: (subscript out of range)\n", stderr);
                    break;
                default:
                    fputs("Caught SIGFPE: Arithmetic Exception\n", stderr);
                    break;
            }
        case SIGILL:
            switch(siginfo->si_code)
            {
                case ILL_ILLOPC:
                    fputs("Caught SIGILL: (illegal opcode)\n", stderr);
                    break;
                case ILL_ILLOPN:
                    fputs("Caught SIGILL: (illegal operand)\n", stderr);
                    break;
                case ILL_ILLADR:
                    fputs("Caught SIGILL: (illegal addressing mode)\n", stderr);
                    break;
                case ILL_ILLTRP:
                    fputs("Caught SIGILL: (illegal trap)\n", stderr);
                    break;
                case ILL_PRVOPC:
                    fputs("Caught SIGILL: (privileged opcode)\n", stderr);
                    break;
                case ILL_PRVREG:
                    fputs("Caught SIGILL: (privileged register)\n", stderr);
                    break;
                case ILL_COPROC:
                    fputs("Caught SIGILL: (coprocessor error)\n", stderr);
                    break;
                case ILL_BADSTK:
                    fputs("Caught SIGILL: (internal stack error)\n", stderr);
                    break;
                default:
                    fputs("Caught SIGILL: Illegal Instruction\n", stderr);
                    break;
            }
            break;
        case SIGTERM:
            fputs("Caught SIGTERM: a termination request was sent to the program\n",
                  stderr);
            break;
        case SIGABRT:
            fputs("Caught SIGABRT: usually caused by an abort() or assert()\n", stderr);
            break;
        default:
            break;
    }
    struct ucontext * context = (struct ucontext*)ctx;
    unsigned long PC = _Unwind_GetIP(context);
//#if defined(__aarch64__)
//    unsigned long PC = context->uc_mcontext.pc;
//    unsigned long SP = context->uc_mcontext.sp;
//#else
//    unsigned long PC = 0;
//    unsigned long SP = 0;
//#endif


    const size_t maxNumAddresses = 50;
    void* addresses[maxNumAddresses];

    Dl_info info;

    size_t n = android_print_stackTrace(addresses, maxNumAddresses, (void*)PC);

    exit(EXIT_FAILURE);
}


void
install_sigsegv_handler()
{
    static struct sigaction sa;

    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = posix_signal_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        abort();
}
*/