#ifndef LINK_AR_H
#define LINK_AR_H
typedef struct _object {
    char * name;
    uint8_t * image;
    unsigned size;
    struct _object * next;
} Object;

Object * read_archive(FILE *ar);
#endif //LINK_AR_H
