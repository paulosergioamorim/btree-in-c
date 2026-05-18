#ifndef _BTREE_H
#define _BTREE_H

#define BTREE_OK 0
#define BTREE_ERROR_SMALL_PARAM_T 1
#define BTREE_ERROR_KEY_NOT_FOUND 2
#define BTREE_ERROR_KEY_ALREADY_EXISTS 3
#define BTREE_ERROR_ALLOC 4
#define BTREE_ERROR_OPEN_FILE 5
#define BTREE_ERROR_CLOSE_FILE 6
#define BTREE_ERROR_NULLPTR 7
#define BTREE_ERROR_FORMAT 8

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
} Btree_Node;

typedef struct btree {
    int t;
    int M;
    int count_nodes;
    long next_offset;
    long next_free;
    int fd;
    Btree_Node *root;
    Btree_Node *aux_x;
    Btree_Node *aux_x_ci;
    Btree_Node *aux_y;
    Btree_Node *aux_z;
    Btree_Node *aux_sbl;
    Btree_Node *aux_sbr;
} Btree;

int btree_init(Btree **btree_ptr, const char *path, int t);

int btree_find(Btree *btree, int key, int *value);

int btree_insert(Btree *btree, int key, int value);

int btree_delete(Btree *btree, int key);

int btree_destroy(Btree *btree);

int btree_display(Btree *btree);

const char *btree_strerr(int err);

void btree_node_clear_v2(Btree *btree, Btree_Node *node);

void btree_node_read_v2(Btree *btree, long offset, Btree_Node *node);

void btree_node_write_v2(Btree *btree, Btree_Node *node);

Btree_Node *btree_node_init_v2(Btree *btree);

#ifdef BTREE_IMPLEMENTATION
#define BTREE_INIT_IMPLEMENTATION
#define BTREE_FIND_IMPLEMENTATION
#define BTREE_INSERT_IMPLEMENTATION
#define BTREE_DELETE_IMPLEMENTATION
#define BTREE_DISPLAY_IMPLEMENTATION
#endif

#ifdef BTREE_DISPLAY_IMPLEMENTATION
#include <stdio.h>

#define btree_node_enqueue(queue, head, count, offset)                                                                 \
    ({                                                                                                                 \
        int idx = (head + count) % btree->count_nodes;                                                                 \
        queue[idx] = offset;                                                                                           \
        count++;                                                                                                       \
    })

#define btree_node_dequeue(queue, head, count)                                                                         \
    ({                                                                                                                 \
        int offset = queue[head];                                                                                      \
        head = (head + 1) % btree->count_nodes;                                                                        \
        count--;                                                                                                       \
        offset;                                                                                                        \
    })

int btree_display(Btree *btree) {
    if (!btree)
        return BTREE_ERROR_NULLPTR;

    long last_level_offset = btree->root->offset;
    // todo: add upper bound limit
    long queue[btree->count_nodes];
    int head = 0;
    int count = 0;
    btree_node_enqueue(queue, head, count, btree->root->offset);

    while (count > 0) {
        long offset = btree_node_dequeue(queue, head, count);
        btree_node_read_v2(btree, offset, btree->aux_y);
        Btree_Node *node = btree->aux_y;

        printf("[ ");

        for (int i = 0; i < node->count_keys; i++)
            printf("%d ", node->buf[i].key);

        printf("] ");

        if (offset == last_level_offset) {
            last_level_offset = node->children[node->count_keys];
            printf("\n");
        }

        if (node->is_leaf) {
            btree_node_clear_v2(btree, node);
            continue;
        }

        for (int i = 0; i <= node->count_keys; i++) {
            btree_node_enqueue(queue, head, count, node->children[i]);
        }

        btree_node_clear_v2(btree, node);
    }

    return BTREE_OK;
}

#endif

#endif
