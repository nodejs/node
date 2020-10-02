# treeverse

Walk any kind of tree structure depth- or breadth-first. Supports promises
and advanced map-reduce operations with a very small API.

Treeverse does not care what kind of tree it is, it will traverse it for
you just fine.  It does the right thing with functions that return
Promises, and returns a non-Promise value if your functions don't return
Promises.

Rather than imposing a specific structure, like requiring you to have child
nodes stored in a `children` array, it calls the supplied `getChildren()`
function, so the children can be anywhere (or not even exist yet!)  This
makes it suitable for _creating_ an optimized tree from a set of dependency
manifests, for example.

## USAGE

```js
const {depth, breadth} = require('treeverse')

// depth-first traversal
// returns a promise if any visit/leave function returns a promise
// otherwise returns the result of leave, or visit if no leave function
// provided.
depth({
  // the root node where we start the traversal
  tree: rootNode,

  visit (node) {
    // optional
    // called upon descent into the node.
    // return a promise, or a mapped value, or nothing to just leave it
    // as-is
  },
  leave (node, children) {
    // optional
    // called as we ascend back to the root of the tree.
    // return a promise, or a reduced value, or nothing to leave it as is
    // the children array is a list of the child nodes that have been
    // visited (and potentially left) already.  If the tree is acyclic,
    // then leave() will have been called on all of them.  If it has
    // cycles, then the children may not have been left yet.
  },
  getChildren (node, nodeResult) {
    // required
    // return an array of child nodes in the tree, if any exist
    // returning a promise is totally ok, of course.
    // the first argument is the original value of the node.  The second
    // argument is the result of visit(node).
  },
  filter (node) {
    // optional
    // return true if the node should be visited, false otherwise
    // initial tree is always visited, so this only filters children
    // note that filtering a node _also_ filters all of its children.
  },
})

// breadth first traversal
// returns a promise if any visit function returns a promise
// otherwise returns the result of the top-level node.
// note that only a visit() function is supported here, since a node's
// children are typically traversed much later in the process.
breadth({
  // the root node where we start the traversal
  tree: rootNode,

  visit (node) {
    // optional, but a no-op if not provided.
    // called when this node is encountered in the traversal.
    // return a promise, or a mapped value, or nothing to leave as-is.
  },
  getChildren (node, nodeResult) {
    // required, same as depth()
  },
  filter (node) {
    // optional, same as depth()
  },
})
```

## API

Both functions take a single options object as an argument, and return
either the result value, or a Promise to the result value if the
methods in the options argument ever return a Promise.

* `treeverse.breadth` - Perform a breadth-first traversal.  That is, walk
  across node siblings before traversing node children.
* `treeverse.depth` - Perform a depth-first traversal.  That is, walk
  down into child nodes before traversing siblings.

## OPTIONS

All function options can return a Promise or actual value.

The return value is the result of the top level visit function if no leave
function is provided, or leave.  If any method along the way returns a
promise, then the top level function will return a promise which resolves
to the result of visiting (and leaving) the top node in the tree.

* `tree` - The initial node where the traversal begins.
* `visit(node)` - Function to call upon visiting a node.
* `leave(node, children)` - (Depth only) Function to call upon leaving a
  node, once all of its children have been visited, and potentially left.
  `children` is an array of child node visit results.  If the graph is
  cyclic, then some children _may_ have been visited but not left.
* `getChildren(node, nodeResult)` - Get an array of child nodes to process.
* `filter` - Filter out child nodes from the traversal.  Note that this
  filters the entire branch of the tree, not just that one node.  That is,
  children of filtered nodes are not traversed either.

## STACK DEPTH WARNING

When a `leave` method is specified, then recursion is used, because
maintaining state otherwise is challenging.  This means that using `leave`
with a synchronous depth first traversal of very deeply nested trees will
result in stack overflow errors.

To avoid this, either make one or more of the functions async, or do all of
the work in the `visit` method.

Breadth-first traversal always uses a loop, and is stack-safe.

It is _possible_ to implement depth first traversal with a leave method
using a loop rather than recursion, but maintaining the `leave(node,
[children])` API surface would be challenging, and is not implemented at
this time.
