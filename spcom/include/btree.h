/** binary tree for string key **/
#ifndef BTREE_INCLUDE_H_
#define BTREE_INCLUDE_H_

struct btree_node {
    struct btree_node *less;
    struct btree_node *more;
    const char *sval;
    const void *data;
};

typedef int(btree_traverse_cb)(const struct btree_node *node);

struct btree {
    struct btree_node *root;
    // const char *termchars;
    struct _btree_traverse_tmp {
        const char *searchstr;
        unsigned int searchlen;
        btree_traverse_cb *cb;
        int count;
    } _tmp;
};

int btree_traverse(struct btree *bt, 
                   const struct btree_node *node, 
                   const char *search, 
                   btree_traverse_cb *cb);

int btree_add_node(struct btree *bt, struct btree_node *new);

struct btree_node *btree_find(struct btree *bt, const char *sval);

#endif
