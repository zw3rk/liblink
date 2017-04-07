#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "Ar.h"
#include "debug.h"

typedef struct _ar_header {
    char name[16];
    char time[12];
    char own[6];
    char grp[6];
    char mode[8];
    char size[10];
    char magic[2];
} ar_header;

Object *
read_archive(FILE *ar) {
    char tmp[20];
    size_t n = 0;

    size_t member_size = 0;

    ar_header h;

    n = fread( tmp , 1, 8, ar);
    if( n != 8 ) { abort(); }

    if (strncmp(tmp, "!<arch>\n", 8) != 0) { abort(); }

    Object dummy = { .next = NULL };
    Object * objects = &dummy;


    char * symTab = NULL;
    char * nameTab = NULL;
    // load archive
    while (!feof(ar)) {

        // structure:
        // |--- name(16) --|-- mod(12) --|-- own(6) --|-- grp(6) --|-- mode(8) --|-- size(10) --|

        n = fread( &h, sizeof(ar_header), 1, ar );
        if (n != 1) {
            if (feof(ar)) { continue; }
            else { __link_log("Failed to read filename!\n"); abort(); }
        }
        for(int i = 0; i < 10; i++) {
            if(h.size[i] == ' ') {
                h.size[i] = '\0';
                break;
            }
        }

        for(int i=0; i < 16; i++) {
            if(h.name[i] == ' ') {
                h.name[i] = '\0';
                break;
            }
        }

        member_size = strtol(h.size, NULL, 10);

        if(h.magic[0] != 0x60 || h.magic[1] != 0x0A) {
            __link_log("Magic not found!\n"); abort(); }

        // BSD has #1 prefix for large file names
        // E.g. instead of <name.o>/ we have #1/<fileNameSize>
        //
        // We might also have a single '/' entry, coming first, which would
        // denote the random access table.
        //
        // '/<n>' implies that the name needs to be looked up in the symbol
        // table. with offset n.
        char fileName[64];
        memset(fileName, 0, sizeof(fileName));

        size_t fileNameSize = 0;
        if(0 == strncmp(h.name, "/", 2)) {
            /* empty name */
            assert(symTab == NULL);
            symTab = calloc(member_size, sizeof(char));
            assert(symTab != NULL);
            n = fread(symTab, 1, member_size, ar);
            if (member_size != n) {
                __link_log("Failed to read symbol table\n");
                abort();
            }
            continue;
        } else if (0 == strncmp(h.name, "//", 3)) {
            /* extended file names */
            assert(nameTab == NULL);
            nameTab = calloc(member_size, sizeof(char));
            assert(nameTab != NULL);
            n = fread(nameTab, 1, member_size, ar);
            if(member_size != n) {
                __link_log("Faield to read extended names table\n");
                abort();
            }
            /* fix \n separators */
            for(unsigned n = 0; n < member_size; n++)
                if(nameTab[n] == '\n')
                    nameTab[n] = '\0';
            continue;
        } else if(h.name[0] == '/') {
            /* gnu style extended file name */
            /* try to read the size in base 10 */
            long offset = strtol(h.name + 1, NULL, 10);
            strncpy(fileName,nameTab+offset, sizeof(fileName));
            assert(strlen(fileName) < sizeof(fileName));
        } else if(0 == strncmp(h.name, "#1/", 3)) {
            /* bsd style extended file name */
            fileNameSize = strtol(h.name+3, NULL, 10);
            n = fread(fileName, 1, fileNameSize, ar);
            if (n != fileNameSize) { __link_log("Filename could not be read!\n"); abort(); }
            member_size -= fileNameSize;
        } else {
            strncpy(fileName, h.name, sizeof(h.name));
            fileName[sizeof(h.name)] = '\0';
        }

        /* drop any trailing '/' */
        n = strnlen(fileName, sizeof(fileName));
        if(fileName[n-1] == '/') {
            fileName[n-1] = '\0';
        }

        if(0 == strncmp(fileName + strlen(fileName)-2, ".o", 2)
           || 0 == strncmp(fileName + strlen(fileName)-2, "_o", 2)) {

            Object * o = calloc(1, sizeof(Object));
            assert(o != NULL);
            o->image = calloc(member_size, sizeof(char));
            assert(o->image != NULL);
            o->size  = fread(o->image, 1, member_size, ar);
            o->name  = strdup(fileName);

            if(o->size != member_size) {
                __link_log("Failed to read image\n");
                /* TODO: Free o */
                abort();
            }

            /* append o to objects */
            Object * tmp = objects;
            while(tmp->next != NULL) tmp = tmp->next;
            tmp->next = o;

        } else {
            __link_log("Skipping non object file: %s\n", fileName);
            fseek(ar, member_size, SEEK_CUR);
        }
    }
    if(symTab != NULL) free(symTab);
    return objects->next;
}
