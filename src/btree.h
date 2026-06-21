#ifndef _BTREE_H
#define _BTREE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef int Btree_Fd;

typedef enum btree_error {
    BTREE_OK,
    BTREE_ERROR,
    BTREE_ERROR_NULLPTR,
    BTREE_ERROR_SMALL_T,
    BTREE_ERROR_KEY_NOT_FOUND,
    BTREE_ERROR_KEY_EXISTS,
    BTREE_ERROR_FORMAT
} Btree_Error;

typedef struct item {
    int key;
    int value;
} Item;

typedef struct btree_node {
    size_t offset;
    int count_keys;
    bool is_leaf;
    Item *items;
    size_t *children;
} Btree_Node;

typedef struct btree_header {
    int t;
    int M;
    int count_nodes;
    size_t next_offset;
    size_t next_free_offset;
    size_t root_offset;
} Btree_Header;

typedef struct btree {
    Btree_Header header;
    Btree_Fd fd;
    Btree_Node *root;
} Btree;

int btree_init(Btree *btree, const char *path, int t);

int btree_find(Btree *btree, int key, int *value);

int btree_insert(Btree *btree, int key, int value);

int btree_delete(Btree *btree, int key);

int btree_destroy(Btree *btree);

int btree_display(Btree *btree, FILE *fp);

const char *btree_strerr(int err);

#ifdef BTREE_IMPLEMENTATION
#endif

#endif
