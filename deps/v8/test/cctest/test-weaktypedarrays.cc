// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>

#include "v8.h"
#include "api.h"
#include "heap.h"
#include "objects.h"

#include "cctest.h"

using namespace v8::internal;

static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}


static int CountArrayBuffersInWeakList(Heap* heap) {
  int count = 0;
  for (Object* o = heap->array_buffers_list();
       o != Smi::FromInt(0);
       o = JSArrayBuffer::cast(o)->weak_next()) {
    count++;
  }
  return count;
}


static bool HasArrayBufferInWeakList(Heap* heap, JSArrayBuffer* ab) {
  for (Object* o = heap->array_buffers_list();
       o != Smi::FromInt(0);
       o = JSArrayBuffer::cast(o)->weak_next()) {
    if (ab == o) return true;
  }
  return false;
}


static int CountTypedArrays(JSArrayBuffer* array_buffer) {
  int count = 0;
  for (Object* o = array_buffer->weak_first_array();
       o != Smi::FromInt(0);
       o = JSTypedArray::cast(o)->weak_next()) {
    count++;
  }

  return count;
}

static bool HasTypedArrayInWeakList(JSArrayBuffer* array_buffer,
                                    JSTypedArray* ta) {
  for (Object* o = array_buffer->weak_first_array();
       o != Smi::FromInt(0);
       o = JSTypedArray::cast(o)->weak_next()) {
    if (ta == o) return true;
  }
  return false;
}


TEST(WeakArrayBuffersFromApi) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);

  CHECK_EQ(0, CountArrayBuffersInWeakList(isolate->heap()));
  {
    v8::HandleScope s1(context->GetIsolate());
    v8::Handle<v8::ArrayBuffer> ab1 = v8::ArrayBuffer::New(256);
    {
      v8::HandleScope s2(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab2 = v8::ArrayBuffer::New(128);

      Handle<JSArrayBuffer> iab1 = v8::Utils::OpenHandle(*ab1);
      Handle<JSArrayBuffer> iab2 = v8::Utils::OpenHandle(*ab2);
      CHECK_EQ(2, CountArrayBuffersInWeakList(isolate->heap()));
      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab1));
      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab2));
    }
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()));
    {
      HandleScope scope2(isolate);
      Handle<JSArrayBuffer> iab1 = v8::Utils::OpenHandle(*ab1);

      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab1));
    }
  }

  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
  CHECK_EQ(0, CountArrayBuffersInWeakList(isolate->heap()));
}


TEST(WeakArrayBuffersFromScript) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);

  for (int i = 1; i <= 3; i++) {
    // Create 3 array buffers, make i-th of them garbage,
    // validate correct state of array buffer weak list.
    CHECK_EQ(0, CountArrayBuffersInWeakList(isolate->heap()));
    {
      v8::HandleScope scope(context->GetIsolate());

      {
        v8::HandleScope s1(context->GetIsolate());
        CompileRun("var ab1 = new ArrayBuffer(256);"
                   "var ab2 = new ArrayBuffer(256);"
                   "var ab3 = new ArrayBuffer(256);");
        v8::Handle<v8::ArrayBuffer> ab1(
            v8::ArrayBuffer::Cast(*CompileRun("ab1")));
        v8::Handle<v8::ArrayBuffer> ab2(
            v8::ArrayBuffer::Cast(*CompileRun("ab2")));
        v8::Handle<v8::ArrayBuffer> ab3(
            v8::ArrayBuffer::Cast(*CompileRun("ab3")));

        CHECK_EQ(3, CountArrayBuffersInWeakList(isolate->heap()));
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab1)));
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab2)));
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab3)));
      }

      i::ScopedVector<char> source(1024);
      i::OS::SNPrintF(source, "ab%d = null;", i);
      CompileRun(source.start());
      isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
      isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);

      CHECK_EQ(2, CountArrayBuffersInWeakList(isolate->heap()));

      {
        v8::HandleScope s2(context->GetIsolate());
        for (int j = 1; j <= 3; j++) {
          if (j == i) continue;
          i::OS::SNPrintF(source, "ab%d", j);
          v8::Handle<v8::ArrayBuffer> ab(
              v8::ArrayBuffer::Cast(*CompileRun(source.start())));
          CHECK(HasArrayBufferInWeakList(isolate->heap(),
                *v8::Utils::OpenHandle(*ab)));
          }
      }

      CompileRun("ab1 = null; ab2 = null; ab3 = null;");
    }

    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(0, CountArrayBuffersInWeakList(isolate->heap()));
  }
}

template <typename TypedArray>
void TestTypedArrayFromApi() {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);

  v8::HandleScope s1(context->GetIsolate());
  v8::Handle<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(2048);
  Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
  {
    v8::HandleScope s2(context->GetIsolate());
    v8::Handle<TypedArray> ta1 = TypedArray::New(ab, 0, 256);
    {
      v8::HandleScope s3(context->GetIsolate());
      v8::Handle<TypedArray> ta2 = TypedArray::New(ab, 0, 128);

      Handle<JSTypedArray> ita1 = v8::Utils::OpenHandle(*ta1);
      Handle<JSTypedArray> ita2 = v8::Utils::OpenHandle(*ta2);
      CHECK_EQ(2, CountTypedArrays(*iab));
      CHECK(HasTypedArrayInWeakList(*iab, *ita1));
      CHECK(HasTypedArrayInWeakList(*iab, *ita2));
    }
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    CHECK_EQ(1, CountTypedArrays(*iab));
    Handle<JSTypedArray> ita1 = v8::Utils::OpenHandle(*ta1);
    CHECK(HasTypedArrayInWeakList(*iab, *ita1));
  }
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
  isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);

  CHECK_EQ(0, CountTypedArrays(*iab));
}


