//
//  BinaryTree.c
//  mach-o
//
//  Created by Moritz Angermann on 2/25/17.
//  Copyright © 2017 Moritz Angermann. All rights reserved.
//

#include "BinaryTree.h"
#include <stdlib.h>

void
binary_tree_insert(binary_tree_node ** root, hash_t key, void * value)
{
    binary_tree_node * node = calloc(1, sizeof(binary_tree_node));
    
    node->key = key;
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    
    if(*root == NULL) {
        *root = node;
        return;
    }
    binary_tree_node * leaf = *root;
    while(1) {
        if(leaf->key > key) {
            if(leaf->left == NULL) {
                leaf->left = node;
                return;
            } else {
                leaf = leaf->left;
            }
        } else if (leaf->key < key) {
            if(leaf->right == NULL) {
                leaf->right = node;
                return;
            } else {
                leaf = leaf->right;
            }
        } else {
            abort(/* duplicate key */);
        }
    }
}

bool
binary_tree_lookup(binary_tree_node * root, hash_t key, void ** value)
{
    binary_tree_node * leaf = root;
    while(leaf != NULL) {
        if(leaf->key == key) { *value = leaf->value; return EXIT_SUCCESS; }
        else if(leaf->key > key) leaf = leaf->left;
        else if(leaf->key < key) leaf = leaf->right;
    }
    return EXIT_FAILURE;
}
