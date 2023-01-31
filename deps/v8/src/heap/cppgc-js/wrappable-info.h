// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_H_
#define V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_H_

#include "include/v8-cppgc.h"
#include "src/base/optional.h"
#include "src/objects/embedder-data-slot.h"
#include "src/objects/js-objects.h"

namespace v8::internal {

class Isolate;

struct WrappableInfo final {
 public:
  static V8_INLINE base::Optional<WrappableInfo> From(Isolate*, JSObject,
                                                      const WrapperDescriptor&);
  static V8_INLINE base::Optional<WrappableInfo> From(
      Isolate*, const EmbedderDataSlot& type_slot,
      const EmbedderDataSlot& instance_slot, const WrapperDescriptor&);

  constexpr WrappableInfo(void* type, void* instance)
      : type(type), instance(instance) {}

  void* type = nullptr;
  void* instance = nullptr;
};

}  // namespace v8::internal

#endif  // V8_HEAP_CPPGC_JS_WRAPPABLE_INFO_H_
