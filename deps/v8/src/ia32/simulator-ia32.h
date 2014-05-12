// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IA32_SIMULATOR_IA32_H_
#define V8_IA32_SIMULATOR_IA32_H_

#include "allocation.h"

namespace v8 {
namespace internal {

// Since there is no simulator for the ia32 architecture the only thing we can
// do is to call the entry directly.
#define CALL_GENERATED_CODE(entry, p0, p1, p2, p3, p4) \
  (entry(p0, p1, p2, p3, p4))


typedef int (*regexp_matcher)(String*, int, const byte*,
                              const byte*, int*, int, Address, int, Isolate*);

// Call the generated regexp code directly. The code at the entry address should
// expect eight int/pointer sized arguments and return an int.
#define CALL_GENERATED_REGEXP_CODE(entry, p0, p1, p2, p3, p4, p5, p6, p7, p8) \
  (FUNCTION_CAST<regexp_matcher>(entry)(p0, p1, p2, p3, p4, p5, p6, p7, p8))


#define TRY_CATCH_FROM_ADDRESS(try_catch_address) \
  (reinterpret_cast<TryCatch*>(try_catch_address))

// The stack limit beyond which we will throw stack overflow errors in
// generated code. Because generated code on ia32 uses the C stack, we
// just use the C stack limit.
class SimulatorStack : public v8::internal::AllStatic {
 public:
  static inline uintptr_t JsLimitFromCLimit(Isolate* isolate,
                                            uintptr_t c_limit) {
    USE(isolate);
    return c_limit;
  }

  static inline uintptr_t RegisterCTryCatch(uintptr_t try_catch_address) {
    return try_catch_address;
  }

  static inline void UnregisterCTryCatch() { }
};

} }  // namespace v8::internal

#endif  // V8_IA32_SIMULATOR_IA32_H_
