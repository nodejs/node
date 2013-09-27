// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_WEAK_OBJECT_INL_H_
#define SRC_WEAK_OBJECT_INL_H_

#include "weak-object.h"
#include "async-wrap.h"
#include "async-wrap-inl.h"
#include "util.h"
#include "util-inl.h"

namespace node {

WeakObject::WeakObject(Environment* env, v8::Local<v8::Object> object)
    : AsyncWrap(env, object) {
  persistent().MarkIndependent();

  // The pointer is resolved as void*.
  Wrap<WeakObject>(object, this);
  MakeWeak();
}

WeakObject::~WeakObject() {
}

void WeakObject::MakeWeak() {
  persistent().MakeWeak(this, WeakCallback);
}

void WeakObject::ClearWeak() {
  persistent().ClearWeak();
}

void WeakObject::WeakCallback(v8::Isolate* isolate,
                              v8::Persistent<v8::Object>* persistent,
                              WeakObject* self) {
  // Dispose now instead of in the destructor to avoid child classes that call
  // `delete this` in their destructor from blowing up.
  // Dispose the class member instead of the argument or else the IsEmpty()
  // check in ~AsyncWrap will fail.
  self->persistent().Dispose();
  delete self;
}

}  // namespace node

#endif  // SRC_WEAK_OBJECT_INL_H_
