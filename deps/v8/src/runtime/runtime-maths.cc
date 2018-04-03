// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/assembler.h"
#include "src/base/utils/random-number-generator.h"
#include "src/bootstrapper.h"
#include "src/counters.h"
#include "src/double.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_GenerateRandomNumbers) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());

  Handle<Context> native_context = isolate->native_context();
  DCHECK_EQ(0, native_context->math_random_index()->value());

  static const int kCacheSize = 64;
  static const int kState0Offset = kCacheSize - 1;
  static const int kState1Offset = kState0Offset - 1;
  // The index is decremented before used to access the cache.
  static const int kInitialIndex = kState1Offset;

  Handle<FixedDoubleArray> cache;
  uint64_t state0 = 0;
  uint64_t state1 = 0;
  if (native_context->math_random_cache()->IsFixedDoubleArray()) {
    cache = Handle<FixedDoubleArray>(
        FixedDoubleArray::cast(native_context->math_random_cache()), isolate);
    state0 = double_to_uint64(cache->get_scalar(kState0Offset));
    state1 = double_to_uint64(cache->get_scalar(kState1Offset));
  } else {
    cache = Handle<FixedDoubleArray>::cast(
        isolate->factory()->NewFixedDoubleArray(kCacheSize, TENURED));
    native_context->set_math_random_cache(*cache);
    // Initialize state if not yet initialized. If a fixed random seed was
    // requested, use it to reset our state the first time a script asks for
    // random numbers in this context. This ensures the script sees a consistent
    // sequence.
    if (FLAG_random_seed != 0) {
      state0 = FLAG_random_seed;
      state1 = FLAG_random_seed;
    } else {
      while (state0 == 0 || state1 == 0) {
        isolate->random_number_generator()->NextBytes(&state0, sizeof(state0));
        isolate->random_number_generator()->NextBytes(&state1, sizeof(state1));
      }
    }
  }

  DisallowHeapAllocation no_gc;
  FixedDoubleArray* raw_cache = *cache;
  // Create random numbers.
  for (int i = 0; i < kInitialIndex; i++) {
    // Generate random numbers using xorshift128+.
    base::RandomNumberGenerator::XorShift128(&state0, &state1);
    raw_cache->set(i, base::RandomNumberGenerator::ToDouble(state0, state1));
  }

  // Persist current state.
  raw_cache->set(kState0Offset, uint64_to_double(state0));
  raw_cache->set(kState1Offset, uint64_to_double(state1));
  return Smi::FromInt(kInitialIndex);
}
}  // namespace internal
}  // namespace v8
