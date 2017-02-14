// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/frames-inl.h"

namespace v8 {
namespace internal {

void Builtins::Generate_IteratorPrototypeIterator(
    CodeStubAssembler* assembler) {
  assembler->Return(assembler->Parameter(0));
}

BUILTIN(ModuleNamespaceIterator) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> receiver = args.at<Object>(0);

  if (!receiver->IsJSModuleNamespace()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              isolate->factory()->iterator_symbol(), receiver));
  }
  auto ns = Handle<JSModuleNamespace>::cast(receiver);

  Handle<FixedArray> names =
      KeyAccumulator::GetKeys(ns, KeyCollectionMode::kOwnOnly, SKIP_SYMBOLS)
          .ToHandleChecked();
  return *isolate->factory()->NewJSFixedArrayIterator(names);
}

BUILTIN(FixedArrayIteratorNext) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  Handle<Object> receiver = args.at<Object>(0);

  // It is an error if this function is called on anything other than the
  // particular iterator object for which the function was created.
  if (!receiver->IsJSFixedArrayIterator() ||
      Handle<JSFixedArrayIterator>::cast(receiver)->initial_next() !=
          *args.target()) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewTypeError(MessageTemplate::kIncompatibleMethodReceiver,
                              isolate->factory()->next_string(), receiver));
  }

  auto iterator = Handle<JSFixedArrayIterator>::cast(receiver);
  Handle<Object> value;
  bool done;

  int index = iterator->index();
  if (index < iterator->array()->length()) {
    value = handle(iterator->array()->get(index), isolate);
    done = false;
    iterator->set_index(index + 1);
  } else {
    value = isolate->factory()->undefined_value();
    done = true;
  }

  return *isolate->factory()->NewJSIteratorResult(value, done);
}

}  // namespace internal
}  // namespace v8
