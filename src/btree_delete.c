#include "btree.h"
#include <assert.h>

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

void btree_node_delete(BTree *btree, BTree_Node *node, int key) {
    assert(0 && "TODO!");
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
