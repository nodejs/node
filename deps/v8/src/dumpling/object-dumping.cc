// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/dumpling/object-dumping.h"

#include <fstream>

#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/tagged.h"

namespace v8::internal {

void DifferentialFuzzingPrint(Tagged<Object> obj, std::ofstream& os) {
  Tagged<HeapObject> heap_object;

  DCHECK(!obj.IsCleared());

  if (!IsAnyHole(obj) && IsNumber(obj)) {
    static const int kBufferSize = 100;
    char chars[kBufferSize];
    base::Vector<char> buffer(chars, kBufferSize);
    if (IsSmi(obj)) {
      os << IntToStringView(obj.ToSmi().value(), buffer);
    } else {
      double number = Cast<HeapNumber>(obj)->value();
      os << DoubleToStringView(number, buffer);
    }
  } else if (obj.GetHeapObjectIfWeak(&heap_object)) {
    os << "[weak] HeapObject";
    // TODO(mdanylo): add function for detailed print of heap objects
  } else if (obj.GetHeapObjectIfStrong(&heap_object)) {
    os << "HeapObject";
    // TODO(mdanylo): add function for detailed print of heap objects.
  } else {
    UNREACHABLE();
  }
}

}  // namespace v8::internal
