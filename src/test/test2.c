#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../btree.h"

void swap(int *x1, int *x2);

void shuffle(int *vec, int len);

int main() {
    int len = 10000000; // the test size
    long seed = 42;
    int t = 200;
    Btree btree;
    remove("benchmark2.db");
    int ok = btree_init(&btree, "benchmark2.db", t);
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
    }
    shuffle(keys, len);
    time_t start = clock();
    for (int i = 0; i < len; i++) {
        int key = keys[i];
        int ok = btree_delete(&btree, key);
        if (ok != BTREE_OK) {
            printf("%s\n", btree_strerr(ok));
            assert(0);
        }
    }
    time_t end = clock();
    double dtime = difftime(end, start);
    double dtime_secs = dtime / CLOCKS_PER_SEC;

    printf("Time to exec btree_delete over %d random unique keys: %.2lf\n"
           "t = %d\n",
           len, dtime_secs, t);

    btree_destroy(&btree);
    free(keys);
    return 0;
}

void swap(int *x1, int *x2) {
    int temp = *x1;
    *x1 = *x2;
    *x2 = temp;
}

void shuffle(int *vec, int len) {
    for (int i = 0; i < len; i++) {
        int j = rand() % (i + 1);
        swap(&vec[i], &vec[j]);
    }
}
