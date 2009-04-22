// Copyright 2006-2009 the V8 project authors. All rights reserved.
//
// Tests of profiler-related functions from log.h

#ifdef ENABLE_LOGGING_AND_PROFILING

#include <stdlib.h>

#include "v8.h"

#include "log.h"
#include "top.h"
#include "cctest.h"

using v8::Function;
using v8::Local;
using v8::Object;
using v8::Script;
using v8::String;
using v8::Value;

using v8::internal::byte;
using v8::internal::Handle;
using v8::internal::JSFunction;
using v8::internal::StackTracer;
using v8::internal::TickSample;
using v8::internal::Top;


static v8::Persistent<v8::Context> env;


static struct {
  StackTracer* tracer;
  TickSample* sample;
} trace_env = { NULL, NULL };


static void InitTraceEnv(StackTracer* tracer, TickSample* sample) {
  trace_env.tracer = tracer;
  trace_env.sample = sample;
}


static void DoTrace(unsigned int fp) {
  trace_env.sample->fp = fp;
  // sp is only used to define stack high bound
  trace_env.sample->sp =
      reinterpret_cast<unsigned int>(trace_env.sample) - 10240;
  trace_env.tracer->Trace(trace_env.sample);
}


// Hide c_entry_fp to emulate situation when sampling is done while
// pure JS code is being executed
static void DoTraceHideCEntryFPAddress(unsigned int fp) {
  v8::internal::Address saved_c_frame_fp = *(Top::c_entry_fp_address());
  CHECK(saved_c_frame_fp);
  *(Top::c_entry_fp_address()) = 0;
  DoTrace(fp);
  *(Top::c_entry_fp_address()) = saved_c_frame_fp;
}


static void CheckRetAddrIsInFunction(const char* func_name,
                                     unsigned int ret_addr,
                                     unsigned int func_start_addr,
                                     unsigned int func_len) {
  printf("CheckRetAddrIsInFunction \"%s\": %08x %08x %08x\n",
         func_name, func_start_addr, ret_addr, func_start_addr + func_len);
  CHECK_GE(ret_addr, func_start_addr);
  CHECK_GE(func_start_addr + func_len, ret_addr);
}


