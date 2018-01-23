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

#include "src/api.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/platform/platform.h"

v8::base::Semaphore* semaphore = nullptr;

void Signal(const v8::FunctionCallbackInfo<v8::Value>& args) {
  semaphore->Signal();
}


void TerminateCurrentThread(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  args.GetIsolate()->TerminateExecution();
}

void Fail(const v8::FunctionCallbackInfo<v8::Value>& args) { UNREACHABLE(); }

void Loop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(args.GetIsolate()->GetCurrentContext(),
                 "try { doloop(); fail(); } catch(e) { fail(); }");
  CHECK(result.IsEmpty());
  CHECK(args.GetIsolate()->IsExecutionTerminating());
}


void DoLoop(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(args.GetIsolate()->GetCurrentContext(),
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
                 "f()");
  CHECK(result.IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(args.GetIsolate()->IsExecutionTerminating());
}


void DoLoopNoCall(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(args.GetIsolate()->GetCurrentContext(),
                 "var term = true;"
                 "while(true) {"
                 "  if (term) terminate();"
                 "  term = false;"
                 "}");
  CHECK(result.IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(args.GetIsolate()->IsExecutionTerminating());
}


v8::Local<v8::ObjectTemplate> CreateGlobalTemplate(
    v8::Isolate* isolate, v8::FunctionCallback terminate,
    v8::FunctionCallback doloop) {
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(v8_str("terminate"),
              v8::FunctionTemplate::New(isolate, terminate));
  global->Set(v8_str("fail"), v8::FunctionTemplate::New(isolate, Fail));
  global->Set(v8_str("loop"), v8::FunctionTemplate::New(isolate, Loop));
  global->Set(v8_str("doloop"), v8::FunctionTemplate::New(isolate, doloop));
  return global;
}


// Test that a single thread of JavaScript execution can terminate
// itself.
TEST(TerminateOnlyV8ThreadFromThreadItself) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), TerminateCurrentThread, DoLoop);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::MaybeLocal<v8::Value> result =
      CompileRun(CcTest::isolate()->GetCurrentContext(),
                 "try { loop(); fail(); } catch(e) { fail(); }");
  CHECK(result.IsEmpty());
  // Test that we can run the code again after thread termination.
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  result = CompileRun(CcTest::isolate()->GetCurrentContext(),
                      "try { loop(); fail(); } catch(e) { fail(); }");
  CHECK(result.IsEmpty());
}


// Test that a single thread of JavaScript execution can terminate
// itself in a loop that performs no calls.
TEST(TerminateOnlyV8ThreadFromThreadItselfNoLoop) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::ObjectTemplate> global = CreateGlobalTemplate(
      CcTest::isolate(), TerminateCurrentThread, DoLoopNoCall);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  static const char* source = "try { loop(); fail(); } catch(e) { fail(); }";
  v8::MaybeLocal<v8::Value> result =
      CompileRun(CcTest::isolate()->GetCurrentContext(), source);
  CHECK(result.IsEmpty());
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  // Test that we can run the code again after thread termination.
  result = CompileRun(CcTest::isolate()->GetCurrentContext(), source);
  CHECK(result.IsEmpty());
}


class TerminatorThread : public v8::base::Thread {
 public:
  explicit TerminatorThread(i::Isolate* isolate)
      : Thread(Options("TerminatorThread")),
        isolate_(reinterpret_cast<v8::Isolate*>(isolate)) {}
  void Run() {
    semaphore->Wait();
    CHECK(!isolate_->IsExecutionTerminating());
    isolate_->TerminateExecution();
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
  v8::Local<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), Signal, DoLoop);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  v8::MaybeLocal<v8::Value> result =
      CompileRun(CcTest::isolate()->GetCurrentContext(),
                 "try { loop(); fail(); } catch(e) { fail(); }");
  CHECK(result.IsEmpty());
  thread.Join();
  delete semaphore;
  semaphore = nullptr;
}

// Test that execution can be terminated from within JSON.stringify.
TEST(TerminateJsonStringify) {
  semaphore = new v8::base::Semaphore(0);
  TerminatorThread thread(CcTest::i_isolate());
  thread.Start();

  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), Signal, DoLoop);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(CcTest::isolate()->GetCurrentContext(),
                 "var x = [];"
                 "x[2**31]=1;"
                 "terminate();"
                 "JSON.stringify(x);"
                 "fail();");
  CHECK(result.IsEmpty());
  thread.Join();
  delete semaphore;
  semaphore = nullptr;
}

