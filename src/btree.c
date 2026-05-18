#include "btree.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define HEADER_BYTES {0x7f, 'B', 'T', 'F'}
#define HEADER_BYTES_LEN 4
#define BTREE_SIZEOF_HEADER (HEADER_BYTES_LEN + 2 * sizeof(int) + 3 * sizeof(long))

#define BTREE_SIZEOF_NODE(btree)                                                                                       \
    (sizeof(btree->root->count_keys) + (btree->M - 1) * sizeof(*btree->root->buf) +                                    \
     btree->M * sizeof(*btree->root->children))

/* === IO === */
void btree_node_destroy(Btree_Node *node);

void btree_write_header(Btree *btree);

int btree_pop_free_offset(Btree *btree);
/* === IO === */

void btree_node_destroy(Btree_Node *node);

int btree_node_delete(Btree *btree, Btree_Node *node, int key);

Btree_Node *btree_append_node(Btree *btree);

int btree_node_find(Btree *btree, Btree_Node *x, int key, int *value);

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *x_ci, int i);

int btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value);

void btree_write_header(Btree *btree) {
    lseek(btree->fd, 0, SEEK_SET);
    unsigned char header_bytes[] = HEADER_BYTES;
    write(btree->fd, header_bytes, HEADER_BYTES_LEN * sizeof(*header_bytes));
    write(btree->fd, &btree->t, sizeof(btree->t));
    write(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    write(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    write(btree->fd, &btree->root->offset, sizeof(btree->root->offset));
    write(btree->fd, &btree->next_free, sizeof(btree->next_free));
}

int btree_init(Btree **btree_ptr, const char *path, int t) {
    struct stat statbuf = (struct stat){0};
    int res = stat(path, &statbuf);

    if (res == -1 && errno == ENOENT) {
        if (t < 2) {
            *btree_ptr = NULL;
            return BTREE_ERROR_SMALL_PARAM_T;
        }

        Btree *btree = malloc(sizeof(*btree));

        if (!btree) {
            *btree_ptr = NULL;
            return BTREE_ERROR_ALLOC;
        }

        btree->t = t;
        btree->M = 2 * t;
        btree->count_nodes = 0;
        btree->next_offset = BTREE_SIZEOF_HEADER;
        btree->fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        if (btree->fd == -1) {
            free(btree);
            *btree_ptr = NULL;
            return BTREE_ERROR_OPEN_FILE;
        }

        btree->next_free = -1;
        btree->root = btree_node_init_v2(btree);
        btree->aux_x = btree_node_init_v2(btree);
        btree->aux_x_ci = btree_node_init_v2(btree);
        btree->aux_y = btree_node_init_v2(btree);
        btree->aux_z = btree_node_init_v2(btree);
        btree->aux_sbl = btree_node_init_v2(btree);
        btree->aux_sbr = btree_node_init_v2(btree);
        btree->count_nodes++;
        btree_write_header(btree);
        btree_node_write_v2(btree, btree->root);
        *btree_ptr = btree;

        return BTREE_OK;
    }

    Btree *btree = malloc(sizeof(*btree));

    if (!btree) {
        *btree_ptr = NULL;
        return BTREE_ERROR_ALLOC;
    }

    btree->fd = open(path, O_RDWR);

    if (btree->fd == -1) {
        free(btree);
        *btree_ptr = NULL;
        return BTREE_ERROR_OPEN_FILE;
    }

    long root_offset = 0;
    unsigned char check_bytes[] = HEADER_BYTES;
    unsigned char header_bytes[HEADER_BYTES_LEN];
    read(btree->fd, header_bytes, HEADER_BYTES_LEN * sizeof(*header_bytes));

    for (int i = 0; i < HEADER_BYTES_LEN; i++) {
        if (header_bytes[i] != check_bytes[i]) {
            close(btree->fd);
            free(btree);
            *btree_ptr = NULL;
            return BTREE_ERROR_FORMAT;
        }
    }

    read(btree->fd, &btree->t, sizeof(btree->t));
    btree->M = 2 * btree->t;
    read(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    read(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    read(btree->fd, &root_offset, sizeof(root_offset));
    read(btree->fd, &btree->next_free, sizeof(btree->next_free));
    btree->root = btree_node_init_v2(btree);
    btree->aux_x = btree_node_init_v2(btree);
    btree->aux_x_ci = btree_node_init_v2(btree);
    btree->aux_y = btree_node_init_v2(btree);
    btree->aux_z = btree_node_init_v2(btree);
    btree->aux_sbl = btree_node_init_v2(btree);
    btree->aux_sbr = btree_node_init_v2(btree);
    btree_node_read_v2(btree, root_offset, btree->root);
    *btree_ptr = btree;

    return BTREE_OK;
}

int btree_find(Btree *btree, int key, int *value) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    int ok = btree_node_find(btree, btree->root, key, value);
    btree_node_clear_v2(btree, btree->aux_x_ci);
    return ok;
}

int btree_insert(Btree *btree, int key, int value) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    if (btree->root->count_keys < btree->M - 1) {
        int res = btree_node_insert_nonfull(btree, btree->root, key, value);
        btree_write_header(btree);
        return res;
    }

    Btree_Node *s = btree->aux_y;
    btree->count_nodes++;
    int offset = btree_pop_free_offset(btree);
    s->offset = offset;
    s->is_leaf = 0;
    s->count_keys = 0;
    s->children[0] = btree->root->offset;
    btree_node_split_child(btree, s, btree->root, 0);
    btree_node_clear_v2(btree, btree->root);
    btree->aux_y = btree->root;
    btree->root = s;
    s->offset = offset;
    btree_write_header(btree);
    int res = btree_node_insert_nonfull(btree, s, key, value);
    return res;
}

int btree_delete(Btree *btree, int key) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    btree_node_delete(btree, btree->root, key);
    btree_write_header(btree);
    return BTREE_OK;
}

int btree_destroy(Btree *btree) {
    if (!btree) {
        return BTREE_ERROR_NULLPTR;
    }

    btree_node_destroy(btree->root);
    btree_node_destroy(btree->aux_x);
    btree_node_destroy(btree->aux_x_ci);
    btree_node_destroy(btree->aux_y);
    btree_node_destroy(btree->aux_z);
    btree_node_destroy(btree->aux_sbl);
    btree_node_destroy(btree->aux_sbr);

    if (close(btree->fd) == -1) {
        free(btree);
        return BTREE_ERROR_CLOSE_FILE;
    }

    free(btree);
    return BTREE_OK;
}

void btree_node_destroy(Btree_Node *node) {
    free(node->buf);
    free(node->children);
    free(node);
}

int btree_node_find(Btree *btree, Btree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->buf[i].key)
        i++;

    if (i < x->count_keys && key == x->buf[i].key) {
        if (value)
            *value = x->buf[i].value;
        return BTREE_OK;
    }

    if (x->is_leaf)
        return BTREE_ERROR_KEY_NOT_FOUND;

    btree_node_read_v2(btree, x->children[i], btree->aux_x_ci);
    int hit = btree_node_find(btree, btree->aux_x_ci, key, value);
    return hit;
}

