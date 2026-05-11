#include "btree_fs.h"
#include <stdlib.h>
#include <unistd.h>

BTree_Node *btree_node_read(BTree *btree, long offset) {
    if (offset < 0 || offset >= btree->next_offset)
        return NULL;

    lseek(btree->fd, offset, SEEK_SET);
    BTree_Node *node = malloc(sizeof(*node));
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

BTree_Node *btree_node_read_child(BTree *btree, BTree_Node *node, int i) {
    if (i < 0 || i > node->count_keys)
        return NULL;

    long offset = node->children[i];
    return btree_node_read(btree, offset);
}

void btree_node_write(BTree *btree, BTree_Node *node) {
    long offset = node->offset;
    if (offset < 0 || offset >= btree->next_offset)
        return;
    lseek(btree->fd, offset, SEEK_SET);
    write(btree->fd, &node->count_keys, sizeof(node->count_keys));
    write(btree->fd, node->buf, (btree->M - 1) * sizeof(*node->buf));
    write(btree->fd, node->children, btree->M * sizeof(*node->children));
}

void btree_write_header(BTree *btree) {
    lseek(btree->fd, 0, SEEK_SET);
    write(btree->fd, &btree->t, sizeof(btree->t));
    write(btree->fd, &btree->count_nodes, sizeof(btree->count_nodes));
    write(btree->fd, &btree->next_offset, sizeof(btree->next_offset));
    write(btree->fd, &btree->root->offset, sizeof(btree->root->offset));
    write(btree->fd, &btree->next_free, sizeof(btree->next_free));
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
