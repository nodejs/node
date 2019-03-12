// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARGUMENTS_INL_H_
#define V8_ARGUMENTS_INL_H_

#include "src/arguments.h"

#include "src/handles-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

template <class S>
Handle<S> Arguments::at(int index) {
  return Handle<S>::cast(at<Object>(index));
}

int Arguments::smi_at(int index) {
  return Smi::ToInt(Object(*address_of_arg_at(index)));
}

double Arguments::number_at(int index) { return (*this)[index]->Number(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_ARGUMENTS_INL_H_
