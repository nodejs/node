// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_PROTECTORS_H_
#define V8_EXECUTION_PROTECTORS_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Protectors : public AllStatic {
 public:
  static const int kProtectorValid = 1;
  static const int kProtectorInvalid = 0;

#define DECLARED_PROTECTORS_ON_NATIVE_CONTEXT(V) \
  V(RegExpSpeciesLookupChainProtector, regexp_species_protector)

#define DECLARED_PROTECTORS_ON_ISOLATE(V) \
  V(ArraySpeciesLookupChain, ArraySpeciesProtector, array_species_protector)

#define DECLARE_PROTECTOR_ON_NATIVE_CONTEXT(name, unused_cell)               \
  static inline bool Is##name##Intact(Handle<NativeContext> native_context); \
  static void Invalidate##name(Isolate* isolate,                             \
                               Handle<NativeContext> native_context);
  DECLARED_PROTECTORS_ON_NATIVE_CONTEXT(DECLARE_PROTECTOR_ON_NATIVE_CONTEXT)
#undef DECLARE_PROTECTOR_ON_NATIVE_CONTEXT

#define DECLARE_PROTECTOR_ON_ISOLATE(name, unused_root_index, unused_cell) \
  static inline bool Is##name##Intact(Isolate* isolate);                   \
  static void Invalidate##name(Isolate* isolate);

  DECLARED_PROTECTORS_ON_ISOLATE(DECLARE_PROTECTOR_ON_ISOLATE)
#undef DECLARE_PROTECTOR_ON_ISOLATE
};

}  // namespace internal
}  // namespace v8

#endif  // V8_EXECUTION_PROTECTORS_H_
