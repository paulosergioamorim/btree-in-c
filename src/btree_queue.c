#include "btree_queue.h"
#include <stdlib.h>

struct btree_queue {
    long count;
    long capacity;
    long head;
    long *buf;
};

BTree_Queue *btree_queue_init(int capacity) {
    BTree_Queue *queue = malloc(sizeof(*queue));
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

void btree_queue_enqueue(BTree_Queue *queue, long offset) {
    if (queue->count >= queue->capacity)
        return;
    int idx = (queue->head + queue->count) % queue->capacity;
    queue->buf[idx] = offset;
    queue->count++;
}

int btree_queue_is_empty(BTree_Queue *queue) {
    return queue->count == 0;
}

long btree_queue_dequeue(BTree_Queue *queue) {
    if (queue->count == 0)
        return -1;
    int offset = queue->buf[queue->head];
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return offset;
}

void btree_queue_destroy(BTree_Queue *queue) {
    free(queue->buf);
    free(queue);
}
