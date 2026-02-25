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

void btree_node_split_child(BTree *btree, BTree_Node *x, int i);

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
    btree_node_destroy(btree->root);
    btree->root = s;
    btree_node_split_child(btree, s, 0);
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
    free(node->keys);
    free(node->values);
    free(node->children);
    free(node);
}

int btree_node_search(BTree *btree, BTree_Node *x, int key, int *value) {
    int i = 0;

    while (i < x->count_keys && key > x->keys[i])
        i++;

    if (i < x->count_keys && key == x->keys[i]) {
        *value = x->values[i];
        return 1;
    }

    if (x->is_leaf)
        return 0;

    BTree_Node *x_ci = btree_node_read_child(btree, x, i);

    int hit = btree_node_search(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
    return hit;
}

BTree_Node *btree_append_node(BTree *btree) {
    BTree_Node *node = malloc(sizeof(*node));
    node->offset = btree->next_offset;
    btree->next_offset += btree->size_node;
    assert(node);
    node->is_leaf = 1;
    node->count_keys = 0;
    node->keys = calloc((btree->M - 1), sizeof(*node->keys));
    assert(node->keys);
    node->values = calloc((btree->M - 1), sizeof(*node->values));
    assert(node->values);
    node->children = calloc(btree->M, sizeof(*node->children));
    assert(node->children);
    btree->count_nodes++;
    btree_node_write(btree, node);
    return node;
}

void btree_node_split_child(BTree *btree, BTree_Node *x, int i) {
    BTree_Node *z = btree_append_node(btree);
    BTree_Node *y = NULL;
    int t = btree->t;

    y = btree_node_read_child(btree, x, i);
    z->is_leaf = y->is_leaf;
    z->count_keys = t - 1;

    memcpy(z->keys, y->keys + t, (t - 1) * sizeof(*z->keys));
    memcpy(z->values, y->values + t, (t - 1) * sizeof(*z->values));

    if (!y->is_leaf)
        memcpy(z->children, y->children + t, t * sizeof(*z->children));

    y->count_keys = t - 1;

    memmove(x->children + i + 1, x->children + i, (x->count_keys - i + 1) * sizeof(*x->children));

    x->children[i + 1] = z->offset;

    memmove(x->keys + i + 1, x->keys + i, (x->count_keys - i) * sizeof(*x->keys));
    memmove(x->values + i + 1, x->values + i, (x->count_keys - i) * sizeof(*x->values));

    x->keys[i] = y->keys[t - 1];
    x->values[i] = y->values[t - 1];
    x->count_keys++;

    btree_node_write(btree, x);
    btree_node_write(btree, y);
    btree_node_write(btree, z);
    btree_node_destroy(y);
    btree_node_destroy(z);
}

void btree_node_insert_nonfull(BTree *btree, BTree_Node *x, int key, int value) {
    int i = x->count_keys - 1;

    if (x->is_leaf) {
        while (i >= 0 && key < x->keys[i]) {
            x->keys[i + 1] = x->keys[i];
            x->values[i + 1] = x->values[i];
            i--;
        }

        x->keys[i + 1] = key;
        x->values[i + 1] = value;
        x->count_keys++;
        btree_node_write(btree, x);
        return;
    }

    while (i >= 0 && key < x->keys[i])
        i--;

    i++;

    BTree_Node *x_ci = btree_node_read_child(btree, x, i);

    if (x_ci->count_keys < btree->M - 1) {
        btree_node_insert_nonfull(btree, x_ci, key, value);
        btree_node_destroy(x_ci);
        return;
    }

    btree_node_split_child(btree, x, i);

    if (key > x->keys[i])
        i++;

    btree_node_destroy(x_ci);
    x_ci = btree_node_read_child(btree, x, i);
    btree_node_insert_nonfull(btree, x_ci, key, value);
    btree_node_destroy(x_ci);
}

void btree_display(BTree *btree) {
    int last_level_offset = btree->root->offset;

    printf("[ ");

    for (int i = 0; i < btree->root->count_keys; i++)
        printf("%d ", btree->root->keys[i]);

    printf("] ");

    if (btree->root->is_leaf) {
        printf("\n");
        return;
    }

    BTree_Queue *queue = btree_queue_init(btree->count_nodes);

    if (btree->root->offset == last_level_offset) {
        last_level_offset = btree->root->children[btree->root->count_keys];
        printf("\n");
    }

    for (int i = 0; i <= btree->root->count_keys; i++)
        btree_queue_enqueue(queue, btree->root->children[i]);

    while (!btree_queue_is_empty(queue)) {
        int offset = btree_queue_dequeue(queue);
        BTree_Node *node = btree_node_read(btree, offset);
        printf("[ ");

        for (int i = 0; i < node->count_keys; i++)
            printf("%d ", node->keys[i]);

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
    node->keys = malloc((btree->M - 1) * sizeof(*node->keys));
    assert(node->keys);
    node->values = malloc((btree->M - 1) * sizeof(*node->values));
    assert(node->values);
    node->children = malloc(btree->M * sizeof(*node->children));
    assert(node->children);
    fread(&node->offset, sizeof(node->offset), 1, btree->fp);
    fread(&node->is_leaf, sizeof(node->is_leaf), 1, btree->fp);
    fread(&node->count_keys, sizeof(node->count_keys), 1, btree->fp);
    fread(node->keys, sizeof(*node->keys), btree->M - 1, btree->fp);
    fread(node->values, sizeof(*node->values), btree->M - 1, btree->fp);
    fread(node->children, sizeof(*node->children), btree->M, btree->fp);
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
    fwrite(&node->offset, sizeof(node->offset), 1, btree->fp);
    fwrite(&node->is_leaf, sizeof(node->is_leaf), 1, btree->fp);
    fwrite(&node->count_keys, sizeof(node->count_keys), 1, btree->fp);
    fwrite(node->keys, sizeof(*node->keys), btree->M - 1, btree->fp);
    fwrite(node->values, sizeof(*node->values), btree->M - 1, btree->fp);
    fwrite(node->children, sizeof(*node->children), btree->M, btree->fp);
}

void btree_write_header(BTree *btree) {
    fseek(btree->fp, 0, SEEK_SET);
    fwrite(&btree->t, sizeof(btree->t), 1, btree->fp);
    fwrite(&btree->count_nodes, sizeof(btree->count_nodes), 1, btree->fp);
    fwrite(&btree->next_offset, sizeof(btree->next_offset), 1, btree->fp);
    fwrite(&btree->root->offset, sizeof(btree->root->offset), 1, btree->fp);
}

int btree_get_node_size(BTree *btree) {
    return sizeof(btree->root->offset) + sizeof(btree->root->is_leaf) + sizeof(btree->root->count_keys) +
           (btree->M - 1) * (sizeof(*btree->root->keys) + sizeof(*btree->root->values)) +
           btree->M * sizeof(*btree->root->children);
}

int btree_get_header_size(BTree *btree) {
    return sizeof(btree->t) + sizeof(btree->count_nodes) + sizeof(btree->next_offset) + sizeof(btree->root->offset);
}
