#include "btree.h"
#include "btree_delete.h"
#include "queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void btree_write_header(BTree *btree);

int btree_get_node_size(BTree *btree);

int btree_get_header_size(BTree *btree);

BTree_Node *btree_append_node(BTree *btree);

int btree_node_search(BTree *btree, BTree_Node *x, int key, int *value);

void btree_node_split_child(BTree *btree, BTree_Node *x, BTree_Node *x_ci, int i);

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value);

BTree_Node *btree_node_read(BTree *btree, int offset);

BTree *btree_init_from_db(char *path) {
    BTree *btree = malloc(sizeof(*btree));
    assert(btree);
    btree->fp = fopen(path, "rb+");
    assert(btree->fp);
    int root_offset = 0;
    fread(&btree->t, sizeof(btree->t), 1, btree->fp);
    btree->M = 2 * btree->t;
    fread(&btree->count_nodes, sizeof(btree->count_nodes), 1, btree->fp);
    fread(&btree->next_offset, sizeof(btree->next_offset), 1, btree->fp);
    fread(&root_offset, sizeof(root_offset), 1, btree->fp);
    fread(&btree->next_free, sizeof(btree->next_free), 1, btree->fp);
    btree->root = btree_node_read(btree, root_offset);
    btree->size_node = btree_get_node_size(btree);
    return btree;
}

BTree *btree_init_from_memory(char *path, int t) {
    BTree *btree = malloc(sizeof(*btree));
    assert(btree);
    btree->t = t;
    btree->M = 2 * t;
    btree->count_nodes = 0;
    btree->next_offset = btree_get_header_size(btree);
    btree->size_node = btree_get_node_size(btree);
    btree->fp = fopen(path, "wb+");
    btree->next_free = -1;
    btree->root = btree_append_node(btree);
    btree_write_header(btree);
    assert(btree->fp);
    return btree;
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

int btree_delete(BTree *btree, int key) {
    int hit = btree_node_delete(btree, btree->root, key);
    btree_write_header(btree);
    return hit;
}

void btree_destroy(BTree *btree) {
    assert(btree);
    btree_node_destroy(btree->root);
    fclose(btree->fp);
    free(btree);
}

void btree_node_destroy(BTree_Node *node) {
    assert(node);
    free(node->buf);
    free(node->children);
    free(node);
}

int btree_node_search(BTree *btree, BTree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->buf[i].key)
        i++;

    if (i < x->count_keys && key == x->buf[i].key) {
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
        int offset = btree->next_offset;
        btree->next_offset += btree->size_node;
        return offset;
    }

    int offset = btree->next_free;
    fseek(btree->fp, btree->next_free, SEEK_SET);
    fread(&btree->next_free, sizeof(btree->next_free), 1, btree->fp);
    return offset;
}

BTree_Node *btree_append_node(BTree *btree) {
    BTree_Node *node = malloc(sizeof(*node));
    assert(node);
    node->offset = btree_pop_free_offset(btree);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->buf = calloc((btree->M - 1), sizeof(*node->buf));
    assert(node->buf);
    node->children = calloc(btree->M, sizeof(*node->children));
    assert(node->children);
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
    int last_level_offset = btree->root->offset;
    BTree_Queue *queue = btree_queue_init(btree->count_nodes);
    btree_queue_enqueue(queue, btree->root->offset);

    while (!btree_queue_is_empty(queue)) {
        int offset = btree_queue_dequeue(queue);
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

BTree_Node *btree_node_read(BTree *btree, int offset) {
    assert(0 <= offset && offset < btree->next_offset);
    fseek(btree->fp, offset, SEEK_SET);
    BTree_Node *node = malloc(sizeof(*node));
    assert(node);
    node->buf = malloc((btree->M - 1) * sizeof(*node->buf));
    assert(node->buf);
    node->children = malloc(btree->M * sizeof(*node->children));
    assert(node->children);
    node->offset = offset;
    fread(&node->count_keys, sizeof(node->count_keys), 1, btree->fp);
    fread(node->buf, sizeof(*node->buf), btree->M - 1, btree->fp);
    fread(node->children, sizeof(*node->children), btree->M, btree->fp);
    node->is_leaf = node->children[0] == 0;
    return node;
}

BTree_Node *btree_node_read_child(BTree *btree, BTree_Node *node, int i) {
    if (i < 0 || i > node->count_keys)
        return NULL;

    int offset = node->children[i];
    return btree_node_read(btree, offset);
}

void btree_node_write(BTree *btree, BTree_Node *node) {
    int offset = node->offset;
    assert(0 <= offset && offset < btree->next_offset);
    fseek(btree->fp, offset, SEEK_SET);
    fwrite(&node->count_keys, sizeof(node->count_keys), 1, btree->fp);
    fwrite(node->buf, sizeof(*node->buf), btree->M - 1, btree->fp);
    fwrite(node->children, sizeof(*node->children), btree->M, btree->fp);
}

void btree_write_header(BTree *btree) {
    fseek(btree->fp, 0, SEEK_SET);
    fwrite(&btree->t, sizeof(btree->t), 1, btree->fp);
    fwrite(&btree->count_nodes, sizeof(btree->count_nodes), 1, btree->fp);
    fwrite(&btree->next_offset, sizeof(btree->next_offset), 1, btree->fp);
    fwrite(&btree->root->offset, sizeof(btree->root->offset), 1, btree->fp);
    fwrite(&btree->next_free, sizeof(btree->next_free), 1, btree->fp);
}

int btree_get_node_size(BTree *btree) {
    return sizeof(btree->root->count_keys) + (btree->M - 1) * sizeof(*btree->root->buf) +
           btree->M * sizeof(*btree->root->children);
}

int btree_get_header_size(BTree *btree) {
    return sizeof(btree->t) + sizeof(btree->count_nodes) + sizeof(btree->next_offset) + sizeof(btree->root->offset) +
           sizeof(btree->next_free);
}

void btree_node_refresh_child(BTree *btree, BTree_Node *node, BTree_Node *x_ci, int new_i) {
    int offset = node->children[new_i];
    assert(0 <= offset && offset < btree->next_offset);
    fseek(btree->fp, offset, SEEK_SET);
    x_ci->offset = offset;
    fread(&x_ci->count_keys, sizeof(x_ci->count_keys), 1, btree->fp);
    fread(x_ci->buf, sizeof(*x_ci->buf), btree->M - 1, btree->fp);
    fread(x_ci->children, sizeof(*x_ci->children), btree->M, btree->fp);
    x_ci->is_leaf = x_ci->children[0] == 0;
}
