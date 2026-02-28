#pragma once

#include <cstdio>
#include <cstring>
#include <queue>

template <typename K, typename V> struct Item {
    K key;
    V value;
};

template <typename K, typename V> struct BTree;

template <typename K, typename V> struct BTree_Node {
    int offset;
    bool is_leaf;
    int count_keys;
    Item<K, V> *buf;
    int *children;
    BTree<K, V> *btree;

    BTree_Node(BTree<K, V> *btree, int offset) {
        this->btree = btree;
        this->offset = offset;
        this->buf = new Item<K, V>[btree->M - 1];
        this->children = new int[btree->M];
        fseek(btree->fp, offset, SEEK_SET);
        fread(&this->count_keys, sizeof(this->count_keys), 1, btree->fp);
        fread(this->buf, sizeof(*this->buf), btree->M - 1, btree->fp);
        fread(this->children, sizeof(*this->children), btree->M, btree->fp);
        this->is_leaf = this->children[0] == 0;
    }

    static BTree_Node<K, V> *fetch(BTree<K, V> *btree, int offset) {
        if (offset <= 0 || offset >= btree->next_offset)
            return nullptr;

        return new BTree_Node<K, V>(btree, offset);
    }

    BTree_Node(BTree<K, V> *btree) {
        this->btree = btree;
        this->offset = btree->next_offset;
        btree->next_offset += btree->size_node;
        this->is_leaf = 1;
        this->count_keys = 0;
        this->buf = new Item<K, V>[btree->M - 1]();
        this->children = new int[btree->M]();
    }

    ~BTree_Node() {
        delete[] this->buf;
        delete[] this->children;
    }

    void remove_node() {
        int offset = this->offset;
        this->count_keys = 0;
        this->is_leaf = 0;
        memset(this->buf, 0, (this->btree->M - 1) * sizeof(*this->buf));
        memset(this->children, 0, this->btree->M * sizeof(*this->children));
        this->write();
        fseek(this->btree->fp, offset, SEEK_SET);
        fwrite(&this->btree->next_free, sizeof(this->btree->next_free), 1, this->btree->fp);
        this->btree->next_free = this->offset;
        this->btree->count_nodes--;
        delete this;
    }

    void write() {
        fseek(this->btree->fp, this->offset, SEEK_SET);
        fwrite(&this->count_keys, sizeof(this->count_keys), 1, btree->fp);
        fwrite(this->buf, sizeof(*this->buf), btree->M - 1, btree->fp);
        fwrite(this->children, sizeof(*this->children), btree->M, btree->fp);
    }

    bool search(K key, V &value) {
        int i = 0;

        while (i < this->count_keys && key > this->buf[i].key)
            i++;

        if (i < this->count_keys && key == this->buf[i].key) {
            value = this->buf[i].value;
            return 1;
        }

        if (this->is_leaf)
            return false;

        BTree_Node<K, V> *x_ci = new BTree_Node<K, V>(this->btree, this->children[i]);

        int hit = x_ci->search(key, value);
        delete x_ci;
        return hit;
    }

    Item<K, V> get_pred(int i) {
        BTree_Node *pred = new BTree_Node<K, V>(this->btree, this->children[i]);

        while (!pred->is_leaf) {
            BTree_Node *prev = pred;
            pred = new BTree_Node<K, V>(this->btree, pred->children[pred->count_keys]);
            delete prev;
        }

        Item item = pred->buf[pred->count_keys - 1];
        delete pred;
        return item;
    }

    Item<K, V> get_post(int i) {
        BTree_Node *post = new BTree_Node<K, V>(this->btree, this->children[i + 1]);

        while (!post->is_leaf) {
            BTree_Node *prev = post;
            post = new BTree_Node<K, V>(this->btree, post->children[0]);
            delete prev;
        }

        Item item = post->buf[0];
        delete post;
        return item;
    }

    void split_child(BTree_Node<K, V> *y, int i) {
        BTree_Node<K, V> *z = new BTree_Node<K, V>(btree);
        btree->count_nodes++;
        int t = btree->t;

        z->is_leaf = y->is_leaf;
        z->count_keys = t - 1;

        memcpy(z->buf, y->buf + t, (t - 1) * sizeof(*z->buf));

        if (!y->is_leaf)
            memcpy(z->children, y->children + t, t * sizeof(*z->children));

        y->count_keys = t - 1;

        memmove(this->children + i + 1, this->children + i, (this->count_keys - i + 1) * sizeof(*this->children));

        this->children[i + 1] = z->offset;

        memmove(this->buf + i + 1, this->buf + i, (this->count_keys - i) * sizeof(*this->buf));

        this->buf[i] = y->buf[t - 1];
        this->count_keys++;

        memset(y->buf + y->count_keys, 0, t * sizeof(*y->buf));

        if (!y->is_leaf)
            memset(y->children + y->count_keys + 1, 0, t * sizeof(*y->children));

        this->write();
        y->write();
        z->write();
        delete z;
    }

    void merge(BTree_Node<K, V> *y, BTree_Node<K, V> *z, int i) {
        y->buf[y->count_keys] = this->buf[i];
        y->count_keys++;
        memmove(this->buf + i, this->buf + i + 1, (this->count_keys - i - 1) * sizeof(*this->buf));
        memmove(this->children + i + 1, this->children + i + 2, (this->count_keys - i - 1) * sizeof(*this->children));
        this->count_keys--;
        memcpy(y->buf + y->count_keys, z->buf, (btree->t - 1) * sizeof(*y->buf));

        if (!y->is_leaf)
            memcpy(y->children + y->count_keys, z->children, btree->t * sizeof(*y->children));

        y->count_keys = 2 * btree->t - 1;

        this->write();

        if (btree->root == this && this->count_keys == 0) {
            this->remove_node();
            y->btree->root = y;
        }

        z->remove_node();
        y->write();
    }

    bool redistribute(BTree_Node<K, V> *x_ci, BTree_Node<K, V> *sibbling_left, BTree_Node<K, V> *sibbling_right,
                      int i) {
        if (sibbling_left && sibbling_left->count_keys >= this->btree->t) {
            this->rotate_right(sibbling_left, x_ci, i - 1);
            delete sibbling_left;
            delete sibbling_right;
            return true;
        }

        if (sibbling_right && sibbling_right->count_keys >= this->btree->t) {
            this->rotate_left(x_ci, sibbling_right, i);
            delete sibbling_left;
            delete sibbling_right;
            return true;
        }

        return false;
    }

    BTree_Node *concatenate(BTree_Node *x_ci, BTree_Node<K, V> *sibbling_left, BTree_Node<K, V> *sibbling_right,
                            int i) {
        if (sibbling_left) {
            this->merge(sibbling_left, x_ci, i - 1);
            delete sibbling_right;
            return sibbling_left;
        }

        this->merge(x_ci, sibbling_right, i);
        delete sibbling_left;
        return x_ci;
    }

    void rotate_left(BTree_Node<K, V> *y, BTree_Node<K, V> *z, int i) {
        y->buf[y->count_keys] = this->buf[i];

        if (!y->is_leaf)
            y->children[y->count_keys + 1] = z->children[0];

        y->count_keys++;

        this->buf[i] = z->buf[0];

        memmove(z->buf, z->buf + 1, (z->count_keys - 1) * sizeof(*z->buf));

        if (!z->is_leaf)
            memmove(z->children, z->children + 1, z->count_keys * sizeof(*z->children));

        z->count_keys--;
        z->buf[z->count_keys] = {0};

        if (!z->is_leaf)
            z->children[z->count_keys + 1] = 0;

        this->write();
        y->write();
        z->write();
    }

    void rotate_right(BTree_Node<K, V> *y, BTree_Node<K, V> *z, int i) {
        memmove(z->buf + 1, z->buf, z->count_keys * sizeof(*z->buf));

        if (!z->is_leaf)
            memmove(z->children + 1, z->children, (z->count_keys + 1) * sizeof(*z->children));

        z->buf[0] = this->buf[i];

        if (!z->is_leaf)
            z->children[0] = y->children[y->count_keys];

        z->count_keys++;

        this->buf[i] = y->buf[y->count_keys - 1];
        y->count_keys--;
        y->buf[y->count_keys] = {0};

        if (!y->is_leaf)
            y->children[y->count_keys + 1] = 0;

        this->write();
        y->write();
        z->write();
    }

    void insert_nonfull(K key, V value) {
        int i = this->count_keys - 1;

        if (this->is_leaf) {
            while (i >= 0 && key < this->buf[i].key) {
                this->buf[i + 1] = this->buf[i];
                i--;
            }

            this->buf[i + 1] = (Item<K, V>){.key = key, .value = value};
            this->count_keys++;
            this->write();
            return;
        }

        while (i >= 0 && key < this->buf[i].key)
            i--;

        i++;

        BTree_Node<K, V> *x_ci = new BTree_Node<K, V>(this->btree, this->children[i]);

        if (x_ci->count_keys < btree->M - 1) {
            x_ci->insert_nonfull(key, value);
            delete x_ci;
            return;
        }

        this->split_child(x_ci, i);

        if (key > this->buf[i].key) {
            i++;
            delete x_ci;
            x_ci = new BTree_Node<K, V>(this->btree, this->children[i]);
        }

        x_ci->insert_nonfull(key, value);
        delete x_ci;
    }

    void remove(K key) {
        int i = 0;
        int t = btree->t;

        while (i < this->count_keys && key > this->buf[i].key)
            i++;

        if (i < this->count_keys && key == this->buf[i].key) {
            if (this->is_leaf) {
                memmove(this->buf + i, this->buf + i + 1, (this->count_keys - i - 1) * sizeof(*this->buf));
                this->count_keys--;
                this->buf[this->count_keys] = (Item<K, V>){0};
                this->write();
                return;
            }

            BTree_Node<K, V> *y = new BTree_Node<K, V>(this->btree, this->children[i]);

            if (y->count_keys >= t) {
                Item pred = this->get_pred(i);
                this->buf[i] = pred;
                this->write();
                y->remove(pred.key);
                delete y;
                return;
            }

            BTree_Node<K, V> *z = new BTree_Node<K, V>(this->btree, this->children[i + 1]);

            if (z->count_keys >= t) {
                delete y;
                Item post = this->get_post(i);
                this->buf[i] = post;
                this->write();
                z->remove(post.key);
                delete z;
                return;
            }

            this->merge(y, z, i);
            y->remove(key);

            if (y->btree->root != y)
                delete y;

            return;
        }

        if (this->is_leaf)
            return;

        BTree_Node *x_ci = new BTree_Node<K, V>(this->btree, this->children[i]);

        if (x_ci->count_keys > t - 1) {
            x_ci->remove(key);

            if (btree->root != x_ci)
                delete x_ci;

            return;
        }

        BTree_Node *sibbling_left = BTree_Node<K, V>::fetch(this->btree, this->children[i - 1]);
        BTree_Node *sibbling_right = BTree_Node<K, V>::fetch(this->btree, this->children[i + 1]);

        if (this->redistribute(x_ci, sibbling_left, sibbling_right, i))
            x_ci->remove(key);

        else {
            x_ci = this->concatenate(x_ci, sibbling_left, sibbling_right, i);
            x_ci->remove(key);
        }

        if (btree->root != x_ci)
            delete x_ci;
    }
};