int call_count = 0;


void TerminateOrReturnObject(const v8::FunctionCallbackInfo<v8::Value>& args) {
  if (++call_count == 10) {
    CHECK(!args.GetIsolate()->IsExecutionTerminating());
    args.GetIsolate()->TerminateExecution();
    return;
  }
  v8::Local<v8::Object> result = v8::Object::New(args.GetIsolate());
  v8::Maybe<bool> val =
      result->Set(args.GetIsolate()->GetCurrentContext(), v8_str("x"),
                  v8::Integer::New(args.GetIsolate(), 42));
  CHECK(val.FromJust());
  args.GetReturnValue().Set(result);
}


void LoopGetProperty(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(args.GetIsolate()->GetCurrentContext(),
                 "function f() {"
                 "  try {"
                 "    while(true) {"
                 "      terminate_or_return_object().x;"
                 "    }"
                 "    fail();"
                 "  } catch(e) {"
                 "    (function() {})();"  // trigger stack check.
                 "    fail();"
                 "  }"
                 "}"
                 "f()");
  CHECK(result.IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(args.GetIsolate()->IsExecutionTerminating());
}


// Test that we correctly handle termination exceptions if they are
// triggered by the creation of error objects in connection with ICs.
TEST(TerminateLoadICException) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = v8::ObjectTemplate::New(isolate);
  global->Set(v8_str("terminate_or_return_object"),
              v8::FunctionTemplate::New(isolate, TerminateOrReturnObject));
  global->Set(v8_str("fail"), v8::FunctionTemplate::New(isolate, Fail));
  global->Set(v8_str("loop"),
              v8::FunctionTemplate::New(isolate, LoopGetProperty));

  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!isolate->IsExecutionTerminating());
  // Run a loop that will be infinite if thread termination does not work.
  static const char* source = "try { loop(); fail(); } catch(e) { fail(); }";
  call_count = 0;
  v8::MaybeLocal<v8::Value> result =
      CompileRun(isolate->GetCurrentContext(), source);
  CHECK(result.IsEmpty());
  // Test that we can run the code again after thread termination.
  CHECK(!isolate->IsExecutionTerminating());
  call_count = 0;
  result = CompileRun(isolate->GetCurrentContext(), source);
  CHECK(result.IsEmpty());
}


v8::Persistent<v8::String> reenter_script_1;
v8::Persistent<v8::String> reenter_script_2;

void ReenterAfterTermination(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  v8::Isolate* isolate = args.GetIsolate();
  CHECK(!isolate->IsExecutionTerminating());
  v8::Local<v8::String> script =
      v8::Local<v8::String>::New(isolate, reenter_script_1);
  v8::MaybeLocal<v8::Value> result = CompileRun(script);
  CHECK(result.IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(isolate->IsExecutionTerminating());
  script = v8::Local<v8::String>::New(isolate, reenter_script_2);
  v8::MaybeLocal<v8::Script> compiled_script =
      v8::Script::Compile(isolate->GetCurrentContext(), script);
  CHECK(compiled_script.IsEmpty());
}


// Test that reentry into V8 while the termination exception is still pending
// (has not yet unwound the 0-level JS frame) does not crash.
TEST(TerminateAndReenterFromThreadItself) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, ReenterAfterTermination);
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!v8::Isolate::GetCurrent()->IsExecutionTerminating());
  // Create script strings upfront as it won't work when terminating.
  reenter_script_1.Reset(isolate, v8_str(
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
                                      "f()"));
  reenter_script_2.Reset(isolate, v8_str("function f() { fail(); } f()"));
  CompileRun("try { loop(); fail(); } catch(e) { fail(); }");
  CHECK(!isolate->IsExecutionTerminating());
  // Check we can run JS again after termination.
  CHECK(CompileRun("function f() { return true; } f()")->IsTrue());
  reenter_script_1.Reset();
  reenter_script_2.Reset();
}


