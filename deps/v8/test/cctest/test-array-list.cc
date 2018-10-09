// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/heap/factory.h"
#include "src/objects-inl.h"
#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

TEST(ArrayList) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<ArrayList> array(
      ArrayList::cast(ReadOnlyRoots(isolate).empty_fixed_array()), isolate);
  CHECK_EQ(0, array->Length());
  array = ArrayList::Add(isolate, array, handle(Smi::FromInt(100), isolate));
  CHECK_EQ(1, array->Length());
  CHECK_EQ(100, Smi::ToInt(array->Get(0)));
  array = ArrayList::Add(isolate, array, handle(Smi::FromInt(200), isolate),
                         handle(Smi::FromInt(300), isolate));
  CHECK_EQ(3, array->Length());
  CHECK_EQ(100, Smi::ToInt(array->Get(0)));
  CHECK_EQ(200, Smi::ToInt(array->Get(1)));
  CHECK_EQ(300, Smi::ToInt(array->Get(2)));
  array->Set(2, Smi::FromInt(400));
  CHECK_EQ(400, Smi::ToInt(array->Get(2)));
  array->Clear(2, ReadOnlyRoots(isolate).undefined_value());
  array->SetLength(2);
  CHECK_EQ(2, array->Length());
  CHECK_EQ(100, Smi::ToInt(array->Get(0)));
  CHECK_EQ(200, Smi::ToInt(array->Get(1)));
}

}  // namespace internal
}  // namespace v8
