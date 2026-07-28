#include "btree.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, const char **argv) {
    if (argc < 2)
        return 1;

    const char *path = argv[1];
    Btree btree;

    int t = argc > 2 ? atoi(argv[2]) : 0;
    int res = BTREE_INIT(&btree, .path = path, .t = t);

    if (res != BTREE_OK) {
        printf("%s\n", btree_strerr(res));
        return 0;
    }

    char prompt[256];
    char op = '\0';

    while (1) {
        fgets(prompt, sizeof(prompt), stdin);
        sscanf(prompt, "%c", &op);

        if (op == 'Q') {
            break;
        } else if (op == 'I') {
            int key, value;
            if (sscanf(prompt, "I %d %d", &key, &value) != 2) {
                printf("Bad input\n");
            }
            btree_insert(&btree, key, value);
        } else if (op == 'S') {
            int key = 0, value = 0;
            if (sscanf(prompt, "S %d", &key) != 1) {
                printf("Bad input\n");
            }
            int res = btree_find(&btree, key, &value);
            printf("%s %d\n", res == BTREE_OK ? "HIT VALUE" : "MISS KEY", res == BTREE_OK ? value : key);
        } else if (op == 'D') {
            int key = 0;
            if (sscanf(prompt, "D %d", &key) != 1) {
                printf("Bad input\n");
            }
            btree_delete(&btree, key);
        } else if (op == 'P') {
            btree_display(&btree, stdout);
        } else if (op == 'V') {
            btree_is_valid(&btree);
        }
    }

    btree_destroy(&btree);

    return 0;
}
