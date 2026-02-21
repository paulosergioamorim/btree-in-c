#include "btree.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct kv {
    int key;
    int value;
} KV;

KV btree_node_get_pred(BTree_Node *node, int i);

KV btree_node_get_post(BTree_Node *node, int i);

BTree_Node *btree_poll_node(BTree *btree, BTree_Node *node);

BTree_Node *btree_node_merge(BTree *btree, BTree_Node *x, int i, int t);

int btree_node_redistribute(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t);

BTree_Node *btree_node_concatenate(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t);

/**
 * Rotação à esquerda em torno da i-ésima chave de x.
 */
void btree_node_rotate_left(BTree_Node *x, int i);

/**
 * Rotação à direita em torno da i-ésima chave de x.
 */
void btree_node_rotate_right(BTree_Node *x, int i);

int btree_node_delete(BTree *btree, BTree_Node *node, int key) {
    int i = 0;
    int t = btree->t;

    while (i < node->count_keys && key > node->keys[i])
        i++;

    if (i < node->count_keys && key == node->keys[i]) {
        if (node->is_leaf) {
            memmove(node->keys + i, node->keys + i + 1, (node->count_keys - i - 1) * sizeof(*node->keys));
            memmove(node->values + i, node->values + i + 1, (node->count_keys - i - 1) * sizeof(*node->values));
            node->count_keys--;
            return 1;
        }

        BTree_Node *y = node->children[i];
        BTree_Node *z = node->children[i + 1];

        if (y->count_keys >= t) {
            KV pred = btree_node_get_pred(node, i);
            node->keys[i] = pred.key;
            node->values[i] = pred.value;
            return btree_node_delete(btree, y, pred.key);
        }

        if (z->count_keys >= t) {
            KV post = btree_node_get_post(node, i);
            node->keys[i] = post.key;
            node->values[i] = post.value;
            return btree_node_delete(btree, z, post.key);
        }

        btree->root = btree_node_merge(btree, node, i, t);
        return btree_node_delete(btree, y, key);
    }

    if (node->is_leaf)
        return 0;

    BTree_Node *x_ci = node->children[i];

    if (x_ci->count_keys > t - 1)
        return btree_node_delete(btree, x_ci, key);

    if (btree_node_redistribute(btree, node, x_ci, i, t))
        return btree_node_delete(btree, x_ci, key);

    x_ci = btree_node_concatenate(btree, node, x_ci, i, t);
    return btree_node_delete(btree, x_ci, key);
}

KV btree_node_get_pred(BTree_Node *node, int i) {
    assert(!node->is_leaf);
    BTree_Node *pred = node->children[i];

    while (!pred->is_leaf)
        pred = pred->children[pred->count_keys];

    return (KV){.key = pred->keys[pred->count_keys - 1], .value = pred->values[pred->count_keys - 1]};
}

KV btree_node_get_post(BTree_Node *node, int i) {
    assert(!node->is_leaf);
    BTree_Node *post = node->children[i + 1];

    while (!post->is_leaf)
        post = post->children[0];

    return (KV){.key = post->keys[0], .value = post->values[0]};
}

BTree_Node *btree_node_merge(BTree *btree, BTree_Node *x, int i, int t) {
    BTree_Node *y = x->children[i];
    BTree_Node *z = x->children[i + 1];
    y->keys[y->count_keys] = x->keys[i];
    y->values[y->count_keys] = x->values[i];
    y->count_keys++;
    memmove(x->keys + i, x->keys + i + 1, (x->count_keys - i - 1) * sizeof(*x->keys));
    memmove(x->values + i, x->values + i + 1, (x->count_keys - i - 1) * sizeof(*x->values));
    memmove(x->children + i + 1, x->children + i + 2, (x->count_keys - i) * sizeof(*x->children));
    x->count_keys--;
    memcpy(y->keys + y->count_keys, z->keys, (t - 1) * sizeof(*y->keys));
    memcpy(y->values + y->count_keys, z->values, (t - 1) * sizeof(*y->values));

    if (!y->is_leaf)
        memcpy(y->children + y->count_keys, z->children, t * sizeof(*y->children));

    y->count_keys = 2 * t - 1;
    z = btree_poll_node(btree, z);

    if (btree->root == x && x->count_keys == 0) {
        BTree_Node *s = x->children[0];
        btree_poll_node(btree, x);
        return s;
    }

    return btree->root;
}

BTree_Node *btree_poll_node(BTree *btree, BTree_Node *node) {
    assert(node);
    free(node->keys);
    free(node->values);
    free(node->children);
    free(node);
    btree->count_nodes--;
    return NULL;
}

int btree_node_redistribute(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t) {
    BTree_Node *sibbling_left = (i > 0) ? x->children[i - 1] : NULL;
    BTree_Node *sibbling_right = (i < x->count_keys) ? x->children[i + 1] : NULL;

    if (sibbling_left && sibbling_left->count_keys >= t) {
        btree_node_rotate_right(x, i - 1);
        return 1;
    }

    if (sibbling_right && sibbling_right->count_keys >= t) {
        btree_node_rotate_left(x, i);
        return 1;
    }

    return 0;
}

BTree_Node *btree_node_concatenate(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t) {
    BTree_Node *sibbling_left = (i > 0) ? x->children[i - 1] : NULL;

    if (sibbling_left) {
        btree->root = btree_node_merge(btree, x, i - 1, t);
        return sibbling_left;
    }

    btree->root = btree_node_merge(btree, x, i, t);
    return x_ci;
}

void btree_node_rotate_left(BTree_Node *x, int i) {
    BTree_Node *y = x->children[i];
    BTree_Node *z = x->children[i + 1];

    y->keys[y->count_keys] = x->keys[i];
    y->values[y->count_keys] = x->values[i];

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = z->children[0];

    y->count_keys++;

    x->keys[i] = z->keys[0];
    x->values[i] = z->keys[0];

    memmove(z->keys, z->keys + 1, z->count_keys * sizeof(*z->keys));
    memmove(z->values, z->values + 1, z->count_keys * sizeof(*z->values));

    if (!z->is_leaf)
        memmove(z->children, z->children + 1, (z->count_keys + 1) * sizeof(*z->children));

    z->count_keys--;
}

void btree_node_rotate_right(BTree_Node *x, int i) {
    BTree_Node *y = x->children[i];
    BTree_Node *z = x->children[i + 1];

    memmove(z->keys + 1, z->keys, z->count_keys * sizeof(*z->keys));
    memmove(z->values + 1, z->values, z->count_keys * sizeof(*z->values));

    if (!z->is_leaf)
        memmove(z->children + 1, z->children, (z->count_keys + 1) * sizeof(*z->children));

    z->keys[0] = x->keys[i];
    z->values[0] = x->values[i];

    if (!z->is_leaf)
        z->children[0] = y->children[y->count_keys];

    z->count_keys++;

    x->keys[i] = y->keys[y->count_keys - 1];
    x->values[i] = y->values[y->count_keys - 1];
    y->count_keys--;
}
