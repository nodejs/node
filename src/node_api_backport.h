#ifndef SRC_NODE_API_BACKPORT_H_
#define SRC_NODE_API_BACKPORT_H_

// This file contains stubs for symbols and other features (such as async hooks)
// which have appeared in later versions of Node.js, and which are required for
// N-API.

#define ToChecked FromJust

#endif  // SRC_NODE_API_BACKPORT_H_
