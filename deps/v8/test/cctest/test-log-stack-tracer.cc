// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of profiler-related functions from log.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#include <stdlib.h>

#include "v8.h"

#include "codegen.h"
#include "log.h"
#include "top.h"
#include "cctest.h"
#include "disassembler.h"
#include "register-allocator-inl.h"

using v8::Function;
using v8::Local;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

using v8::internal::byte;
using v8::internal::Address;
using v8::internal::Handle;
using v8::internal::JSFunction;
using v8::internal::StackTracer;
using v8::internal::TickSample;
using v8::internal::Top;

namespace i = v8::internal;


static v8::Persistent<v8::Context> env;


static struct {
  TickSample* sample;
} trace_env = { NULL };


static void InitTraceEnv(TickSample* sample) {
  trace_env.sample = sample;
}


static void DoTrace(Address fp) {
  trace_env.sample->fp = reinterpret_cast<uintptr_t>(fp);
  // sp is only used to define stack high bound
  trace_env.sample->sp =
      reinterpret_cast<uintptr_t>(trace_env.sample) - 10240;
  StackTracer::Trace(trace_env.sample);
}


// Hide c_entry_fp to emulate situation when sampling is done while
// pure JS code is being executed
static void DoTraceHideCEntryFPAddress(Address fp) {
  v8::internal::Address saved_c_frame_fp = *(Top::c_entry_fp_address());
  CHECK(saved_c_frame_fp);
  *(Top::c_entry_fp_address()) = 0;
  DoTrace(fp);
  *(Top::c_entry_fp_address()) = saved_c_frame_fp;
}


static void CheckRetAddrIsInFunction(const char* func_name,
                                     Address ret_addr,
                                     Address func_start_addr,
                                     unsigned int func_len) {
  printf("CheckRetAddrIsInFunction \"%s\": %p %p %p\n",
         func_name, func_start_addr, ret_addr, func_start_addr + func_len);
  CHECK_GE(ret_addr, func_start_addr);
  CHECK_GE(func_start_addr + func_len, ret_addr);
}


static void CheckRetAddrIsInJSFunction(const char* func_name,
                                       Address ret_addr,
                                       Handle<JSFunction> func) {
  v8::internal::Code* func_code = func->code();
  CheckRetAddrIsInFunction(
      func_name, ret_addr,
      func_code->instruction_start(),
      func_code->ExecutableSize());
}


// --- T r a c e   E x t e n s i o n ---

class TraceExtension : public v8::Extension {
 public:
  TraceExtension() : v8::Extension("v8/trace", kSource) { }
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<String> name);
  static v8::Handle<v8::Value> Trace(const v8::Arguments& args);
  static v8::Handle<v8::Value> JSTrace(const v8::Arguments& args);
  static v8::Handle<v8::Value> JSEntrySP(const v8::Arguments& args);
  static v8::Handle<v8::Value> JSEntrySPLevel2(const v8::Arguments& args);
 private:
  static Address GetFP(const v8::Arguments& args);
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


Address TraceExtension::GetFP(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  // CodeGenerator::GenerateGetFramePointer pushes EBP / RBP value
  // on stack. In 64-bit mode we can't use Smi operations code because
  // they check that value is within Smi bounds.
  Address fp = *reinterpret_cast<Address*>(*args[0]);
  printf("Trace: %p\n", fp);
  return fp;
}


v8::Handle<v8::Value> TraceExtension::Trace(const v8::Arguments& args) {
  DoTrace(GetFP(args));
  return v8::Undefined();
}


v8::Handle<v8::Value> TraceExtension::JSTrace(const v8::Arguments& args) {
  DoTraceHideCEntryFPAddress(GetFP(args));
  return v8::Undefined();
}


static Address GetJsEntrySp() {
  CHECK_NE(NULL, Top::GetCurrentThread());
  return Top::js_entry_sp(Top::GetCurrentThread());
}


v8::Handle<v8::Value> TraceExtension::JSEntrySP(const v8::Arguments& args) {
  CHECK_NE(0, GetJsEntrySp());
  return v8::Undefined();
}


static void CompileRun(const char* source) {
  Script::Compile(String::New(source))->Run();
}


v8::Handle<v8::Value> TraceExtension::JSEntrySPLevel2(
    const v8::Arguments& args) {
  v8::HandleScope scope;
  const Address js_entry_sp = GetJsEntrySp();
  CHECK_NE(0, js_entry_sp);
  CompileRun("js_entry_sp();");
  CHECK_EQ(js_entry_sp, GetJsEntrySp());
  return v8::Undefined();
}


static TraceExtension kTraceExtension;
v8::DeclareExtension kTraceExtensionDeclaration(&kTraceExtension);


static void InitializeVM() {
  if (env.IsEmpty()) {
    v8::HandleScope scope;
    const char* extensions[] = { "v8/trace" };
    v8::ExtensionConfiguration config(1, extensions);
    env = v8::Context::New(&config);
  }
  v8::HandleScope scope;
  env->Enter();
}


static Handle<JSFunction> CompileFunction(const char* source) {
  return v8::Utils::OpenHandle(*Script::Compile(String::New(source)));
}


static Local<Value> GetGlobalProperty(const char* name) {
  return env->Global()->Get(String::New(name));
}


