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

#include "v8.h"

#include "platform.h"

#include "cctest.h"


TEST(Preemption) {
  v8::Locker locker;
  v8::V8::Initialize();
  v8::HandleScope scope;
  v8::Context::Scope context_scope(v8::Context::New());

  v8::Locker::StartPreemption(100);

  v8::Handle<v8::Script> script = v8::Script::Compile(
      v8::String::New("var count = 0; var obj = new Object(); count++;\n"));

  script->Run();

  v8::Locker::StopPreemption();
  v8::internal::OS::Sleep(500);  // Make sure the timer fires.

  script->Run();
}


enum Turn {
  FILL_CACHE,
  CLEAN_CACHE,
  SECOND_TIME_FILL_CACHE,
  DONE
};

static Turn turn = FILL_CACHE;


class ThreadA: public v8::internal::Thread {
 public:
  void Run() {
    v8::Locker locker;
    v8::HandleScope scope;
    v8::Context::Scope context_scope(v8::Context::New());

    CHECK_EQ(FILL_CACHE, turn);

    // Fill String.search cache.
    v8::Handle<v8::Script> script = v8::Script::Compile(
        v8::String::New(
          "for (var i = 0; i < 3; i++) {"
          "  var result = \"a\".search(\"a\");"
          "  if (result != 0) throw \"result: \" + result + \" @\" + i;"
          "};"
          "true"));
    CHECK(script->Run()->IsTrue());

    turn = CLEAN_CACHE;
    do {
      {
        v8::Unlocker unlocker;
        Thread::YieldCPU();
      }
    } while (turn != SECOND_TIME_FILL_CACHE);

    // Rerun the script.
    CHECK(script->Run()->IsTrue());

    turn = DONE;
  }
};


class ThreadB: public v8::internal::Thread {
 public:
  void Run() {
    do {
      {
        v8::Locker locker;
        if (turn == CLEAN_CACHE) {
          v8::HandleScope scope;
          v8::Context::Scope context_scope(v8::Context::New());

          // Clear the caches by forcing major GC.
          v8::internal::Heap::CollectAllGarbage(false);
          turn = SECOND_TIME_FILL_CACHE;
          break;
        }
      }

      Thread::YieldCPU();
    } while (true);
  }
};


TEST(JSFunctionResultCachesInTwoThreads) {
  v8::V8::Initialize();

  ThreadA threadA;
  ThreadB threadB;

  threadA.Start();
  threadB.Start();

  threadA.Join();
  threadB.Join();

  CHECK_EQ(DONE, turn);
}
