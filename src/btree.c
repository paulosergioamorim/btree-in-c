#include "btree.h"
#include <assert.h>
#include <stdlib.h>

BTree_Node *btree_append_node(BTree *btree);

int btree_node_search(BTree_Node *x, int key, int *value);

void btree_node_split_child(BTree *btree, BTree_Node *x, int i);

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value);

BTree_Node *btree_node_destroy(BTree_Node *node);

BTree *btree_init(int t) {
    BTree *btree = malloc(sizeof(*btree));
    assert(btree);
    btree->t = t;
    btree->M = 2 * t;
    btree->count_nodes = 0;
    btree->root = btree_append_node(btree);
    return btree;
}

int btree_search(BTree *btree, int key, int *value) {
    return btree_node_search(btree->root, key, value);
}

void btree_insert(BTree *btree, int key, int value) {
    if (btree->root->count_keys == btree->M - 1) {
        BTree_Node *s = btree_append_node(btree);
        s->is_leaf = 0;
        s->count_keys = 0;
        s->children[0] = btree->root;
        btree->root = s;
        btree_node_split_child(btree, s, 0);
        btree_node_insert_nonfull(btree, s, key, value);
        return;
    }

    btree_node_insert_nonfull(btree, btree->root, key, value);
}

void btree_delete(BTree *btree, int key);

BTree *btree_destroy(BTree *btree) {
    assert(btree);
    btree->root = btree_node_destroy(btree->root);
    free(btree);
    return NULL;
}

BTree_Node *btree_node_destroy(BTree_Node *node) {
    assert(node);

    if (node->is_leaf) {
        free(node->keys);
        free(node->values);
        free(node->children);
        free(node);
        return NULL;
    }

    for (int i = 0; i < node->count_keys; i++)
        node->children[i] = btree_node_destroy(node->children[i]);

    free(node->values);
    free(node->children);
    free(node);
    return NULL;
}

int btree_node_search(BTree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->keys[i])
        i++;

    if (i < x->count_keys && key == x->keys[i]) {
        *value = x->values[i];
        return 1;
    }

    if (x->is_leaf)
        return 0;

    return btree_node_search(x->children[i], key, value);
}

BTree_Node *btree_append_node(BTree *btree) {
    BTree_Node *node = malloc(sizeof(*node));
    assert(node);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->keys = malloc((btree->M - 1) * sizeof(*node->keys));
    assert(node->keys);
    node->values = malloc((btree->M - 1) * sizeof(*node->values));
    assert(node->values);
    node->children = malloc(btree->M * sizeof(*node->children));
    assert(node->children);
    btree->count_nodes++;
    return node;
}

void btree_node_split_child(BTree *btree, BTree_Node *x, int i) {
    BTree_Node *z = btree_append_node(btree);
    BTree_Node *y = NULL;

    y = x->children[i];
    z->is_leaf = y->is_leaf;
    z->count_keys = btree->t - 1;

    for (int j = 0; j < btree->t - 1; j++) {
        z->keys[j] = y->keys[j + btree->t];
        z->values[j] = y->values[j + btree->t];
    }

    if (!y->is_leaf)
        for (size_t j = 0; j < btree->t; j++)
            z->children[j] = y->children[j + btree->t];

    y->count_keys = btree->t - 1;

    for (int j = x->count_keys; j > i + 1; j--)
        x->children[j + 1] = x->children[j];

    x->children[i + 1] = z;

    for (int j = x->count_keys - 1; j > i; j--)
        x->keys[j + 1] = x->keys[j];

    x->keys[i] = y->keys[i];
    x->count_keys++;
}

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value) {
    int i = x->count_keys;

    if (x->is_leaf) {
        while (i >= 0 && key < x->keys[i]) {
            x->keys[i + 1] = x->keys[i];
            x->values[i + 1] = x->values[i];
            i--;
        }

        x->keys[i + 1] = key;
        x->values[i + 1] = value;
        x->count_keys++;
        return;
    }

    while (i >= 0 && key < x->keys[i])
        i--;

    i++;

    if (x->children[i]->count_keys == btree->M - 1) {
        btree_node_split_child(btree, x, i);

        if (key > x->keys[i])
            i++;
    }

    btree_node_insert_nonfull(btree, x->children[i], key, value);
}
