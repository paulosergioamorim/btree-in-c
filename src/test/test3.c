#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../btree.h"
#include "utils.h"

int main() {
    int len = 100; // the test size
    long seed = 42;
    int t = 5;
    Btree btree;
    remove("test3.db");
    int ok = btree_init(&btree, .path = "test3.db", .t = t);
    assert(ok == BTREE_OK && "Failed to init btree");
    srand48(seed);
    int *keys = malloc(len * sizeof(*keys));

    for (int i = 0; i < len; i++) {
        keys[i] = i + 1;
    }

    shuffle(keys, len);

    for (int i = 0; i < len; i++) {
        int key = keys[i];
        int ok = btree_insert(&btree, key, key);
        if (ok != BTREE_OK) {
            printf("%s\n", btree_strerr(ok));
            assert(0);
        }
        assert(btree_is_valid(&btree));
    }

    shuffle(keys, len);

    for (int i = 0; i < len; i++) {
        int key = keys[i];
        int ok = btree_delete(&btree, key);
        if (ok != BTREE_OK) {
            printf("%s\n", btree_strerr(ok));
            assert(0);
        }
        assert(btree_is_valid(&btree));
    }

    btree_destroy(&btree);
    free(keys);
    return 0;
}
