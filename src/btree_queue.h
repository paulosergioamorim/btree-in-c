#pragma once

typedef struct btree_queue BTree_Queue;

BTree_Queue *btree_queue_init(int capacity);

void btree_queue_enqueue(BTree_Queue *queue, long offset);

int btree_queue_is_empty(BTree_Queue *queue);

long btree_queue_dequeue(BTree_Queue *queue);

void btree_queue_destroy(BTree_Queue *queue);
