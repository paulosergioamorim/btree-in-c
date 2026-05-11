#ifndef __BTREE_FS_H__
#define __BTREE_FS_H__

#include "btree.h"

void btree_node_refresh_child(BTree *btree, BTree_Node *node, BTree_Node *x_ci, int new_i);

BTree_Node *btree_node_read_child(BTree *btree, BTree_Node *node, int i);

void btree_node_write(BTree *btree, BTree_Node *node);

void btree_node_destroy(BTree_Node *node);

#endif