int btree_pop_free_offset(Btree *btree) {
    if (btree->next_free == -1) {
        long offset = btree->next_offset;
        btree->next_offset += BTREE_SIZEOF_NODE(btree);
        return offset;
    }

    long offset = btree->next_free;
    lseek(btree->fd, btree->next_free, SEEK_SET);
    read(btree->fd, &btree->next_free, sizeof(btree->next_free));
    return offset;
}

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *y, int i) {
    Btree_Node *z = btree->aux_z;
    z->offset = btree_pop_free_offset(btree);
    btree->count_nodes++;
    int t = btree->t;

    z->is_leaf = y->is_leaf;
    z->count_keys = t - 1;

    memcpy(z->buf, y->buf + t, (t - 1) * sizeof(*z->buf));

    if (!y->is_leaf)
        memcpy(z->children, y->children + t, t * sizeof(*z->children));

    y->count_keys = t - 1;

    memmove(x->children + i + 1, x->children + i, (x->count_keys - i + 1) * sizeof(*x->children));

    x->children[i + 1] = z->offset;

    memmove(x->buf + i + 1, x->buf + i, (x->count_keys - i) * sizeof(*x->buf));

    x->buf[i] = y->buf[t - 1];
    x->count_keys++;

    memset(y->buf + y->count_keys, 0, t * sizeof(*y->buf));

    if (!y->is_leaf)
        memset(y->children + y->count_keys + 1, 0, t * sizeof(*y->children));

    btree_node_write_v2(btree, x);
    btree_node_write_v2(btree, y);
    btree_node_write_v2(btree, z);
    btree_node_clear_v2(btree, z);
}

int btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value) {
    int i = x->count_keys - 1;

    if (x->is_leaf) {
        while (i >= 0 && key < x->buf[i].key) {
            x->buf[i + 1] = x->buf[i];
            i--;
        }

        if (i >= 0 && x->buf[i].key == key) {
            return BTREE_ERROR_KEY_ALREADY_EXISTS;
        }

        x->buf[i + 1] = (Item){.key = key, .value = value};
        x->count_keys++;
        btree_node_write_v2(btree, x);
        return BTREE_OK;
    }

    while (i >= 0 && key < x->buf[i].key)
        i--;

    i++;

    Btree_Node *aux = NULL;
    if (x == btree->aux_x_ci) {
        aux = btree->aux_y;
    } else {
        aux = btree->aux_x_ci;
    }

    btree_node_read_v2(btree, x->children[i], aux);
    Btree_Node *x_ci = aux;

    if (x_ci->count_keys < btree->M - 1) {
        int res = btree_node_insert_nonfull(btree, x_ci, key, value);
        btree_node_clear_v2(btree, x_ci);
        return res;
    }

    btree_node_split_child(btree, x, x_ci, i);

    if (key > x->buf[i].key) {
        i++;
        btree_node_clear_v2(btree, x_ci);
        btree_node_read_v2(btree, x->children[i], x_ci);
    }

    int res = btree_node_insert_nonfull(btree, x_ci, key, value);
    btree_node_clear_v2(btree, x_ci);
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
    int t = btree->t;
    int res = 0;

    while (i < node->count_keys && key > node->buf[i].key)
        i++;

    if (i < node->count_keys && key == node->buf[i].key) {
        if (node->is_leaf) {
            memmove(node->buf + i, node->buf + i + 1, (node->count_keys - i - 1) * sizeof(*node->buf));
            node->count_keys--;
            node->buf[node->count_keys] = (Item){0};
            btree_node_write_v2(btree, node);
            return BTREE_OK;
        }

        btree_node_read_v2(btree, node->children[i], btree->aux_y);
        Btree_Node *y = btree->aux_y;

        if (y->count_keys >= t) {
            Item pred = btree_node_get_pred(btree, node, i);
            node->buf[i] = pred;
            btree_node_write_v2(btree, node);
            res = btree_node_delete(btree, y, pred.key);
            btree_node_clear_v2(btree, y);
            return res;
        }

        btree_node_read_v2(btree, node->children[i + 1], btree->aux_z);
        Btree_Node *z = btree->aux_z;

        if (z->count_keys >= t) {
            btree_node_destroy(y);
            Item post = btree_node_get_post(btree, node, i);
            node->buf[i] = post;
            btree_node_write_v2(btree, node);
            res = btree_node_delete(btree, z, post.key);
            btree_node_clear_v2(btree, z);
            return res;
        }

        btree_node_merge(btree, node, y, z, i);
        res = btree_node_delete(btree, y, key);

        if (btree->root->offset != y->offset)
            btree_node_clear_v2(btree, y);

        return res;
    }

    if (node->is_leaf)
        return BTREE_ERROR_KEY_NOT_FOUND;

    Btree_Node *aux = NULL;

    if (node == btree->aux_x_ci) {
        aux = btree->aux_x;
    } else {
        aux = btree->aux_x_ci;
    }

    btree_node_read_v2(btree, node->children[i], aux);
    Btree_Node *x_ci = aux;

    if (x_ci->count_keys > t - 1) {
        res = btree_node_delete(btree, x_ci, key);

        if (btree->root->offset != x_ci->offset)
            btree_node_clear_v2(btree, x_ci);

        return res;
    }

    Btree_Node *sibbling_left = NULL;
    Btree_Node *sibbling_right = NULL;

    if (i == 0) {
        sibbling_left = NULL;
    } else {
        btree_node_read_v2(btree, node->children[i - 1], btree->aux_sbl);
        sibbling_left = btree->aux_sbl;
    }

    if (i == node->count_keys) {
        sibbling_right = NULL;
    } else {
        btree_node_read_v2(btree, node->children[i + 1], btree->aux_sbr);
        sibbling_right = btree->aux_sbr;
    }

    if (btree_node_redistribute(btree, node, x_ci, sibbling_left, sibbling_right, i)) {
        if (sibbling_left)
            btree_node_clear_v2(btree, sibbling_left);
        if (sibbling_right)
            btree_node_clear_v2(btree, sibbling_right);
        res = btree_node_delete(btree, x_ci, key);
    } else {
        x_ci = btree_node_concatenate(btree, node, x_ci, sibbling_left, sibbling_right, i);
        res = btree_node_delete(btree, x_ci, key);
    }

    if (btree->root->offset != aux->offset)
        btree_node_clear_v2(btree, aux);

    return res;
}

