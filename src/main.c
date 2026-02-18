#include "btree.h"
#include <assert.h>
#include <stdio.h>

int main(int argc, char **argv) {
    BTree *btree = btree_init(5);

    for (int i = 1; i <= 1000; i++) {
        btree_insert(btree, i, i);
    }

    int value = 0;

    for (int i = 1; i <= 10; i++) {
        if (!btree_search(btree, i, &value))
            printf("CHAVE %d PERDIDA\n", i);

        else
            printf("CHAVE %d ENCONTRADA\n", i);
    }

    return 0;
}
