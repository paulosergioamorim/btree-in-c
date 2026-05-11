#include "btree.h"
#include "btree_delete.h"
#include "btree_fs.h"
#include "btree_queue.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void btree_write_header(BTree *btree);

int btree_get_node_size(BTree *btree);

int btree_get_header_size(BTree *btree);

BTree_Node *btree_append_node(BTree *btree);

int btree_node_search(BTree *btree, BTree_Node *x, int key, int *value);

void btree_node_split_child(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i);

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value);

BTree_Node *btree_node_read(BTree *btree, long offset);

BTree *btree_init_from_db(char *path) {
    BTree *btree = malloc(sizeof(*btree));
    if (!btree)
        return NULL;
    btree->fd = open(path, O_RDWR | O_CREAT);
    if (btree->fd == -1) {
        free(btree);
        return NULL;
    }
    long root_offset = 0;
    read(btree->fd, &btree->t, sizeof(btree->t));
    btree->M = 2 * btree->t;
    read(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    read(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    read(btree->fd, &root_offset, sizeof(root_offset));
    read(btree->fd, &btree->next_free, sizeof(btree->next_free));
    btree->root = btree_node_read(btree, root_offset);
    btree->size_node = btree_get_node_size(btree);
    return btree;
}

BTree *btree_init_from_memory(char *path, int t) {
    if (t < 2)
        return NULL;
    BTree *btree = malloc(sizeof(*btree));
    if (!btree)
        return NULL;
    btree->t = t;
    btree->M = 2 * t;
    btree->count_nodes = 0;
    btree->next_offset = btree_get_header_size(btree);
    btree->size_node = btree_get_node_size(btree);
    btree->fd = open(path, O_RDWR);
    if (btree->fd == -1) {
        free(btree);
        return NULL;
    }
    btree->next_free = -1;
    btree->root = btree_append_node(btree);
    btree_write_header(btree);
    return btree;
}

int btree_hit(BTree *btree, int key) {
    return btree_node_search(btree, btree->root, key, NULL);
}

int btree_search(BTree *btree, int key, int *value) {
    return btree_node_search(btree, btree->root, key, value);
}

void btree_insert(BTree *btree, int key, int value) {
    int search_value;

    if (btree_search(btree, key, &search_value))
        return;

    if (btree->root->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, btree->root, key, value);
        btree_write_header(btree);
        return;
    }

    BTree_Node *s = btree_append_node(btree);
    s->is_leaf = 0;
    s->count_keys = 0;
    s->children[0] = btree->root->offset;
    btree_node_split_child(btree, s, btree->root, 0);
    btree_node_destroy(btree->root);
    btree->root = s;
    btree_node_insert_nonfull(btree, s, key, value);
    btree_write_header(btree);
}

void btree_delete(BTree *btree, int key) {
    btree_node_delete(btree, btree->root, key);
    btree_write_header(btree);
}

void btree_destroy(BTree *btree) {
    btree_node_destroy(btree->root);
    close(btree->fd);
    free(btree);
}

void btree_node_destroy(BTree_Node *node) {
    free(node->buf);
    free(node->children);
    free(node);
}

int btree_node_search(BTree *btree, BTree_Node *x, int key, int *value) {
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

    BTree_Node *x_ci = btree_node_read_child(btree, x, i);

    int hit = btree_node_search(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
    return hit;
}

int btree_pop_free_offset(BTree *btree) {
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

BTree_Node *btree_append_node(BTree *btree) {
    BTree_Node *node = malloc(sizeof(*node));
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

void btree_node_split_child(BTree *btree, BTree_Node *x, BTree_Node *y, int i) {
    BTree_Node *z = btree_append_node(btree);
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

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value) {
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

    BTree_Node *x_ci = btree_node_read_child(btree, x, i);

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

void btree_display(BTree *btree) {
    long last_level_offset = btree->root->offset;
    BTree_Queue *queue = btree_queue_init(btree->count_nodes);
    btree_queue_enqueue(queue, btree->root->offset);

    while (!btree_queue_is_empty(queue)) {
        long offset = btree_queue_dequeue(queue);
        BTree_Node *node = btree_node_read(btree, offset);

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
