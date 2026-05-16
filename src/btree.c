#include "btree.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BTREE_SIZEOF_HEADER (2 * sizeof(int) + 3 * sizeof(long))

#define BTREE_SIZEOF_NODE(btree)                                                                                       \
    (sizeof(btree->root->count_keys) + (btree->M - 1) * sizeof(*btree->root->buf) +                                    \
     btree->M * sizeof(*btree->root->children))

typedef struct item {
    int key;
    int value;
} Item;

typedef struct btree_node {
    long offset;
    int count_keys;
    int is_leaf;
    Item *buf;
    long *children;
} Btree_Node;

struct btree {
    int t;
    int M;
    int count_nodes;
    int size_node;
    long next_offset;
    long next_free;
    int fd;
    Btree_Node *root;
};

void btree_node_refresh_child(Btree *btree, Btree_Node *node, Btree_Node *x_ci, int new_i);

Btree_Node *btree_node_read_child(Btree *btree, Btree_Node *node, int i);

void btree_node_write(Btree *btree, Btree_Node *node);

void btree_node_destroy(Btree_Node *node);

void btree_write_header(Btree *btree);

void btree_node_destroy(Btree_Node *node);

void btree_node_delete(Btree *btree, Btree_Node *node, int key);

Btree_Node *btree_append_node(Btree *btree);

int btree_node_search(Btree *btree, Btree_Node *x, int key, int *value);

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *x_ci, int i);

void btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value);

Btree_Node *btree_node_read(Btree *btree, long offset);

Btree_Node *btree_node_read(Btree *btree, long offset) {
    if (offset < 0 || offset >= btree->next_offset)
        return NULL;

    lseek(btree->fd, offset, SEEK_SET);
    Btree_Node *node = malloc(sizeof(*node));

    if (!node)
        return NULL;

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
    node->offset = offset;
    read(btree->fd, &node->count_keys, sizeof(node->count_keys));
    read(btree->fd, node->buf, (btree->M - 1) * sizeof(*node->buf));
    read(btree->fd, node->children, btree->M * sizeof(*node->children));
    node->is_leaf = node->children[0] == 0;
    return node;
}

Btree_Node *btree_node_read_child(Btree *btree, Btree_Node *node, int i) {
    if (i < 0 || i > node->count_keys)
        return NULL;

    long offset = node->children[i];
    return btree_node_read(btree, offset);
}

void btree_node_write(Btree *btree, Btree_Node *node) {
    long offset = node->offset;
    if (offset < 0 || offset >= btree->next_offset)
        return;
    lseek(btree->fd, offset, SEEK_SET);
    write(btree->fd, &node->count_keys, sizeof(node->count_keys));
    write(btree->fd, node->buf, (btree->M - 1) * sizeof(*node->buf));
    write(btree->fd, node->children, btree->M * sizeof(*node->children));
}