TEST(Uint8ArrayFromApi) {
  TestTypedArrayFromApi<v8::Uint8Array>();
}


TEST(Int8ArrayFromApi) {
  TestTypedArrayFromApi<v8::Int8Array>();
}


TEST(Uint16ArrayFromApi) {
  TestTypedArrayFromApi<v8::Uint16Array>();
}


TEST(Int16ArrayFromApi) {
  TestTypedArrayFromApi<v8::Int16Array>();
}


TEST(Uint32ArrayFromApi) {
  TestTypedArrayFromApi<v8::Uint32Array>();
}


TEST(Int32ArrayFromApi) {
  TestTypedArrayFromApi<v8::Int32Array>();
}


TEST(Float32ArrayFromApi) {
  TestTypedArrayFromApi<v8::Float32Array>();
}


TEST(Float64ArrayFromApi) {
  TestTypedArrayFromApi<v8::Float64Array>();
}


TEST(Uint8ClampedArrayFromApi) {
  TestTypedArrayFromApi<v8::Uint8ClampedArray>();
}

template <typename TypedArray>
static void TestTypedArrayFromScript(const char* constructor) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  v8::HandleScope scope(context->GetIsolate());
  CompileRun("var ab = new ArrayBuffer(2048);");
  for (int i = 1; i <= 3; i++) {
    // Create 3 typed arrays, make i-th of them garbage,
    // validate correct state of typed array weak list.
    v8::HandleScope s0(context->GetIsolate());
    i::ScopedVector<char> source(2048);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()));

    {
      v8::HandleScope s1(context->GetIsolate());
      i::OS::SNPrintF(source,
                  "var ta1 = new %s(ab);"
                  "var ta2 = new %s(ab);"
                  "var ta3 = new %s(ab)",
                  constructor, constructor, constructor);

      CompileRun(source.start());
      v8::Handle<v8::ArrayBuffer> ab(v8::ArrayBuffer::Cast(*CompileRun("ab")));
      v8::Handle<TypedArray> ta1(TypedArray::Cast(*CompileRun("ta1")));
      v8::Handle<TypedArray> ta2(TypedArray::Cast(*CompileRun("ta2")));
      v8::Handle<TypedArray> ta3(TypedArray::Cast(*CompileRun("ta3")));
      CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()));
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(3, CountTypedArrays(*iab));
      CHECK(HasTypedArrayInWeakList(*iab, *v8::Utils::OpenHandle(*ta1)));
      CHECK(HasTypedArrayInWeakList(*iab, *v8::Utils::OpenHandle(*ta2)));
      CHECK(HasTypedArrayInWeakList(*iab, *v8::Utils::OpenHandle(*ta3)));
    }

    i::OS::SNPrintF(source, "ta%d = null;", i);
    CompileRun(source.start());
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()));

    {
      v8::HandleScope s2(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab(v8::ArrayBuffer::Cast(*CompileRun("ab")));
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(2, CountTypedArrays(*iab));
      for (int j = 1; j <= 3; j++) {
        if (j == i) continue;
        i::OS::SNPrintF(source, "ta%d", j);
        v8::Handle<TypedArray> ta(
            TypedArray::Cast(*CompileRun(source.start())));
        CHECK(HasTypedArrayInWeakList(*iab, *v8::Utils::OpenHandle(*ta)));
      }
    }

    CompileRun("ta1 = null; ta2 = null; ta3 = null;");
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);
    isolate->heap()->CollectAllGarbage(Heap::kNoGCFlags);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()));

    {
      v8::HandleScope s3(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab(v8::ArrayBuffer::Cast(*CompileRun("ab")));
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(0, CountTypedArrays(*iab));
    }
  }
}


TEST(Uint8ArrayFromScript) {
  TestTypedArrayFromScript<v8::Uint8Array>("Uint8Array");
}


TEST(Int8ArrayFromScript) {
  TestTypedArrayFromScript<v8::Int8Array>("Int8Array");
}


TEST(Uint16ArrayFromScript) {
  TestTypedArrayFromScript<v8::Uint16Array>("Uint16Array");
}


TEST(Int16ArrayFromScript) {
  TestTypedArrayFromScript<v8::Int16Array>("Int16Array");
}


TEST(Uint32ArrayFromScript) {
  TestTypedArrayFromScript<v8::Uint32Array>("Uint32Array");
}


TEST(Int32ArrayFromScript) {
  TestTypedArrayFromScript<v8::Int32Array>("Int32Array");
}


TEST(Float32ArrayFromScript) {
  TestTypedArrayFromScript<v8::Float32Array>("Float32Array");
}


TEST(Float64ArrayFromScript) {
  TestTypedArrayFromScript<v8::Float64Array>("Float64Array");
}


TEST(Uint8ClampedArrayFromScript) {
  TestTypedArrayFromScript<v8::Uint8ClampedArray>("Uint8ClampedArray");
}

