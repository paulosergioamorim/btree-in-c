#include <assert.h>

#include "../btree.h"

int main() {
    Item items[] = {
        {1, -1}, {-1, 10}, {10, 5}, {20, 8}, {3, 2},
    };
    int len = (int)(sizeof(items) / sizeof(*items));
    remove("test4.db");
    Btree btree;
    assert(BTREE_INIT(&btree, .path = "test4.db", .t = 2) == BTREE_OK);
    for (int i = 0; i < len; i++) {
        assert(btree_put(&btree, items[i].key, items[i].value) == BTREE_OK);
    }
    assert(btree_destroy(&btree) == BTREE_OK);
    assert(BTREE_INIT(&btree, .path = "test4.db", .t = 2) == BTREE_OK);
    for (int i = 0; i < len; i++) {
        int value = 0;
        assert(btree_find(&btree, items[i].key, &value) == BTREE_OK);
        assert(value == items[i].value);
    }
    return 0;
}
