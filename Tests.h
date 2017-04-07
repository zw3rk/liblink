//
// Created by Moritz Angermann on 3/29/17.
//

#ifndef LINK_TESTS_H
#define LINK_TESTS_H

#include <stdbool.h>

typedef bool (*finder)(char *, size_t, char *, char *);

bool  testSimple(finder f);
bool  testMultiple(finder f);
bool  testGlobalReloc(finder f);
bool  testArchive(finder f);
bool  testRelocCounter(finder f);
bool  testLoadHS(finder f);

#endif //LINK_TESTS_H
