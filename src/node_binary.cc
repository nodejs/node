// Copyright 2009, Ryan Dahl <ry@tinyclouds.org>. All rights reserved.
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
#include <node_byte_string.h>

namespace node {

using namespace v8;

static v8::Persistent<v8::FunctionTemplate> byte_string_template;

// V8 contstructor
static Handle<Value> New(const Arguments& args) {
  HandleScope scope;

  // somehow create the c object.
  byte_string *bs = malloc(sizeof(byte_string) + length);


  bs->handle = v8::Persistent<v8::Object>::New(args.This());
  bs->handle->SetInternalField(0, v8::External::New(bs));

  return args.This();
}


byte_string* byte_string_new(size_t length)
{
  byte_string_template->GetFunction()->NewInstance();

  // unwrap handle

  return bs;
}



void byte_string_init(v8::Local<v8::Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(New);

  byte_string_template = Persistent<FunctionTemplate>::New(t);
  byte_string_template->InstanceTemplate()->SetInternalFieldCount(1);
  // Set class name for debugging output
  byte_string_template->SetClassName(String::NewSymbol("Binary"));

  NODE_SET_PROTOTYPE_METHOD(byte_string_template, "toString", ToString);

  target->Set(String::NewSymbol("Binary"),
      byte_string_template->GetFunction());
}

}  // namespace node
