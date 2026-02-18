#include "queue.h"
#include "btree.h"
#include <assert.h>
#include <stdlib.h>

BTree_Queue *btree_queue_init(int capacity) {
    BTree_Queue *queue = malloc(sizeof(*queue));
    assert(queue);
    queue->count = 0;
    queue->head = 0;
    queue->buf = malloc(capacity * sizeof(*queue->buf));
    queue->capacity = capacity;
    assert(queue->buf);
    return queue;
}

void btree_queue_enqueue(BTree_Queue *queue, BTree_Node *node) {
    assert(queue->count < queue->capacity);
    int idx = (queue->head + queue->count) % queue->capacity;
    queue->buf[idx] = node;
    queue->count++;
}

int btree_queue_is_empty(BTree_Queue *queue) {
    return queue->count == 0;
}

BTree_Node *btree_queue_dequeue(BTree_Queue *queue) {
    if (queue->count == 0)
        return NULL;

    BTree_Node *node = queue->buf[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return node;
}

BTree_Queue *btree_queue_destroy(BTree_Queue *queue) {
    assert(queue);
    free(queue->buf);
    free(queue);
    return NULL;
}
