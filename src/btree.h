#ifndef BTREE_H
#define BTREE_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef int Btree_Fd;

typedef enum btree_log_level {
    BTREE_LOG_DEBUG,
    BTREE_LOG_WARN,
    BTREE_LOG_INFO,
    BTREE_LOG_ERROR,
} Btree_Log_Level;

typedef void (*Btree_Log_Handler)(Btree_Log_Level level, const char *fmt, va_list args);

typedef enum btree_result {
    BTREE_OK,
    BTREE_ERROR,
    BTREE_ERROR_NULLPTR,
    BTREE_ERROR_SMALL_T,
    BTREE_ERROR_KEY_NOT_FOUND,
    BTREE_ERROR_KEY_EXISTS,
    BTREE_ERROR_FORMAT,
} Btree_Result;

void btree_default_log_handler(Btree_Log_Level level, const char *fmt, va_list args);

void btree_discard_log_handler(Btree_Log_Level level, const char *fmt, va_list args);

typedef struct item {
    int key;
    int value;
} Item;

typedef struct btree_node {
    size_t offset;
    int count_keys;
    bool is_leaf;
    Item *items;
    size_t *children;
} Btree_Node;

typedef struct btree_header {
    int t;
    int M;
    int count_nodes;
    size_t next_offset;
    size_t next_free_offset;
    size_t root_offset;
} Btree_Header;

typedef struct btree {
    Btree_Header header;
    Btree_Log_Handler log_handler;
    Btree_Fd fd;
    Btree_Node *root;
} Btree;

typedef struct btree_opt {
    const char *path;
    int t;
    Btree_Log_Handler log_handler;
} Btree_Options;

#define BTREE_UNUSED(x) (void)(x)

Btree_Result btree_init_(Btree *btree, Btree_Options options);

#define btree_init(btree, ...)                                                                                         \
    btree_init_(btree, (Btree_Options){.log_handler = btree_default_log_handler, __VA_ARGS__})

Btree_Result btree_find(const Btree *btree, int key, int *value);

Btree_Result btree_insert(Btree *btree, int key, int value);

Btree_Result btree_delete(Btree *btree, int key);

Btree_Result btree_destroy(Btree *btree);

Btree_Result btree_display(const Btree *btree, FILE *fp);

int btree_is_valid(const Btree *btree);

const char *btree_strerr(int err);

#ifdef BTREE_IMPLEMENTATION
#endif // BTREE_IMPLEMENTATION

#endif // BTREE_H
