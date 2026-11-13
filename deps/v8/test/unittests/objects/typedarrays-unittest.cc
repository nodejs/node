// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <memory>
#include <string>

#include "include/v8-array-buffer.h"
#include "include/v8-context.h"
#include "include/v8-exception.h"
#include "include/v8-isolate.h"
#include "src/execution/protectors-inl.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace typedarrays_unittest {

class TypedArraysTest : public v8::TestWithContext {
 protected:
  void TestArrayBufferViewContents(bool should_use_buffer) {
    v8::Local<v8::Object> obj_a = v8::Local<v8::Object>::Cast(
        context()->Global()->Get(context(), NewString("a")).ToLocalChecked());
    CHECK(obj_a->IsArrayBufferView());
    v8::Local<v8::ArrayBufferView> array_buffer_view =
        v8::Local<v8::ArrayBufferView>::Cast(obj_a);
    CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
    unsigned char contents[4] = {23, 23, 23, 23};
    CHECK_EQ(sizeof(contents),
             array_buffer_view->CopyContents(contents, sizeof(contents)));
    CHECK_EQ(array_buffer_view->HasBuffer(), should_use_buffer);
    for (size_t i = 0; i < sizeof(contents); ++i) {
      CHECK_EQ(i, contents[i]);
    }
  }
};

using TypedArraysUninitializedTest = v8::TestWithPlatform;

v8::Local<v8::Value> CompileRun(v8::Isolate* isolate,
                                v8::Local<v8::Context> context,
                                const char* source) {
  v8::Local<v8::String> source_str =
      v8::String::NewFromUtf8(isolate, source).ToLocalChecked();
  v8::Local<v8::Script> script =
      v8::Script::Compile(context, source_str).ToLocalChecked();
  return script->Run(context).ToLocalChecked();
}

void TestSpeciesProtector(char* code,
                          bool invalidates_species_protector = true) {
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = allocator.get();
  std::string typed_array_constructors[] = {
#define TYPED_ARRAY_CTOR(Type, type, TYPE, ctype) #Type "Array",

      TYPED_ARRAYS(TYPED_ARRAY_CTOR)
#undef TYPED_ARRAY_CTOR
  };

  for (auto& constructor : typed_array_constructors) {
    v8::Isolate* isolate = v8::Isolate::New(create_params);
    isolate->Enter();
    {
      v8::HandleScope scope(isolate);
      v8::Local<v8::Context> context = v8::Context::New(isolate);
      v8::Context::Scope context_scope(context);
      v8::TryCatch try_catch(isolate);

      CompileRun(isolate, context,
                 ("let x = new " + constructor + "();").c_str());
      CompileRun(isolate, context,
                 ("let constructor = " + constructor + ";").c_str());
      v8::Local<v8::Value> constructor_obj =
          CompileRun(isolate, context, constructor.c_str());
      CHECK_EQ(constructor_obj,
               CompileRun(isolate, context, "x.slice().constructor"));
      CHECK_EQ(constructor_obj,
               CompileRun(isolate, context, "x.subarray().constructor"));
      CHECK_EQ(constructor_obj,
               CompileRun(isolate, context, "x.map(()=>{}).constructor"));
      std::string decl = "class MyTypedArray extends " + constructor + " { }";
      CompileRun(isolate, context, decl.c_str());

      v8::internal::Isolate* i_isolate =
          reinterpret_cast<v8::internal::Isolate*>(isolate);
      CHECK(Protectors::IsTypedArraySpeciesLookupChainIntact(i_isolate));
      CompileRun(isolate, context, code);
      if (invalidates_species_protector) {
        CHECK(!Protectors::IsTypedArraySpeciesLookupChainIntact(i_isolate));
      } else {
        CHECK(Protectors::IsTypedArraySpeciesLookupChainIntact(i_isolate));
      }

      v8::Local<v8::Value> my_typed_array =
          CompileRun(isolate, context, "MyTypedArray");
      CHECK_EQ(my_typed_array,
               CompileRun(isolate, context, "x.slice().constructor"));
      CHECK_EQ(my_typed_array,
               CompileRun(isolate, context, "x.subarray().constructor"));
      CHECK_EQ(my_typed_array,
               CompileRun(isolate, context, "x.map(()=>{}).constructor"));
    }
    isolate->Exit();
    isolate->Dispose();
  }
}

TEST_F(TypedArraysTest, CopyContentsTypedArray) {
  RunJS(
      "var a = new Uint8Array(4);"
      "a[0] = 0;"
      "a[1] = 1;"
      "a[2] = 2;"
      "a[3] = 3;");
  TestArrayBufferViewContents(false);
}

TEST_F(TypedArraysTest, CopyContentsArray) {
  RunJS("var a = new Uint8Array([0, 1, 2, 3]);");
  TestArrayBufferViewContents(false);
}

TEST_F(TypedArraysTest, CopyContentsView) {
  RunJS(
      "var b = new ArrayBuffer(6);"
      "var c = new Uint8Array(b);"
      "c[0] = -1;"
      "c[1] = -1;"
      "c[2] = 0;"
      "c[3] = 1;"
      "c[4] = 2;"
      "c[5] = 3;"
      "var a = new DataView(b, 2);");
  TestArrayBufferViewContents(true);
}

TEST_F(TypedArraysUninitializedTest, SpeciesConstructor) {
  char code[] = "x.constructor = MyTypedArray";
  TestSpeciesProtector(code);
}

TEST_F(TypedArraysUninitializedTest, SpeciesConstructorAccessor) {
  char code[] =
      "Object.defineProperty(x, 'constructor',{get() {return MyTypedArray;}})";
  TestSpeciesProtector(code);
}

TEST_F(TypedArraysUninitializedTest, SpeciesModified) {
  char code[] =
      "Object.defineProperty(constructor, Symbol.species, "
      "{value:MyTypedArray})";
  TestSpeciesProtector(code);
}

TEST_F(TypedArraysUninitializedTest, SpeciesParentConstructor) {
  char code[] = "constructor.prototype.constructor = MyTypedArray";
  TestSpeciesProtector(code);
}

TEST_F(TypedArraysUninitializedTest, SpeciesProto) {
  char code[] = "x.__proto__ = MyTypedArray.prototype";
  TestSpeciesProtector(code, false);
}

}  // namespace typedarrays_unittest
}  // namespace internal
}  // namespace v8
