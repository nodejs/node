// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
#define V8_BUILTINS_BUILTINS_DESCRIPTORS_H_

#include "src/builtins/builtins.h"
#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

// Define interface descriptors for builtins with JS linkage.
#define DEFINE_TFJ_INTERFACE_DESCRIPTOR(Name, Argc, ...) \
  struct Builtin_##Name##_InterfaceDescriptor {          \
    enum ParameterIndices {                              \
      kReceiver,                                         \
      ##__VA_ARGS__,                                     \
      kNewTarget,                                        \
      kActualArgumentsCount,                             \
      kContext,                                          \
      kParameterCount,                                   \
    };                                                   \
  };

// Define interface descriptors for builtins with StubCall linkage.
#define DEFINE_TFC_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor, \
                                        result_size)               \
  typedef InterfaceDescriptor##Descriptor Builtin_##Name##_InterfaceDescriptor;

#define DEFINE_TFS_INTERFACE_DESCRIPTOR(Name, ...) \
  typedef Name##Descriptor Builtin_##Name##_InterfaceDescriptor;

// Define interface descriptors for IC handlers/dispatchers.
#define DEFINE_TFH_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  typedef InterfaceDescriptor##Descriptor Builtin_##Name##_InterfaceDescriptor;

BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, DEFINE_TFJ_INTERFACE_DESCRIPTOR,
             DEFINE_TFC_INTERFACE_DESCRIPTOR, DEFINE_TFS_INTERFACE_DESCRIPTOR,
             DEFINE_TFH_INTERFACE_DESCRIPTOR, IGNORE_BUILTIN)

#undef DEFINE_TFJ_INTERFACE_DESCRIPTOR
#undef DEFINE_TFC_INTERFACE_DESCRIPTOR
#undef DEFINE_TFS_INTERFACE_DESCRIPTOR
#undef DEFINE_TFH_INTERFACE_DESCRIPTOR

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