Item btree_node_get_pred(Btree *btree, Btree_Node *node, int i) {
    btree_node_read_v2(btree, node->children[i], btree->aux_sbl);
    Btree_Node *pred = btree->aux_sbl;

    while (!pred->is_leaf) {
        btree_node_read_v2(btree, pred->children[pred->count_keys], pred);
    }

    Item item = pred->buf[pred->count_keys - 1];
    btree_node_destroy(pred);
    return item;
}

Item btree_node_get_post(Btree *btree, Btree_Node *node, int i) {
    btree_node_read_v2(btree, node->children[i + 1], btree->aux_sbr);
    Btree_Node *post = btree->aux_sbr;

    while (!post->is_leaf) {
        btree_node_read_v2(btree, post->children[0], post);
    }

    Item item = post->buf[0];
    btree_node_destroy(post);
    return item;
}

void btree_remove_node(Btree *btree, Btree_Node *x) {
    long offset = x->offset;
    x->count_keys = 0;
    x->is_leaf = 0;
    memset(x->buf, 0, (btree->M - 1) * sizeof(*x->buf));
    memset(x->children, 0, btree->M * sizeof(*x->children));
    btree_node_write_v2(btree, x);
    lseek(btree->fd, offset, SEEK_SET);
    write(btree->fd, &btree->next_free, sizeof(btree->next_free));
    btree->next_free = x->offset;
    btree_node_clear_v2(btree, x);
    btree->count_nodes--;
}

void btree_node_merge(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    int t = btree->t;
    y->buf[y->count_keys] = x->buf[i];
    y->count_keys++;
    memmove(x->buf + i, x->buf + i + 1, (x->count_keys - i - 1) * sizeof(*x->buf));
    memmove(x->children + i + 1, x->children + i + 2, (x->count_keys - i - 1) * sizeof(*x->children));
    x->count_keys--;
    memcpy(y->buf + y->count_keys, z->buf, (t - 1) * sizeof(*y->buf));

    if (!y->is_leaf)
        memcpy(y->children + y->count_keys, z->children, t * sizeof(*y->children));

    y->count_keys = 2 * t - 1;

    btree_node_write_v2(btree, x);

    if (btree->root == x && x->count_keys == 0) {
        btree_remove_node(btree, x);
        btree->root = y;
    }

    btree_remove_node(btree, z);
    btree_node_write_v2(btree, y);
}

int btree_node_redistribute(Btree *btree, Btree_Node *x, Btree_Node *x_ci, Btree_Node *sibbling_left,
                            Btree_Node *sibbling_right, int i) {
    int t = btree->t;

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
            btree_node_clear_v2(btree, sibbling_right);

        return sibbling_left;
    }

    btree_node_merge(btree, x, x_ci, sibbling_right, i);

    if (sibbling_left)
        btree_node_clear_v2(btree, sibbling_left);

    return x_ci;
}

