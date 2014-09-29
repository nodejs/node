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

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/platform/platform.h"


v8::base::Semaphore* semaphore = NULL;


void Signal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  semaphore->Signal();
}


void TerminateCurrentThread(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::V8::TerminateExecution(args.GetIsolate());
}


void Fail(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(false);
}


void Loop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      args.GetIsolate(), "try { doloop(); fail(); } catch(e) { fail(); }");
  v8::Handle<v8::Value> result = v8::Script::Compile(source)->Run();
  CHECK(result.IsEmpty());
  CHECK(v8::V8::IsExecutionTerminating(args.GetIsolate()));
}


void DoLoop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Script::Compile(v8::String::NewFromUtf8(args.GetIsolate(),
                                              "function f() {"
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
  CHECK(v8::V8::IsExecutionTerminating(args.GetIsolate()));
}


void DoLoopNoCall(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Script::Compile(v8::String::NewFromUtf8(args.GetIsolate(),
                                              "var term = true;"
                                              "while(true) {"
                                              "  if (term) terminate();"
                                              "  term = false;"
                                              "}"))->Run();
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::V8::IsExecutionTerminating(args.GetIsolate()));
}


v8::Handle<v8::ObjectTemplate> CreateGlobalTemplate(
    v8::Isolate* isolate,
    v8::FunctionCallback terminate,
    v8::FunctionCallback doloop) {
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(v8::String::NewFromUtf8(isolate, "terminate"),
              v8::FunctionTemplate::New(isolate, terminate));
  global->Set(v8::String::NewFromUtf8(isolate, "fail"),
              v8::FunctionTemplate::New(isolate, Fail));
  global->Set(v8::String::NewFromUtf8(isolate, "loop"),
              v8::FunctionTemplate::New(isolate, Loop));
  global->Set(v8::String::NewFromUtf8(isolate, "doloop"),
              v8::FunctionTemplate::New(isolate, doloop));
  return global;
}


// Test that a single thread of JavaScript execution can terminate
// itself.
TEST(TerminateOnlyV8ThreadFromThreadItself) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), TerminateCurrentThread, DoLoop);
  v8::Handle<v8::Context> context =
      v8::Context::New(CcTest::isolate(), NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      CcTest::isolate(), "try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  // Test that we can run the code again after thread termination.
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  v8::Script::Compile(source)->Run();
}


// Test that a single thread of JavaScript execution can terminate
// itself in a loop that performs no calls.
TEST(TerminateOnlyV8ThreadFromThreadItselfNoLoop) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::ObjectTemplate> global = CreateGlobalTemplate(
      CcTest::isolate(), TerminateCurrentThread, DoLoopNoCall);
  v8::Handle<v8::Context> context =
      v8::Context::New(CcTest::isolate(), NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      CcTest::isolate(), "try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  // Test that we can run the code again after thread termination.
  v8::Script::Compile(source)->Run();
}


class TerminatorThread : public v8::base::Thread {
 public:
  explicit TerminatorThread(i::Isolate* isolate)
      : Thread(Options("TerminatorThread")),
        isolate_(reinterpret_cast<v8::Isolate*>(isolate)) {}
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
  semaphore = new v8::base::Semaphore(0);
  TerminatorThread thread(CcTest::i_isolate());
  thread.Start();

  v8::HandleScope scope(CcTest::isolate());
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), Signal, DoLoop);
  v8::Handle<v8::Context> context =
      v8::Context::New(CcTest::isolate(), NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      CcTest::isolate(), "try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();

  thread.Join();
  delete semaphore;
  semaphore = NULL;
}


int call_count = 0;


void TerminateOrReturnObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (++call_count == 10) {
    CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
    v8::V8::TerminateExecution(args.GetIsolate());
    return;
  }
  v8::Local<v8::Object> result = v8::Object::New(args.GetIsolate());
  result->Set(v8::String::NewFromUtf8(args.GetIsolate(), "x"),
              v8::Integer::New(args.GetIsolate(), 42));
  args.GetReturnValue().Set(result);
}


void LoopGetProperty(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Script::Compile(
      v8::String::NewFromUtf8(args.GetIsolate(),
                              "function f() {"
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
  CHECK(v8::V8::IsExecutionTerminating(args.GetIsolate()));
}


// Test that we correctly handle termination exceptions if they are
// triggered by the creation of error objects in connection with ICs.
TEST(TerminateLoadICException) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(
      v8::String::NewFromUtf8(isolate, "terminate_or_return_object"),
      v8::FunctionTemplate::New(isolate, TerminateOrReturnObject));
  global->Set(v8::String::NewFromUtf8(isolate, "fail"),
              v8::FunctionTemplate::New(isolate, Fail));
  global->Set(v8::String::NewFromUtf8(isolate, "loop"),
              v8::FunctionTemplate::New(isolate, LoopGetProperty));

  v8::Handle<v8::Context> context =
      v8::Context::New(isolate, NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating(isolate));
  // Run a loop that will be infinite if thread termination does not work.
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      isolate, "try { loop(); fail(); } catch(e) { fail(); }");
  call_count = 0;
  v8::Script::Compile(source)->Run();
  // Test that we can run the code again after thread termination.
  CHECK(!v8::V8::IsExecutionTerminating(isolate));
  call_count = 0;
  v8::Script::Compile(source)->Run();
}