static void CheckRetAddrIsInJSFunction(const char* func_name,
                                       unsigned int ret_addr,
                                       Handle<JSFunction> func) {
  v8::internal::Code* func_code = func->code();
  CheckRetAddrIsInFunction(
      func_name, ret_addr,
      reinterpret_cast<unsigned int>(func_code->instruction_start()),
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
 private:
  static unsigned int GetFP(const v8::Arguments& args);
  static const char* kSource;
};


const char* TraceExtension::kSource =
    "native function trace();"
    "native function js_trace();";


v8::Handle<v8::FunctionTemplate> TraceExtension::GetNativeFunction(
    v8::Handle<String> name) {
  if (name->Equals(String::New("trace"))) {
    return v8::FunctionTemplate::New(TraceExtension::Trace);
  } else if (name->Equals(String::New("js_trace"))) {
    return v8::FunctionTemplate::New(TraceExtension::JSTrace);
  } else {
    CHECK(false);
    return v8::Handle<v8::FunctionTemplate>();
  }
}


unsigned int TraceExtension::GetFP(const v8::Arguments& args) {
  CHECK_EQ(1, args.Length());
  unsigned int fp = args[0]->Int32Value() << 2;
  printf("Trace: %08x\n", fp);
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


static void CompileRun(const char* source) {
  Script::Compile(String::New(source))->Run();
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
                                       unsigned int ret_addr) {
  CheckRetAddrIsInJSFunction(func_name, ret_addr,
                             GetGlobalJSFunction(func_name));
}


static void SetGlobalProperty(const char* name, Local<Value> value) {
  env->Global()->Set(String::New(name), value);
}


static bool Patch(byte* from,
                  size_t num,
                  byte* original,
                  byte* patch,
                  size_t patch_len) {
  byte* to = from + num;
  do {
    from = static_cast<byte*>(memchr(from, *original, to - from));
    CHECK(from != NULL);
    if (memcmp(original, from, patch_len) == 0) {
      memcpy(from, patch, patch_len);
      return true;
    } else {
      from++;
    }
  } while (to - from > 0);
  return false;
}


// Creates a global function named 'func_name' that calls the tracing
// function 'trace_func_name' with an actual EBP register value,
// shifted right to be presented as Smi.
static void CreateTraceCallerFunction(const char* func_name,
                                      const char* trace_func_name) {
  ::v8::internal::EmbeddedVector<char, 256> trace_call_buf;
  ::v8::internal::OS::SNPrintF(trace_call_buf, "%s(0x6666);", trace_func_name);
  Handle<JSFunction> func = CompileFunction(trace_call_buf.start());
  CHECK(!func.is_null());
  v8::internal::Code* func_code = func->code();
  CHECK(func_code->IsCode());

  // push 0xcccc (= 0x6666 << 1)
  byte original[] = { 0x68, 0xcc, 0xcc, 0x00, 0x00 };
  // mov eax,ebp; shr eax; push eax;
  byte patch[] = { 0x89, 0xe8, 0xd1, 0xe8, 0x50 };
  // Patch generated code to replace pushing of a constant with
  // pushing of ebp contents in a Smi
  CHECK(Patch(func_code->instruction_start(),
              func_code->instruction_size(),
              original, patch, sizeof(patch)));

  SetGlobalProperty(func_name, v8::ToApi<Value>(func));
}


TEST(CFromJSStackTrace) {
  TickSample sample;
  StackTracer tracer(reinterpret_cast<unsigned int>(&sample));
  InitTraceEnv(&tracer, &sample);

  InitializeVM();
  v8::HandleScope scope;
  CreateTraceCallerFunction("JSFuncDoTrace", "trace");
  CompileRun(
      "function JSTrace() {"
      "  JSFuncDoTrace();"
      "};\n"
      "JSTrace();");
  CHECK_GT(sample.frames_count, 1);
  // Stack sampling will start from the first JS function, i.e. "JSFuncDoTrace"
  CheckRetAddrIsInJSFunction("JSFuncDoTrace",
                             reinterpret_cast<unsigned int>(sample.stack[0]));
  CheckRetAddrIsInJSFunction("JSTrace",
                             reinterpret_cast<unsigned int>(sample.stack[1]));
}


TEST(PureJSStackTrace) {
  TickSample sample;
  StackTracer tracer(reinterpret_cast<unsigned int>(&sample));
  InitTraceEnv(&tracer, &sample);

  InitializeVM();
  v8::HandleScope scope;
  CreateTraceCallerFunction("JSFuncDoTrace", "js_trace");
  CompileRun(
      "function JSTrace() {"
      "  JSFuncDoTrace();"
      "};\n"
      "function OuterJSTrace() {"
      "  JSTrace();"
      "};\n"
      "OuterJSTrace();");
  CHECK_GT(sample.frames_count, 1);
  // Stack sampling will start from the caller of JSFuncDoTrace, i.e. "JSTrace"
  CheckRetAddrIsInJSFunction("JSTrace",
                             reinterpret_cast<unsigned int>(sample.stack[0]));
  CheckRetAddrIsInJSFunction("OuterJSTrace",
                             reinterpret_cast<unsigned int>(sample.stack[1]));
}


static void CFuncDoTrace() {
  unsigned int fp;
#ifdef __GNUC__
  fp = reinterpret_cast<unsigned int>(__builtin_frame_address(0));
#elif defined _MSC_VER
  __asm mov [fp], ebp  // NOLINT
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
  StackTracer tracer(reinterpret_cast<unsigned int>(&sample));
  InitTraceEnv(&tracer, &sample);
  // Check that sampler doesn't crash
  CHECK_EQ(10, CFunc(10));
}


#endif  // ENABLE_LOGGING_AND_PROFILING
