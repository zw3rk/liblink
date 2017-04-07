#include "Hash.h"
#include <string.h>
#include <stdlib.h>

uint32_t jenkins_one_at_a_time_hash(const char* key, size_t length) {
    size_t i = 0;
    uint32_t hash = 0;
    while (i != length) {
        hash += key[i++];
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

uint64_t knuth_hash(const char* key, size_t length) {
    uint64_t hashedValue = 3074457345618258791ul;
    for(size_t i=0; i < length; i++)
    {
        hashedValue += (uint8_t)key[i];
        hashedValue *= 3074457345618258799ul;
    }
    return hashedValue;
}

#define KNUTH 1

hash_t hash(const char * key)
{
#if JENKINS
    return jenkins_one_at_a_time_hash(key, strlen(key));
#endif
#if KNUTH
    return knuth_hash(key, strlen(key));
#endif
    abort(/* no hash function defined */);
}
