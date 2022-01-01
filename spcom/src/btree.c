#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "btree.h"

static inline bool _is_termchar(int c, const char* termchars)
{
   if (c == '\0' || c == ' ' || c == '\n') 
       return true;

   if (termchars)
        return (bool) strchr(termchars, c);

   return false;
}

struct btree_node *btree_find(struct btree *bt, const char *name)
{
    if (!name || (*name == '\0')) {
        return NULL;
    }

    struct btree_node *match = NULL;
    struct btree_node *node = bt->root;

    while (node) {
        int len = strlen(node->sval);
        int a = strncmp(name, node->sval, len);
        if (a < 0) {
            node = node->less;
        } else if (a > 0) {
            node = node->more;
        } else {
            char c = name[len];
            if (_is_termchar(c, NULL)) {
                match = node;
                // have a match candidate
            }
            // mihgt have a better longer match
            node = node->more; 
        }
    }

    return match;
}

/**
 * unsafe if not called from btree_traverse 
 * not thread safe!
 * Uses tree recursion traverse. in alphabetical order. 
 * save some stack by passing fewer arguments!?
 * @return non-zero callback error. non-zero return value is also condition to
 * break recursion
 */
static int _traverse(struct btree *bt, const struct btree_node *node)
{

    if (!node)
        return 0;
    // could add a counter here to limit recursive depth.
    int cberr = _traverse(bt, node->less);
    if (cberr)
        return cberr;

    const char *s = bt->_tmp.searchstr;
    size_t slen = bt->_tmp.searchlen;
 
    if (!s || !strncmp(node->sval, s, slen)) {
        cberr = bt->_tmp.cb(node);
        if (cberr)
            return cberr;
    }

    cberr = _traverse(bt, node->more);
    return cberr;
}
/**
 * traverse tree in alphabetical order and run callback.
 * not thread safe!
 * @param search - optional. only run callback on nodes matching this string
 *     (begins with). match everyting if NULL
 *
 * @param node - optional. base node to begin traverse. from root if NULL
 *
 * @param cb - callback for each node. if the callback returns non-zero,
 * traverse will stop.
 */
int btree_traverse(struct btree *bt, 
                   const struct btree_node *node, 
                   const char *search, 
                   btree_traverse_cb *cb)
{
    if (!bt)
        return -EINVAL;

    if (!cb)
        return -EINVAL;

    if (!bt->root)
        return -EINVAL;

    if (!node)
        node = bt->root;

    if (search) {
        bt->_tmp.searchstr = search;
        bt->_tmp.searchlen = strlen(search);
    }
    else {
        bt->_tmp.searchstr = NULL;
        bt->_tmp.searchlen = 0;
    }
    bt->_tmp.cb = cb;
    bt->_tmp.count = 0;

    int cberr = _traverse(bt, node);
    // callback error is not a traverse error, return success
    (void) cberr;
    return 0;

}

int btree_add_node(struct btree *bt, struct btree_node *new)
{
    new->less = NULL;
    new->more = NULL;
    if (!bt->root) {
        bt->root = new;
        return 0;
    }

    struct btree_node *node = bt->root;
    while (node) {
        int a = strcmp(new->sval, node->sval);
        if (a < 0) {
            if (!node->less) {
                node->less = new;
                return 0;
            }
            node = node->less;
        } else if (a > 0) {
            if (!node->more) {
                node->more = new;
                return 0;
            }
            node = node->more;
        } else {
            // conflicting values, already added
            return -EEXIST;
        }
    }

    // never
    return -2;
}

