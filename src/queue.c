#include "queue.h"
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

void btree_queue_enqueue(BTree_Queue *queue, int offset) {
    assert(queue->count < queue->capacity);
    int idx = (queue->head + queue->count) % queue->capacity;
    queue->buf[idx] = offset;
    queue->count++;
}

int btree_queue_is_empty(BTree_Queue *queue) {
    return queue->count == 0;
}

int btree_queue_dequeue(BTree_Queue *queue) {
    assert(queue->count > 0);
    int offset = queue->buf[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return offset;
}

void btree_queue_destroy(BTree_Queue *queue) {
    assert(queue);
    free(queue->buf);
    free(queue);
}
