#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    char *path = argv[1];
    Btree *btree;

    if (argc > 2) {
        int t = atoi(argv[2]);
        btree_init(&btree, BTREE_OPTS_NEW_FILE(path, t));
    } else
        btree_init(&btree, BTREE_OPTS_FROM_FILE(path));

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

#ifdef BTREE_DISPLAY_IMPLEMENTATION
        if (op == 'P')
            btree_display(btree);
#endif
    }

    btree_destroy(btree);

    return 0;
}
