// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "src/v8.h"

#include "src/factory.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/factory.h -> src/objects-inl.h
#include "src/objects-inl.h"
// FIXME(mstarzinger, marja): This is weird, but required because of the missing
// (disallowed) include: src/feedback-vector.h ->
// src/feedback-vector-inl.h
#include "src/feedback-vector-inl.h"
#include "test/cctest/cctest.h"

namespace {

using namespace v8::internal;


TEST(ArrayList) {
  LocalContext context;
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);
  Handle<ArrayList> array(
      ArrayList::cast(isolate->heap()->empty_fixed_array()));
  CHECK_EQ(0, array->Length());
  array = ArrayList::Add(array, handle(Smi::FromInt(100), isolate));
  CHECK_EQ(1, array->Length());
  CHECK_EQ(100, Smi::cast(array->Get(0))->value());
  array = ArrayList::Add(array, handle(Smi::FromInt(200), isolate),
                         handle(Smi::FromInt(300), isolate));
  CHECK_EQ(3, array->Length());
  CHECK_EQ(100, Smi::cast(array->Get(0))->value());
  CHECK_EQ(200, Smi::cast(array->Get(1))->value());
  CHECK_EQ(300, Smi::cast(array->Get(2))->value());
  array->Set(2, Smi::FromInt(400));
  CHECK_EQ(400, Smi::cast(array->Get(2))->value());
  array->Clear(2, isolate->heap()->undefined_value());
  array->SetLength(2);
  CHECK_EQ(2, array->Length());
  CHECK_EQ(100, Smi::cast(array->Get(0))->value());
  CHECK_EQ(200, Smi::cast(array->Get(1))->value());
}

}  // namespace