void DoLoopCancelTerminate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::TryCatch try_catch(args.GetIsolate());
  CHECK(!v8::Isolate::GetCurrent()->IsExecutionTerminating());
  v8::MaybeLocal<v8::Value> result =
      CompileRun(args.GetIsolate()->GetCurrentContext(),
                 "var term = true;"
                 "while(true) {"
                 "  if (term) terminate();"
                 "  term = false;"
                 "}"
                 "fail();");
  CHECK(result.IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(try_catch.Exception()->IsNull());
  CHECK(try_catch.Message().IsEmpty());
  CHECK(!try_catch.CanContinue());
  CHECK(v8::Isolate::GetCurrent()->IsExecutionTerminating());
  CHECK(try_catch.HasTerminated());
  CcTest::isolate()->CancelTerminateExecution();
  CHECK(!v8::Isolate::GetCurrent()->IsExecutionTerminating());
}


// Test that a single thread of JavaScript execution can terminate
// itself and then resume execution.
TEST(TerminateCancelTerminateFromThreadItself) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, DoLoopCancelTerminate);
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!CcTest::isolate()->IsExecutionTerminating());
  // Check that execution completed with correct return value.
  v8::Local<v8::Value> result =
      CompileRun(isolate->GetCurrentContext(),
                 "try { doloop(); } catch(e) { fail(); } 'completed';")
          .ToLocalChecked();
  CHECK(result->Equals(isolate->GetCurrentContext(), v8_str("completed"))
            .FromJust());
}


void MicrotaskShouldNotRun(const v8::FunctionCallbackInfo<v8::Value>& info) {
  UNREACHABLE();
}


void MicrotaskLoopForever(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope scope(isolate);
  // Enqueue another should-not-run task to ensure we clean out the queue
  // when we terminate.
  isolate->EnqueueMicrotask(
      v8::Function::New(isolate->GetCurrentContext(), MicrotaskShouldNotRun)
          .ToLocalChecked());
  CompileRun("terminate(); while (true) { }");
  CHECK(v8::Isolate::GetCurrent()->IsExecutionTerminating());
}


TEST(TerminateFromOtherThreadWhileMicrotaskRunning) {
  semaphore = new v8::base::Semaphore(0);
  TerminatorThread thread(CcTest::i_isolate());
  thread.Start();

  v8::Isolate* isolate = CcTest::isolate();
  isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), Signal, DoLoop);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);
  isolate->EnqueueMicrotask(
      v8::Function::New(isolate->GetCurrentContext(), MicrotaskLoopForever)
          .ToLocalChecked());
  // The second task should never be run because we bail out if we're
  // terminating.
  isolate->EnqueueMicrotask(
      v8::Function::New(isolate->GetCurrentContext(), MicrotaskShouldNotRun)
          .ToLocalChecked());
  isolate->RunMicrotasks();

  isolate->CancelTerminateExecution();
  isolate->RunMicrotasks();  // should not run MicrotaskShouldNotRun

  thread.Join();
  delete semaphore;
  semaphore = nullptr;
}


static int callback_counter = 0;


static void CounterCallback(v8::Isolate* isolate, void* data) {
  callback_counter++;
}


