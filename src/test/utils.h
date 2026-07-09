#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>

static inline void swap(int *ptr1, int *ptr2) {
    int temp = *ptr1;
    *ptr1 = *ptr2;
    *ptr2 = temp;
}

static inline void shuffle(int *vec, int len) {
    for (int i = 1; i < len; i++) {
        int j = rand() % (i + 1);
        swap(vec + i, vec + j);
    }
}

#endif // UTILS_H
