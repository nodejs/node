#ifndef SRC_NODE_CONTEXT_DATA_H_
#define SRC_NODE_CONTEXT_DATA_H_

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

enum ContextEmbedderIndex {
  kEnvironment = NODE_CONTEXT_EMBEDDER_DATA_INDEX,
  kSandboxObject = NODE_CONTEXT_SANDBOX_OBJECT_INDEX,
  kAllowWasmCodeGeneration = NODE_CONTEXT_ALLOW_WASM_CODE_GENERATION_INDEX,
};

}  // namespace node

#endif  // SRC_NODE_CONTEXT_DATA_H_
