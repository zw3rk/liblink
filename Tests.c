#include <stdio.h>
#include "Tests.h"

#include "Linker.h"
#include "Elf.h"
#include "debug.h"

#include <libgen.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <android/log.h>

static void ___log(const char *fmt, ...)
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

bool
testSimple(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: Simple\n");

    char lib[128];     memset(lib, 0, sizeof lib);

    if(findFile(lib, sizeof(lib), "lib", "o")) abort();
    ___log("file: %s\n", lib);

    ObjectCode *o = loadObject(&LINKER, basename(lib),
                               lib);

    if(resolveObject(&LINKER, o)) abort();

    ___log("global symbols\n");
    list_global_symbols();

    int *myglob = (void*)lookupSymbol_(&LINKER, "myglob");
    if (NULL != myglob) {
        ___log("myglob: %d\n", *myglob);
    }
    int (*ml_util_func)(int) = (void*)lookupSymbol_(&LINKER, "ml_util_func");
    if (NULL != ml_util_func) {
        ___log("ml_util_func: %d\n", ml_util_func(1));
    }
    int (*ml_func)(int, int) = (void*)lookupSymbol_(&LINKER, "ml_func");
    if (NULL != ml_func) {
        ___log("ml_func: %d\n", ml_func(2, 3));
    }
    ___log("myglob: %d\n", *myglob);
    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}

bool
testMultiple(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: Multiple\n");

    char lib[128];     memset(lib, 0, sizeof lib);

    char * objects[3] = {"a","b","c"};

    ObjectCode * oc[3];

    for (unsigned i=0; i < 3; i++) {

        if(findFile(lib,sizeof(lib), objects[i], "o")) abort();
        oc[i] = loadObject(&LINKER, basename(lib), lib);
    }

    for (unsigned i=0; i < 3; i++) {
        if(resolveObject(&LINKER, oc[i])) abort();
    }

    ___log("global symbols\n");
    list_global_symbols();

// list global symbols

// find "glob".
/* in a.o */
    uint64_t *glob       = (void*)lookupSymbol_(&LINKER, "glob");
    int (*square)(int)   = (void*)lookupSymbol_(&LINKER, "square");
/* in b.o */
    void (*inc)()        = (void*)lookupSymbol_(&LINKER, "inc");
/* in c.o */
    int (*quad)(int)     = (void*)lookupSymbol_(&LINKER, "quad");
    void (*double_inc)() = (void*)lookupSymbol_(&LINKER, "double_inc");

    ___log("glob: %llu\n", *glob);
    ___log("square: %d\n", square(2));
    inc();
    ___log("inc: %llu\n", *glob);
    ___log("quad: %d\n", quad(2));
    double_inc();
    ___log("double inc: %llu\n", *glob);


    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}
//
bool
testGlobalReloc(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: Global\n");

    char lib[128];     memset(lib, 0, sizeof lib);

    if(findFile(lib, sizeof(lib), "libGlob", "a")) abort();
    ObjectCode *oc = loadArchive(&LINKER, lib);

    for(ObjectCode *o = oc; o != NULL; o=o->next)
        if(resolveObject(&LINKER, o)) abort();

    ___log("global symbols\n");
    list_global_symbols();

    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}

bool
testArchive(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: Multiple\n");

    char lib[128];     memset(lib, 0, sizeof lib);

    if(findFile(lib, sizeof(lib), "lib", "a")) abort();
    ObjectCode *oc = loadArchive(&LINKER, lib);

    for(ObjectCode *o = oc; o != NULL; o=o->next)
        if(resolveObject(&LINKER, o)) abort();

// list global symbols
    ___log("global symbols\n");
    list_global_symbols();

// find "glob".
/* in a.o */
    uint64_t *glob       = (void*)lookupSymbol_(&LINKER, "glob");
    int (*square)(int)   = (void*)lookupSymbol_(&LINKER, "square");
/* in b.o */
    void (*inc)()        = (void*)lookupSymbol_(&LINKER, "inc");
/* in c.o */
    int (*quad)(int)     = (void*)lookupSymbol_(&LINKER, "quad");
    void (*double_inc)() = (void*)lookupSymbol_(&LINKER, "double_inc");

    ___log("glob: %llu\n", *glob);
    ___log("square: %d\n", square(2));
    inc();
    ___log("inc: %llu\n", *glob);
    ___log("quad: %d\n", quad(2));
    double_inc();
    ___log("double inc: %llu\n", *glob);

    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}

bool
testRelocCounter(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: reloc counter\n");

    char lib[128];     memset(lib, 0, sizeof lib);
    if(findFile(lib, sizeof(lib), "Counter", "o")) abort();

    ObjectCode *o = loadObject(&LINKER, basename(lib),
                               lib);

    if(resolveObject(&LINKER, o)) abort();

    ___log("global symbols\n");
    list_global_symbols();

    void (*startCounter)(int64_t) = (void*)lookupSymbol_(&LINKER, "startCounter");
    if (NULL == startCounter)
        abort(/* start counter not found */);

    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}

