cmake_minimum_required(VERSION 3.4.1)

add_library( # Sets the name of the library.
             link-lib

             # Sets the library as a shared library.
             SHARED

             Types.c

             elf/luts.c
             elf/plt.c
             elf/plt/arm.c
             elf/plt/arm64.c
             elf/got.c
             elf/reloc.c
             elf/reloc/util.c
             elf/reloc/arm.c
             elf/reloc/arm64.c

             Elf.c

             Ar.c
             Linker.c

             Hash.c
             BinaryTree.c

             debug.c

             Tests.c
             )

find_library( log-lib log )
target_link_libraries(link-lib ${log-lib})

# set C99
set_property(TARGET link-lib PROPERTY C_STANDARD 99)
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -Wall -pedantic")