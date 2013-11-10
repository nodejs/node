// Copyright 2011 the V8 project authors. All rights reserved.
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
//
// Tests of profiler-related functions from log.h

#include <stdlib.h>

#include "v8.h"

#include "api.h"
#include "cctest.h"
#include "codegen.h"
#include "disassembler.h"
#include "isolate.h"
#include "log.h"
#include "sampler.h"
#include "vm-state-inl.h"

using v8::Function;
using v8::Local;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

using v8::internal::byte;
using v8::internal::Address;
using v8::internal::Handle;
using v8::internal::Isolate;
using v8::internal::JSFunction;
using v8::internal::RegisterState;
using v8::internal::TickSample;


static struct {
  TickSample* sample;
} trace_env = { NULL };


static void InitTraceEnv(TickSample* sample) {
  trace_env.sample = sample;
}


static void DoTrace(Address fp) {
  RegisterState regs;
  regs.fp = fp;
  // sp is only used to define stack high bound
  regs.sp =
      reinterpret_cast<Address>(trace_env.sample) - 10240;
  trace_env.sample->Init(CcTest::i_isolate(), regs);
}


// Hide c_entry_fp to emulate situation when sampling is done while
// pure JS code is being executed
static void DoTraceHideCEntryFPAddress(Address fp) {
  v8::internal::Address saved_c_frame_fp =
      *(CcTest::i_isolate()->c_entry_fp_address());
  CHECK(saved_c_frame_fp);
  *(CcTest::i_isolate()->c_entry_fp_address()) = 0;
  DoTrace(fp);
  *(CcTest::i_isolate()->c_entry_fp_address()) = saved_c_frame_fp;
}


// --- T r a c e   E x t e n s i o n ---

