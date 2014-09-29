// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_MACHINE_TYPE_H_
#define V8_COMPILER_MACHINE_TYPE_H_

namespace v8 {
namespace internal {
namespace compiler {

// An enumeration of the storage representations at the machine level.
// - Words are uninterpreted bits of a given fixed size that can be used
//   to store integers and pointers. They are normally allocated to general
//   purpose registers by the backend and are not tracked for GC.
// - Floats are bits of a given fixed size that are used to store floating
//   point numbers. They are normally allocated to the floating point
//   registers of the machine and are not tracked for the GC.
// - Tagged values are the size of a reference into the heap and can store
//   small words or references into the heap using a language and potentially
//   machine-dependent tagging scheme. These values are tracked by the code
//   generator for precise GC.
enum MachineType {
  kMachineWord8,
  kMachineWord16,
  kMachineWord32,
  kMachineWord64,
  kMachineFloat64,
  kMachineTagged,
  kMachineLast
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_MACHINE_TYPE_H_