TEST(PostponeTerminateException) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global =
      CreateGlobalTemplate(CcTest::isolate(), TerminateCurrentThread, DoLoop);
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global);
  v8::Context::Scope context_scope(context);

  v8::TryCatch try_catch(isolate);
  static const char* terminate_and_loop =
      "terminate(); for (var i = 0; i < 10000; i++);";

  { // Postpone terminate execution interrupts.
    i::PostponeInterruptsScope p1(CcTest::i_isolate(),
                                  i::StackGuard::TERMINATE_EXECUTION);

    // API interrupts should still be triggered.
    CcTest::isolate()->RequestInterrupt(&CounterCallback, nullptr);
    CHECK_EQ(0, callback_counter);
    CompileRun(terminate_and_loop);
    CHECK(!try_catch.HasTerminated());
    CHECK_EQ(1, callback_counter);

    { // Postpone API interrupts as well.
      i::PostponeInterruptsScope p2(CcTest::i_isolate(),
                                    i::StackGuard::API_INTERRUPT);

      // None of the two interrupts should trigger.
      CcTest::isolate()->RequestInterrupt(&CounterCallback, nullptr);
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


TEST(ErrorObjectAfterTermination) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(CcTest::isolate());
  v8::Context::Scope context_scope(context);
  isolate->TerminateExecution();
  v8::Local<v8::Value> error = v8::Exception::Error(v8_str("error"));
  CHECK(error->IsNativeError());
}


void InnerTryCallTerminate(const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  v8::Local<v8::Object> global = CcTest::global();
  v8::Local<v8::Function> loop = v8::Local<v8::Function>::Cast(
      global->Get(CcTest::isolate()->GetCurrentContext(), v8_str("loop"))
          .ToLocalChecked());
  i::MaybeHandle<i::Object> exception;
  i::MaybeHandle<i::Object> result =
      i::Execution::TryCall(CcTest::i_isolate(), v8::Utils::OpenHandle((*loop)),
                            v8::Utils::OpenHandle((*global)), 0, nullptr,
                            i::Execution::MessageHandling::kReport, &exception);
  CHECK(result.is_null());
  // TryCall ignores terminate execution, but rerequests the interrupt.
  CHECK(!args.GetIsolate()->IsExecutionTerminating());
  CHECK(CompileRun("1 + 1;").IsEmpty());
}


TEST(TerminationInInnerTryCall) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global_template = CreateGlobalTemplate(
      CcTest::isolate(), TerminateCurrentThread, DoLoopNoCall);
  global_template->Set(
      v8_str("inner_try_call_terminate"),
      v8::FunctionTemplate::New(isolate, InnerTryCallTerminate));
  v8::Local<v8::Context> context =
      v8::Context::New(CcTest::isolate(), nullptr, global_template);
  v8::Context::Scope context_scope(context);
  {
    v8::TryCatch try_catch(isolate);
    CompileRun("inner_try_call_terminate()");
    CHECK(try_catch.HasTerminated());
  }
  v8::Maybe<int32_t> result = CompileRun("2 + 2")->Int32Value(
      v8::Isolate::GetCurrent()->GetCurrentContext());
  CHECK_EQ(4, result.FromJust());
  CHECK(!v8::Isolate::GetCurrent()->IsExecutionTerminating());
}


TEST(TerminateAndTryCall) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, DoLoopCancelTerminate);
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!isolate->IsExecutionTerminating());
  v8::TryCatch try_catch(isolate);
  CHECK(!isolate->IsExecutionTerminating());
  // Terminate execution has been triggered inside TryCall, but re-requested
  // to trigger later.
  CHECK(CompileRun("terminate(); reference_error();").IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(!isolate->IsExecutionTerminating());
  v8::Local<v8::Value> value =
      CcTest::global()
          ->Get(isolate->GetCurrentContext(), v8_str("terminate"))
          .ToLocalChecked();
  CHECK(value->IsFunction());
  // The first stack check after terminate has been re-requested fails.
  CHECK(CompileRun("1 + 1").IsEmpty());
  CHECK(!isolate->IsExecutionTerminating());
  // V8 then recovers.
  v8::Maybe<int32_t> result = CompileRun("2 + 2")->Int32Value(
      v8::Isolate::GetCurrent()->GetCurrentContext());
  CHECK_EQ(4, result.FromJust());
  CHECK(!isolate->IsExecutionTerminating());
}

class ConsoleImpl : public v8::debug::ConsoleDelegate {
 private:
  void Log(const v8::debug::ConsoleCallArguments& args,
           const v8::debug::ConsoleContext&) override {
    CompileRun("1 + 1");
  }
};

TEST(TerminateConsole) {
  i::FLAG_allow_natives_syntax = true;
  v8::Isolate* isolate = CcTest::isolate();
  ConsoleImpl console;
  v8::debug::SetConsoleDelegate(isolate, &console);
  v8::HandleScope scope(isolate);
  v8::Local<v8::ObjectTemplate> global = CreateGlobalTemplate(
      isolate, TerminateCurrentThread, DoLoopCancelTerminate);
  v8::Local<v8::Context> context = v8::Context::New(isolate, nullptr, global);
  v8::Context::Scope context_scope(context);
  CHECK(!isolate->IsExecutionTerminating());
  v8::TryCatch try_catch(isolate);
  CHECK(!isolate->IsExecutionTerminating());
  CHECK(CompileRun("terminate(); console.log(); fail();").IsEmpty());
  CHECK(try_catch.HasCaught());
  CHECK(!isolate->IsExecutionTerminating());
}
