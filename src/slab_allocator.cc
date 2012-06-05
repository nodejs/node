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

#include "v8.h"
#include "node.h"
#include "node_buffer.h"
#include "slab_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;
using v8::V8;


namespace node {

SlabAllocator::SlabAllocator(unsigned int size) {
  size_ = ROUND_UP(size ? size : 1, 8192);
  initialized_ = false;
}


SlabAllocator::~SlabAllocator() {
  if (!initialized_) return;
  if (V8::IsDead()) return;
  slab_sym_.Dispose();
  slab_sym_.Clear();
  slab_.Dispose();
  slab_.Clear();
}


void SlabAllocator::Initialize() {
  HandleScope scope;
  char sym[256];
  snprintf(sym, sizeof(sym), "slab_%p", this); // namespace object key
  offset_ = 0;
  last_ptr_ = NULL;
  initialized_ = true;
  slab_sym_ = Persistent<String>::New(String::New(sym));
}


static Local<Object> NewSlab(unsigned int size) {
  HandleScope scope;
  Local<Value> arg = Integer::NewFromUnsigned(ROUND_UP(size, 16));
  Local<Object> buf = Buffer::constructor_template
                      ->GetFunction()
                      ->NewInstance(1, &arg);
  return scope.Close(buf);
}


char* SlabAllocator::Allocate(Handle<Object> obj, unsigned int size) {
  HandleScope scope;

  assert(!obj.IsEmpty());

  if (size == 0) return NULL;
  if (!initialized_) Initialize();

  if (size > size_) {
    Local<Object> buf = NewSlab(size);
    obj->SetHiddenValue(slab_sym_, buf);
    return Buffer::Data(buf);
  }

  if (slab_.IsEmpty() || offset_ + size > size_) {
    slab_.Dispose();
    slab_.Clear();
    slab_ = Persistent<Object>::New(NewSlab(size_));
    offset_ = 0;
    last_ptr_ = NULL;
  }

  obj->SetHiddenValue(slab_sym_, slab_);
  last_ptr_ = Buffer::Data(slab_) + offset_;
  offset_ += size;

  return last_ptr_;
}


Local<Object> SlabAllocator::Shrink(Handle<Object> obj,
                                    char* ptr,
                                    unsigned int size) {
  HandleScope scope;
  Local<Value> slab_v = obj->GetHiddenValue(slab_sym_);
  obj->SetHiddenValue(slab_sym_, Null());
  assert(!slab_v.IsEmpty());
  assert(slab_v->IsObject());
  Local<Object> slab = slab_v->ToObject();
  assert(ptr != NULL);
  if (ptr == last_ptr_) {
    last_ptr_ = NULL;
    offset_ = ptr - Buffer::Data(slab) + ROUND_UP(size, 16);
  }
  return scope.Close(slab);
}


} // namespace node
