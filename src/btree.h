#pragma once

#include <stddef.h>

typedef struct btree_node BTree_Node;

struct btree_node {
    int count_keys;
    int is_leaf;
    int *keys;
    int *values;
    BTree_Node **children;
};

typedef struct btree {
    int t;
    int M;
    int count_nodes;
    BTree_Node *root;
} BTree;

BTree *btree_init(int t);

int btree_search(BTree *btree, int key, int *value);

void btree_insert(BTree *btree, int key, int value);

int btree_delete(BTree *btree, int key);

BTree *btree_destroy(BTree *btree);

void btree_display(BTree *btree);
