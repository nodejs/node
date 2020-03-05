// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_OFF_THREAD_FACTORY_H_
#define V8_HEAP_OFF_THREAD_FACTORY_H_

#include <map>
#include <vector>
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/factory-base.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

class AstValueFactory;
class AstRawString;
class AstConsString;

class OffThreadFactory;

template <>
struct FactoryTraits<OffThreadFactory> {
  template <typename T>
  using HandleType = OffThreadHandle<T>;
  template <typename T>
  using MaybeHandleType = OffThreadHandle<T>;
};

struct RelativeSlot {
  RelativeSlot() = default;
  RelativeSlot(Address object_address, int slot_offset)
      : object_address(object_address), slot_offset(slot_offset) {}

  Address object_address;
  int slot_offset;
};

class V8_EXPORT_PRIVATE OffThreadFactory
    : public FactoryBase<OffThreadFactory> {
 public:
  explicit OffThreadFactory(Isolate* isolate);

  ReadOnlyRoots read_only_roots() const { return roots_; }

#define ROOT_ACCESSOR(Type, name, CamelName) \
  inline OffThreadHandle<Type> name();
  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  void FinishOffThread();
  void Publish(Isolate* isolate);

  OffThreadHandle<Object> NewInvalidStringLengthError();

  OffThreadHandle<FixedArray> StringWrapperForTest(
      OffThreadHandle<String> string);

 private:
  friend class FactoryBase<OffThreadFactory>;

  // ------
  // Customization points for FactoryBase.
  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kWordAligned);
  template <typename T>
  OffThreadHandle<T> Throw(OffThreadHandle<Object> exception) {
    // TODO(leszeks): Figure out what to do here.
    return OffThreadHandle<T>();
  }
  [[noreturn]] void FatalProcessOutOfHeapMemory(const char* location);
  inline bool CanAllocateInReadOnlySpace() { return false; }
  inline bool EmptyStringRootIsInitialized() { return true; }
  // ------

  OffThreadHandle<String> MakeOrFindTwoCharacterString(uint16_t c1,
                                                       uint16_t c2);

  ReadOnlyRoots roots_;
  OffThreadSpace space_;
  OffThreadLargeObjectSpace lo_space_;
  std::vector<RelativeSlot> string_slots_;
  bool is_finished = false;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_OFF_THREAD_FACTORY_H_
