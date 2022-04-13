#ifndef SRC_NODE_CONTEXT_DATA_H_
#define SRC_NODE_CONTEXT_DATA_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

namespace node {

// Pick an index that's hopefully out of the way when we're embedded inside
// another application. Performance-wise or memory-wise it doesn't matter:
// Context::SetAlignedPointerInEmbedderData() is backed by a FixedArray,
// worst case we pay a one-time penalty for resizing the array.
#ifndef NODE_CONTEXT_EMBEDDER_DATA_INDEX
#define NODE_CONTEXT_EMBEDDER_DATA_INDEX 32
#endif

#ifndef NODE_CONTEXT_SANDBOX_OBJECT_INDEX
#define NODE_CONTEXT_SANDBOX_OBJECT_INDEX 33
#endif

#ifndef NODE_CONTEXT_ALLOW_WASM_CODE_GENERATION_INDEX
#define NODE_CONTEXT_ALLOW_WASM_CODE_GENERATION_INDEX 34
#endif

#ifndef NODE_CONTEXT_TAG
#define NODE_CONTEXT_TAG 35
#endif

#ifndef NODE_BINDING_LIST
#define NODE_BINDING_LIST_INDEX 36
#endif

enum ContextEmbedderIndex {
  kEnvironment = NODE_CONTEXT_EMBEDDER_DATA_INDEX,
  kSandboxObject = NODE_CONTEXT_SANDBOX_OBJECT_INDEX,
  kAllowWasmCodeGeneration = NODE_CONTEXT_ALLOW_WASM_CODE_GENERATION_INDEX,
  kContextTag = NODE_CONTEXT_TAG,
  kBindingListIndex = NODE_BINDING_LIST_INDEX
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_CONTEXT_DATA_H_
