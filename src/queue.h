#pragma once

typedef struct btree_queue {
    int count;
    int head;
    int capacity;
    int *buf;
} BTree_Queue;

BTree_Queue *btree_queue_init(int capacity);

void btree_queue_enqueue(BTree_Queue *queue, int offset);

int btree_queue_is_empty(BTree_Queue *queue);

int btree_queue_dequeue(BTree_Queue *queue);

BTree_Queue *btree_queue_destroy(BTree_Queue *queue);
