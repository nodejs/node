// Copyright 2009 the V8 project authors. All rights reserved.
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


v8::internal::Semaphore* semaphore = NULL;


v8::Handle<v8::Value> Signal(const v8::Arguments& args) {
  semaphore->Signal();
  return v8::Undefined();
}


v8::Handle<v8::Value> TerminateCurrentThread(const v8::Arguments& args) {
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::V8::TerminateExecution();
  return v8::Undefined();
}


v8::Handle<v8::Value> Fail(const v8::Arguments& args) {
  CHECK(false);
  return v8::Undefined();
}


v8::Handle<v8::Value> Loop(const v8::Arguments& args) {
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Handle<v8::String> source =
      v8::String::New("try { doloop(); fail(); } catch(e) { fail(); }");
  v8::Handle<v8::Value> result = v8::Script::Compile(source)->Run();
  CHECK(result.IsEmpty());
  CHECK(v8::V8::IsExecutionTerminating());
  return v8::Undefined();
}


v8::Handle<v8::Value> DoLoop(const v8::Arguments& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("function f() {"
                                      "  var term = true;"
                                      "  try {"
                                      "    while(true) {"
                                      "      if (term) terminate();"
                                      "      term = false;"
                                      "    }"
                                      "    fail();"
                                      "  } catch(e) {"
                                      "    fail();"
                                      "  }"
                                      "}"
                                      "f()"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating());
  return v8::Undefined();
}


v8::Handle<v8::Value> DoLoopNoCall(const v8::Arguments& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("var term = true;"
                                      "while(true) {"
                                      "  if (term) terminate();"
                                      "  term = false;"
                                      "}"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating());
  return v8::Undefined();
}


v8::Handle<v8::ObjectTemplate> CreateGlobalTemplate(
    v8::InvocationCallback terminate,
    v8::InvocationCallback doloop) {
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  global->Set(v8::String::New("terminate"),
              v8::FunctionTemplate::New(terminate));
  global->Set(v8::String::New("fail"), v8::FunctionTemplate::New(Fail));
  global->Set(v8::String::New("loop"), v8::FunctionTemplate::New(Loop));
  global->Set(v8::String::New("doloop"), v8::FunctionTemplate::New(doloop));
  return global;
}


// Test that a single thread of JavaScript execution can terminate
// itself.
TEST(TerminateOnlyV8ThreadFromThreadItself) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(TerminateCurrentThread, DoLoop);
  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source =
      v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  // Test that we can run the code again after thread termination.
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(source)->Run();
  context.Dispose(context->GetIsolate());
}


// Test that a single thread of JavaScript execution can terminate
// itself in a loop that performs no calls.
TEST(TerminateOnlyV8ThreadFromThreadItselfNoLoop) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(TerminateCurrentThread, DoLoopNoCall);
  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source =
      v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  CHECK(!v8::V8::IsExecutionTerminating());
  // Test that we can run the code again after thread termination.
  v8::Script::Compile(source)->Run();
  context.Dispose(context->GetIsolate());
}


class TerminatorThread : public v8::internal::Thread {
 public:
  explicit TerminatorThread(i::Isolate* isolate)
      : Thread("TerminatorThread"),
        isolate_(reinterpret_cast<v8::Isolate*>(isolate)) { }
  void Run() {
    semaphore->Wait();
    CHECK(!v8::V8::IsExecutionTerminating(isolate_));
    v8::V8::TerminateExecution(isolate_);
  }

 private:
  v8::Isolate* isolate_;
};


// Test that a single thread of JavaScript execution can be terminated
// from the side by another thread.
TEST(TerminateOnlyV8ThreadFromOtherThread) {
  semaphore = v8::internal::OS::CreateSemaphore(0);
  TerminatorThread thread(i::Isolate::Current());
  thread.Start();

  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::ObjectTemplate> global = CreateGlobalTemplate(Signal, DoLoop);
  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source =
      v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();

  thread.Join();
  delete semaphore;
  semaphore = NULL;
  context.Dispose(context->GetIsolate());
}


class LoopingThread : public v8::internal::Thread {
 public:
  LoopingThread() : Thread("LoopingThread") { }
  void Run() {
    v8::Locker locker(CcTest::default_isolate());
    v8::HandleScope scope(CcTest::default_isolate());
    v8_thread_id_ = v8::V8::GetCurrentThreadId();
    v8::Handle<v8::ObjectTemplate> global =
        CreateGlobalTemplate(Signal, DoLoop);
    v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
    v8::Context::Scope context_scope(context);
    CHECK(!v8::V8::IsExecutionTerminating());
    // Run a loop that will be infinite if thread termination does not work.
    v8::Handle<v8::String> source =
        v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
    v8::Script::Compile(source)->Run();
    context.Dispose(context->GetIsolate());
  }

  int GetV8ThreadId() { return v8_thread_id_; }

 private:
  int v8_thread_id_;
};


