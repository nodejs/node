// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/machine-type.h"
#include "src/ostreams.h"

namespace v8 {
namespace internal {
namespace compiler {

#define PRINT(bit)         \
  if (type & bit) {        \
    if (before) os << "|"; \
    os << #bit;            \
    before = true;         \
  }


std::ostream& operator<<(std::ostream& os, const MachineType& type) {
  bool before = false;
  PRINT(kRepBit);
  PRINT(kRepWord8);
  PRINT(kRepWord16);
  PRINT(kRepWord32);
  PRINT(kRepWord64);
  PRINT(kRepFloat32);
  PRINT(kRepFloat64);
  PRINT(kRepTagged);

  PRINT(kTypeBool);
  PRINT(kTypeInt32);
  PRINT(kTypeUint32);
  PRINT(kTypeInt64);
  PRINT(kTypeUint64);
  PRINT(kTypeNumber);
  PRINT(kTypeAny);
  return os;
}


#undef PRINT

}  // namespace compiler
}  // namespace internal
}  // namespace v8