void btree_node_rotate_left(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    y->buf[y->count_keys] = x->buf[i];

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = z->children[0];

    y->count_keys++;

    x->buf[i] = z->buf[0];

    memmove(z->buf, z->buf + 1, (z->count_keys - 1) * sizeof(*z->buf));

    if (!z->is_leaf)
        memmove(z->children, z->children + 1, z->count_keys * sizeof(*z->children));

    z->count_keys--;
    z->buf[z->count_keys] = (Item){0};

    if (!z->is_leaf)
        z->children[z->count_keys + 1] = 0;

    btree_node_write_v2(btree, x);
    btree_node_write_v2(btree, y);
    btree_node_write_v2(btree, z);
}

void btree_node_rotate_right(Btree *btree, Btree_Node *x, Btree_Node *y, Btree_Node *z, int i) {
    memmove(z->buf + 1, z->buf, z->count_keys * sizeof(*z->buf));

    if (!z->is_leaf)
        memmove(z->children + 1, z->children, (z->count_keys + 1) * sizeof(*z->children));

    z->buf[0] = x->buf[i];

    if (!z->is_leaf)
        z->children[0] = y->children[y->count_keys];

    z->count_keys++;

    x->buf[i] = y->buf[y->count_keys - 1];
    y->count_keys--;
    y->buf[y->count_keys] = (Item){0};

    if (!y->is_leaf)
        y->children[y->count_keys + 1] = 0;

    btree_node_write_v2(btree, x);
    btree_node_write_v2(btree, y);
    btree_node_write_v2(btree, z);
}

const char *btree_strerr(int err) {
    switch (err) {
    case BTREE_OK:
        return "Ok";
    case BTREE_ERROR_SMALL_PARAM_T:
        return "Param t must be >= 2";
    case BTREE_ERROR_KEY_NOT_FOUND:
        return "Key not found";
    case BTREE_ERROR_KEY_ALREADY_EXISTS:
        return "Key already exists";
    case BTREE_ERROR_ALLOC:
        return "Error to alloc memory";
    case BTREE_ERROR_OPEN_FILE:
        return "Error to open the file";
    case BTREE_ERROR_CLOSE_FILE:
        return "Error to close the file";
    case BTREE_ERROR_NULLPTR:
        return "Null pointer to struct";
    case BTREE_ERROR_FORMAT:
        return "Invalid file format";
    default:
        return "Unknown";
    }
}

void btree_node_clear_v2(Btree *btree, Btree_Node *node) {
    node->offset = 0;
    node->is_leaf = 0;
    node->count_keys = 0;

    for (int i = 0; i < btree->M - 1; i++) {
        node->buf[i] = (Item){0};
        node->children[i] = 0;
    }

    node->children[btree->M - 1] = 0;
}

void btree_node_read_v2(Btree *btree, long offset, Btree_Node *node) {
    lseek(btree->fd, offset, SEEK_SET);
    node->offset = offset;
    read(btree->fd, &node->count_keys, sizeof(node->count_keys));
    read(btree->fd, node->buf, (btree->M - 1) * sizeof(*node->buf));
    read(btree->fd, node->children, btree->M * sizeof(*node->children));
    node->is_leaf = node->children[0] == 0;
}

void btree_node_write_v2(Btree *btree, Btree_Node *node) {
    lseek(btree->fd, node->offset, SEEK_SET);
    write(btree->fd, &node->count_keys, sizeof(node->count_keys));
    write(btree->fd, node->buf, (btree->M - 1) * sizeof(*node->buf));
    write(btree->fd, node->children, btree->M * sizeof(*node->children));
}

Btree_Node *btree_node_init_v2(Btree *btree) {
    Btree_Node *node = malloc(sizeof(*node));

    if (!node) {
        return NULL;
    }

    node->offset = btree_pop_free_offset(btree);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->buf = malloc((btree->M - 1) * sizeof(*node->buf));

    if (!node->buf) {
        free(node);
        return NULL;
    }

    node->children = malloc(btree->M * sizeof(*node->children));

    if (!node->children) {
        free(node->buf);
        free(node);
        return NULL;
    }

    for (int i = 0; i < btree->M - 1; i++) {
        node->buf[i] = (Item){0};
        node->children[i] = 0;
    }

    node->children[btree->M - 1] = 0;
    return node;
}
