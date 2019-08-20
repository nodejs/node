// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/handles/handles.h"
#include "src/objects/maybe-object.h"
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

Handle<Object> CauseGC(Handle<Object> obj, Isolate* isolate) {
  isolate->heap()->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kTesting);

  return obj;
}

void TwoArgumentsFunction(Object a, Object b) {
  a->Print();
  b->Print();
}

void TestTwoArguments(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Handle<JSObject> obj2 = isolate->factory()->NewJSObjectWithNullProto();
  TwoArgumentsFunction(*CauseGC(obj1, isolate), *CauseGC(obj2, isolate));
}

void TwoSizeTArgumentsFunction(size_t a, size_t b) {
  USE(a);
  USE(b);
}

void TestTwoSizeTArguments(Isolate* isolate) {
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  Handle<JSObject> obj2 = isolate->factory()->NewJSObjectWithNullProto();
  TwoSizeTArgumentsFunction(sizeof(*CauseGC(obj1, isolate)),
                            sizeof(*CauseGC(obj2, isolate)));
}

class SomeObject : public Object {
 public:
  void Method(Object a) { a->Print(); }

  SomeObject& operator=(const Object& b) {
    this->Print();
    return *this;
  }

  DECL_CAST(SomeObject)

  OBJECT_CONSTRUCTORS(SomeObject, Object);
};

void TestMethodCall(Isolate* isolate) {
  SomeObject obj;
  Handle<SomeObject> so = handle(obj, isolate);
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  so->Method(*CauseGC(obj1, isolate));
}

void TestOperatorCall(Isolate* isolate) {
  SomeObject obj;
  Handle<JSObject> obj1 = isolate->factory()->NewJSObjectWithNullProto();
  obj = *CauseGC(obj1, isolate);
}

}  // namespace internal
}  // namespace v8
