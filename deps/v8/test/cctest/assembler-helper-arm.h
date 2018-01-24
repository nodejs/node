// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
#define V8_CCTEST_ASSEMBLER_HELPER_ARM_H_

#include <functional>

#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

// These function prototypes have 5 arguments since they are used with the
// CALL_GENERATED_CODE macro.
typedef Object* (*F_iiiii)(int x, int p1, int p2, int p3, int p4);
typedef Object* (*F_piiii)(void* p0, int p1, int p2, int p3, int p4);
typedef Object* (*F_ppiii)(void* p0, void* p1, int p2, int p3, int p4);
typedef Object* (*F_pppii)(void* p0, void* p1, void* p2, int p3, int p4);
typedef Object* (*F_ippii)(int p0, void* p1, void* p2, int p3, int p4);

Address AssembleCode(std::function<void(Assembler&)> assemble);

}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_ASSEMBLER_HELPER_ARM_H_
