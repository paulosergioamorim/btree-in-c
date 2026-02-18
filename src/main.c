#include "btree.h"
#include <assert.h>
#include <stdlib.h>
#include <time.h>

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

int main(int argc, char **argv) {
    srand(time(0) % 43);
    BTree *btree = btree_init(5);

    int len = 30000;
    int *keys = malloc(len * sizeof(*keys));

    for (int i = 0; i < len; i++)
        keys[i] = i + 1;

    shuffle(keys, len);

    for (int i = 0; i < len; i++)
        btree_insert(btree, keys[i], keys[i]);

    // btree_display(btree);
    btree = btree_destroy(btree);
    free(keys);

    return 0;
}
