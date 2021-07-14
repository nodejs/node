// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VERIFIER_H_
#define V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VERIFIER_H_

#include "src/heap/cppgc/marking-verifier.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE UnifiedHeapMarkingVerifier final
    : public cppgc::internal::MarkingVerifierBase {
 public:
  explicit UnifiedHeapMarkingVerifier(cppgc::internal::HeapBase&);
  ~UnifiedHeapMarkingVerifier() final = default;

 private:
  // TODO(chromium:1056170): Use a verification state that can handle JS
  // references.
  cppgc::internal::VerificationState state_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CPPGC_JS_UNIFIED_HEAP_MARKING_VERIFIER_H_
