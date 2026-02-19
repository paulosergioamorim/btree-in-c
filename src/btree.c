#include "btree.h"
#include "btree_delete.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    if (btree->root->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, btree->root, key, value);
        return;
    }

    BTree_Node *s = btree_append_node(btree);
    s->is_leaf = 0;
    s->count_keys = 0;
    s->children[0] = btree->root;
    btree->root = s;
    btree_node_split_child(btree, s, 0);
    btree_node_insert_nonfull(btree, s, key, value);
}

void btree_delete(BTree *btree, int key) {
    btree_node_delete(btree, btree->root, key);
}

BTree *btree_destroy(BTree *btree) {
    assert(btree);
    btree->root = btree_node_destroy(btree->root);
    free(btree);
    return NULL;
}

BTree_Node *btree_node_destroy(BTree_Node *node) {
    assert(node);

    if (!node->is_leaf)
        for (int i = 0; i <= node->count_keys; i++)
            node->children[i] = btree_node_destroy(node->children[i]);

    free(node->keys);
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
    int t = btree->t;

    y = x->children[i];
    z->is_leaf = y->is_leaf;
    z->count_keys = t - 1;

    memcpy(z->keys, y->keys + t, (t - 1) * sizeof(*z->keys));
    memcpy(z->values, y->values + t, (t - 1) * sizeof(*z->values));

    if (!y->is_leaf)
        memcpy(z->children, y->children + t, t * sizeof(*z->children));

    y->count_keys = t - 1;

    memmove(x->children + i + 1, x->children + i, (x->count_keys - i + 1) * sizeof(*x->children));

    x->children[i + 1] = z;

    memmove(x->keys + i + 1, x->keys + i, (x->count_keys - i) * sizeof(*x->keys));
    memmove(x->values + i + 1, x->values + i, (x->count_keys - i) * sizeof(*x->values));

    x->keys[i] = y->keys[t - 1];
    x->count_keys++;
}

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value) {
    int i = x->count_keys - 1;

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

    if (x->children[i]->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, x->children[i], key, value);
        return;
    }

    btree_node_split_child(btree, x, i);

    if (key > x->keys[i])
        i++;

    btree_node_insert_nonfull(btree, x->children[i], key, value);
}

void btree_display(BTree *btree) {
    BTree_Queue *queue = btree_queue_init(btree->count_nodes);
    BTree_Node *last_level_node = btree->root;
    btree_queue_enqueue(queue, btree->root);

    while (!btree_queue_is_empty(queue)) {
        BTree_Node *node = btree_queue_dequeue(queue);
        printf("[ ");

        for (int i = 0; i < node->count_keys; i++)
            printf("%d ", node->keys[i]);

        printf("] ");

        if (node == last_level_node)
            printf("\n");

        if (node->is_leaf)
            continue;

        for (int i = 0; i <= node->count_keys; i++)
            btree_queue_enqueue(queue, node->children[i]);

        last_level_node = node->children[node->count_keys];
    }

    queue = btree_queue_destroy(queue);
}
