#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void swap(int *pX, int *pY) {
    int temp = *pX;
    *pX = *pY;
    *pY = temp;
}

void shuffle(int *vec, int len) {
    for (int i = 0; i < len; i++) {
        int j = rand() % (i + 1);
        swap(vec + i, vec + j);
    }
}

int btree_node_validate(BTree *btree, BTree_Node *node) {
    if (!btree->root && !(btree->t - 1 <= node->count_keys && node->count_keys <= btree->M - 1))
        return 0;

    for (int i = 1; i < node->count_keys; i++) {
        if (node->keys[i] <= node->keys[i - 1])
            return 0;
    }

    if (!node->is_leaf)
        for (int i = 0; i <= node->count_keys; i++) {
            if (!btree_node_validate(btree, node->children[i]))
                return 0;
        }

    return 1;
}

int btree_validate(BTree *btree) {
    return btree_node_validate(btree, btree->root);
}

int main(int argc, char **argv) {
    srand(42);
    BTree *btree = btree_init(5);

    int len = 1000;
    int *keys = malloc(len * sizeof(*keys));
    int *removed = malloc((len + 1) * sizeof(*removed));

    for (int i = 0; i < len; i++) {
        keys[i] = i + 1;
        removed[i] = 0;
    }

    removed[len] = 0;

    shuffle(keys, len);

    for (int i = 0; i < len; i++)
        btree_insert(btree, keys[i], keys[i]);

    btree_display(btree);
    printf("\n");

    for (int i = 0; i < len; i++) {
        int key = keys[i];
        btree_delete(btree, key);
        btree_display(btree);
    }

    btree = btree_destroy(btree);
    free(keys);
    free(removed);

    return 0;
}
