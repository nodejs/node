/* Copyright (c) 2008 Derrick Coetzee
 * http://en.literateprograms.org/Red-black_tree_(C)?oldid=7982
 * Small changes by Ryah Dahl
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef _RBTREE_H_
#define _RBTREE_H_

enum rbtree_node_color { RED, BLACK };

typedef int (*rbtree_compare_func)(void* left_key, void* right_key);

typedef struct rbtree_node_t {
    struct rbtree_node_t* left;    /* private */
    struct rbtree_node_t* right;   /* private */
    struct rbtree_node_t* parent;  /* private */
    enum rbtree_node_color color;  /* private */
    void* key;   /* public */
    void* value; /* public */
} *rbtree_node;

typedef struct rbtree_t {
    rbtree_node root;             /* private */
    rbtree_compare_func compare;  /* private */
} *rbtree;


void rbtree_init(rbtree t, rbtree_compare_func);
void* rbtree_lookup(rbtree t, void* key);
void rbtree_insert(rbtree t, rbtree_node);
/* you must free the returned node */
rbtree_node rbtree_delete(rbtree t, void* key);

#endif