template <typename K, typename V> struct BTree {
    int t;
    int M;
    int count_nodes;
    int size_node;
    int next_offset;
    int next_free;
    FILE *fp;
    BTree_Node<K, V> *root;

    BTree(const char *path, int t) {
        this->t = t;
        this->M = 2 * t;
        this->count_nodes = 1;
        this->size_node =
            sizeof(this->root->count_keys) + (M - 1) * sizeof(*this->root->buf) + M * sizeof(*this->root->children);
        this->next_offset = sizeof(this->t) + sizeof(this->count_nodes) + sizeof(this->next_offset) +
                            sizeof(this->root->offset) + sizeof(this->next_free);
        this->next_free = -1;
        this->fp = fopen(path, "wb+");
        this->root = new BTree_Node<K, V>(this);
        this->write_header();
        this->root->write();
    }

    BTree(const char *path) {
        this->fp = fopen(path, "rb+");
        fread(&this->t, sizeof(this->t), 1, this->fp);
        this->M = 2 * t;
        this->size_node =
            sizeof(this->root->count_keys) + (M - 1) * sizeof(*this->root->buf) + M * sizeof(*this->root->children);
        fread(&this->count_nodes, sizeof(this->count_nodes), 1, this->fp);
        fread(&this->next_offset, sizeof(this->next_offset), 1, this->fp);
        int root_offset = 0;
        fread(&root_offset, sizeof(root_offset), 1, this->fp);
        fread(&this->next_free, sizeof(this->next_free), 1, this->fp);
        this->root = new BTree_Node<K, V>(this, root_offset);
    }

    ~BTree() {
        fclose(this->fp);
        delete this->root;
    }

    void write_header() {
        fseek(this->fp, 0, SEEK_SET);
        fwrite(&this->t, sizeof(this->t), 1, this->fp);
        fwrite(&this->count_nodes, sizeof(this->count_nodes), 1, this->fp);
        fwrite(&this->next_offset, sizeof(this->next_offset), 1, this->fp);
        fwrite(&this->root->offset, sizeof(this->root->offset), 1, this->fp);
        fwrite(&this->next_free, sizeof(this->next_free), 1, this->fp);
    }

    bool search(K key, V &value) {
        return this->root->search(key, value);
    }

    void insert(K key, V value) {
        if (this->root->count_keys == this->M - 1) {
            BTree_Node<K, V> *s = new BTree_Node<K, V>(this);
            s->is_leaf = 0;
            s->count_keys = 0;
            s->children[0] = this->root->offset;
            s->split_child(this->root, 0);
            delete this->root;
            this->root = s;
        }

        this->root->insert_nonfull(key, value);
        this->write_header();
    }

    void remove(K key) {
        this->root->remove(key);
        this->write_header();
    }

    void display() {
        std::queue<int> queue;
        queue.push(this->root->offset);
        int last_offset_level = this->root->offset;

        while (!queue.empty()) {
            int &offset = queue.front();
            queue.pop();
            BTree_Node<K, V> *node = new BTree_Node<K, V>(this, offset);

            printf("[ ");

            for (int i = 0; i < node->count_keys; i++)
                printf("%d ", node->buf[i].key);

            printf("] ");

            if (offset == last_offset_level) {
                printf("\n");
                last_offset_level = node->children[node->count_keys];
            }

            if (node->is_leaf) {
                delete node;
                continue;
            }

            for (int i = 0; i <= node->count_keys; i++)
                queue.push(node->children[i]);

            delete node;
        }
    }
};
