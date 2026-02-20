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

BTree_Node *btree_node_merge(BTree *btree, BTree_Node *x, int i, int t);

int btree_node_redistribute(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t);

BTree_Node *btree_node_concat(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t);

void btree_node_rotate_left(BTree_Node *x, int i);

void btree_node_rotate_right(BTree_Node *x, int i);

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

        btree->root = btree_node_merge(btree, node, i, t);
        btree_node_delete(btree, y, key);
    }

    if (node->is_leaf)
        return;

    BTree_Node *x_ci = node->children[i];

    if (x_ci->count_keys > t - 1) {
        btree_node_delete(btree, x_ci, key);
        return;
    }

    if (btree_node_redistribute(btree, node, x_ci, i, t)) {
        btree_node_delete(btree, x_ci, key);
        return;
    }

    x_ci = btree_node_concat(btree, node, x_ci, i, t);
    btree_node_delete(btree, x_ci, key);
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
        btree->root = s;
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

    if (!sibbling_right)
        i--;

    if (sibbling_left && sibbling_left->count_keys >= t) {
        memmove(x_ci->keys + 1, x_ci->keys, (t - 1) * sizeof(*x_ci->keys));
        memmove(x_ci->values + 1, x_ci->values, (t - 1) * sizeof(*x_ci->values));

        if (!x_ci->is_leaf)
            memmove(x_ci->children + 1, x_ci->children, t * sizeof(*x_ci->children));

        KVC max = btree_node_get_max(sibbling_left);
        x_ci->keys[0] = x->keys[i];
        x_ci->values[0] = x->values[i];
        x_ci->children[0] = max.child;
        x_ci->count_keys++;
        sibbling_left->count_keys--;
        x->keys[i] = max.key;
        x->values[i] = max.value;

        return 1;
    }

    if (sibbling_right && sibbling_right->count_keys >= t) {
        KVC min = btree_node_get_min(sibbling_right);
        memmove(sibbling_right->keys, sibbling_right->keys + 1,
                (sibbling_right->count_keys) * sizeof(*sibbling_right->keys));
        memmove(sibbling_right->values, sibbling_right->values + 1,
                (sibbling_right->count_keys) * sizeof(*sibbling_right->values));

        if (!sibbling_right->is_leaf)
            memmove(sibbling_right->children, sibbling_right->children + 1,
                    (sibbling_right->count_keys + 1) * sizeof(*sibbling_right->children));

        sibbling_right->count_keys--;
        x_ci->keys[x_ci->count_keys] = x->keys[i];
        x_ci->values[x_ci->count_keys] = x->values[i];
        x_ci->count_keys++;
        x_ci->children[x_ci->count_keys] = min.child;
        x->keys[i] = min.key;
        x->values[i] = min.value;

        return 1;
    }

    return 0;
}

BTree_Node *btree_node_concat(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i, int t) {
    BTree_Node *sibbling_left = (i > 0) ? x->children[i - 1] : NULL;

    if (sibbling_left) {
        btree->root = btree_node_merge(btree, x, i - 1, t);
        return sibbling_left;
    }

    btree->root = btree_node_merge(btree, x, i, t);
    return x_ci;
}

