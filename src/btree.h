#ifndef _BTREE_H
#define _BTREE_H

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
} Btree;

int btree_init(Btree **btree_ptr, const char *path, int t);

int btree_find(Btree *btree, int key, int *value);

int btree_insert(Btree *btree, int key, int value);

int btree_delete(Btree *btree, int key);

int btree_destroy(Btree *btree);

int btree_display(Btree *btree);

const char *btree_strerr(int err);

#ifdef BTREE_IMPLEMENTATION
#define BTREE_INIT_IMPLEMENTATION
#define BTREE_FIND_IMPLEMENTATION
#define BTREE_INSERT_IMPLEMENTATION
#define BTREE_DELETE_IMPLEMENTATION
#define BTREE_DISPLAY_IMPLEMENTATION
#endif

#ifdef BTREE_DISPLAY_IMPLEMENTATION
#include <stdio.h>

Btree_Node *btree_node_read(Btree *btree, long offset);

void btree_node_destroy(Btree_Node *node);

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
        Btree_Node *node = btree_node_read(btree, offset);

        printf("[ ");

        for (int i = 0; i < node->count_keys; i++)
            printf("%d ", node->buf[i].key);

        printf("] ");

        if (offset == last_level_offset) {
            last_level_offset = node->children[node->count_keys];
            printf("\n");
        }

        if (node->is_leaf) {
            btree_node_destroy(node);
            continue;
        }

        for (int i = 0; i <= node->count_keys; i++) {
            btree_node_enqueue(queue, head, count, node->children[i]);
        }

        btree_node_destroy(node);
    }

    return BTREE_OK;
}

#endif

#endif
