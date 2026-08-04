#ifndef SRC_NODE_V8_EMBEDDER_H_
#define SRC_NODE_V8_EMBEDDER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>

namespace node {

// Type tags for different kinds of embedder data stored in V8 aligned pointer
// slots.
enum EmbedderDataTag : uint16_t {
  // kDefault is used in slots that don't use V8 type tagging.
  kDefault = 0,
  // kEmbedderType is used by BaseObject to store the kEmbedderType value.
  kEmbedderType,
  // kPerContextData is used to store data on a `v8::Context`, including
  // slots indexed by ContextEmbedderIndex.
  kPerContextData,
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_EMBEDDER_H_
