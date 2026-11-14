#ifndef SRC_NODE_V8_EMBEDDER_H_
#define SRC_NODE_V8_EMBEDDER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstdint>
#include "v8-sandbox.h"

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

// Tags for cppgc-managed wrappers passed to v8::Object::Wrap(). Each wrapper
// type needs its own tag so that, with the V8 sandbox enabled, unwrapping an
// object as the wrong type yields nullptr.
enum class CppgcWrapperTag : uint16_t {
  // Used by the deprecated SetCppgcReference(), which wraps embedder types
  // unknown to Node.js.
  kSetCppgcReference =
      static_cast<uint16_t>(v8::CppHeapPointerTag::kFirstEmbedderWrappableTag),
  kContextifyContext,
  kContextifyScript,
  kLast = kContextifyScript,
};

static_assert(
    static_cast<uint16_t>(CppgcWrapperTag::kLast) <=
    static_cast<uint16_t>(v8::CppHeapPointerTag::kLastEmbedderWrappableTag));

constexpr v8::CppHeapPointerTag ToCppHeapPointerTag(CppgcWrapperTag tag) {
  return static_cast<v8::CppHeapPointerTag>(tag);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_V8_EMBEDDER_H_
