// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
#define V8_BUILTINS_BUILTINS_DESCRIPTORS_H_

#include "src/builtins/builtins.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/code-assembler.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

#define REVERSE_0(a) a
#define REVERSE_1(a, b) b, a
#define REVERSE_2(a, b, c) c, b, a
#define REVERSE_3(a, b, c, d) d, c, b, a
#define REVERSE_4(a, b, c, d, e) e, d, c, b, a
#define REVERSE_5(a, b, c, d, e, f) f, e, d, c, b, a
#define REVERSE_6(a, b, c, d, e, f, g) g, f, e, d, c, b, a
#define REVERSE_7(a, b, c, d, e, f, g, h) h, g, f, e, d, c, b, a
#define REVERSE_8(a, b, c, d, e, f, g, h, i) i, h, g, f, e, d, c, b, a
#define REVERSE_kDontAdaptArgumentsSentinel(...)
#define REVERSE(N, ...) REVERSE_##N(__VA_ARGS__)

// Define interface descriptors for builtins with JS linkage.
#define DEFINE_TFJ_INTERFACE_DESCRIPTOR_HELPER(Name, Argc, ...)         \
  struct Builtin_##Name##_InterfaceDescriptor {                         \
    enum ParameterIndices {                                             \
      kJSTarget = compiler::CodeAssembler::kTargetParameterIndex,       \
      ##__VA_ARGS__,                                                    \
      kJSNewTarget,                                                     \
      kJSActualArgumentsCount,                                          \
      kContext,                                                         \
      kParameterCount,                                                  \
    };                                                                  \
    static_assert((Argc) == static_cast<uint16_t>(kParameterCount - 4), \
                  "Inconsistent set of arguments");                     \
    static_assert(kJSTarget == -1, "Unexpected kJSTarget index value"); \
  };

#ifdef V8_REVERSE_JSARGS
#define DEFINE_TFJ_INTERFACE_DESCRIPTOR(Name, Argc, ...) \
  DEFINE_TFJ_INTERFACE_DESCRIPTOR_HELPER(Name, Argc, REVERSE(Argc, __VA_ARGS__))
#else
#define DEFINE_TFJ_INTERFACE_DESCRIPTOR(Name, Argc, ...) \
  DEFINE_TFJ_INTERFACE_DESCRIPTOR_HELPER(Name, Argc, ##__VA_ARGS__)
#endif

// Define interface descriptors for builtins with StubCall linkage.
#define DEFINE_TFC_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

#define DEFINE_TFS_INTERFACE_DESCRIPTOR(Name, ...) \
  using Builtin_##Name##_InterfaceDescriptor = Name##Descriptor;

// Define interface descriptors for IC handlers/dispatchers.
#define DEFINE_TFH_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

#define DEFINE_ASM_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

BUILTIN_LIST(IGNORE_BUILTIN, DEFINE_TFJ_INTERFACE_DESCRIPTOR,
             DEFINE_TFC_INTERFACE_DESCRIPTOR, DEFINE_TFS_INTERFACE_DESCRIPTOR,
             DEFINE_TFH_INTERFACE_DESCRIPTOR, IGNORE_BUILTIN,
             DEFINE_ASM_INTERFACE_DESCRIPTOR)

#undef DEFINE_TFJ_INTERFACE_DESCRIPTOR
#undef DEFINE_TFC_INTERFACE_DESCRIPTOR
#undef DEFINE_TFS_INTERFACE_DESCRIPTOR
#undef DEFINE_TFH_INTERFACE_DESCRIPTOR
#undef DEFINE_ASM_INTERFACE_DESCRIPTOR

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
