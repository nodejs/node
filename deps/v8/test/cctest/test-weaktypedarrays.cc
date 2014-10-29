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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/api.h"
#include "src/heap/heap.h"
#include "src/objects.h"

using namespace v8::internal;

static Isolate* GetIsolateFrom(LocalContext* context) {
  return reinterpret_cast<Isolate*>((*context)->GetIsolate());
}


static int CountArrayBuffersInWeakList(Heap* heap) {
  int count = 0;
  for (Object* o = heap->array_buffers_list();
       !o->IsUndefined();
       o = JSArrayBuffer::cast(o)->weak_next()) {
    count++;
  }
  return count;
}


static bool HasArrayBufferInWeakList(Heap* heap, JSArrayBuffer* ab) {
  for (Object* o = heap->array_buffers_list();
       !o->IsUndefined();
       o = JSArrayBuffer::cast(o)->weak_next()) {
    if (ab == o) return true;
  }
  return false;
}


static int CountViews(JSArrayBuffer* array_buffer) {
  int count = 0;
  for (Object* o = array_buffer->weak_first_view();
       !o->IsUndefined();
       o = JSArrayBufferView::cast(o)->weak_next()) {
    count++;
  }

  return count;
}

static bool HasViewInWeakList(JSArrayBuffer* array_buffer,
                              JSArrayBufferView* ta) {
  for (Object* o = array_buffer->weak_first_view();
       !o->IsUndefined();
       o = JSArrayBufferView::cast(o)->weak_next()) {
    if (ta == o) return true;
  }
  return false;
}


TEST(WeakArrayBuffersFromApi) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);

  int start = CountArrayBuffersInWeakList(isolate->heap());
  {
    v8::HandleScope s1(context->GetIsolate());
    v8::Handle<v8::ArrayBuffer> ab1 =
        v8::ArrayBuffer::New(context->GetIsolate(), 256);
    {
      v8::HandleScope s2(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab2 =
          v8::ArrayBuffer::New(context->GetIsolate(), 128);

      Handle<JSArrayBuffer> iab1 = v8::Utils::OpenHandle(*ab1);
      Handle<JSArrayBuffer> iab2 = v8::Utils::OpenHandle(*ab2);
      CHECK_EQ(2, CountArrayBuffersInWeakList(isolate->heap()) - start);
      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab1));
      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab2));
    }
    isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()) - start);
    {
      HandleScope scope2(isolate);
      Handle<JSArrayBuffer> iab1 = v8::Utils::OpenHandle(*ab1);

      CHECK(HasArrayBufferInWeakList(isolate->heap(), *iab1));
    }
  }

  isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
  CHECK_EQ(start, CountArrayBuffersInWeakList(isolate->heap()));
}


TEST(WeakArrayBuffersFromScript) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  int start = CountArrayBuffersInWeakList(isolate->heap());

  for (int i = 1; i <= 3; i++) {
    // Create 3 array buffers, make i-th of them garbage,
    // validate correct state of array buffer weak list.
    CHECK_EQ(start, CountArrayBuffersInWeakList(isolate->heap()));
    {
      v8::HandleScope scope(context->GetIsolate());

      {
        v8::HandleScope s1(context->GetIsolate());
        CompileRun("var ab1 = new ArrayBuffer(256);"
                   "var ab2 = new ArrayBuffer(256);"
                   "var ab3 = new ArrayBuffer(256);");
        v8::Handle<v8::ArrayBuffer> ab1 =
            v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab1"));
        v8::Handle<v8::ArrayBuffer> ab2 =
            v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab2"));
        v8::Handle<v8::ArrayBuffer> ab3 =
            v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab3"));

        CHECK_EQ(3, CountArrayBuffersInWeakList(isolate->heap()) - start);
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab1)));
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab2)));
        CHECK(HasArrayBufferInWeakList(isolate->heap(),
              *v8::Utils::OpenHandle(*ab3)));
      }

      i::ScopedVector<char> source(1024);
      i::SNPrintF(source, "ab%d = null;", i);
      CompileRun(source.start());
      isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

      CHECK_EQ(2, CountArrayBuffersInWeakList(isolate->heap()) - start);

      {
        v8::HandleScope s2(context->GetIsolate());
        for (int j = 1; j <= 3; j++) {
          if (j == i) continue;
          i::SNPrintF(source, "ab%d", j);
          v8::Handle<v8::ArrayBuffer> ab =
              v8::Handle<v8::ArrayBuffer>::Cast(CompileRun(source.start()));
          CHECK(HasArrayBufferInWeakList(isolate->heap(),
                *v8::Utils::OpenHandle(*ab)));
          }
      }

      CompileRun("ab1 = null; ab2 = null; ab3 = null;");
    }

    isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
    CHECK_EQ(start, CountArrayBuffersInWeakList(isolate->heap()));
  }
}

