#include "btree.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <unistd.h>

static const int __btree_magic_bytes = 0x4654427f; // 0x7F 'B' 'T' 'F'

#define BTREE_SIZEOF_NODE(btree)                                                                                       \
    (sizeof(btree->root->count_keys) + (btree->header.M - 1) * sizeof(*btree->root->items) +                           \
     btree->header.M * sizeof(*btree->root->children))

/* === IO === */
void btree_node_write(Btree *btree, Btree_Node *node);

Btree_Node *btree_node_read(Btree *btree, size_t offset);

void btree_node_destroy(Btree_Node *node);

void btree_header_write(Btree *btree);

void btree_header_read(Btree *btree, int *magic_bytes);
/* === IO === */

void btree_node_destroy(Btree_Node *node);

int btree_node_delete(Btree *btree, Btree_Node *node, int key);

Btree_Node *btree_append_node(Btree *btree);

int btree_node_find(Btree *btree, Btree_Node *x, int key, int *value);

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *x_ci, int i);

int btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value);

Btree_Node *btree_node_read(Btree *btree, size_t offset) {
    if (offset < 0 || offset >= btree->header.next_offset)
        return NULL;

    Btree_Node *node = malloc(sizeof(*node));

    if (!node)
        return NULL;

    node->items = malloc((btree->header.M - 1) * sizeof(*node->items));

    if (!node->items) {
        free(node);
        return NULL;
    }

    node->children = malloc(btree->header.M * sizeof(*node->children));
    if (!node->children) {
        free(node->items);
        free(node);
        return NULL;
    }
    node->offset = offset;
    const int n = 3;
    struct iovec vec[n];
    vec[0].iov_base = &node->count_keys;
    vec[0].iov_len = sizeof(node->count_keys);
    vec[1].iov_base = node->items;
    vec[1].iov_len = (btree->header.M - 1) * sizeof(*node->items);
    vec[2].iov_base = node->children;
    vec[2].iov_len = (btree->header.M) * sizeof(*node->children);
    preadv(btree->fd, vec, n, offset);
    node->is_leaf = node->children[0] == 0;
    return node;
}

void btree_node_write(Btree *btree, Btree_Node *node) {
    long offset = node->offset;

    if (offset < 0 || offset >= btree->header.next_offset)
        return;

    const int n = 3;
    struct iovec vec[n];
    vec[0].iov_base = &node->count_keys;
    vec[0].iov_len = sizeof(node->count_keys);
    vec[1].iov_base = node->items;
    vec[1].iov_len = (btree->header.M - 1) * sizeof(*node->items);
    vec[2].iov_base = node->children;
    vec[2].iov_len = (btree->header.M) * sizeof(*node->children);
    pwritev(btree->fd, vec, n, offset);
}

void btree_header_write(Btree *btree) {
    const int n = 2;
    struct iovec vec[n];
    vec[0].iov_base = (void *)&__btree_magic_bytes;
    vec[0].iov_len = sizeof(__btree_magic_bytes);
    vec[1].iov_base = &btree->header;
    vec[1].iov_len = sizeof(btree->header);
    pwritev(btree->fd, vec, n, 0);
}

int btree_init(Btree *btree, const char *path, int t) {
    struct stat statbuf = (struct stat){0};
    int res = stat(path, &statbuf);

    if (res == -1 && errno == ENOENT) {
        if (t < 2) {
            return BTREE_ERROR_SMALL_T;
        }

        if (!btree) {
            return BTREE_ERROR;
        }

        btree->header.t = t;
        btree->header.M = 2 * t;
        btree->header.count_nodes = 0;
        btree->header.next_offset = sizeof(__btree_magic_bytes) + sizeof(btree->header);
        btree->fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        if (btree->fd == -1) {
            free(btree);
            return BTREE_ERROR;
        }

        btree->header.next_free_offset = -1;
        btree->root = btree_append_node(btree);
        btree->header.root_offset = btree->root->offset;
        btree_header_write(btree);

        return BTREE_OK;
    }

    if (!btree) {
        return BTREE_ERROR;
    }

    btree->fd = open(path, O_RDWR);

    if (btree->fd == -1) {
        return BTREE_ERROR;
    }

    int magic_bytes = 0;
    btree_header_read(btree, &magic_bytes);

    if (magic_bytes != __btree_magic_bytes) {
        close(btree->fd);
        return BTREE_ERROR_FORMAT;
    }

    btree->root = btree_node_read(btree, btree->header.root_offset);
    return BTREE_OK;
}

int btree_find(Btree *btree, int key, int *value) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }
    return btree_node_find(btree, btree->root, key, value);
}

