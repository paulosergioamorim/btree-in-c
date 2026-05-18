#define BTREE_DISPLAY_IMPLEMENTATION
#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    const char *path = argv[1];
    Btree *btree;

    int t = argc > 2 ? atoi(argv[2]) : 0;
    int res = btree_init(&btree, path, t);

    if (res != BTREE_OK) {
        printf("%s\n", btree_strerr(res));
        return 0;
    }

    char op = '\0';

    while (1) {
        scanf("%c", &op);

        if (op == 'Q')
            break;

        if (op == 'I') {
            int key, value;
            scanf("%d %d", &key, &value);
            btree_insert(btree, key, value);
            continue;
        }

        if (op == 'S') {
            int key, value;
            scanf("%d", &key);
            int res = btree_find(btree, key, &value);
            printf("%s %d\n", res == BTREE_OK ? "HIT VALUE" : "MISS KEY", res == BTREE_OK ? value : key);
            continue;
        }

        if (op == 'D') {
            int key;
            scanf("%d", &key);
            btree_delete(btree, key);
            continue;
        }

        if (op == 'P')
            btree_display(btree);
    }

    btree_destroy(btree);

    return 0;
}
