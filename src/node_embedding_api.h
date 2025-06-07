//
// Description: C-based API for embedding Node.js.
//
// !!! WARNING !!! WARNING !!! WARNING !!!
// This is a new API and is subject to change.
// While it is C-based, it is not ABI safe yet.
// Consider all functions and data structures as experimental.
// !!! WARNING !!! WARNING !!! WARNING !!!
//
// This file contains the C-based API for embedding Node.js in a host
// application. The API is designed to be used by applications that want to
// embed Node.js as a shared library (.so or .dll) and can interop with
// C-based API.
//

#ifndef SRC_NODE_EMBEDDING_API_H_
#define SRC_NODE_EMBEDDING_API_H_

#include "node_api.h"

#define NODE_EMBEDDING_VERSION 1

EXTERN_C_START

// Runs Node.js main function. It is the same as running Node.js from CLI.
NAPI_EXTERN int32_t NAPI_CDECL node_embedding_main(int32_t argc, char* argv[]);

EXTERN_C_END

#endif  // SRC_NODE_EMBEDDING_API_H_