void ReenterAfterTermination(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Script::Compile(v8::String::NewFromUtf8(args.GetIsolate(),
                                              "function f() {"
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
  CHECK(v8::V8::IsExecutionTerminating(args.GetIsolate()));
  v8::Script::Compile(v8::String::NewFromUtf8(args.GetIsolate(),
                                              "function f() { fail(); } f()"))
      ->Run();
}


// Test that reentry into V8 while the termination exception is still pending
// (has not yet unwound the 0-level JS frame) does not crash.
TEST(TerminateAndReenterFromThreadItself) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, ReenterAfterTermination);
  v8::Handle<v8::Context> context =
      v8::Context::New(isolate, NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      isolate, "try { loop(); fail(); } catch(e) { fail(); }");
  v8::Script::Compile(source)->Run();
  CHECK(!v8::V8::IsExecutionTerminating(isolate));
  // Check we can run JS again after termination.
  CHECK(v8::Script::Compile(
      v8::String::NewFromUtf8(isolate,
                              "function f() { return true; }"
                              "f()"))
            ->Run()
            ->IsTrue());
}


void DoLoopCancelTerminate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch;
  CHECK(!v8::V8::IsExecutionTerminating());
  v8::Script::Compile(v8::String::NewFromUtf8(args.GetIsolate(),
                                              "var term = true;"
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
  v8::V8::CancelTerminateExecution(CcTest::isolate());
  CHECK(!v8::V8::IsExecutionTerminating());
}


// Test that a single thread of JavaScript execution can terminate
// itself and then resume execution.
TEST(TerminateCancelTerminateFromThreadItself) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, DoLoopCancelTerminate);
  v8::Handle<v8::Context> context = v8::Context::New(isolate, NULL, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::V8::IsExecutionTerminating(CcTest::isolate()));
  v8::Handle<v8::String> source = v8::String::NewFromUtf8(
      isolate, "try { doloop(); } catch(e) { fail(); } 'completed';");
  // Check that execution completed with correct return value.
  CHECK(v8::Script::Compile(source)->Run()->Equals(v8_str("completed")));
}


void MicrotaskShouldNotRun(const v8::FunctionCallbackInfo<v8::Value>& info) {
  CHECK(false);
}


void MicrotaskLoopForever(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  // Enqueue another should-not-run task to ensure we clean out the queue
  // when we terminate.
  isolate->EnqueueMicrotask(v8::Function::New(isolate, MicrotaskShouldNotRun));
  CompileRun("terminate(); while (true) { }");
  CHECK(v8::V8::IsExecutionTerminating());
}


TEST(TerminateFromOtherThreadWhileMicrotaskRunning) {
  semaphore = new v8::base::Semaphore(0);
  TerminatorThread thread(CcTest::i_isolate());
  thread.Start();

  v8::Isolate* isolate = CcTest::isolate();
  isolate->SetAutorunMicrotasks(false);
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), Signal, DoLoop);
  v8::Handle<v8::Context> context =
      v8::Context::New(CcTest::isolate(), NULL, global);
  v8::Context::Scope context_scope(context);
  isolate->EnqueueMicrotask(v8::Function::New(isolate, MicrotaskLoopForever));
  // The second task should never be run because we bail out if we're
  // terminating.
  isolate->EnqueueMicrotask(v8::Function::New(isolate, MicrotaskShouldNotRun));
  isolate->RunMicrotasks();

  v8::V8::CancelTerminateExecution(isolate);
  isolate->RunMicrotasks();  // should not run MicrotaskShouldNotRun

  thread.Join();
  delete semaphore;
  semaphore = NULL;
}


static int callback_counter = 0;


static void CounterCallback(v8::Isolate* isolate, void* data) {
  callback_counter++;
}


TEST(PostponeTerminateException) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Handle<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), TerminateCurrentThread, DoLoop);
  v8::Handle<v8::Context> context =
      v8::Context::New(CcTest::isolate(), NULL, global);
  v8::Context::Scope context_scope(context);

  v8::TryCatch try_catch;
  static const char* terminate_and_loop =
      "terminate(); for (var i = 0; i < 10000; i++);";

  { // Postpone terminate execution interrupts.
    i::PostponeInterruptsScope p1(CcTest::i_isolate(),
                                  i::StackGuard::TERMINATE_EXECUTION) ;

    // API interrupts should still be triggered.
    CcTest::isolate()->RequestInterrupt(&CounterCallback, NULL);
    CHECK_EQ(0, callback_counter);
    CompileRun(terminate_and_loop);
    CHECK(!try_catch.HasTerminated());
    CHECK_EQ(1, callback_counter);

    { // Postpone API interrupts as well.
      i::PostponeInterruptsScope p2(CcTest::i_isolate(),
                                    i::StackGuard::API_INTERRUPT);

      // None of the two interrupts should trigger.
      CcTest::isolate()->RequestInterrupt(&CounterCallback, NULL);
      CompileRun(terminate_and_loop);
      CHECK(!try_catch.HasTerminated());
      CHECK_EQ(1, callback_counter);
    }

    // Now the previously requested API interrupt should trigger.
    CompileRun(terminate_and_loop);
    CHECK(!try_catch.HasTerminated());
    CHECK_EQ(2, callback_counter);
  }

  // Now the previously requested terminate execution interrupt should trigger.
  CompileRun("for (var i = 0; i < 10000; i++);");
  CHECK(try_catch.HasTerminated());
  CHECK_EQ(2, callback_counter);
}
