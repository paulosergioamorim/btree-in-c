#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    char *path = argv[1];
    BTree *btree;

    if (argc > 2) {
        int t = atoi(argv[2]);
        btree = btree_init_from_memory(path, t);
    } else
        btree = btree_init_from_db(path);

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
            int hit = btree_search(btree, key, &value);
            printf("%s %d\n", hit ? "HIT VALUE" : "MISS KEY", hit ? value : key);
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