void btree_write_header(Btree *btree) {
    lseek(btree->fd, 0, SEEK_SET);
    write(btree->fd, &btree->t, sizeof(btree->t));
    write(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    write(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    write(btree->fd, &btree->root->offset, sizeof(btree->root->offset));
    write(btree->fd, &btree->next_free, sizeof(btree->next_free));
}

int btree_get_node_size(Btree *btree) {
    return sizeof(btree->root->count_keys) + (btree->M - 1) * sizeof(*btree->root->buf) +
           btree->M * sizeof(*btree->root->children);
}

int btree_get_header_size(Btree *btree) {
    return sizeof(btree->t) + sizeof(btree->count_nodes) + sizeof(btree->next_offset) + sizeof(btree->root->offset) +
           sizeof(btree->next_free);
}

void btree_node_refresh_child(Btree *btree, Btree_Node *node, Btree_Node *x_ci, int new_i) {
    long offset = node->children[new_i];
    if (offset < 0 || offset >= btree->next_offset)
        return;
    lseek(btree->fd, offset, SEEK_SET);
    x_ci->offset = offset;
    read(btree->fd, &x_ci->count_keys, sizeof(x_ci->count_keys));
    read(btree->fd, x_ci->buf, (btree->M - 1) * sizeof(*x_ci->buf));
    read(btree->fd, x_ci->children, btree->M * sizeof(*x_ci->children));
    x_ci->is_leaf = x_ci->children[0] == 0;
}

int btree_init(Btree **btree_ptr, Btree_Opts opts) {
    if (opts.override_file) {
        if (opts.t < 2) {
            *btree_ptr = NULL;
            return BTREE_ERROR_TO_SMALL_PARAM_T;
        }

        Btree *btree = malloc(sizeof(*btree));

        if (!btree) {
            *btree_ptr = NULL;
            return BTREE_ERROR_ALLOC;
        }

        btree->t = opts.t;
        btree->M = 2 * opts.t;
        btree->count_nodes = 0;
        btree->next_offset = BTREE_SIZEOF_HEADER;
        btree->size_node = BTREE_SIZEOF_NODE(btree);
        btree->fd = open(opts.path, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);

        if (btree->fd == -1) {
            free(btree);
            *btree_ptr = NULL;
            return BTREE_ERROR_OPEN_FILE;
        }

        btree->next_free = -1;
        btree->root = btree_append_node(btree);
        btree_write_header(btree);
        *btree_ptr = btree;

        return BTREE_OK;
    }

    Btree *btree = malloc(sizeof(*btree));

    if (!btree) {
        *btree_ptr = NULL;
        return BTREE_ERROR_ALLOC;
    }

    btree->fd = open(opts.path, O_RDWR);

    if (btree->fd == -1) {
        free(btree);
        *btree_ptr = NULL;
        return BTREE_ERROR_OPEN_FILE;
    }

    long root_offset = 0;
    read(btree->fd, &btree->t, sizeof(btree->t));
    btree->M = 2 * btree->t;
    read(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    read(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    read(btree->fd, &root_offset, sizeof(root_offset));
    read(btree->fd, &btree->next_free, sizeof(btree->next_free));
    btree->root = btree_node_read(btree, root_offset);
    btree->size_node = BTREE_SIZEOF_NODE(btree);
    *btree_ptr = btree;

    return BTREE_OK;
}

int btree_search(Btree *btree, int key, int *value) {
    return btree_node_search(btree, btree->root, key, value);
}

void btree_insert(Btree *btree, int key, int value) {
    int search_value;

    if (btree_search(btree, key, &search_value))
        return;

    if (btree->root->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, btree->root, key, value);
        btree_write_header(btree);
        return;
    }

    Btree_Node *s = btree_append_node(btree);
    s->is_leaf = 0;
    s->count_keys = 0;
    s->children[0] = btree->root->offset;
    btree_node_split_child(btree, s, btree->root, 0);
    btree_node_destroy(btree->root);
    btree->root = s;
    btree_node_insert_nonfull(btree, s, key, value);
    btree_write_header(btree);
}

void btree_delete(Btree *btree, int key) {
    btree_node_delete(btree, btree->root, key);
    btree_write_header(btree);
}

void btree_destroy(Btree *btree) {
    btree_node_destroy(btree->root);
    close(btree->fd);
    free(btree);
}

void btree_node_destroy(Btree_Node *node) {
    free(node->buf);
    free(node->children);
    free(node);
}

int btree_node_search(Btree *btree, Btree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->buf[i].key)
        i++;

    if (i < x->count_keys && key == x->buf[i].key) {
        if (value)
            *value = x->buf[i].value;
        return 1;
    }

    if (x->is_leaf)
        return 0;

    Btree_Node *x_ci = btree_node_read_child(btree, x, i);

    int hit = btree_node_search(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
    return hit;
}

int btree_pop_free_offset(Btree *btree) {
    if (btree->next_free == -1) {
        long offset = btree->next_offset;
        btree->next_offset += btree->size_node;
        return offset;
    }

    long offset = btree->next_free;
    lseek(btree->fd, btree->next_free, SEEK_SET);
    read(btree->fd, &btree->next_free, sizeof(btree->next_free));
    return offset;
}

Btree_Node *btree_append_node(Btree *btree) {
    Btree_Node *node = malloc(sizeof(*node));
    if (!node)
        return NULL;
    node->offset = btree_pop_free_offset(btree);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->buf = calloc((btree->M - 1), sizeof(*node->buf));
    if (!node->buf) {
        free(node);
        return NULL;
    }
    node->children = calloc(btree->M, sizeof(*node->children));
    if (!node->children) {
        free(node->buf);
        free(node);
        return NULL;
    }
    btree->count_nodes++;
    btree_node_write(btree, node);
    return node;
}

void btree_node_split_child(Btree *btree, Btree_Node *x, Btree_Node *y, int i) {
    Btree_Node *z = btree_append_node(btree);
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

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
    btree_node_destroy(z);
}

void btree_node_insert_nonfull(Btree *btree, Btree_Node *x, int key, int value) {
    int i = x->count_keys - 1;

    if (x->is_leaf) {
        while (i >= 0 && key < x->buf[i].key) {
            x->buf[i + 1] = x->buf[i];
            i--;
        }

        x->buf[i + 1] = (Item){.key = key, .value = value};
        x->count_keys++;
        btree_node_write(btree, x);
        return;
    }

    while (i >= 0 && key < x->buf[i].key)
        i--;

    i++;

    Btree_Node *x_ci = btree_node_read_child(btree, x, i);

    if (x_ci->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, x_ci, key, value);
        btree_node_destroy(x_ci);
        return;
    }

    btree_node_split_child(btree, x, x_ci, i);

    if (key > x->buf[i].key) {
        i++;
        btree_node_refresh_child(btree, x, x_ci, i);
    }

    btree_node_insert_nonfull(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
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

void btree_node_delete(Btree *btree, Btree_Node *node, int key) {
    int i = 0;
    int t = btree->t;

    while (i < node->count_keys && key > node->buf[i].key)
        i++;

    if (i < node->count_keys && key == node->buf[i].key) {
        if (node->is_leaf) {
            memmove(node->buf + i, node->buf + i + 1, (node->count_keys - i - 1) * sizeof(*node->buf));
            node->count_keys--;
            node->buf[node->count_keys] = (Item){0};
            btree_node_write(btree, node);
            return;
        }

        Btree_Node *y = btree_node_read_child(btree, node, i);

        if (y->count_keys >= t) {
            Item pred = btree_node_get_pred(btree, node, i);
            node->buf[i] = pred;
            btree_node_write(btree, node);
            btree_node_delete(btree, y, pred.key);
            btree_node_destroy(y);
            return;
        }

        Btree_Node *z = btree_node_read_child(btree, node, i + 1);

        if (z->count_keys >= t) {
            btree_node_destroy(y);
            Item post = btree_node_get_post(btree, node, i);
            node->buf[i] = post;
            btree_node_write(btree, node);
            btree_node_delete(btree, z, post.key);
            btree_node_destroy(z);
            return;
        }

        btree_node_merge(btree, node, y, z, i);
        btree_node_delete(btree, y, key);

        if (btree->root != y)
            btree_node_destroy(y);

        return;
    }

    if (node->is_leaf)
        return;

    Btree_Node *x_ci = btree_node_read_child(btree, node, i);

    if (x_ci->count_keys > t - 1) {
        btree_node_delete(btree, x_ci, key);

        if (btree->root != x_ci)
            btree_node_destroy(x_ci);

        return;
    }

    Btree_Node *sibbling_left = btree_node_read_child(btree, node, i - 1);
    Btree_Node *sibbling_right = btree_node_read_child(btree, node, i + 1);

    if (btree_node_redistribute(btree, node, x_ci, sibbling_left, sibbling_right, i)) {
        if (sibbling_left)
            btree_node_destroy(sibbling_left);
        if (sibbling_right)
            btree_node_destroy(sibbling_right);
        btree_node_delete(btree, x_ci, key);
    }

    else {
        x_ci = btree_node_concatenate(btree, node, x_ci, sibbling_left, sibbling_right, i);
        btree_node_delete(btree, x_ci, key);
    }

    if (btree->root != x_ci)
        btree_node_destroy(x_ci);
}

Item btree_node_get_pred(Btree *btree, Btree_Node *node, int i) {
    Btree_Node *pred = btree_node_read_child(btree, node, i);

    while (!pred->is_leaf) {
        Btree_Node *prev = pred;
        pred = btree_node_read_child(btree, pred, pred->count_keys);
        btree_node_destroy(prev);
    }

    Item item = pred->buf[pred->count_keys - 1];
    btree_node_destroy(pred);
    return item;
}

Item btree_node_get_post(Btree *btree, Btree_Node *node, int i) {
    Btree_Node *post = btree_node_read_child(btree, node, i + 1);

    while (!post->is_leaf) {
        Btree_Node *prev = post;
        post = btree_node_read_child(btree, post, 0);
        btree_node_destroy(prev);
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
    btree_node_write(btree, x);
    lseek(btree->fd, offset, SEEK_SET);
    write(btree->fd, &btree->next_free, sizeof(btree->next_free));
    btree->next_free = x->offset;
    btree_node_destroy(x);
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
            btree_node_destroy(sibbling_right);

        return sibbling_left;
    }

    btree_node_merge(btree, x, x_ci, sibbling_right, i);

    if (sibbling_left)
        btree_node_destroy(sibbling_left);

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

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
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

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
}

#ifdef BTREE_DISPLAY_IMPLEMENTATION
#include <stdio.h>

typedef struct btree_queue {
    long capacity;
    long *buf;
    long count;
    long head;
} Btree_Queue;

Btree_Queue *btree_queue_init(int capacity);

void btree_queue_enqueue(Btree_Queue *queue, long offset);

int btree_queue_is_empty(Btree_Queue *queue);

long btree_queue_dequeue(Btree_Queue *queue);

void btree_queue_destroy(Btree_Queue *queue);

Btree_Queue *btree_queue_init(int capacity) {
    Btree_Queue *queue = malloc(sizeof(*queue));
    if (!queue)
        return NULL;
    queue->count = 0;
    queue->head = 0;
    queue->buf = malloc(capacity * sizeof(*queue->buf));
    queue->capacity = capacity;
    if (!queue->buf) {
        free(queue);
        return NULL;
    }
    return queue;
}

void btree_queue_enqueue(Btree_Queue *queue, long offset) {
    if (queue->count >= queue->capacity)
        return;
    int idx = (queue->head + queue->count) % queue->capacity;
    queue->buf[idx] = offset;
    queue->count++;
}

int btree_queue_is_empty(Btree_Queue *queue) {
    return queue->count == 0;
}

long btree_queue_dequeue(Btree_Queue *queue) {
    if (queue->count == 0)
        return -1;
    int offset = queue->buf[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return offset;
}

void btree_queue_destroy(Btree_Queue *queue) {
    free(queue->buf);
    free(queue);
}

void btree_display(Btree *btree) {
    long last_level_offset = btree->root->offset;
    Btree_Queue *queue = btree_queue_init(btree->count_nodes);
    btree_queue_enqueue(queue, btree->root->offset);

    while (!btree_queue_is_empty(queue)) {
        long offset = btree_queue_dequeue(queue);
        Btree_Node *node = btree_node_read(btree, offset);

        printf("[ ");

        for (int i = 0; i < node->count_keys; i++)
            printf("%d ", node->buf[i].key);

        printf("] ");

        if (offset == last_level_offset) {
            last_level_offset = node->children[node->count_keys];
            printf("\n");
        }

        if (node->is_leaf) {
            btree_node_destroy(node);
            continue;
        }

        for (int i = 0; i <= node->count_keys; i++)
            btree_queue_enqueue(queue, node->children[i]);

        btree_node_destroy(node);
    }

    btree_queue_destroy(queue);
}

#endif
