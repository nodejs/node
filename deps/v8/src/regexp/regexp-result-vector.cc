// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/regexp-result-vector.h"

#include "src/execution/isolate.h"
#include "src/sandbox/sandbox-malloc.h"

namespace v8 {
namespace internal {

RegExpResultVectorScope::RegExpResultVectorScope(Isolate* isolate)
    : isolate_(isolate) {}

RegExpResultVectorScope::RegExpResultVectorScope(Isolate* isolate, int size)
    : isolate_(isolate) {
  Initialize(size);
}

RegExpResultVectorScope::~RegExpResultVectorScope() {
  if (is_dynamic_) {
    RegExpResultVector::Free(isolate_, value_);
  } else if (value_ != nullptr) {
    // Return ownership of the static vector.
    isolate_->set_regexp_static_result_offsets_vector(value_);
  } else {
    // The scope was created but Initialize never called. Nothing to do.
  }
}

int32_t* RegExpResultVectorScope::Initialize(int size) {
  DCHECK_NULL(value_);
  int32_t* static_vector_or_null =
      isolate_->regexp_static_result_offsets_vector();
  if (size > Isolate::kJSRegexpStaticOffsetsVectorSize ||
      static_vector_or_null == nullptr) {
    is_dynamic_ = true;
    value_ = RegExpResultVector::Allocate(isolate_, size);
  } else {
    value_ = static_vector_or_null;
    // Take ownership of the static vector. See also:
    // RegExpBuiltinsAssembler::TryLoadStaticRegExpResultVector.
    isolate_->set_regexp_static_result_offsets_vector(nullptr);
  }
  DCHECK_NOT_NULL(value_);
  return value_;
}

// Note this may be called through CallCFunction.
// static
int32_t* RegExpResultVector::Allocate(Isolate* isolate, uint32_t size) {
  DisallowGarbageCollection no_gc;
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  // TODO(428659277): this vector must be accessible to sandboxed code. Since
  // it only contains 32-bit offsets into the source string, it should be fine
  // to always allocate it inside the sandox once we have a performant
  // general-purpose in-sandbox memory allocator (see crbug.com/427464384). We
  // should double-check that it's safe to do that and probably change the type
  // to uint32_t to avoid accidental sign extension to size_t or similar bugs.
  int32_t* vector = SandboxAllocArray<int32_t>(size);
#else
  int32_t* vector = new int32_t[size];
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  isolate->active_dynamic_regexp_result_vectors().insert(vector);
  return vector;
}

// Note this may be called through CallCFunction.
// static
void RegExpResultVector::Free(Isolate* isolate, int32_t* vector) {
  DisallowGarbageCollection no_gc;
  DCHECK_NOT_NULL(vector);
  isolate->active_dynamic_regexp_result_vectors().erase(vector);
#ifdef V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
  SandboxFree(vector);
#else
  delete[] vector;
#endif  // V8_ENABLE_SANDBOX_HARDWARE_SUPPORT
}

}  // namespace internal
}  // namespace v8
