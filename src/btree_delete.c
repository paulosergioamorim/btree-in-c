#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

Item btree_node_get_pred(BTree *btree, BTree_Node *node, int i);

Item btree_node_get_post(BTree *btree, BTree_Node *node, int i);

void btree_node_merge(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i, int t);

int btree_node_redistribute(BTree *btree, BTree_Node *x, BTree_Node *x_ci, BTree_Node *sibbling_left,
                            BTree_Node *sibbling_right, int i, int t);

BTree_Node *btree_node_concatenate(BTree *btree, BTree_Node *x, BTree_Node *x_ci, BTree_Node *sibbling_left,
                                   BTree_Node *sibbling_right, int i, int t);

/**
 * Rotação à esquerda em torno da i-ésima chave de x.
 */
void btree_node_rotate_left(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i);

/**
 * Rotação à direita em torno da i-ésima chave de x.
 */
void btree_node_rotate_right(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i);

void btree_remove_node(BTree *btree, BTree_Node *x);

int btree_node_delete(BTree *btree, BTree_Node *node, int key) {
    int i = 0;
    int t = btree->t;
    int hit = 0;

    while (i < node->count_keys && key > node->buf[i].key)
        i++;

    if (i < node->count_keys && key == node->buf[i].key) {
        if (node->is_leaf) {
            memmove(node->buf + i, node->buf + i + 1, (node->count_keys - i - 1) * sizeof(*node->buf));
            node->count_keys--;
            node->buf[node->count_keys] = (Item){0};
            btree_node_write(btree, node);
            return 1;
        }

        BTree_Node *y = btree_node_read_child(btree, node, i);

        if (y->count_keys >= t) {
            Item pred = btree_node_get_pred(btree, node, i);
            node->buf[i] = pred;
            btree_node_write(btree, node);
            hit = btree_node_delete(btree, y, pred.key);
            btree_node_destroy(y);
            return hit;
        }

        BTree_Node *z = btree_node_read_child(btree, node, i + 1);

        if (z->count_keys >= t) {
            btree_node_destroy(y);
            Item post = btree_node_get_post(btree, node, i);
            node->buf[i] = post;
            btree_node_write(btree, node);
            int hit = btree_node_delete(btree, z, post.key);
            btree_node_destroy(z);
            return hit;
        }

        btree_node_merge(btree, node, y, z, i, t);
        hit = btree_node_delete(btree, y, key);

        if (btree->root != y)
            btree_node_destroy(y);

        return hit;
    }

    if (node->is_leaf)
        return 0;

    BTree_Node *x_ci = btree_node_read_child(btree, node, i);

    if (x_ci->count_keys > t - 1) {
        hit = btree_node_delete(btree, x_ci, key);

        if (btree->root != x_ci)
            btree_node_destroy(x_ci);

        return hit;
    }

    BTree_Node *sibbling_left = btree_node_read_child(btree, node, i - 1);
    BTree_Node *sibbling_right = btree_node_read_child(btree, node, i + 1);

    if (btree_node_redistribute(btree, node, x_ci, sibbling_left, sibbling_right, i, t)) {
        if (sibbling_left)
            btree_node_destroy(sibbling_left);
        if (sibbling_right)
            btree_node_destroy(sibbling_right);
        hit = btree_node_delete(btree, x_ci, key);
    }

    else {
        x_ci = btree_node_concatenate(btree, node, x_ci, sibbling_left, sibbling_right, i, t);
        hit = btree_node_delete(btree, x_ci, key);
    }

    if (btree->root != x_ci)
        btree_node_destroy(x_ci);

    return hit;
}

Item btree_node_get_pred(BTree *btree, BTree_Node *node, int i) {
    assert(!node->is_leaf);
    BTree_Node *pred = btree_node_read_child(btree, node, i);

    while (!pred->is_leaf) {
        BTree_Node *prev = pred;
        pred = btree_node_read_child(btree, pred, pred->count_keys);
        btree_node_destroy(prev);
    }

    Item item = pred->buf[pred->count_keys - 1];
    btree_node_destroy(pred);
    return item;
}

