// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_CPPHEAP_POINTER_TAG_H_
#define V8_SANDBOX_CPPHEAP_POINTER_TAG_H_

#include "include/v8-sandbox.h"

namespace v8::internal {

// Tags for V8-internal CppHeap objects that inherit from v8::Object::Wrappable.
#define V8_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(V) \
  V(kTagForTesting)                                \
  V(kInspectorV8ConsoleTag)                        \
  V(kInspectorTaskInfoTag)                         \
  V(kWasmMemoryMapDescriptorTag)

// Tags for V8-internal CppHeap objects that do NOT inherit from
// v8::Object::Wrappable.
#define V8_NON_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(V) \
  V(kMicrotaskQueueTag)                                \
  V(kCppGCManagedTag)                                  \
  V(kEmbedderDataSlotTag)

enum class WrappableCppHeapPointerTag : uint16_t {
  kFirst =
      static_cast<uint16_t>(CppHeapPointerTag::kFirstV8InternalWrappableTag),
  // Repeat kFirst so the first macro entry gets value kFirst.
  kBeforeFirst = kFirst - 1,
#define DECLARE_WRAPPABLE_TAG_ENUM(name) name,
  V8_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(DECLARE_WRAPPABLE_TAG_ENUM)
#undef DECLARE_WRAPPABLE_TAG_ENUM
      kLast,
};

constexpr CppHeapPointerTagRange kV8InternalWrappableTagRange(
    CppHeapPointerTag::kFirstV8InternalWrappableTag,
    CppHeapPointerTag::kLastV8InternalWrappableTag);

constexpr CppHeapPointerTagRange kV8InternalNonWrappableTagRange(
    CppHeapPointerTag::kFirstV8InternalNonWrappableTag,
    CppHeapPointerTag::kLastV8InternalNonWrappableTag);

static_assert(kObjectWrappableTagRange.Contains(kV8InternalWrappableTagRange));
static_assert(kNonWrappableTagRange.Contains(kV8InternalNonWrappableTagRange));

static_assert(
    static_cast<uint16_t>(WrappableCppHeapPointerTag::kLast) - 1 <=
        static_cast<uint16_t>(CppHeapPointerTag::kLastV8InternalWrappableTag),
    "WrappableCppHeapPointerTag exceeds kV8InternalWrappableTagRange");

#define DEFINE_WRAPPABLE_TAG_CONSTANT(name) \
  inline constexpr CppHeapPointerTag name = \
      static_cast<CppHeapPointerTag>(WrappableCppHeapPointerTag::name);
V8_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(DEFINE_WRAPPABLE_TAG_CONSTANT)
#undef DEFINE_WRAPPABLE_TAG_CONSTANT

enum class NonWrappableCppHeapPointerTag : uint16_t {
  kFirst =
      static_cast<uint16_t>(CppHeapPointerTag::kFirstV8InternalNonWrappableTag),
  kBeforeFirst = kFirst - 1,
#define DECLARE_NON_WRAPPABLE_TAG_ENUM(name) name,
  V8_NON_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(DECLARE_NON_WRAPPABLE_TAG_ENUM)
#undef DECLARE_NON_WRAPPABLE_TAG_ENUM
      kLast,
};

static_assert(
    static_cast<uint16_t>(NonWrappableCppHeapPointerTag::kLast) - 1 <=
        static_cast<uint16_t>(
            CppHeapPointerTag::kLastV8InternalNonWrappableTag),
    "NonWrappableCppHeapPointerTag exceeds kV8InternalNonWrappableTagRange");

#define DEFINE_NON_WRAPPABLE_TAG_CONSTANT(name) \
  inline constexpr CppHeapPointerTag name =     \
      static_cast<CppHeapPointerTag>(NonWrappableCppHeapPointerTag::name);
V8_NON_WRAPPABLE_CPP_HEAP_POINTER_TAGS_LIST(DEFINE_NON_WRAPPABLE_TAG_CONSTANT)
#undef DEFINE_NON_WRAPPABLE_TAG_CONSTANT

}  // namespace v8::internal

#endif  // V8_SANDBOX_CPPHEAP_POINTER_TAG_H_
