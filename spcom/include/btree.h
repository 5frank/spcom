/** binary tree for string key **/
#ifndef BTREE_INCLUDE_H_
#define BTREE_INCLUDE_H_

struct btree_node {
    struct btree_node *less;
    struct btree_node *more;
    unsigned int id;
    const char *sval;
    const void *data;
};

typedef int(btree_traverse_cb)(const struct btree_node *node, void *arg);

struct btree {
    struct btree_node *root;
    // const char *termchars;
    struct btree_travdata {
        const char *searchstr;
        unsigned int searchlen;
        void *cb_arg;
        btree_traverse_cb *cb;
        int count;
    } travdata;
};

int btree_traverse(struct btree *bt,
                   const struct btree_node *node,
                   const char *search,
                   btree_traverse_cb *cb,
                   void *cb_arg);

int btree_add_node(struct btree *bt, struct btree_node *new);

struct btree_node *btree_find(struct btree *bt, const char *sval);

#endif
