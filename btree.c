#include "btree.h"
#include <assert.h>
#include <stdlib.h>

BTree_Node *btree_append_node(BTree *btree);

int btree_node_search(BTree_Node *node, int key, int *value);

BTree_Node *btree_node_destroy(BTree_Node *node);

BTree *btree_init(int t) {
    BTree *btree = malloc(sizeof(*btree));
    assert(btree);
    btree->t = t;
    btree->M = 2 * t;
    btree->count_nodes = 0;
    btree->root = NULL;
    return 0;
}

int btree_search(BTree *btree, int key, int *value);

int btree_insert(BTree *btree, int key, int value) {
    if (!btree->root)
        btree->root = btree_append_node(btree);

    return 0;
}

int btree_delete(BTree *btree, int key);

BTree *btree_destroy(BTree *btree) {
    assert(btree);
    btree->root = btree_node_destroy(btree->root);
    free(btree);
    return NULL;
}

BTree_Node *btree_node_destroy(BTree_Node *node) {
    assert(node);

    if (node->is_leaf) {
        free(node->buf);
        free(node);
        return NULL;
    }

    for (size_t i = 0; i < node->count_keys; i++)
        node->buf[i].pred = btree_node_destroy(node->buf[i].pred);

    node->post = btree_node_destroy(node->post);
    free(node->buf);
    free(node);
    return NULL;
}

int btree_node_search(BTree_Node *node, int key, int *value) {
    int i = 0;

    while (i < node->count_keys && key > node->buf[i].key)
        i++;

    if (i < node->count_keys && key == node->buf[i].key) {
        *value = node->buf[i].value;
        return 1;
    }

    if (node->is_leaf)
        return 0;

    if (i == node->count_keys)
        return btree_node_search(node->post, key, value);

    return btree_node_search(node->buf[i].pred, key, value);
}

BTree_Node *btree_append_node(BTree *btree) {
    BTree_Node *node = malloc(sizeof(*node));
    assert(node);
    node->is_leaf = 0;
    node->count_keys = 0;
    node->post = NULL;
    node->buf = malloc((btree->M - 1) * sizeof(*node->buf));
    assert(node->buf);
    btree->count_nodes++;
    return node;
}
