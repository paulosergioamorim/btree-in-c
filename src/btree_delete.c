#include "btree.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct kv {
    int key;
    int value;
} KV;

typedef struct kvc {
    int key;
    int value;
    BTree_Node *child; // pred or post key
} KVC;

KV btree_node_get_pred(BTree_Node *node, int i);

KV btree_node_get_post(BTree_Node *node, int i);

KVC btree_node_get_max(BTree_Node *node);

KVC btree_node_get_min(BTree_Node *node);

BTree_Node *btree_poll_node(BTree *btree, BTree_Node *node);

void btree_node_merge(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i, int t);

void btree_node_delete(BTree *btree, BTree_Node *node, int key) {
    int i = 0;
    int t = btree->t;

    while (i < node->count_keys && key > node->keys[i])
        i++;

    if (i < node->count_keys && key == node->keys[i]) {
        if (node->is_leaf) {
            memmove(node->keys + i, node->keys + i + 1, (node->count_keys - i - 1) * sizeof(*node->keys));
            memmove(node->values + i, node->values + i + 1, (node->count_keys - i - 1) * sizeof(*node->values));
            node->count_keys--;
            return;
        }

        BTree_Node *y = node->children[i];
        BTree_Node *z = node->children[i + 1];

        if (y->count_keys >= t) {
            KV pred = btree_node_get_pred(node, i);
            node->keys[i] = pred.key;
            node->values[i] = pred.value;
            btree_node_delete(btree, y, pred.key);
            return;
        }

        if (z->count_keys >= t) {
            KV post = btree_node_get_post(node, i);
            node->keys[i] = post.key;
            node->values[i] = post.value;
            btree_node_delete(btree, z, post.key);
            return;
        }

        btree_node_merge(btree, node, y, z, i, t);
        btree_node_delete(btree, y, key);

        if (btree->root == node && node->count_keys == 0) {
            BTree_Node *s = node->children[0];
            btree_poll_node(btree, node);
            btree->root = s;
            return;
        }
    }

    assert(0 && "Caso 3");
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

KVC btree_node_get_max(BTree_Node *node) {
    return (KVC){.key = node->keys[node->count_keys - 1],
                 .value = node->values[node->count_keys - 1],
                 .child = node->children[node->count_keys]};
}

KVC btree_node_get_min(BTree_Node *node) {
    return (KVC){.key = node->keys[0], .value = node->values[0], .child = node->children[0]};
}

void btree_node_merge(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i, int t) {
    y->keys[y->count_keys] = x->keys[i];
    y->values[y->count_keys] = x->values[i];
    y->count_keys++;
    memmove(x->keys + i, x->keys + i + 1, (x->count_keys - i - 1) * sizeof(*x->keys));
    memmove(x->values + i, x->values + i + 1, (x->count_keys - i - 1) * sizeof(*x->values));
    x->count_keys--;
    memcpy(y->keys + y->count_keys, z->keys, (t - 1) * sizeof(*y->keys));
    memcpy(y->values + y->count_keys, z->values, (t - 1) * sizeof(*y->keys));

    if (y->is_leaf)
        memcpy(y->children + y->count_keys, z->children, t * sizeof(*y->keys));

    y->count_keys = 2 * t - 1;
    z = btree_poll_node(btree, z);
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