class TraceExtension : public v8::Extension {
 public:
  TraceExtension() : v8::Extension("v8/trace", kSource) { }
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<String> name);
  static void Trace(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void JSTrace(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void JSEntrySP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void JSEntrySPLevel2(const v8::FunctionCallbackInfo<v8::Value>& args);
 private:
  static Address GetFP(const v8::FunctionCallbackInfo<v8::Value>& args);
  static const char* kSource;
};


const char* TraceExtension::kSource =
    "native function trace();"
    "native function js_trace();"
    "native function js_entry_sp();"
    "native function js_entry_sp_level2();";

v8::Handle<v8::FunctionTemplate> TraceExtension::GetNativeFunction(
    v8::Handle<String> name) {
  if (name->Equals(String::New("trace"))) {
    return v8::FunctionTemplate::New(TraceExtension::Trace);
  } else if (name->Equals(String::New("js_trace"))) {
    return v8::FunctionTemplate::New(TraceExtension::JSTrace);
  } else if (name->Equals(String::New("js_entry_sp"))) {
    return v8::FunctionTemplate::New(TraceExtension::JSEntrySP);
  } else if (name->Equals(String::New("js_entry_sp_level2"))) {
    return v8::FunctionTemplate::New(TraceExtension::JSEntrySPLevel2);
  } else {
    CHECK(false);
    return v8::Handle<v8::FunctionTemplate>();
  }
}


Address TraceExtension::GetFP(const v8::FunctionCallbackInfo<v8::Value>& args) {
  // Convert frame pointer from encoding as smis in the arguments to a pointer.
  CHECK_EQ(2, args.Length());  // Ignore second argument on 32-bit platform.
#if defined(V8_HOST_ARCH_32_BIT)
  Address fp = *reinterpret_cast<Address*>(*args[0]);
#elif defined(V8_HOST_ARCH_64_BIT)
  int64_t low_bits = *reinterpret_cast<uint64_t*>(*args[0]) >> 32;
  int64_t high_bits = *reinterpret_cast<uint64_t*>(*args[1]);
  Address fp = reinterpret_cast<Address>(high_bits | low_bits);
#else
#error Host architecture is neither 32-bit nor 64-bit.
#endif
  printf("Trace: %p\n", fp);
  return fp;
}


void TraceExtension::Trace(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DoTrace(GetFP(args));
}


void TraceExtension::JSTrace(const v8::FunctionCallbackInfo<v8::Value>& args) {
  DoTraceHideCEntryFPAddress(GetFP(args));
}


static Address GetJsEntrySp() {
  CHECK_NE(NULL, CcTest::i_isolate()->thread_local_top());
  return CcTest::i_isolate()->js_entry_sp();
}


void TraceExtension::JSEntrySP(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  CHECK_NE(0, GetJsEntrySp());
}


void TraceExtension::JSEntrySPLevel2(
    const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::HandleScope scope(args.GetIsolate());
  const Address js_entry_sp = GetJsEntrySp();
  CHECK_NE(0, js_entry_sp);
  CompileRun("js_entry_sp();");
  CHECK_EQ(js_entry_sp, GetJsEntrySp());
}


static TraceExtension kTraceExtension;
v8::DeclareExtension kTraceExtensionDeclaration(&kTraceExtension);


static bool IsAddressWithinFuncCode(JSFunction* function, Address addr) {
  i::Code* code = function->code();
  return code->contains(addr);
}


static bool IsAddressWithinFuncCode(v8::Local<v8::Context> context,
                                    const char* func_name,
                                    Address addr) {
  v8::Local<v8::Value> func = context->Global()->Get(v8_str(func_name));
  CHECK(func->IsFunction());
  JSFunction* js_func = JSFunction::cast(*v8::Utils::OpenHandle(*func));
  return IsAddressWithinFuncCode(js_func, addr);
}


// This C++ function is called as a constructor, to grab the frame pointer
// from the calling function.  When this function runs, the stack contains
// a C_Entry frame and a Construct frame above the calling function's frame.
static void construct_call(const v8::FunctionCallbackInfo<v8::Value>& args) {
  i::Isolate* isolate = reinterpret_cast<i::Isolate*>(args.GetIsolate());
  i::StackFrameIterator frame_iterator(isolate);
  CHECK(frame_iterator.frame()->is_exit());
  frame_iterator.Advance();
  CHECK(frame_iterator.frame()->is_construct());
  frame_iterator.Advance();
  i::StackFrame* calling_frame = frame_iterator.frame();
  CHECK(calling_frame->is_java_script());

#if defined(V8_HOST_ARCH_32_BIT)
  int32_t low_bits = reinterpret_cast<int32_t>(calling_frame->fp());
  args.This()->Set(v8_str("low_bits"), v8_num(low_bits >> 1));
#elif defined(V8_HOST_ARCH_64_BIT)
  uint64_t fp = reinterpret_cast<uint64_t>(calling_frame->fp());
  int32_t low_bits = static_cast<int32_t>(fp & 0xffffffff);
  int32_t high_bits = static_cast<int32_t>(fp >> 32);
  args.This()->Set(v8_str("low_bits"), v8_num(low_bits));
  args.This()->Set(v8_str("high_bits"), v8_num(high_bits));
#else
#error Host architecture is neither 32-bit nor 64-bit.
#endif
  args.GetReturnValue().Set(args.This());
}


// Use the API to create a JSFunction object that calls the above C++ function.
void CreateFramePointerGrabberConstructor(v8::Local<v8::Context> context,
                                          const char* constructor_name) {
    Local<v8::FunctionTemplate> constructor_template =
        v8::FunctionTemplate::New(construct_call);
    constructor_template->SetClassName(v8_str("FPGrabber"));
    Local<Function> fun = constructor_template->GetFunction();
    context->Global()->Set(v8_str(constructor_name), fun);
}


// Creates a global function named 'func_name' that calls the tracing
// function 'trace_func_name' with an actual EBP register value,
// encoded as one or two Smis.
static void CreateTraceCallerFunction(v8::Local<v8::Context> context,
                                      const char* func_name,
                                      const char* trace_func_name) {
  i::EmbeddedVector<char, 256> trace_call_buf;
  i::OS::SNPrintF(trace_call_buf,
                  "function %s() {"
                  "  fp = new FPGrabber();"
                  "  %s(fp.low_bits, fp.high_bits);"
                  "}",
                  func_name, trace_func_name);

  // Create the FPGrabber function, which grabs the caller's frame pointer
  // when called as a constructor.
  CreateFramePointerGrabberConstructor(context, "FPGrabber");

  // Compile the script.
  CompileRun(trace_call_buf.start());
}


// This test verifies that stack tracing works when called during
// execution of a native function called from JS code. In this case,
// TickSample::Trace uses Isolate::c_entry_fp as a starting point for stack
// walking.
TEST(CFromJSStackTrace) {
  // BUG(1303) Inlining of JSFuncDoTrace() in JSTrace below breaks this test.
  i::FLAG_use_inlining = false;

  TickSample sample;
  InitTraceEnv(&sample);

  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(TRACE_EXTENSION);
  v8::Context::Scope context_scope(context);

  // Create global function JSFuncDoTrace which calls
  // extension function trace() with the current frame pointer value.
  CreateTraceCallerFunction(context, "JSFuncDoTrace", "trace");
  Local<Value> result = CompileRun(
      "function JSTrace() {"
      "         JSFuncDoTrace();"
      "};\n"
      "JSTrace();\n"
      "true;");
  CHECK(!result.IsEmpty());
  // When stack tracer is invoked, the stack should look as follows:
  // script [JS]
  //   JSTrace() [JS]
  //     JSFuncDoTrace() [JS] [captures EBP value and encodes it as Smi]
  //       trace(EBP) [native (extension)]
  //         DoTrace(EBP) [native]
  //           TickSample::Trace

  CHECK(sample.has_external_callback);
  CHECK_EQ(FUNCTION_ADDR(TraceExtension::Trace), sample.external_callback);

  // Stack tracing will start from the first JS function, i.e. "JSFuncDoTrace"
  int base = 0;
  CHECK_GT(sample.frames_count, base + 1);

  CHECK(IsAddressWithinFuncCode(
      context, "JSFuncDoTrace", sample.stack[base + 0]));
  CHECK(IsAddressWithinFuncCode(context, "JSTrace", sample.stack[base + 1]));
}


// This test verifies that stack tracing works when called during
// execution of JS code. However, as calling TickSample::Trace requires
// entering native code, we can only emulate pure JS by erasing
// Isolate::c_entry_fp value. In this case, TickSample::Trace uses passed frame
// pointer value as a starting point for stack walking.
TEST(PureJSStackTrace) {
  // This test does not pass with inlining enabled since inlined functions
  // don't appear in the stack trace.
  i::FLAG_use_inlining = false;

  TickSample sample;
  InitTraceEnv(&sample);

  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(TRACE_EXTENSION);
  v8::Context::Scope context_scope(context);

  // Create global function JSFuncDoTrace which calls
  // extension function js_trace() with the current frame pointer value.
  CreateTraceCallerFunction(context, "JSFuncDoTrace", "js_trace");
  Local<Value> result = CompileRun(
      "function JSTrace() {"
      "         JSFuncDoTrace();"
      "};\n"
      "function OuterJSTrace() {"
      "         JSTrace();"
      "};\n"
      "OuterJSTrace();\n"
      "true;");
  CHECK(!result.IsEmpty());
  // When stack tracer is invoked, the stack should look as follows:
  // script [JS]
  //   OuterJSTrace() [JS]
  //     JSTrace() [JS]
  //       JSFuncDoTrace() [JS]
  //         js_trace(EBP) [native (extension)]
  //           DoTraceHideCEntryFPAddress(EBP) [native]
  //             TickSample::Trace
  //

  CHECK(sample.has_external_callback);
  CHECK_EQ(FUNCTION_ADDR(TraceExtension::JSTrace), sample.external_callback);

  // Stack sampling will start from the caller of JSFuncDoTrace, i.e. "JSTrace"
  int base = 0;
  CHECK_GT(sample.frames_count, base + 1);
  CHECK(IsAddressWithinFuncCode(context, "JSTrace", sample.stack[base + 0]));
  CHECK(IsAddressWithinFuncCode(
      context, "OuterJSTrace", sample.stack[base + 1]));
}


static void CFuncDoTrace(byte dummy_parameter) {
  Address fp;
#ifdef __GNUC__
  fp = reinterpret_cast<Address>(__builtin_frame_address(0));
#elif defined _MSC_VER
  // Approximate a frame pointer address. We compile without base pointers,
  // so we can't trust ebp/rbp.
  fp = &dummy_parameter - 2 * sizeof(void*);  // NOLINT
#else
#error Unexpected platform.
#endif
  DoTrace(fp);
}


static int CFunc(int depth) {
  if (depth <= 0) {
    CFuncDoTrace(0);
    return 0;
  } else {
    return CFunc(depth - 1) + 1;
  }
}


// This test verifies that stack tracing doesn't crash when called on
// pure native code. TickSample::Trace only unrolls JS code, so we can't
// get any meaningful info here.
TEST(PureCStackTrace) {
  TickSample sample;
  InitTraceEnv(&sample);
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(TRACE_EXTENSION);
  v8::Context::Scope context_scope(context);
  // Check that sampler doesn't crash
  CHECK_EQ(10, CFunc(10));
}


TEST(JsEntrySp) {
  v8::HandleScope scope(CcTest::isolate());
  v8::Local<v8::Context> context = CcTest::NewContext(TRACE_EXTENSION);
  v8::Context::Scope context_scope(context);
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("a = 1; b = a + 1;");
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("js_entry_sp();");
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("js_entry_sp_level2();");
  CHECK_EQ(0, GetJsEntrySp());
}