static Handle<JSFunction> GetGlobalJSFunction(const char* name) {
  Handle<JSFunction> js_func(JSFunction::cast(
                                 *(v8::Utils::OpenHandle(
                                       *GetGlobalProperty(name)))));
  return js_func;
}


static void CheckRetAddrIsInJSFunction(const char* func_name,
                                       Address ret_addr) {
  CheckRetAddrIsInJSFunction(func_name, ret_addr,
                             GetGlobalJSFunction(func_name));
}


static void SetGlobalProperty(const char* name, Local<Value> value) {
  env->Global()->Set(String::New(name), value);
}


static Handle<v8::internal::String> NewString(const char* s) {
  return i::Factory::NewStringFromAscii(i::CStrVector(s));
}


namespace v8 {
namespace internal {

class CodeGeneratorPatcher {
 public:
  CodeGeneratorPatcher() {
    CodeGenerator::InlineRuntimeLUT genGetFramePointer =
        {&CodeGenerator::GenerateGetFramePointer, "_GetFramePointer"};
    // _FastCharCodeAt is not used in our tests.
    bool result = CodeGenerator::PatchInlineRuntimeEntry(
        NewString("_FastCharCodeAt"),
        genGetFramePointer, &oldInlineEntry);
    CHECK(result);
  }

  ~CodeGeneratorPatcher() {
    CHECK(CodeGenerator::PatchInlineRuntimeEntry(
        NewString("_GetFramePointer"),
        oldInlineEntry, NULL));
  }

 private:
  CodeGenerator::InlineRuntimeLUT oldInlineEntry;
};

} }  // namespace v8::internal


// Creates a global function named 'func_name' that calls the tracing
// function 'trace_func_name' with an actual EBP register value,
// shifted right to be presented as Smi.
static void CreateTraceCallerFunction(const char* func_name,
                                      const char* trace_func_name) {
  i::EmbeddedVector<char, 256> trace_call_buf;
  i::OS::SNPrintF(trace_call_buf, "%s(%%_GetFramePointer());", trace_func_name);

  // Compile the script.
  i::CodeGeneratorPatcher patcher;
  bool allow_natives_syntax = i::FLAG_allow_natives_syntax;
  i::FLAG_allow_natives_syntax = true;
  Handle<JSFunction> func = CompileFunction(trace_call_buf.start());
  CHECK(!func.is_null());
  i::FLAG_allow_natives_syntax = allow_natives_syntax;

#ifdef DEBUG
  v8::internal::Code* func_code = func->code();
  CHECK(func_code->IsCode());
  func_code->Print();
#endif

  SetGlobalProperty(func_name, v8::ToApi<Value>(func));
}


TEST(CFromJSStackTrace) {
  TickSample sample;
  InitTraceEnv(&sample);

  InitializeVM();
  v8::HandleScope scope;
  CreateTraceCallerFunction("JSFuncDoTrace", "trace");
  CompileRun(
      "function JSTrace() {"
      "         JSFuncDoTrace();"
      "};\n"
      "JSTrace();");
  CHECK_GT(sample.frames_count, 1);
  // Stack sampling will start from the first JS function, i.e. "JSFuncDoTrace"
  CheckRetAddrIsInJSFunction("JSFuncDoTrace",
                             sample.stack[0]);
  CheckRetAddrIsInJSFunction("JSTrace",
                             sample.stack[1]);
}


TEST(PureJSStackTrace) {
  TickSample sample;
  InitTraceEnv(&sample);

  InitializeVM();
  v8::HandleScope scope;
  CreateTraceCallerFunction("JSFuncDoTrace", "js_trace");
  CompileRun(
      "function JSTrace() {"
      "         JSFuncDoTrace();"
      "};\n"
      "function OuterJSTrace() {"
      "         JSTrace();"
      "};\n"
      "OuterJSTrace();");
  CHECK_GT(sample.frames_count, 1);
  // Stack sampling will start from the caller of JSFuncDoTrace, i.e. "JSTrace"
  CheckRetAddrIsInJSFunction("JSTrace",
                             sample.stack[0]);
  CheckRetAddrIsInJSFunction("OuterJSTrace",
                             sample.stack[1]);
}


static void CFuncDoTrace() {
  Address fp;
#ifdef __GNUC__
  fp = reinterpret_cast<Address>(__builtin_frame_address(0));
#elif defined _MSC_VER && defined V8_TARGET_ARCH_IA32
  __asm mov [fp], ebp  // NOLINT
#elif defined _MSC_VER && defined V8_TARGET_ARCH_X64
  // FIXME: I haven't really tried to compile it.
  __asm movq [fp], rbp  // NOLINT
#endif
  DoTrace(fp);
}


static int CFunc(int depth) {
  if (depth <= 0) {
    CFuncDoTrace();
    return 0;
  } else {
    return CFunc(depth - 1) + 1;
  }
}


TEST(PureCStackTrace) {
  TickSample sample;
  InitTraceEnv(&sample);
  // Check that sampler doesn't crash
  CHECK_EQ(10, CFunc(10));
}


TEST(JsEntrySp) {
  InitializeVM();
  v8::HandleScope scope;
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("a = 1; b = a + 1;");
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("js_entry_sp();");
  CHECK_EQ(0, GetJsEntrySp());
  CompileRun("js_entry_sp_level2();");
  CHECK_EQ(0, GetJsEntrySp());
}

#endif  // ENABLE_LOGGING_AND_PROFILING