int btree_insert(Btree *btree, int key, int value) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    if (btree->root->count_keys < btree->header.M - 1) {
        int res = btree_node_insert_nonfull(btree, btree->root, key, value);
        btree_header_write(btree);
        return res;
    }

    Btree_Node *s = btree_append_node(btree);
    s->is_leaf = 0;
    s->count_keys = 0;
    s->children[0] = btree->root->offset;
    btree_node_split_child(btree, s, btree->root, 0);
    btree_node_destroy(btree->root);
    btree->root = s;
    int res = btree_node_insert_nonfull(btree, s, key, value);
    btree_header_write(btree);
    return res;
}

int btree_delete(Btree *btree, int key) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    btree_node_delete(btree, btree->root, key);
    btree_header_write(btree);
    return BTREE_OK;
}

int btree_destroy(Btree *btree) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    btree_node_destroy(btree->root);

    if (close(btree->fd) == -1) {
        return BTREE_ERROR;
    }

    return BTREE_OK;
}

void btree_node_destroy(Btree_Node *node) {
    free(node->items);
    free(node->children);
    free(node);
}

int btree_node_find(Btree *btree, Btree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->items[i].key)
        i++;

    if (i < x->count_keys && key == x->items[i].key) {
        if (value)
            *value = x->items[i].value;
        return BTREE_OK;
    }

    if (x->is_leaf)
        return BTREE_ERROR_KEY_NOT_FOUND;

    Btree_Node *x_ci = btree_node_read(btree, x->children[i]);

    int hit = btree_node_find(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
    return hit;
}

int btree_pop_free_offset(Btree *btree) {
    if (btree->header.next_free_offset == -1) {
        long offset = btree->header.next_offset;
        btree->header.next_offset += BTREE_SIZEOF_NODE(btree);
        return offset;
    }

    long offset = btree->header.next_free_offset;
    pread(btree->fd, &btree->header.next_free_offset, sizeof(btree->header.next_free_offset),
          btree->header.next_free_offset);
    return offset;
}

Btree_Node *btree_append_node(Btree *btree) {
    Btree_Node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;
    node->offset = btree_pop_free_offset(btree);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->items = calloc((btree->header.M - 1), sizeof(*node->items));
    if (!node->items) {
        free(node);
        return NULL;
    }
    node->children = calloc(btree->header.M, sizeof(*node->children));
    if (!node->children) {
        free(node->items);
        free(node);
        return NULL;
    }
    btree->header.count_nodes++;
    btree_node_write(btree, node);
    return node;
}

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *y, int i) {
    Btree_Node *z = btree_append_node(btree);
    int t = btree->header.t;

    z->is_leaf = y->is_leaf;
    z->count_keys = t - 1;

    memcpy(z->items, y->items + t, (t - 1) * sizeof(*z->items));

    if (!y->is_leaf)
        memcpy(z->children, y->children + t, t * sizeof(*z->children));

    y->count_keys = t - 1;

    memmove(x->children + i + 1, x->children + i, (x->count_keys - i + 1) * sizeof(*x->children));

    x->children[i + 1] = z->offset;

    memmove(x->items + i + 1, x->items + i, (x->count_keys - i) * sizeof(*x->items));

    x->items[i] = y->items[t - 1];
    x->count_keys++;

    memset(y->items + y->count_keys, 0, t * sizeof(*y->items));

    if (!y->is_leaf)
        memset(y->children + y->count_keys + 1, 0, t * sizeof(*y->children));

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
    btree_node_destroy(z);
}

int btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value) {
    int i = x->count_keys - 1;

    if (x->is_leaf) {
        while (i >= 0 && key < x->items[i].key) {
            x->items[i + 1] = x->items[i];
            i--;
        }

        if (i >= 0 && x->items[i].key == key) {
            return BTREE_ERROR_KEY_EXISTS;
        }

        x->items[i + 1] = (Item){.key = key, .value = value};
        x->count_keys++;
        btree_node_write(btree, x);
        return BTREE_OK;
    }

    while (i >= 0 && key < x->items[i].key)
        i--;

    i++;

    Btree_Node *x_ci = btree_node_read(btree, x->children[i]);

    if (x_ci->count_keys < btree->header.M - 1) {
        int res = btree_node_insert_nonfull(btree, x_ci, key, value);
        btree_node_destroy(x_ci);
        return res;
    }

    btree_node_split_child(btree, x, x_ci, i);

    if (key > x->items[i].key) {
        i++;
        btree_node_destroy(x_ci);
        x_ci = btree_node_read(btree, x->children[i]);
    }

    int res = btree_node_insert_nonfull(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
    return res;
}

Item btree_node_get_pred(Btree *btree, Btree_Node *node, int i);

Item btree_node_get_post(Btree *btree, Btree_Node *node, int i);

void btree_node_merge(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i);

int btree_node_redistribute(Btree *btree, Btree_Node *x, Btree_Node *x_ci, Btree_Node *sibbling_left,
                            Btree_Node *sibbling_right, int i);

Btree_Node *btree_node_concatenate(Btree *btree, Btree_Node *x, Btree_Node *x_ci, Btree_Node *sibbling_left,
                                   Btree_Node *sibbling_right, int i);

void btree_node_rotate_left(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i);

void btree_node_rotate_right(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i);

void btree_remove_node(Btree *btree, Btree_Node *x);

int btree_node_delete(Btree *btree, Btree_Node *node, int key) {
    int i = 0;
    int t = btree->header.t;
    int res = 0;

    while (i < node->count_keys && key > node->items[i].key)
        i++;

    if (i < node->count_keys && key == node->items[i].key) {
        if (node->is_leaf) {
            memmove(node->items + i, node->items + i + 1, (node->count_keys - i - 1) * sizeof(*node->items));
            node->count_keys--;
            node->items[node->count_keys] = (Item){0};
            btree_node_write(btree, node);
            return BTREE_OK;
        }

        Btree_Node *y = btree_node_read(btree, node->children[i]);

        if (y->count_keys >= t) {
            Item pred = btree_node_get_pred(btree, node, i);
            node->items[i] = pred;
            btree_node_write(btree, node);
            res = btree_node_delete(btree, y, pred.key);
            btree_node_destroy(y);
            return res;
        }

        Btree_Node *z = btree_node_read(btree, node->children[i + 1]);

        if (z->count_keys >= t) {
            btree_node_destroy(y);
            Item post = btree_node_get_post(btree, node, i);
            node->items[i] = post;
            btree_node_write(btree, node);
            res = btree_node_delete(btree, z, post.key);
            btree_node_destroy(z);
            return res;
        }

        btree_node_merge(btree, node, y, z, i);
        res = btree_node_delete(btree, y, key);

        if (btree->root != y)
            btree_node_destroy(y);

        return res;
    }

    if (node->is_leaf)
        return BTREE_ERROR_KEY_NOT_FOUND;

    Btree_Node *x_ci = btree_node_read(btree, node->children[i]);

    if (x_ci->count_keys > t - 1) {
        res = btree_node_delete(btree, x_ci, key);

        if (btree->root != x_ci)
            btree_node_destroy(x_ci);

        return res;
    }

    Btree_Node *sibbling_left = i == 0 ? NULL : btree_node_read(btree, node->children[i - 1]);
    Btree_Node *sibbling_right = i == node->count_keys ? NULL : btree_node_read(btree, node->children[i + 1]);

    if (btree_node_redistribute(btree, node, x_ci, sibbling_left, sibbling_right, i)) {
        if (sibbling_left)
            btree_node_destroy(sibbling_left);
        if (sibbling_right)
            btree_node_destroy(sibbling_right);
        res = btree_node_delete(btree, x_ci, key);
    } else {
        x_ci = btree_node_concatenate(btree, node, x_ci, sibbling_left, sibbling_right, i);
        res = btree_node_delete(btree, x_ci, key);
    }

    if (btree->root != x_ci)
        btree_node_destroy(x_ci);

    return res;
}

Item btree_node_get_pred(Btree *btree, Btree_Node *node, int i) {
    Btree_Node *pred = btree_node_read(btree, node->children[i]);

    while (!pred->is_leaf) {
        Btree_Node *prev = pred;
        pred = btree_node_read(btree, pred->children[pred->count_keys]);
        btree_node_destroy(prev);
    }

    Item item = pred->items[pred->count_keys - 1];
    btree_node_destroy(pred);
    return item;
}

Item btree_node_get_post(Btree *btree, Btree_Node *node, int i) {
    Btree_Node *post = btree_node_read(btree, node->children[i + 1]);

    while (!post->is_leaf) {
        Btree_Node *prev = post;
        post = btree_node_read(btree, post->children[0]);
        btree_node_destroy(prev);
    }

    Item item = post->items[0];
    btree_node_destroy(post);
    return item;
}

void btree_remove_node(Btree *btree, Btree_Node *x) {
    long offset = x->offset;
    x->count_keys = 0;
    x->is_leaf = 0;
    memset(x->items, 0, (btree->header.M - 1) * sizeof(*x->items));
    memset(x->children, 0, btree->header.M * sizeof(*x->children));
    btree_node_write(btree, x);
    lseek(btree->fd, offset, SEEK_SET);
    write(btree->fd, &btree->header.next_free_offset, sizeof(btree->header.next_free_offset));
    btree->header.next_free_offset = x->offset;
    btree_node_destroy(x);
    btree->header.count_nodes--;
}

void btree_node_merge(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    int t = btree->header.t;
    y->items[y->count_keys] = x->items[i];
    y->count_keys++;
    memmove(x->items + i, x->items + i + 1, (x->count_keys - i - 1) * sizeof(*x->items));
    memmove(x->children + i + 1, x->children + i + 2, (x->count_keys - i - 1) * sizeof(*x->children));
    x->count_keys--;
    memcpy(y->items + y->count_keys, z->items, (t - 1) * sizeof(*y->items));

    if (!y->is_leaf)
        memcpy(y->children + y->count_keys, z->children, t * sizeof(*y->children));

    y->count_keys = 2 * t - 1;

    btree_node_write(btree, x);

    if (btree->root == x && x->count_keys == 0) {
        btree_remove_node(btree, x);
        btree->root = y;
    }

    btree_remove_node(btree, z);
    btree_node_write(btree, y);
}

int btree_node_redistribute(Btree *btree, Btree_Node *x, Btree_Node *x_ci, Btree_Node *sibbling_left,
                            Btree_Node *sibbling_right, int i) {
    int t = btree->header.t;

    if (sibbling_left && sibbling_left->count_keys >= t) {
        btree_node_rotate_right(btree, x, sibbling_left, x_ci, i - 1);
        return 1;
    }

    if (sibbling_right && sibbling_right->count_keys >= t) {
        btree_node_rotate_left(btree, x, x_ci, sibbling_right, i);
        return 1;
    }

    return 0;
}

Btree_Node *btree_node_concatenate(Btree *btree, Btree_Node *x, Btree_Node *x_ci, Btree_Node *sibbling_left,
                                   Btree_Node *sibbling_right, int i) {
    if (sibbling_left) {
        btree_node_merge(btree, x, sibbling_left, x_ci, i - 1);

        if (sibbling_right)
            btree_node_destroy(sibbling_right);

        return sibbling_left;
    }

    btree_node_merge(btree, x, x_ci, sibbling_right, i);

    if (sibbling_left)
        btree_node_destroy(sibbling_left);

    return x_ci;
}

void btree_node_rotate_left(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    y->items[y->count_keys] = x->items[i];

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = z->children[0];

    y->count_keys++;

    x->items[i] = z->items[0];

    memmove(z->items, z->items + 1, (z->count_keys - 1) * sizeof(*z->items));

    if (!z->is_leaf)
        memmove(z->children, z->children + 1, z->count_keys * sizeof(*z->children));

    z->count_keys--;
    z->items[z->count_keys] = (Item){0};

    if (!z->is_leaf)
        z->children[z->count_keys + 1] = 0;

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
}

void btree_node_rotate_right(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    memmove(z->items + 1, z->items, z->count_keys * sizeof(*z->items));

    if (!z->is_leaf)
        memmove(z->children + 1, z->children, (z->count_keys + 1) * sizeof(*z->children));

    z->items[0] = x->items[i];

    if (!z->is_leaf)
        z->children[0] = y->children[y->count_keys];

    z->count_keys++;

    x->items[i] = y->items[y->count_keys - 1];
    y->count_keys--;
    y->items[y->count_keys] = (Item){0};

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = 0;

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
}

const char *btree_strerr(int err) {
    switch (err) {
    case BTREE_OK:
        return "Ok";
    case BTREE_ERROR_SMALL_T:
        return "Param t must be >= 2";
    case BTREE_ERROR_KEY_NOT_FOUND:
        return "Key not found";
    case BTREE_ERROR_KEY_EXISTS:
        return "Key already exists";
    case BTREE_ERROR_NULLPTR:
        return "Null pointer to struct";
    case BTREE_ERROR_FORMAT:
        return "Invalid file format";
    case BTREE_ERROR:
        return strerror(errno); // fallback to errno
    default:
        return "Unknown";
    }
}

void btree_header_read(Btree *btree, int *magic_bytes) {
    const int n = 2;
    struct iovec vec[n];
    vec[0].iov_base = magic_bytes;
    vec[0].iov_len = sizeof(*magic_bytes);
    vec[1].iov_base = &btree->header;
    vec[1].iov_len = sizeof(btree->header);
    preadv(btree->fd, vec, n, 0);
}