bool
testLoadHS(finder findFile) {
    ___log("================================================================================\n");
    ___log("Test: load haskell\n");

    char charset[128]; memset(charset, 0, sizeof charset);
    char iconv[128];   memset(iconv, 0, sizeof iconv);
    char lib[128];     memset(lib, 0, sizeof lib);

    if(findFile(charset, sizeof(charset), "libcharset", "so"))
    abort();
    dlopen(charset, RTLD_NOW | RTLD_GLOBAL);
    if(findFile(iconv, sizeof(iconv), "libiconv", "so")) abort();
    dlopen(iconv, RTLD_NOW | RTLD_GLOBAL);


    char * archives[] = {
            "libgcc",
            "libCffi_thr",
            "libHSrts_thr_debug",
            "libHSghc-prim-0.5.0.0",
            "libHSinteger-simple-0.1.1.1",
            "libHSbase-4.10.0.0",
            "libHSarray-0.5.1.2",
            "libHSdeepseq-1.4.3.0",
            "libHSghc-boot-th-8.3",
            "libHSpretty-1.1.3.3",
            "libHStemplate-haskell-2.12.0.0"
    };

    for (unsigned i=0; i < sizeof(archives)/sizeof(char*); i++) {
        if(findFile(lib, sizeof(lib), archives[i], "a")) abort();
        loadArchive(&LINKER, lib);
    }

    // add exception unwinding symbols.

    GlobalSymbol exidx_start = {
            .oc = NULL,
            .symbol = &(ElfSymbol) {
                    .elf_sym = NULL,
                    .name = "__exidx_start",
                    .hash = hash("__exidx_start"),
                    .addr = 0x1,
                    .got_addr = 0x0
            },
            .is_weak = false,
            .next = NULL
    };
    GlobalSymbol exidx_end = {
            .oc = NULL,
            .symbol = &(ElfSymbol) {
                    .elf_sym = NULL,
                    .name = "__exidx_end",
                    .hash = hash("__exidx_end"),
                    .addr = 0x1,
                    .got_addr = 0x0
            },
            .is_weak = false,
            .next = NULL
    };

    // can't locate atexit at runtime on arm64.
    GlobalSymbol atexit_ = {
            .oc = NULL,
            .symbol = &(ElfSymbol) {
                    .elf_sym = NULL,
                    .name = "atexit",
                    .hash = hash("atexit"),
                    .addr = (addr_t)&atexit,
                    .got_addr = 0x0
            },
            .is_weak = false,
            .next = NULL
    };
    insert_global_symbol(&LINKER, &exidx_start);
    insert_global_symbol(&LINKER, &exidx_end);
    insert_global_symbol(&LINKER, &atexit_);

    unsigned n = count_objects(&LINKER);
    ___log("Loaded %d objects in total.\n", n);
    for(ObjectCode *o = LINKER.objects; o != NULL; o=o->next) {
        if (resolveObject(&LINKER, o))
            abort();
    }
    sleep(1);
    n = count_objects(&LINKER);
    ___log("Loaded %d objects in total after relocation.\n", n);
    sleep(1);

    void (*hs_init)(int *argc, char **argv[]) = (void*)lookupSymbol_(&LINKER, "hs_init");
    if(hs_init == NULL) abort();

    void (*mcpy)(void *, const void *, size_t) = (void*)lookupSymbol_(&LINKER, "memcpy");
    if(mcpy == NULL) abort();

// try to load fib.
    if(findFile(lib, sizeof(lib), "Counter", "o")) abort();
    ObjectCode *o = loadObject(&LINKER, basename(lib), lib);

    if(resolveObject(&LINKER, o)) abort();

    void
    (*setLineBuffering)(void) = (void*)lookupSymbol_(&LINKER, "setLineBuffering");
    void
    (*startCounter)(int64_t) = (void*)lookupSymbol_(&LINKER, "startCounter");
    void (*helloWorld)(void) = (void*)lookupSymbol_(&LINKER, "helloWorld");
    if(startCounter == NULL) abort();
    if(helloWorld   == NULL) abort();


    // try to boot the runtime.
//    char * argv[] = { "link", "+RTS", "-DS", "-Ds", "-Da", "-Di", "-RTS" };
    char ** argv = (char*[]){ "link" };
//    char * argv[] = { "link" };
//    char ** argv_ = argv;
    int argc = sizeof(argv)/sizeof(char*);

    hs_init(&argc, &argv);

    // Run test code
    setLineBuffering();

    helloWorld();
    startCounter(3);

    ___log("================================================================================\n");
    return EXIT_SUCCESS;
}
