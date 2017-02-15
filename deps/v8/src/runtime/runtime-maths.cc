// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/codegen.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_GenerateRandomNumbers) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  if (isolate->serializer_enabled()) {
    // Random numbers in the snapshot are not really that random. And we cannot
    // return a typed array as it cannot be serialized. To make calling
    // Math.random possible when creating a custom startup snapshot, we simply
    // return a normal array with a single random number.
    Handle<HeapNumber> random_number = isolate->factory()->NewHeapNumber(
        isolate->random_number_generator()->NextDouble());
    Handle<FixedArray> array_backing = isolate->factory()->NewFixedArray(1);
    array_backing->set(0, *random_number);
    return *isolate->factory()->NewJSArrayWithElements(array_backing);
  }

  static const int kState0Offset = 0;
  static const int kState1Offset = 1;
  static const int kRandomBatchSize = 64;
  CONVERT_ARG_HANDLE_CHECKED(Object, maybe_typed_array, 0);
  Handle<JSTypedArray> typed_array;
  // Allocate typed array if it does not yet exist.
  if (maybe_typed_array->IsJSTypedArray()) {
    typed_array = Handle<JSTypedArray>::cast(maybe_typed_array);
  } else {
    static const int kByteLength = kRandomBatchSize * kDoubleSize;
    Handle<JSArrayBuffer> buffer =
        isolate->factory()->NewJSArrayBuffer(SharedFlag::kNotShared, TENURED);
    JSArrayBuffer::SetupAllocatingData(buffer, isolate, kByteLength, true,
                                       SharedFlag::kNotShared);
    typed_array = isolate->factory()->NewJSTypedArray(
        kExternalFloat64Array, buffer, 0, kRandomBatchSize);
  }

  DisallowHeapAllocation no_gc;
  double* array =
      reinterpret_cast<double*>(typed_array->GetBuffer()->backing_store());
  // Fetch existing state.
  uint64_t state0 = double_to_uint64(array[kState0Offset]);
  uint64_t state1 = double_to_uint64(array[kState1Offset]);
  // Initialize state if not yet initialized.
  while (state0 == 0 || state1 == 0) {
    isolate->random_number_generator()->NextBytes(&state0, sizeof(state0));
    isolate->random_number_generator()->NextBytes(&state1, sizeof(state1));
  }
  // Create random numbers.
  for (int i = kState1Offset + 1; i < kRandomBatchSize; i++) {
    // Generate random numbers using xorshift128+.
    base::RandomNumberGenerator::XorShift128(&state0, &state1);
    array[i] = base::RandomNumberGenerator::ToDouble(state0, state1);
  }
  // Persist current state.
  array[kState0Offset] = uint64_to_double(state0);
  array[kState1Offset] = uint64_to_double(state1);
  return *typed_array;
}
}  // namespace internal
}  // namespace v8
