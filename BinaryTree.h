#ifndef BinaryTree_h
#define BinaryTree_h

#include <stdio.h>
#include <stdbool.h>
#include "Hash.h"

typedef struct _binary_tree_node {
    hash_t key;
    void * value;
    struct _binary_tree_node * left;
    struct _binary_tree_node * right;
} binary_tree_node;

void
binary_tree_insert(binary_tree_node ** root, hash_t key, void * value);

bool
binary_tree_lookup(binary_tree_node * root, hash_t key, void ** value);


#endif /* BinaryTree_h */
