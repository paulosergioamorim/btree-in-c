#pragma once

typedef struct item {
    int key;
    int value;
} Item;

typedef struct btree_node {
    long offset;
    int count_keys;
    int is_leaf;
    Item *buf;
    long *children;
} BTree_Node;

typedef struct btree {
    int t;
    int M;
    int count_nodes;
    int size_node;
    long next_offset;
    long next_free;
    int fd;
    BTree_Node *root;
} BTree;

BTree *btree_init_from_db(char *path);

BTree *btree_init_from_memory(char *path, int t);

int btree_hit(BTree *btree, int key);

int btree_search(BTree *btree, int key, int *value);

void btree_insert(BTree *btree, int key, int value);

void btree_delete(BTree *btree, int key);

void btree_destroy(BTree *btree);

void btree_display(BTree *btree);

void btree_node_destroy(BTree_Node *node);
