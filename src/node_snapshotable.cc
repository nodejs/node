
#include "node_snapshotable.h"
#include "base_object-inl.h"

namespace node {

using v8::Local;
using v8::Object;
using v8::StartupData;

void DeserializeNodeInternalFields(Local<Object> holder,
                                   int index,
                                   v8::StartupData payload,
                                   void* env) {
  if (payload.raw_size == 0) {
    holder->SetAlignedPointerInInternalField(index, nullptr);
    return;
  }
  // No embedder object in the builtin snapshot yet.
  UNREACHABLE();
}

StartupData SerializeNodeContextInternalFields(Local<Object> holder,
                                               int index,
                                               void* env) {
  void* ptr = holder->GetAlignedPointerFromInternalField(index);
  if (ptr == nullptr || ptr == env) {
    return StartupData{nullptr, 0};
  }
  if (ptr == env && index == ContextEmbedderIndex::kEnvironment) {
    return StartupData{nullptr, 0};
  }

  // No embedder objects in the builtin snapshot yet.
  UNREACHABLE();
  return StartupData{nullptr, 0};
}

}  // namespace node
