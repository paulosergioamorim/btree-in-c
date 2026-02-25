#pragma once

#include <stdio.h>

typedef struct btree_node BTree_Node;

struct btree_node {
    int offset;
    int count_keys;
    int is_leaf;
    int *keys;
    int *values;
    int *children;
};

typedef struct btree {
    int t;
    int M;
    int count_nodes;
    int size_node;
    int next_offset;
    int next_free;
    FILE *fp;
    BTree_Node *root;
} BTree;

BTree *btree_init_from_db(char *path);

BTree *btree_init_from_memory(char *path, int t);

int btree_search(BTree *btree, int key, int *value);

void btree_insert(BTree *btree, int key, int value);

int btree_delete(BTree *btree, int key);

void btree_destroy(BTree *btree);

void btree_display(BTree *btree);

void btree_node_refresh_child(BTree *btree, BTree_Node *node, BTree_Node *x_ci, int new_i);

BTree_Node *btree_node_read_child(BTree *btree, BTree_Node *node, int i);

void btree_node_write(BTree *btree, BTree_Node *node);

void btree_node_destroy(BTree_Node *node);