// Test that multiple threads using default isolate can be terminated
// from another thread when using Lockers and preemption.
TEST(TerminateMultipleV8ThreadsDefaultIsolate) {
  {
    v8::Locker locker(CcTest::default_isolate());
    v8::V8::Initialize();
    v8::Locker::StartPreemption(1);
    semaphore = v8::internal::OS::CreateSemaphore(0);
  }
  const int kThreads = 2;
  i::List<LoopingThread*> threads(kThreads);
  for (int i = 0; i < kThreads; i++) {
    threads.Add(new LoopingThread());
  }
  for (int i = 0; i < kThreads; i++) {
    threads[i]->Start();
  }
  // Wait until all threads have signaled the semaphore.
  for (int i = 0; i < kThreads; i++) {
    semaphore->Wait();
  }
  {
    v8::Locker locker(CcTest::default_isolate());
    for (int i = 0; i < kThreads; i++) {
      v8::V8::TerminateExecution(threads[i]->GetV8ThreadId());
    }
  }
  for (int i = 0; i < kThreads; i++) {
    threads[i]->Join();
    delete threads[i];
  }
  {
    v8::Locker locker(CcTest::default_isolate());
    v8::Locker::StopPreemption();
  }

  delete semaphore;
  semaphore = NULL;
}


int call_count = 0;


v8::Handle<v8::Value> TerminateOrReturnObject(const v8::Arguments& args) {
  if (++call_count == 10) {
    CHECK(!v8::V8::IsExecutionTerminating());
    v8::V8::TerminateExecution();
    return v8::Undefined();
  }
  v8::Local<v8::Object> result = v8::Object::New();
  result->Set(v8::String::New("x"), v8::Integer::New(42));
  return result;
}


v8::Handle<v8::Value> LoopGetProperty(const v8::Arguments& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("function f() {"
                                      "  try {"
                                      "    while(true) {"
                                      "      terminate_or_return_object().x;"
                                      "    }"
                                      "    fail();"
                                      "  } catch(e) {"
                                      "    fail();"
                                      "  }"
                                      "}"
                                      "f()"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating());
  return v8::Undefined();
}


// Test that we correctly handle termination exceptions if they are
// triggered by the creation of error objects in connection with ICs.
TEST(TerminateLoadICException) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
  global->Set(v8::String::New("terminate_or_return_object"),
              v8::FunctionTemplate::New(TerminateOrReturnObject));
  global->Set(v8::String::New("fail"), v8::FunctionTemplate::New(Fail));
  global->Set(v8::String::New("loop"),
              v8::FunctionTemplate::New(LoopGetProperty));

  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source =
      v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
  call_count = 0;
  v8::Script::Compile(source)->Run();
  // Test that we can run the code again after thread termination.
  CHECK(!v8::V8::IsExecutionTerminating());
  call_count = 0;
  v8::Script::Compile(source)->Run();
  context.Dispose(context->GetIsolate());
}

v8::Handle<v8::Value> ReenterAfterTermination(const v8::Arguments& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("function f() {"
                                      "  var term = true;"
                                      "  try {"
                                      "    while(true) {"
                                      "      if (term) terminate();"
                                      "      term = false;"
                                      "    }"
                                      "    fail();"
                                      "  } catch(e) {"
                                      "    fail();"
                                      "  }"
                                      "}"
                                      "f()"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("function f() { fail(); } f()"))->Run();
  return v8::Undefined();
}

// Test that reentry into V8 while the termination exception is still pending
// (has not yet unwound the 0-level JS frame) does not crash.
TEST(TerminateAndReenterFromThreadItself) {
  v8::HandleScope scope(v8::Isolate::GetCurrent());
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(TerminateCurrentThread, ReenterAfterTermination);
  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Handle<v8::String> source =
      v8::String::New("try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  CHECK(!v8::V8::IsExecutionTerminating());
  // Check we can run JS again after termination.
  CHECK(v8::Script::Compile(v8::String::New("function f() { return true; }"
                                            "f()"))->Run()->IsTrue());
  context.Dispose(context->GetIsolate());
}

v8::Handle<v8::Value> DoLoopCancelTerminate(const v8::Arguments& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::New("var term = true;"
                                      "while(true) {"
                                      "  if (term) terminate();"
                                      "  term = false;"
                                      "}"
                                      "fail();"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating());
  CHECK(try_catch.HasTerminated());
  v8::V8::CancelTerminateExecution(v8::Isolate::GetCurrent());
  CHECK(!v8::V8::IsExecutionTerminating());
  return v8::Undefined();
}

// Test that a single thread of JavaScript execution can terminate
// itself and then resume execution.
TEST(TerminateCancelTerminateFromThreadItself) {
  v8::HandleScope scope;
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(TerminateCurrentThread, DoLoopCancelTerminate);
  v8::Persistent<v8::Context> context = v8::Context::New(NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Handle<v8::String> source =
      v8::String::New("try { doloop(); } catch(e) { fail(); } 'completed';");
  // Check that execution completed with correct return value.
  CHECK(v8::Script::Compile(source)->Run()->Equals(v8_str("completed")));
  context.Dispose(context->GetIsolate());
}

