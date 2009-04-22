/* Copyright (c) 2008 Derrick Coetzee
 * http://en.literateprograms.org/Red-black_tree_(C)?oldid=7982
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

#include "rbtree.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h> /* rand(), malloc(), free() */

static int compare_int(void* left, void* right);
static void print_tree(rbtree t);
static void print_tree_helper(rbtree_node n, int indent);

int compare_int(void* leftp, void* rightp) {
    int left = (int)leftp;
    int right = (int)rightp;
    if (left < right) 
        return -1;
    else if (left > right)
        return 1;
    else {
        assert (left == right);
        return 0;
    }
}

#define INDENT_STEP  4

void print_tree_helper(rbtree_node n, int indent);

void print_tree(rbtree t) {
    print_tree_helper(t->root, 0);
    puts("");
}

void print_tree_helper(rbtree_node n, int indent) {
    int i;
    if (n == NULL) {
        fputs("<empty tree>", stdout);
        return;
    }
    if (n->right != NULL) {
        print_tree_helper(n->right, indent + INDENT_STEP);
    }
    for(i=0; i<indent; i++)
        fputs(" ", stdout);
    if (n->color == BLACK)
        printf("%d\n", (int)n->key);
    else
        printf("<%d>\n", (int)n->key);
    if (n->left != NULL) {
        print_tree_helper(n->left, indent + INDENT_STEP);
    }
}

int main() {
    int i;
    struct rbtree_t t;
    rbtree_init(&t, compare_int); 
    print_tree(&t);

    for(i=0; i<5000; i++) {
        int x = rand() % 10000;
        int y = rand() % 10000;
#ifdef TRACE
        print_tree(&t);
        printf("Inserting %d -> %d\n\n", x, y);
#endif
        rbtree_node node = (rbtree_node) malloc(sizeof(struct rbtree_node_t));
        node->key = (void*)x;
        node->value = (void*)y;
        rbtree_insert(&t, node);
        assert(rbtree_lookup(&t, (void*)x) == (void*)y);
    }
    for(i=0; i<60000; i++) {
        int x = rand() % 10000;
#ifdef TRACE
        print_tree(&t);
        printf("Deleting key %d\n\n", x);
#endif
        rbtree_node n = rbtree_delete(&t, (void*)x);
        if(n != NULL) {
            free(n);
        }
    }
    printf("Okay\n");
    return 0;
}

