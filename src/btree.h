#ifndef _BTREE_H
#define _BTREE_H

#define BTREE_OK 0
#define BTREE_ERROR_NO_ERROR 1
#define BTREE_ERROR_TO_SMALL_PARAM_T 2
#define BTREE_ERROR_KEY_NOT_FOUND 3
#define BTREE_ERROR_KEY_ALREADY_EXITS 4
#define BTREE_ERROR_ALLOC 5
#define BTREE_ERROR_OPEN_FILE 6

typedef struct btree Btree;

typedef struct btree_opts {
    const char *path;
    const int t;
    const int override_file;
} Btree_Opts;

#define BTREE_OPTS_NEW_FILE(path, t) ((Btree_Opts){path, t, 1})
#define BTREE_OPTS_FROM_FILE(path) ((Btree_Opts){path, 0, 0})

int btree_init(Btree **btree_ptr, Btree_Opts opts);

int btree_search(Btree *btree, int key, int *value);

void btree_insert(Btree *btree, int key, int value);

void btree_delete(Btree *btree, int key);

void btree_destroy(Btree *btree);

#ifdef BTREE_DISPLAY_IMPLEMENTATION
void btree_display(Btree *btree);
#endif

#endif