template <typename View>
void TestViewFromApi() {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);

  v8::HandleScope s1(context->GetIsolate());
  v8::Handle<v8::ArrayBuffer> ab =
      v8::ArrayBuffer::New(context->GetIsolate(), 2048);
  Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
  {
    v8::HandleScope s2(context->GetIsolate());
    v8::Handle<View> ta1 = View::New(ab, 0, 256);
    {
      v8::HandleScope s3(context->GetIsolate());
      v8::Handle<View> ta2 = View::New(ab, 0, 128);

      Handle<JSArrayBufferView> ita1 = v8::Utils::OpenHandle(*ta1);
      Handle<JSArrayBufferView> ita2 = v8::Utils::OpenHandle(*ta2);
      CHECK_EQ(2, CountViews(*iab));
      CHECK(HasViewInWeakList(*iab, *ita1));
      CHECK(HasViewInWeakList(*iab, *ita2));
    }
    isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);
    CHECK_EQ(1, CountViews(*iab));
    Handle<JSArrayBufferView> ita1 = v8::Utils::OpenHandle(*ta1);
    CHECK(HasViewInWeakList(*iab, *ita1));
  }
  isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

  CHECK_EQ(0, CountViews(*iab));
}


TEST(Uint8ArrayFromApi) {
  TestViewFromApi<v8::Uint8Array>();
}


TEST(Int8ArrayFromApi) {
  TestViewFromApi<v8::Int8Array>();
}


TEST(Uint16ArrayFromApi) {
  TestViewFromApi<v8::Uint16Array>();
}


TEST(Int16ArrayFromApi) {
  TestViewFromApi<v8::Int16Array>();
}


TEST(Uint32ArrayFromApi) {
  TestViewFromApi<v8::Uint32Array>();
}


TEST(Int32ArrayFromApi) {
  TestViewFromApi<v8::Int32Array>();
}


TEST(Float32ArrayFromApi) {
  TestViewFromApi<v8::Float32Array>();
}


TEST(Float64ArrayFromApi) {
  TestViewFromApi<v8::Float64Array>();
}


TEST(Uint8ClampedArrayFromApi) {
  TestViewFromApi<v8::Uint8ClampedArray>();
}


TEST(DataViewFromApi) {
  TestViewFromApi<v8::DataView>();
}

template <typename TypedArray>
static void TestTypedArrayFromScript(const char* constructor) {
  v8::V8::Initialize();
  LocalContext context;
  Isolate* isolate = GetIsolateFrom(&context);
  v8::HandleScope scope(context->GetIsolate());
  int start = CountArrayBuffersInWeakList(isolate->heap());
  CompileRun("var ab = new ArrayBuffer(2048);");
  for (int i = 1; i <= 3; i++) {
    // Create 3 typed arrays, make i-th of them garbage,
    // validate correct state of typed array weak list.
    v8::HandleScope s0(context->GetIsolate());
    i::ScopedVector<char> source(2048);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()) - start);

    {
      v8::HandleScope s1(context->GetIsolate());
      i::SNPrintF(source,
              "var ta1 = new %s(ab);"
              "var ta2 = new %s(ab);"
              "var ta3 = new %s(ab)",
              constructor, constructor, constructor);

      CompileRun(source.start());
      v8::Handle<v8::ArrayBuffer> ab =
          v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab"));
      v8::Handle<TypedArray> ta1 =
          v8::Handle<TypedArray>::Cast(CompileRun("ta1"));
      v8::Handle<TypedArray> ta2 =
          v8::Handle<TypedArray>::Cast(CompileRun("ta2"));
      v8::Handle<TypedArray> ta3 =
          v8::Handle<TypedArray>::Cast(CompileRun("ta3"));
      CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()) - start);
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(3, CountViews(*iab));
      CHECK(HasViewInWeakList(*iab, *v8::Utils::OpenHandle(*ta1)));
      CHECK(HasViewInWeakList(*iab, *v8::Utils::OpenHandle(*ta2)));
      CHECK(HasViewInWeakList(*iab, *v8::Utils::OpenHandle(*ta3)));
    }

    i::SNPrintF(source, "ta%d = null;", i);
    CompileRun(source.start());
    isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()) - start);

    {
      v8::HandleScope s2(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab =
          v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab"));
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(2, CountViews(*iab));
      for (int j = 1; j <= 3; j++) {
        if (j == i) continue;
        i::SNPrintF(source, "ta%d", j);
        v8::Handle<TypedArray> ta =
            v8::Handle<TypedArray>::Cast(CompileRun(source.start()));
        CHECK(HasViewInWeakList(*iab, *v8::Utils::OpenHandle(*ta)));
      }
    }

    CompileRun("ta1 = null; ta2 = null; ta3 = null;");
    isolate->heap()->CollectAllGarbage(Heap::kAbortIncrementalMarkingMask);

    CHECK_EQ(1, CountArrayBuffersInWeakList(isolate->heap()) - start);

    {
      v8::HandleScope s3(context->GetIsolate());
      v8::Handle<v8::ArrayBuffer> ab =
          v8::Handle<v8::ArrayBuffer>::Cast(CompileRun("ab"));
      Handle<JSArrayBuffer> iab = v8::Utils::OpenHandle(*ab);
      CHECK_EQ(0, CountViews(*iab));
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


TEST(DataViewFromScript) {
  TestTypedArrayFromScript<v8::DataView>("DataView");
}