Item btree_node_get_post(BTree *btree, BTree_Node *node, int i) {
    assert(!node->is_leaf);
    BTree_Node *post = btree_node_read_child(btree, node, i + 1);

    while (!post->is_leaf) {
        BTree_Node *prev = post;
        post = btree_node_read_child(btree, post, 0);
        btree_node_destroy(prev);
    }

    Item item = post->buf[0];
    btree_node_destroy(post);
    return item;
}

void btree_remove_node(BTree *btree, BTree_Node *x) {
    int offset = x->offset;
    x->count_keys = 0;
    x->is_leaf = 0;
    memset(x->buf, 0, (btree->M - 1) * sizeof(*x->buf));
    memset(x->children, 0, btree->M * sizeof(*x->children));
    btree_node_write(btree, x);
    fseek(btree->fp, offset, SEEK_SET);
    fwrite(&btree->next_free, sizeof(btree->next_free), 1, btree->fp);
    btree->next_free = x->offset;
    btree_node_destroy(x);
    btree->count_nodes--;
}

void btree_node_merge(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i, int t) {
    y->buf[y->count_keys] = x->buf[i];
    y->count_keys++;
    memmove(x->buf + i, x->buf + i + 1, (x->count_keys - i - 1) * sizeof(*x->buf));
    memmove(x->children + i + 1, x->children + i + 2, (x->count_keys - i - 1) * sizeof(*x->children));
    x->count_keys--;
    memcpy(y->buf + y->count_keys, z->buf, (t - 1) * sizeof(*y->buf));

    if (!y->is_leaf)
        memcpy(y->children + y->count_keys, z->children, t * sizeof(*y->children));

    y->count_keys = 2 * t - 1;

    btree_node_write(btree, x);

    if (btree->root == x && x->count_keys == 0) {
        btree_remove_node(btree, x);
        btree->root = y;
    }

    btree_remove_node(btree, z);
    btree_node_write(btree, y);
}

int btree_node_redistribute(BTree *btree, BTree_Node *x, BTree_Node *x_ci, BTree_Node *sibbling_left,
                            BTree_Node *sibbling_right, int i, int t) {
    if (sibbling_left && sibbling_left->count_keys >= t) {
        btree_node_rotate_right(btree, x, sibbling_left, x_ci, i - 1);
        return 1;
    }

    if (sibbling_right && sibbling_right->count_keys >= t) {
        btree_node_rotate_left(btree, x, x_ci, sibbling_right, i);
        return 1;
    }

    return 0;
}

BTree_Node *btree_node_concatenate(BTree *btree, BTree_Node *x, BTree_Node *x_ci, BTree_Node *sibbling_left,
                                   BTree_Node *sibbling_right, int i, int t) {
    if (sibbling_left) {
        btree_node_merge(btree, x, sibbling_left, x_ci, i - 1, t);

        if (sibbling_right)
            btree_node_destroy(sibbling_right);

        return sibbling_left;
    }

    btree_node_merge(btree, x, x_ci, sibbling_right, i, t);

    if (sibbling_left)
        btree_node_destroy(sibbling_left);

    return x_ci;
}

void btree_node_rotate_left(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i) {
    y->buf[y->count_keys] = x->buf[i];

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = z->children[0];

    y->count_keys++;

    x->buf[i] = z->buf[0];

    memmove(z->buf, z->buf + 1, (z->count_keys - 1) * sizeof(*z->buf));

    if (!z->is_leaf)
        memmove(z->children, z->children + 1, z->count_keys * sizeof(*z->children));

    z->count_keys--;
    z->buf[z->count_keys] = (Item){0};

    if (!z->is_leaf)
        z->children[z->count_keys + 1] = 0;

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
}

void btree_node_rotate_right(BTree *btree, BTree_Node *x, BTree_Node *y, BTree_Node *z, int i) {
    memmove(z->buf + 1, z->buf, z->count_keys * sizeof(*z->buf));

    if (!z->is_leaf)
        memmove(z->children + 1, z->children, (z->count_keys + 1) * sizeof(*z->children));

    z->buf[0] = x->buf[i];

    if (!z->is_leaf)
        z->children[0] = y->children[y->count_keys];

    z->count_keys++;

    x->buf[i] = y->buf[y->count_keys - 1];
    y->count_keys--;
    y->buf[y->count_keys] = (Item){0};

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = 0;

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
}
