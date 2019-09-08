// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/protectors.h"

#include "src/execution/isolate-inl.h"
#include "src/execution/protectors-inl.h"
#include "src/handles/handles-inl.h"
#include "src/objects/contexts.h"
#include "src/objects/property-cell.h"
#include "src/objects/smi.h"
#include "src/tracing/trace-event.h"
#include "src/utils/utils.h"

namespace v8 {
namespace internal {

#define INVALIDATE_PROTECTOR_ON_NATIVE_CONTEXT_DEFINITION(name, cell)       \
  void Protectors::Invalidate##name(Isolate* isolate,                       \
                                    Handle<NativeContext> native_context) { \
    DCHECK_EQ(*native_context, isolate->raw_native_context());              \
    DCHECK(native_context->cell().value().IsSmi());                         \
    DCHECK(Is##name##Intact(native_context));                               \
    Handle<PropertyCell> species_cell(native_context->cell(), isolate);     \
    PropertyCell::SetValueWithInvalidation(                                 \
        isolate, #cell, species_cell,                                       \
        handle(Smi::FromInt(kProtectorInvalid), isolate));                  \
    DCHECK(!Is##name##Intact(native_context));                              \
  }
DECLARED_PROTECTORS_ON_NATIVE_CONTEXT(
    INVALIDATE_PROTECTOR_ON_NATIVE_CONTEXT_DEFINITION)
#undef INVALIDATE_PROTECTOR_ON_NATIVE_CONTEXT_DEFINITION

#define INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION(name, unused_index, cell) \
  void Protectors::Invalidate##name(Isolate* isolate) {                      \
    DCHECK(isolate->factory()->cell()->value().IsSmi());                     \
    DCHECK(Is##name##Intact(isolate));                                       \
    PropertyCell::SetValueWithInvalidation(                                  \
        isolate, #cell, isolate->factory()->cell(),                          \
        handle(Smi::FromInt(kProtectorInvalid), isolate));                   \
    DCHECK(!Is##name##Intact(isolate));                                      \
  }
DECLARED_PROTECTORS_ON_ISOLATE(INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION)
#undef INVALIDATE_PROTECTOR_ON_ISOLATE_DEFINITION

}  // namespace internal
}  // namespace v8
