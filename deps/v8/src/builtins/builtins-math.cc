// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-iterator-inl.h"
#include "src/builtins/builtins-math-xsum.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/execution/execution.h"
#include "src/execution/protectors-inl.h"
#include "src/numbers/conversions.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

BUILTIN(MathSumPrecise) {
  HandleScope scope(isolate);
  Handle<Object> items = args.atOrUndefined(isolate, 1);

  // 1. Perform ? RequireObjectCoercible(items).
  if (IsNullOrUndefined(*items, isolate)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kCalledOnNullOrUndefined,
                              isolate->factory()->NewStringFromAsciiChecked(
                                  "Math.sumPrecise")));
  }

  Xsum xsum;
  auto int_visitor = [&](int val) -> bool {
    xsum.AddForSumPrecise(val);
    return true;
  };

  auto double_visitor = [&](double val) -> bool {
    xsum.AddForSumPrecise(val);
    return true;
  };

  auto generic_visitor = [&](DirectHandle<Object> val) -> bool {
    if (!IsNumber(*val)) {
      DirectHandle<Object> error_args[] = {
          isolate->factory()->NewStringFromAsciiChecked("Iterator value"),
          Object::TypeOf(isolate, val)};
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kIsNotNumber, base::VectorOf(error_args)));
      return false;
    }
    xsum.AddForSumPrecise(Object::NumberValue(*val));
    return true;
  };

  if (IterableForEach(isolate, items, int_visitor, double_visitor,
                      generic_visitor, kMaxSafeIntegerUint64)
          .is_null()) {
    return ReadOnlyRoots(isolate).exception();
  }

  switch (auto res = xsum.GetSumPrecise(); std::get<Xsum::Result>(res)) {
    case Xsum::Result::kMinusZero:
      return ReadOnlyRoots(isolate).minus_zero_value();
    case Xsum::Result::kPlusInfinity:
      return *isolate->factory()->NewNumber(V8_INFINITY);
    case Xsum::Result::kMinusInfinity:
      return *isolate->factory()->NewNumber(-V8_INFINITY);
    case Xsum::Result::kNaN:
      return ReadOnlyRoots(isolate).nan_value();
    case Xsum::Result::kFinite:
      return *isolate->factory()->NewNumber(std::get<double>(res));
  }
}

}  // namespace internal
}  // namespace v8
