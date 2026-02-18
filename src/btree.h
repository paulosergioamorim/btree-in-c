#pragma once

#include <stddef.h>

typedef struct btree_node BTree_Node;

typedef struct key_value {
    int key;
    int value;
    BTree_Node *pred;
} Key_Value;

struct btree_node {
    size_t count_keys;
    int is_leaf;
    Key_Value *buf;
    BTree_Node *post;
};

typedef struct btree {
    size_t t;
    size_t M;
    size_t count_nodes;
    BTree_Node *root;
} BTree;

BTree *btree_init(int t);

int btree_search(BTree *btree, int key, int *value);

int btree_insert(BTree *btree, int key, int value);

int btree_delete(BTree *btree, int key);

BTree *btree_destroy(BTree *btree);

