// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "include/v8-experimental.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

namespace {


static void SlowCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(41);
}


TEST(CompatibleReceiverBuiltin) {
  // Check that the HandleFastApiCall builtin visits the hidden prototypes
  // during the compatible receiver check.
  LocalContext context;
  v8::Isolate* isolate = context->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> current_context = isolate->GetCurrentContext();

  v8::Local<v8::FunctionTemplate> constructor_template =
      v8::FunctionTemplate::New(isolate);
  v8::Local<v8::FunctionTemplate> prototype_template =
      v8::FunctionTemplate::New(isolate);
  prototype_template->SetHiddenPrototype(true);

  v8::Local<v8::ObjectTemplate> proto_instance_template =
      prototype_template->InstanceTemplate();

  v8::experimental::FastAccessorBuilder* fast_accessor_builder =
      v8::experimental::FastAccessorBuilder::New(isolate);
  fast_accessor_builder->ReturnValue(
      fast_accessor_builder->IntegerConstant(42));
  v8::Local<v8::FunctionTemplate> accessor_template =
      v8::FunctionTemplate::NewWithFastHandler(
          isolate, SlowCallback, fast_accessor_builder, v8::Local<v8::Value>(),
          v8::Signature::New(isolate, prototype_template));

  proto_instance_template->SetAccessorProperty(
      v8_str("bar"), accessor_template, v8::Local<v8::FunctionTemplate>(),
      v8::ReadOnly);

  v8::Local<v8::Object> object =
      constructor_template->GetFunction(current_context)
          .ToLocalChecked()
          ->NewInstance(current_context)
          .ToLocalChecked();

  v8::Local<v8::Object> hidden_prototype =
      prototype_template->GetFunction(current_context)
          .ToLocalChecked()
          ->NewInstance(current_context)
          .ToLocalChecked();

  CHECK(object->SetPrototype(current_context, hidden_prototype).FromJust());

  context->Global()
      ->Set(current_context, v8_str("object"), object)
      .FromMaybe(false);

  CHECK_EQ(42, CompileRun("var getter = object.__lookupGetter__('bar');"
                          "getter.call(object)")
                   ->Int32Value(current_context)
                   .FromJust());
}

}  // namespace
