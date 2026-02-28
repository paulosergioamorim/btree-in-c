#include "btree.hpp"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc < 2)
        return 1;

    char *path = argv[1];
    BTree<int, int> *btree;

    if (argc > 2) {
        int t = atoi(argv[2]);
        btree = new BTree<int, int>(path, t);
    } else
        btree = new BTree<int, int>(path);

    char op = '\0';

    while (1) {
        scanf("%c", &op);

        if (op == 'Q')
            break;

        if (op == 'I') {
            int key, value;
            scanf("%d %d", &key, &value);
            btree->insert(key, value);
            continue;
        }

        if (op == 'S') {
            int key, value;
            scanf("%d", &key);
            int hit = btree->search(key, value);
            printf("%s %d\n", hit ? "HIT VALUE" : "MISS KEY", hit ? value : key);
            continue;
        }

        if (op == 'D') {
            int key;
            scanf("%d", &key);
            btree->remove(key);
            continue;
        }

        if (op == 'P')
            btree->display();
    }

    delete btree;
    return 0;
}
