// Copyright 2008 the V8 project authors. All rights reserved.
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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/platform/platform.h"
#include "src/isolate.h"


enum Turn {
  FILL_CACHE,
  CLEAN_CACHE,
  SECOND_TIME_FILL_CACHE,
  DONE
};

static Turn turn = FILL_CACHE;


class ThreadA : public v8::base::Thread {
 public:
  ThreadA() : Thread(Options("ThreadA")) {}
  void Run() {
    v8::Isolate* isolate = CcTest::isolate();
    v8::Locker locker(isolate);
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    CHECK_EQ(FILL_CACHE, turn);

    // Fill String.search cache.
    v8::Handle<v8::Script> script = v8::Script::Compile(
        v8::String::NewFromUtf8(
          isolate,
          "for (var i = 0; i < 3; i++) {"
          "  var result = \"a\".search(\"a\");"
          "  if (result != 0) throw \"result: \" + result + \" @\" + i;"
          "};"
          "true"));
    CHECK(script->Run()->IsTrue());

    turn = CLEAN_CACHE;
    do {
      {
        v8::Unlocker unlocker(CcTest::isolate());
        Thread::YieldCPU();
      }
    } while (turn != SECOND_TIME_FILL_CACHE);

    // Rerun the script.
    CHECK(script->Run()->IsTrue());

    turn = DONE;
  }
};


class ThreadB : public v8::base::Thread {
 public:
  ThreadB() : Thread(Options("ThreadB")) {}
  void Run() {
    do {
      {
        v8::Isolate* isolate = CcTest::isolate();
        v8::Locker locker(isolate);
        v8::Isolate::Scope isolate_scope(isolate);
        if (turn == CLEAN_CACHE) {
          v8::HandleScope scope(isolate);
          v8::Handle<v8::Context> context = v8::Context::New(isolate);
          v8::Context::Scope context_scope(context);

          // Clear the caches by forcing major GC.
          CcTest::heap()->CollectAllGarbage(v8::internal::Heap::kNoGCFlags);
          turn = SECOND_TIME_FILL_CACHE;
          break;
        }
      }

      Thread::YieldCPU();
    } while (true);
  }
};


TEST(JSFunctionResultCachesInTwoThreads) {
  ThreadA threadA;
  ThreadB threadB;

  threadA.Start();
  threadB.Start();

  threadA.Join();
  threadB.Join();

  CHECK_EQ(DONE, turn);
}

class ThreadIdValidationThread : public v8::base::Thread {
 public:
  ThreadIdValidationThread(v8::base::Thread* thread_to_start,
                           i::List<i::ThreadId>* refs, unsigned int thread_no,
                           v8::base::Semaphore* semaphore)
      : Thread(Options("ThreadRefValidationThread")),
        refs_(refs),
        thread_no_(thread_no),
        thread_to_start_(thread_to_start),
        semaphore_(semaphore) {}

  void Run() {
    i::ThreadId thread_id = i::ThreadId::Current();
    for (int i = 0; i < thread_no_; i++) {
      CHECK(!(*refs_)[i].Equals(thread_id));
    }
    CHECK(thread_id.IsValid());
    (*refs_)[thread_no_] = thread_id;
    if (thread_to_start_ != NULL) {
      thread_to_start_->Start();
    }
    semaphore_->Signal();
  }

 private:
  i::List<i::ThreadId>* refs_;
  int thread_no_;
  v8::base::Thread* thread_to_start_;
  v8::base::Semaphore* semaphore_;
};


TEST(ThreadIdValidation) {
  const int kNThreads = 100;
  i::List<ThreadIdValidationThread*> threads(kNThreads);
  i::List<i::ThreadId> refs(kNThreads);
  v8::base::Semaphore semaphore(0);
  ThreadIdValidationThread* prev = NULL;
  for (int i = kNThreads - 1; i >= 0; i--) {
    ThreadIdValidationThread* newThread =
        new ThreadIdValidationThread(prev, &refs, i, &semaphore);
    threads.Add(newThread);
    prev = newThread;
    refs.Add(i::ThreadId::Invalid());
  }
  prev->Start();
  for (int i = 0; i < kNThreads; i++) {
    semaphore.Wait();
  }
  for (int i = 0; i < kNThreads; i++) {
    delete threads[i];
  }
}
