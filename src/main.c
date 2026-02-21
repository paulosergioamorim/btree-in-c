#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    const int t = atoi(argv[1]);

    if (t < 2)
        return 1;

    BTree *btree = btree_init(t);
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
            printf("%d %d\n", hit, value);
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

    btree = btree_destroy(btree);

    return 0;
}
