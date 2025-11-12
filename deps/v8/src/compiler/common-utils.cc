// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/common-utils.h"

#include "src/compiler/js-heap-broker.h"
#include "src/heap/local-heap.h"
#include "src/objects/string-inl.h"

namespace v8::internal::compiler::utils {

// Concatenates {left} and {right}. The result is fairly similar to creating a
// new ConsString with {left} and {right} and then flattening it, which we don't
// do because String::Flatten does not support background threads. Rather than
// implementing a full String::Flatten for background threads, we preferred to
// implement this Concatenate function, which, unlike String::Flatten, doesn't
// need to replace ConsStrings by ThinStrings.
MaybeHandle<String> ConcatenateStrings(Handle<String> left,
                                       Handle<String> right,
                                       JSHeapBroker* broker) {
  if (left->length() == 0) return right;
  if (right->length() == 0) return left;

  if (left->length() + right->length() > String::kMaxLength) {
    return {};
  }

  // Repeated concatenations have a quadratic cost (eg, "s+=a;s+=b;s+=c;...").
  // Rather than doing static analysis to determine how many concatenations we
  // there are and how many uses the result of each concatenation have, we
  // generate ConsString when the result of the concatenation would have more
  // than {kConstantStringFlattenMaxSize} characters, and flattened SeqString
  // otherwise.
  // TODO(dmercadier): ideally, we would like to get rid of this constant, and
  // always flatten. This requires some care to avoid the quadratic worst-case.
  constexpr int32_t kConstantStringFlattenMaxSize = 100;

  int32_t length = left->length() + right->length();
  if (length > kConstantStringFlattenMaxSize) {
    return broker->local_isolate_or_isolate()
        ->factory()
        ->NewConsString(left, right, AllocationType::kOld)
        .ToHandleChecked();
  }

  // If one of the string is not in readonly space, then we need a
  // SharedStringAccessGuardIfNeeded before accessing its content.
  bool require_guard = SharedStringAccessGuardIfNeeded::IsNeeded(
                           *left, broker->local_isolate_or_isolate()) ||
                       SharedStringAccessGuardIfNeeded::IsNeeded(
                           *right, broker->local_isolate_or_isolate());

  // Check string representation of both strings. This does not require the
  // SharedStringAccessGuardIfNeeded as the representation is stable.
  const bool result_is_one_byte_string =
      left->IsOneByteRepresentation() && right->IsOneByteRepresentation();

  if (result_is_one_byte_string) {
    // {left} and {right} are 1-byte ==> the result will be 1-byte.
    // Note that we need a canonical handle, because some callers
    // (JSNativeContextSpecialization) use the handle's address,
    //  which is meaningless if the handle isn't canonical.
    Handle<SeqOneByteString> flat = broker->CanonicalPersistentHandle(
        broker->local_isolate_or_isolate()
            ->factory()
            ->NewRawOneByteString(length, AllocationType::kOld)
            .ToHandleChecked());
    DisallowGarbageCollection no_gc;
    SharedStringAccessGuardIfNeeded access_guard(
        require_guard ? broker->local_isolate_or_isolate() : nullptr);
    String::WriteToFlat(*left, flat->GetChars(no_gc, access_guard), 0,
                        left->length(), access_guard);
    String::WriteToFlat(*right,
                        flat->GetChars(no_gc, access_guard) + left->length(), 0,
                        right->length(), access_guard);
    return flat;
  }

  // One (or both) of {left} and {right} is 2-byte ==> the result will be
  // 2-byte.
  Handle<SeqTwoByteString> flat = broker->CanonicalPersistentHandle(
      broker->local_isolate_or_isolate()
          ->factory()
          ->NewRawTwoByteString(length, AllocationType::kOld)
          .ToHandleChecked());
  DisallowGarbageCollection no_gc;
  SharedStringAccessGuardIfNeeded access_guard(
      require_guard ? broker->local_isolate_or_isolate() : nullptr);
  String::WriteToFlat(*left, flat->GetChars(no_gc, access_guard), 0,
                      left->length(), access_guard);
  String::WriteToFlat(*right,
                      flat->GetChars(no_gc, access_guard) + left->length(), 0,
                      right->length(), access_guard);
  return flat;
}

}  // namespace v8::internal::compiler::utils
