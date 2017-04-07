#ifndef LINK_TARGET_H
#define LINK_TARGET_H

#define GCC_VERSION (__GNUC__ * 10000 \
                     + __GNUC_MINOR__ * 100 \
                     + __GNUC_PATCHLEVEL__)

#if GCC_VERSION > 30900
#define __gcc__
#endif

#include <elf.h>
#include <stdint.h>

#if defined(__clang__) || defined(__gcc__)
#if defined(__x86_64__) || defined(__aarch64__) || defined(__mips64__)
typedef Elf64_Ehdr ElfEhdr;
typedef Elf64_Phdr ElfPhdr;
typedef Elf64_Shdr ElfShdr;

typedef Elf64_Addr ElfAddr;
typedef Elf64_Sym  ElfSym;
typedef Elf64_Rel  ElfRel;
typedef Elf64_Rela ElfRela;

typedef Elf64_Word ElfWord;
typedef Elf64_Half ElfHalf;

typedef uint64_t addr_t;
#elif defined(__i386__) || defined(__arm__) || defined(__mips__)
typedef Elf32_Ehdr ElfEhdr;
typedef Elf32_Phdr ElfPhdr;
typedef Elf32_Shdr ElfShdr;

typedef Elf32_Addr ElfAddr;
typedef Elf32_Sym  ElfSym;
typedef Elf32_Rel  ElfRel;
typedef Elf32_Rela ElfRela;

typedef Elf32_Word ElfWord;
typedef Elf32_Half ElfHalf;

typedef uint32_t addr_t;
#else
#error "unknown architecture"
#endif

#if defined(__x86_64__)
#define __suffix__ x86_64
#elif defined(__aarch64__)
#define __suffix__ arm64
#elif defined(__mips64__)
#define __suffix__ mips64
#elif defined(__i386__)
#define __suffix__ x86
#elif defined(__arm__)
#define __suffix__ arm
#elif defined(__mips__)
#define __suffix__ mips
#else
#error "unknown architecture"
#endif

#define PASTE(x,y) x ## y
#define EVAL(x,y) PASTE(x,y)
#define ADD_SUFFIX(x) EVAL(PASTE(x,_),__suffix__)

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#else /* not __clang__ || __gcc__ */
#error "unsupported compiler"
#endif

#endif //LINK_TARGET_H
