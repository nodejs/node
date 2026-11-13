// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "include/v8-context.h"
#include "include/v8-data.h"
#include "include/v8-exception.h"
#include "include/v8-function.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-microtask-queue.h"
#include "include/v8-object.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-promise.h"
#include "include/v8-script.h"
#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/platform/platform.h"
#include "src/flags/flags.h"
#include "src/objects/module-inl.h"
#include "test/common/flag-utils.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace {

// Provide operator<< and AbslStringify for domain structs that implement
// Print(std::ostream&), matching test/unittests/wasm/*-fuzztest.cc.
template <typename T>
  requires requires(const T& t, std::ostream& os) { t.Print(os); }
std::ostream& operator<<(std::ostream& os, const T& t) {
  t.Print(os);
  return os;
}

template <typename Sink, typename T>
  requires requires(const T& t, std::ostream& os) { t.Print(os); }
void AbslStringify(Sink& sink, const T& t) {
  std::ostringstream os;
  t.Print(os);
  sink.Append(os.str());
}

enum class EdgeKind : uint8_t {
  kStaticNamed,
  kStaticLet,
  kStaticDefault,
  kStaticNamespace,
  kStaticDefaultAndNamespace,
  kStaticDefaultAndNamed,
  kStaticEmpty,
  kStaticQuoted,
  kStaticBare,
  kDeferNamespace,
  kSourceStatic,
  kExportStar,
  kExportNamed,
  kExportNamespace,
  kExportMissing,
  kExportCircular,
};

const char* EdgeKindName(EdgeKind kind) {
  switch (kind) {
    case EdgeKind::kStaticNamed:
      return "import{val}";
    case EdgeKind::kStaticLet:
      return "import{l_val}";
    case EdgeKind::kStaticDefault:
      return "import def";
    case EdgeKind::kStaticNamespace:
      return "import*as ns";
    case EdgeKind::kStaticDefaultAndNamespace:
      return "import def,*as ns";
    case EdgeKind::kStaticDefaultAndNamed:
      return "import def,{val}";
    case EdgeKind::kStaticEmpty:
      return "import{}";
    case EdgeKind::kStaticQuoted:
      return "import{'quoted name'}";
    case EdgeKind::kStaticBare:
      return "import";
    case EdgeKind::kDeferNamespace:
      return "import defer*as ns";
    case EdgeKind::kSourceStatic:
      return "import source";
    case EdgeKind::kExportStar:
      return "export*";
    case EdgeKind::kExportNamed:
      return "export{val}";
    case EdgeKind::kExportNamespace:
      return "export*as ns";
    case EdgeKind::kExportMissing:
      return "export{missing}";
    case EdgeKind::kExportCircular:
      return "export{circular_x}";
  }
  UNREACHABLE();
}

struct EdgeSpec {
  uint8_t target;
  EdgeKind kind;

  void Print(std::ostream& os) const {
    os << EdgeKindName(kind) << "->" << static_cast<int>(target);
  }
};

enum class DefaultExportKind : uint8_t {
  kFunction,
  kClass,
  kAsyncFunction,
  kGenerator,
  kIIFE,
  kConditionalThrowIIFE,
  kAliasFn,
  kObjectLiteral,
};

enum ExtraExportFlags : uint8_t {
  kNoExtraExports = 0,
  kExtraQuotedNames = 1 << 0,
  kExtraHoistedAndClass = 1 << 1,
  kExtraAsyncAndGenerator = 1 << 2,
  kExtraLateLetAndDestructuring = 1 << 3,
  kOmitBaseValExports = 1 << 4,
};

// Orthogonal control-flow / scope wrapper around statements in a module body.
enum class WrapperKind : uint8_t {
  kNone,
  kTryCatch,
  kTryFinally,
  kTryCatchFinally,
  kUsing,
  kAwaitUsing,
  kMicrotask,
  kForAwait,
  kOptimizedFn,
  kStaticBlock,
  kEval,
  kPrepareStackTrace,
  kObjectProtoThen,
  kPromiseThenPatch,
};

const char* WrapperKindName(WrapperKind kind) {
  switch (kind) {
    case WrapperKind::kNone:
      return "bare";
    case WrapperKind::kTryCatch:
      return "try_catch";
    case WrapperKind::kTryFinally:
      return "try_finally";
    case WrapperKind::kTryCatchFinally:
      return "try_catch_finally";
    case WrapperKind::kUsing:
      return "using";
    case WrapperKind::kAwaitUsing:
      return "await_using";
    case WrapperKind::kMicrotask:
      return "microtask";
    case WrapperKind::kForAwait:
      return "for_await";
    case WrapperKind::kOptimizedFn:
      return "opt_fn";
    case WrapperKind::kStaticBlock:
      return "static_block";
    case WrapperKind::kEval:
      return "eval";
    case WrapperKind::kPrepareStackTrace:
      return "prepare_stack_trace";
    case WrapperKind::kObjectProtoThen:
      return "object_proto_then";
    case WrapperKind::kPromiseThenPatch:
      return "promise_then_patch";
  }
  UNREACHABLE();
}

// Orthogonal atomic operations that can be placed inside any wrapper, handler,
// disposer, or thenable export.
enum class OpKind : uint8_t {
  kNop,
  kAwaitZero,
  kAwaitPromiseResolve,
  kAwaitPromiseReject,
  kAwaitPending,
  kAwaitResumable,
  kAwaitThenable,
  kAwaitDynamicImport,
  kAwaitDynamicImportDefer,
  kThrowError,
  kThrowDefaultFlag,
  kTerminate,
  kStackOverflow,
  kDynamicImport,
  kDynamicImportDefer,
  kDynamicImportSource,
  kBadDynamicImport,
  kNsGetVal,
  kNsReflectAndKeys,
  kNsMutateAndIntegrity,
  kNsSpreadAndProxy,
  kImportMeta,
  kTriggerGC,
};

const char* OpKindName(OpKind kind) {
  switch (kind) {
    case OpKind::kNop:
      return "nop";
    case OpKind::kAwaitZero:
      return "await_0";
    case OpKind::kAwaitPromiseResolve:
      return "await_resolve";
    case OpKind::kAwaitPromiseReject:
      return "await_reject";
    case OpKind::kAwaitPending:
      return "await_pending";
    case OpKind::kAwaitResumable:
      return "await_resumable";
    case OpKind::kAwaitThenable:
      return "await_thenable";
    case OpKind::kAwaitDynamicImport:
      return "await_import";
    case OpKind::kAwaitDynamicImportDefer:
      return "await_import_defer";
    case OpKind::kThrowError:
      return "throw";
    case OpKind::kThrowDefaultFlag:
      return "throw_default_flag";
    case OpKind::kTerminate:
      return "terminate";
    case OpKind::kStackOverflow:
      return "stack_overflow";
    case OpKind::kDynamicImport:
      return "import()";
    case OpKind::kDynamicImportDefer:
      return "import.defer()";
    case OpKind::kDynamicImportSource:
      return "import.source()";
    case OpKind::kBadDynamicImport:
      return "bad_import()";
    case OpKind::kNsGetVal:
      return "ns.val";
    case OpKind::kNsReflectAndKeys:
      return "ns_keys";
    case OpKind::kNsMutateAndIntegrity:
      return "ns_mutate";
    case OpKind::kNsSpreadAndProxy:
      return "ns_proxy";
    case OpKind::kImportMeta:
      return "import.meta";
    case OpKind::kTriggerGC:
      return "gc()";
  }
  UNREACHABLE();
}

struct StmtSpec {
  WrapperKind wrapper;
  OpKind primary_op;
  OpKind secondary_op;
  uint8_t target;

  void Print(std::ostream& os) const {
    os << WrapperKindName(wrapper) << "(" << OpKindName(primary_op) << ", "
       << OpKindName(secondary_op) << ", tgt=" << static_cast<int>(target)
       << ")";
  }
};

enum class ThenSettleKind : uint8_t {
  kNone,
  kResolve,
  kReject,
  kAsyncResolve,
};

struct ThenExportSpec {
  ThenSettleKind settle;
  OpKind side_effect;
  uint8_t target;

  void Print(std::ostream& os) const {
    if (settle == ThenSettleKind::kNone) {
      os << "no_then";
      return;
    }
    os << "then(settle=" << static_cast<int>(settle)
       << ", op=" << OpKindName(side_effect)
       << ", tgt=" << static_cast<int>(target) << ")";
  }
};

struct SourceModuleSpec {
  std::vector<EdgeSpec> edges;
  DefaultExportKind default_export;
  uint8_t extra_export_flags;
  ThenExportSpec then_export;
  std::vector<StmtSpec> statements;

  void Print(std::ostream& os) const {
    os << "Mod{edges=[";
    for (size_t i = 0; i < edges.size(); ++i) {
      if (i > 0) os << ", ";
      edges[i].Print(os);
    }
    os << "], def=" << static_cast<int>(default_export)
       << ", extra=" << static_cast<int>(extra_export_flags) << ", ";
    then_export.Print(os);
    os << ", stmts=[";
    for (size_t i = 0; i < statements.size(); ++i) {
      if (i > 0) os << ", ";
      statements[i].Print(os);
    }
    os << "]}";
  }
};

enum class SyntheticModuleKind : uint8_t {
  kFulfill,
  kThrow,
  kTerminate,
  kReentrantDeferAccess,
};

enum class DriverActionKind : uint8_t {
  kInstantiate,
  kEvaluate,
  kEvaluateForImportDefer,
  kEvaluateWithMicrotaskTerminate,
  kGetNamespaceAndAccess,
  kGetDeferNamespaceAndAccess,
  kGetDeferNamespaceAndAccessWithTerminate,
  kDynamicImportJS,
  kDynamicImportDeferJS,
  kDynamicImportSourceJS,
  kResumeTla,
  kDrainMicrotasks,
  kCheckStalledTlaAndGC,
};

const char* DriverActionKindName(DriverActionKind kind) {
  switch (kind) {
    case DriverActionKind::kInstantiate:
      return "Instantiate";
    case DriverActionKind::kEvaluate:
      return "Evaluate";
    case DriverActionKind::kEvaluateForImportDefer:
      return "EvaluateForImportDefer";
    case DriverActionKind::kEvaluateWithMicrotaskTerminate:
      return "EvaluateWithMicrotaskTerminate";
    case DriverActionKind::kGetNamespaceAndAccess:
      return "GetNamespaceAndAccess";
    case DriverActionKind::kGetDeferNamespaceAndAccess:
      return "GetDeferNamespaceAndAccess";
    case DriverActionKind::kGetDeferNamespaceAndAccessWithTerminate:
      return "GetDeferNamespaceAndAccessWithTerminate";
    case DriverActionKind::kDynamicImportJS:
      return "DynamicImportJS";
    case DriverActionKind::kDynamicImportDeferJS:
      return "DynamicImportDeferJS";
    case DriverActionKind::kDynamicImportSourceJS:
      return "DynamicImportSourceJS";
    case DriverActionKind::kResumeTla:
      return "ResumeTla";
    case DriverActionKind::kDrainMicrotasks:
      return "DrainMicrotasks";
    case DriverActionKind::kCheckStalledTlaAndGC:
      return "CheckStalledTlaAndGC";
  }
  UNREACHABLE();
}

struct DriverActionSpec {
  DriverActionKind kind;
  uint8_t target;

  void Print(std::ostream& os) const {
    os << DriverActionKindName(kind) << "(" << static_cast<int>(target) << ")";
  }
};

struct ModuleGraphSpec {
  std::vector<SourceModuleSpec> source_modules;
  std::vector<SyntheticModuleKind> synthetic_modules;
  std::vector<DriverActionSpec> actions;

  void Print(std::ostream& os) const {
    os << "ModuleGraphSpec{\n";
    for (size_t i = 0; i < source_modules.size(); ++i) {
      os << "  ./m" << i << ".mjs: " << source_modules[i] << "\n";
    }
    for (size_t i = 0; i < synthetic_modules.size(); ++i) {
      os << "  ./s" << i
         << ".syn: kind=" << static_cast<int>(synthetic_modules[i]) << "\n";
    }
    os << "  actions: [";
    for (size_t i = 0; i < actions.size(); ++i) {
      if (i > 0) os << ", ";
      os << actions[i];
    }
    os << "]\n}";
  }
};

// Enable required harmony flags before TestWithIsolate initializes the Isolate
// so Genesis registers the corresponding intrinsics on the context.
template <typename TMixin>
class WithModuleFeatureFlagsMixin : public TMixin {
 public:
  WithModuleFeatureFlagsMixin() {
    i::v8_flags.js_defer_import_eval.Set(true);
    i::v8_flags.js_source_phase_imports.Set(true);
    i::v8_flags.expose_gc.Set(true);
    i::v8_flags.allow_natives_syntax.Set(true);
  }

 private:
  i::SaveFlags save_flags_;
};

using TestModuleGraphWithIsolate = WithIsolateScopeMixin<WithIsolateMixin<
    WithDefaultPlatformMixin<WithModuleFeatureFlagsMixin<::testing::Test>>>>;

class ModuleGraphTest : public TestModuleGraphWithIsolate {
 public:
  ModuleGraphTest() {
    // Centipede has a default stack limit of 128kB. Lower V8's stack limit so
    // stack overflows are caught by V8's stack guard before Centipede crashes.
    static_assert(V8_DEFAULT_STACK_SIZE_KB > 100);
    isolate()->SetStackLimit(base::Stack::GetStackStart() - 100 * i::KB);

    isolate()->SetHostImportModuleDynamicallyCallback(
        HostImportModuleDynamicallyCallback);
    isolate()->SetHostImportModuleWithPhaseDynamicallyCallback(
        HostImportModuleWithPhaseDynamicallyCallback);
    isolate()->SetHostInitializeImportMetaObjectCallback(
        HostInitializeImportMetaObjectCallback);
  }

  ~ModuleGraphTest() override {
    module_map_.clear();
    if (active_instance_ == this) {
      active_instance_ = nullptr;
    }
  }

  void FuzzModuleGraph(const ModuleGraphSpec& spec);

 private:
  static ModuleGraphTest* active_instance_;

  struct ExportSnapshot {
    int num_elements;
    bool has_eval_ns;
    bool has_defer_ns;
    bool finalized() const { return has_eval_ns || has_defer_ns; }
  };

  bool allow_terminate_ = false;
  int dynamic_import_count_ = 0;
  static constexpr int kMaxDynamicImportsPerIteration = 16;
  std::vector<std::string> module_names_;
  std::map<std::string, Global<v8::Module>> module_map_;
  mutable std::map<std::string, ExportSnapshot> export_snapshots_;

  std::string TargetName(uint8_t idx) const {
    if (module_names_.empty()) return "./m0.mjs";
    return module_names_[idx % module_names_.size()];
  }

  std::string SourceTargetName(uint8_t idx, size_t num_source) const {
    if (num_source == 0) return "./m0.mjs";
    return "./m" + std::to_string(idx % num_source) + ".mjs";
  }

  std::string EmitOp(OpKind op, size_t mod_idx, size_t stmt_id, uint8_t target,
                     const std::vector<std::string>& ns_vars,
                     bool allow_await) const;

  std::string EmitStmt(const StmtSpec& stmt, size_t mod_idx, size_t stmt_id,
                       const std::vector<std::string>& ns_vars) const;

  std::string BuildModuleSource(size_t mod_idx, const SourceModuleSpec& m,
                                size_t num_source) const;

  static bool HasUnsupportedImportAttributes(
      v8::Isolate* isolate, Local<v8::FixedArray> import_attributes) {
    if (import_attributes.IsEmpty() || import_attributes->Length() == 0) {
      return false;
    }
    v8::String::Utf8Value key(isolate, import_attributes->Get(0).As<Value>());
    v8::String::Utf8Value val(isolate, import_attributes->Get(1).As<Value>());
    return (*key && std::string(*key) != "type") ||
           (*val && std::string(*val) != "json");
  }

  static void TerminateCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    if (active_instance_ && active_instance_->allow_terminate_) {
      reinterpret_cast<i::Isolate*>(info.GetIsolate())->TerminateExecution();
    }
  }

  static MaybeLocal<v8::Module> ResolveModuleCallback(
      Local<Context> context, Local<v8::String> specifier,
      Local<v8::FixedArray> import_attributes, Local<v8::Module> referrer);

  static MaybeLocal<v8::Object> ResolveModuleSourceCallback(
      Local<Context> context, Local<v8::String> specifier,
      Local<v8::FixedArray> import_attributes, Local<v8::Module> referrer);

  static void HostInitializeImportMetaObjectCallback(Local<Context> context,
                                                     Local<v8::Module> module,
                                                     Local<v8::Object> meta);

  static MaybeLocal<v8::Promise> HostImportModuleDynamicallyCallback(
      Local<Context> context, Local<Data> host_defined_options,
      Local<Value> resource_name, Local<v8::String> specifier,
      Local<v8::FixedArray> import_attributes);

  static MaybeLocal<v8::Promise> HostImportModuleWithPhaseDynamicallyCallback(
      Local<Context> context, Local<Data> host_defined_options,
      Local<Value> resource_name, Local<v8::String> specifier,
      ModuleImportPhase phase, Local<v8::FixedArray> import_attributes);

  static void DoDynamicImportMicrotask(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  static void ModuleResolutionSuccessCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  static void ModuleResolutionFailureCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  static MaybeLocal<v8::Promise> SyntheticModuleEvaluationSteps(
      Local<Context> context, Local<v8::Module> module);

  bool EnsureInstantiatedAndBindNamespace(Local<Context> context,
                                          Local<v8::Module> target_mod,
                                          ModuleImportPhase phase,
                                          const char* global_name);
  void VerifyAllModules() const;
  void RunScriptIgnoringExceptions(Local<Context> context,
                                   const std::string& source);
};

ModuleGraphTest* ModuleGraphTest::active_instance_ = nullptr;

MaybeLocal<v8::Module> ModuleGraphTest::ResolveModuleCallback(
    Local<Context> context, Local<v8::String> specifier,
    Local<v8::FixedArray> import_attributes, Local<v8::Module> referrer) {
  v8::Isolate* isolate = Isolate::GetCurrent();
  if (!active_instance_) {
    isolate->ThrowError("No active instance");
    return MaybeLocal<v8::Module>();
  }
  if (HasUnsupportedImportAttributes(isolate, import_attributes)) {
    isolate->ThrowError("Unsupported import attribute");
    return MaybeLocal<v8::Module>();
  }
  v8::String::Utf8Value spec_utf8(isolate, specifier);
  std::string key = *spec_utf8 ? *spec_utf8 : "";
  auto it = active_instance_->module_map_.find(key);
  if (it == active_instance_->module_map_.end()) {
    isolate->ThrowError("Module not found");
    return MaybeLocal<v8::Module>();
  }
  return it->second.Get(isolate);
}

MaybeLocal<v8::Object> ModuleGraphTest::ResolveModuleSourceCallback(
    Local<Context> context, Local<v8::String> specifier,
    Local<v8::FixedArray> import_attributes, Local<v8::Module> referrer) {
  v8::Isolate* isolate = Isolate::GetCurrent();
  if (!active_instance_ ||
      HasUnsupportedImportAttributes(isolate, import_attributes)) {
    isolate->ThrowError("Unsupported import attribute");
    return MaybeLocal<v8::Object>();
  }
  v8::String::Utf8Value spec_utf8(isolate, specifier);
  std::string key = *spec_utf8 ? *spec_utf8 : "";
  if (key.find(".syn") != std::string::npos ||
      active_instance_->module_map_.find(key) ==
          active_instance_->module_map_.end()) {
    isolate->ThrowException(v8::Exception::SyntaxError(
        v8::String::NewFromUtf8Literal(isolate, "Source phase not supported")));
    return MaybeLocal<v8::Object>();
  }
  return v8::Object::New(isolate);
}

void ModuleGraphTest::HostInitializeImportMetaObjectCallback(
    Local<Context> context, Local<v8::Module> module, Local<v8::Object> meta) {
  v8::Isolate* isolate = Isolate::GetCurrent();
  Local<v8::String> url_key = v8::String::NewFromUtf8Literal(isolate, "url");
  USE(meta->CreateDataProperty(context, url_key, module->GetResourceName()));
}

void ModuleGraphTest::ModuleResolutionSuccessCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Array> data = info.DataV2().As<Value>().As<v8::Array>();
  Local<Value> resolver_val;
  Local<Value> ns_val;
  if (!data->Get(context, 0).ToLocal(&resolver_val) ||
      !data->Get(context, 1).ToLocal(&ns_val)) {
    return;
  }
  USE(resolver_val.As<v8::Promise::Resolver>()->Resolve(context, ns_val));
}

void ModuleGraphTest::ModuleResolutionFailureCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Array> data = info.DataV2().As<Value>().As<v8::Array>();
  Local<Value> resolver_val;
  if (!data->Get(context, 0).ToLocal(&resolver_val)) {
    return;
  }
  Local<Value> reason =
      info.Length() > 0 ? info[0] : v8::Undefined(isolate).As<Value>();
  USE(resolver_val.As<v8::Promise::Resolver>()->Reject(context, reason));
}

void ModuleGraphTest::DoDynamicImportMicrotask(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  if (!active_instance_ || isolate->IsExecutionTerminating()) return;

  HandleScope handle_scope(isolate);
  Local<Context> context = isolate->GetCurrentContext();
  Local<v8::Array> data = info.DataV2().As<Value>().As<v8::Array>();

  Local<Value> spec_val;
  Local<Value> phase_val;
  Local<Value> invalid_attrs_val;
  Local<Value> resolver_val;
  if (!data->Get(context, 0).ToLocal(&spec_val) ||
      !data->Get(context, 1).ToLocal(&phase_val) ||
      !data->Get(context, 2).ToLocal(&invalid_attrs_val) ||
      !data->Get(context, 3).ToLocal(&resolver_val)) {
    return;
  }

  Local<v8::Promise::Resolver> resolver =
      resolver_val.As<v8::Promise::Resolver>();
  ModuleImportPhase phase =
      static_cast<ModuleImportPhase>(phase_val.As<v8::Int32>()->Value());
  bool invalid_attrs = invalid_attrs_val->BooleanValue(isolate);

  if (invalid_attrs) {
    USE(resolver->Reject(context,
                         v8::Exception::Error(v8::String::NewFromUtf8Literal(
                             isolate, "Unsupported import attribute"))));
    return;
  }

  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  v8::TryCatch try_catch(isolate);
  auto reject_from_try_catch = [&]() {
    if (isolate->IsExecutionTerminating()) {
      i_isolate->TerminateExecution();
    } else if (try_catch.HasCaught()) {
      Local<Value> exception = try_catch.Exception();
      try_catch.Reset();
      USE(resolver->Reject(context, exception));
    }
  };

  if (phase == ModuleImportPhase::kSource) {
    Local<v8::Object> source_obj;
    if (!ResolveModuleSourceCallback(context, spec_val.As<v8::String>(),
                                     Local<v8::FixedArray>(),
                                     Local<v8::Module>())
             .ToLocal(&source_obj)) {
      reject_from_try_catch();
      return;
    }
    USE(resolver->Resolve(context, source_obj));
    return;
  }

  Local<v8::Module> root_module;
  if (!ResolveModuleCallback(context, spec_val.As<v8::String>(),
                             Local<v8::FixedArray>(), Local<v8::Module>())
           .ToLocal(&root_module)) {
    reject_from_try_catch();
    return;
  }

  if (!root_module
           ->InstantiateModule(context, ResolveModuleCallback,
                               ResolveModuleSourceCallback)
           .FromMaybe(false)) {
    reject_from_try_catch();
    return;
  }

  MaybeLocal<v8::Promise> maybe_result_promise;
  if (phase == ModuleImportPhase::kEvaluation) {
    maybe_result_promise = root_module->Evaluate(context);
  } else {
    DCHECK_EQ(phase, ModuleImportPhase::kDefer);
    if (!isolate->IsExecutionTerminating()) {
      maybe_result_promise = root_module->EvaluateForImportDefer(context);
    }
  }

  Local<v8::Promise> result_promise;
  if (!maybe_result_promise.ToLocal(&result_promise)) {
    reject_from_try_catch();
    return;
  }

  Local<Value> module_ns = root_module->GetModuleNamespace(phase);
  Local<v8::Array> resolution_data = v8::Array::New(isolate, 2);
  if (resolution_data->SetPrototype(context, v8::Null(isolate)).IsNothing() ||
      resolution_data->Set(context, 0, resolver).IsNothing() ||
      resolution_data->Set(context, 1, module_ns).IsNothing()) {
    if (isolate->IsExecutionTerminating()) i_isolate->TerminateExecution();
    return;
  }

  Local<v8::Function> callback_success;
  Local<v8::Function> callback_failure;
  if (!v8::Function::New(context, ModuleResolutionSuccessCallback,
                         resolution_data)
           .ToLocal(&callback_success) ||
      !v8::Function::New(context, ModuleResolutionFailureCallback,
                         resolution_data)
           .ToLocal(&callback_failure) ||
      result_promise->Then(context, callback_success, callback_failure)
          .IsEmpty()) {
    reject_from_try_catch();
  }
}

MaybeLocal<v8::Promise> ModuleGraphTest::HostImportModuleDynamicallyCallback(
    Local<Context> context, Local<Data> host_defined_options,
    Local<Value> resource_name, Local<v8::String> specifier,
    Local<v8::FixedArray> import_attributes) {
  return HostImportModuleWithPhaseDynamicallyCallback(
      context, host_defined_options, resource_name, specifier,
      ModuleImportPhase::kEvaluation, import_attributes);
}

MaybeLocal<v8::Promise>
ModuleGraphTest::HostImportModuleWithPhaseDynamicallyCallback(
    Local<Context> context, Local<Data> host_defined_options,
    Local<Value> resource_name, Local<v8::String> specifier,
    ModuleImportPhase phase, Local<v8::FixedArray> import_attributes) {
  v8::Isolate* isolate = Isolate::GetCurrent();
  if (isolate->IsExecutionTerminating()) return MaybeLocal<v8::Promise>();

  Local<v8::Promise::Resolver> resolver;
  if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) {
    return MaybeLocal<v8::Promise>();
  }

  if (!active_instance_ || ++active_instance_->dynamic_import_count_ >
                               kMaxDynamicImportsPerIteration) {
    USE(resolver->Reject(context,
                         v8::Exception::Error(v8::String::NewFromUtf8Literal(
                             isolate, "Dynamic import limit reached"))));
    return resolver->GetPromise();
  }

  bool invalid_attrs =
      HasUnsupportedImportAttributes(isolate, import_attributes);

  Local<v8::Array> data = v8::Array::New(isolate, 4);
  if (data->SetPrototype(context, v8::Null(isolate)).IsNothing() ||
      data->Set(context, 0, specifier).IsNothing() ||
      data->Set(context, 1, v8::Int32::New(isolate, static_cast<int>(phase)))
          .IsNothing() ||
      data->Set(context, 2, v8::Boolean::New(isolate, invalid_attrs))
          .IsNothing() ||
      data->Set(context, 3, resolver).IsNothing()) {
    return MaybeLocal<v8::Promise>();
  }

  Local<v8::Function> microtask_fn;
  if (!v8::Function::New(context, DoDynamicImportMicrotask, data)
           .ToLocal(&microtask_fn)) {
    return MaybeLocal<v8::Promise>();
  }
  context->GetMicrotaskQueue()->EnqueueMicrotask(isolate, microtask_fn);
  return resolver->GetPromise();
}

MaybeLocal<v8::Promise> ModuleGraphTest::SyntheticModuleEvaluationSteps(
    Local<Context> context, Local<v8::Module> module) {
  v8::Isolate* isolate = Isolate::GetCurrent();
  i::Isolate* i_isolate = reinterpret_cast<i::Isolate*>(isolate);
  SyntheticModuleKind kind = SyntheticModuleKind::kFulfill;
  Local<Data> host_options = module->GetSyntheticModuleHostDefinedOptions();
  if (!host_options.IsEmpty() && host_options->IsValue() &&
      host_options.As<Value>()->IsInt32()) {
    kind =
        static_cast<SyntheticModuleKind>(host_options.As<v8::Int32>()->Value());
  }

  Local<v8::String> default_name =
      v8::String::NewFromUtf8Literal(isolate, "default");
  Local<v8::String> val_name = v8::String::NewFromUtf8Literal(isolate, "val");
  USE(module->SetSyntheticModuleExport(isolate, default_name,
                                       v8::Number::New(isolate, 42)));
  USE(module->SetSyntheticModuleExport(isolate, val_name,
                                       v8::Number::New(isolate, 42)));

  auto resolve_undefined = [&]() -> MaybeLocal<v8::Promise> {
    Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(context).ToLocal(&resolver)) {
      return MaybeLocal<v8::Promise>();
    }
    USE(resolver->Resolve(context, v8::Undefined(isolate)));
    return resolver->GetPromise();
  };

  switch (kind) {
    case SyntheticModuleKind::kFulfill:
      return resolve_undefined();
    case SyntheticModuleKind::kThrow:
      isolate->ThrowException(v8::Exception::Error(
          v8::String::NewFromUtf8Literal(isolate, "synthetic throw")));
      return MaybeLocal<v8::Promise>();
    case SyntheticModuleKind::kTerminate:
      if (active_instance_ && active_instance_->allow_terminate_) {
        i_isolate->TerminateExecution();
        return MaybeLocal<v8::Promise>();
      }
      return resolve_undefined();
    case SyntheticModuleKind::kReentrantDeferAccess:
      if (active_instance_) {
        auto it = active_instance_->module_map_.find("./m0.mjs");
        if (it != active_instance_->module_map_.end()) {
          Local<v8::Module> m0 = it->second.Get(isolate);
          if (m0->GetStatus() >= v8::Module::kInstantiated &&
              m0->GetStatus() != v8::Module::kEvaluating) {
            Local<Value> ns = m0->GetModuleNamespace(ModuleImportPhase::kDefer);
            v8::TryCatch tc(isolate);
            USE(ns.As<v8::Object>()->Get(context, val_name));
            if (isolate->IsExecutionTerminating()) {
              i_isolate->TerminateExecution();
              return MaybeLocal<v8::Promise>();
            }
          }
        }
      }
      return resolve_undefined();
  }
  UNREACHABLE();
}

std::string ModuleGraphTest::EmitOp(OpKind op, size_t mod_idx, size_t stmt_id,
                                    uint8_t target,
                                    const std::vector<std::string>& ns_vars,
                                    bool allow_await) const {
  std::string tgt = TargetName(target);
  std::string ns = ns_vars.empty() ? "" : ns_vars[target % ns_vars.size()];

  // Helper to emit an expression with `await` when in an async/top-level
  // context, or as a detached promise reaction in a synchronous context.
  auto maybe_await = [&](const std::string& promise_expr) -> std::string {
    if (allow_await) {
      return "await " + promise_expr + ";";
    }
    return promise_expr + ".then(() => {}, e => {});";
  };

  switch (op) {
    case OpKind::kNop:
      return ";";
    case OpKind::kAwaitZero:
      return allow_await ? "await 0;" : "Promise.resolve(0);";
    case OpKind::kAwaitPromiseResolve:
      return maybe_await("Promise.resolve(1)");
    case OpKind::kAwaitPromiseReject:
      return maybe_await("Promise.reject(new Error('reject'))");
    case OpKind::kAwaitPending:
      return maybe_await("new Promise(() => {})");
    case OpKind::kAwaitResumable:
      return maybe_await(
          "new Promise(r => { (globalThis.__resumables ??= "
          "[]).push(r); })");
    case OpKind::kAwaitThenable:
      return maybe_await("({ then(r) { terminate(); r(1); } })");
    case OpKind::kAwaitDynamicImport:
      return maybe_await("import('" + tgt + "')");
    case OpKind::kAwaitDynamicImportDefer:
      if (allow_await) {
        return "{ const d = await import.defer('" + tgt +
               "'); d.then; d.val; }";
      }
      return "import.defer('" + tgt +
             "').then(d => { try { d.val; } catch (e) {} }, e => {});";
    case OpKind::kThrowError:
      return "throw new Error('boom_m" + std::to_string(mod_idx) + "');";
    case OpKind::kThrowDefaultFlag:
      return std::string("globalThis.__throwDef = 1; ") +
             (allow_await ? "await 0;" : "");
    case OpKind::kTerminate:
      return "terminate();";
    case OpKind::kStackOverflow:
      return "(function overflow(a) { return overflow(a + 1); })(0);";
    case OpKind::kDynamicImport:
      return "import('" + tgt + "').then(x => x.val, e => {});";
    case OpKind::kDynamicImportDefer:
      return "import.defer('" + tgt +
             "').then(x => { try { x.val; } catch (e) {} }, e => {});";
    case OpKind::kDynamicImportSource:
      return "import.source('" + tgt + "').then(() => {}, e => {});";
    case OpKind::kBadDynamicImport:
      return "import({ toString() { throw new Error('spec'); } }).then(() => "
             "{}, e => {}); "
             "import('" +
             tgt +
             "', { with: { unsupported: 'x' } }).then(() => {}, e => {}); "
             "import('./does_not_exist.mjs').then(() => {}, e => {});";
    case OpKind::kNsGetVal:
      if (ns.empty()) return ";";
      return "if (typeof globalThis.__accessNs === 'function') "
             "globalThis.__accessNs(" +
             ns + ", 'mod" + std::to_string(mod_idx) + "_" + ns + "');";
    case OpKind::kNsReflectAndKeys:
      if (ns.empty()) return ";";
      return "Object.keys(" + ns + "); for (const k of Reflect.ownKeys(" + ns +
             ")) { try { " + ns +
             "[k]; } catch (e) {} try { Object.getOwnPropertyDescriptor(" + ns +
             ", k); } catch (e) {} } Reflect.getOwnPropertyDescriptor(" + ns +
             ", 'val'); Reflect.getOwnPropertyDescriptor(" + ns +
             ", 'then'); Reflect.getOwnPropertyDescriptor(" + ns +
             ", Symbol.toStringTag); for (const k in " + ns + ") break;";
    case OpKind::kNsMutateAndIntegrity:
      if (ns.empty()) return ";";
      return "try { Object.defineProperty(" + ns + ", 'val', { value: " + ns +
             ".val }); } catch (e) {} "
             "try { Object.defineProperty(" +
             ns +
             ", 'val', { value: -1 }); } catch (e) {} "
             "try { delete " +
             ns + ".val; delete " + ns + ".nope; Reflect.set(" + ns +
             ", 'val', 1); } catch (e) {} "
             "try { Object.preventExtensions(" +
             ns + "); Object.isFrozen(" + ns + "); Object.isSealed(" + ns +
             "); Object.setPrototypeOf(" + ns + ", null); } catch (e) {}";
    case OpKind::kNsSpreadAndProxy:
      if (ns.empty()) return ";";
      return "try { JSON.stringify(" + ns + "); ({ ..." + ns +
             " }); new Proxy(" + ns +
             ", { get: (t, k) => Reflect.get(t, k) }).val; 'then' in " + ns +
             "; 'val' in " + ns + "; " + ns +
             "[Symbol.toStringTag]; } catch (e) {}";
    case OpKind::kImportMeta:
      return "import.meta.url; Object.keys(import.meta); import.meta.x = 1;";
    case OpKind::kTriggerGC:
      return "if (typeof gc === 'function') gc();";
  }
  UNREACHABLE();
}

std::string ModuleGraphTest::EmitStmt(
    const StmtSpec& stmt, size_t mod_idx, size_t stmt_id,
    const std::vector<std::string>& ns_vars) const {
  std::ostringstream out;
  switch (stmt.wrapper) {
    case WrapperKind::kNone:
      out << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << "\n";
      break;
    case WrapperKind::kTryCatch:
      out << "try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } catch (e) { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kTryFinally:
      out << "try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } finally { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kTryCatchFinally:
      out << "try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } catch (e) {} finally { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kUsing:
      out << "{ using u_" << stmt_id << " = { [Symbol.dispose]() { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } }; "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kAwaitUsing:
      out << "{ await using u_" << stmt_id
          << " = { async [Symbol.asyncDispose]() { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } }; "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kMicrotask:
      out << "Promise.resolve().then(async () => { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }); "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << "\n";
      break;
    case WrapperKind::kForAwait:
      out << "for await (const x of (async function* () { yield 1; "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " })()) { "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " }\n";
      break;
    case WrapperKind::kOptimizedFn: {
      std::string fn =
          "opt_m" + std::to_string(mod_idx) + "_" + std::to_string(stmt_id);
      out << "function " << fn << "() { try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } catch (e) { return e; } } %PrepareFunctionForOptimization("
          << fn << "); " << fn << "(); " << fn
          << "(); %OptimizeFunctionOnNextCall(" << fn << "); " << fn << "();\n";
      break;
    }
    case WrapperKind::kStaticBlock:
      out << "class C_" << stmt_id << " { static { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } }\n";
      break;
    case WrapperKind::kEval: {
      std::string tgt = TargetName(stmt.target);
      out << "(0, eval)(\"import('" << tgt
          << "').then(m => m.val, e => {})\"); "
             "try { eval(\"import.meta\"); } catch (e) {} "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << "\n";
      break;
    }
    case WrapperKind::kPrepareStackTrace:
      out << "{ Error.prepareStackTrace = (e, s) => { if (!globalThis.__pst) { "
             "globalThis.__pst = 1; try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } catch (e) {} finally { globalThis.__pst = 0; } } return "
             "s.length; }; try { new Error('x').stack; } finally { delete "
             "Error.prepareStackTrace; } }\n";
      break;
    case WrapperKind::kObjectProtoThen:
      out << "try { Object.defineProperty(Object.prototype, 'then', { "
             "configurable: true, get() { delete Object.prototype.then; try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } catch (e) {} return undefined; } }); await {}; "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } finally { delete Object.prototype.then; }\n";
      break;
    case WrapperKind::kPromiseThenPatch:
      out << "{ const __origThen = Promise.prototype.then; try { "
             "Promise.prototype.then = function(a, b) { Promise.prototype.then "
             "= __origThen; try { "
          << EmitOp(stmt.primary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/false)
          << " } catch (e) {} return __origThen.call(this, a, b); }; "
          << EmitOp(stmt.secondary_op, mod_idx, stmt_id, stmt.target, ns_vars,
                    /*allow_await=*/true)
          << " } finally { Promise.prototype.then = __origThen; } }\n";
      break;
  }
  return out.str();
}

std::string ModuleGraphTest::BuildModuleSource(size_t mod_idx,
                                               const SourceModuleSpec& m,
                                               size_t num_source) const {
  std::ostringstream head;
  std::ostringstream body;
  std::vector<std::string> ns_vars;

  head << "export const m" << mod_idx << "_val = " << (mod_idx + 1) << ";\n";
  if (m.extra_export_flags & kOmitBaseValExports) {
    head << "const val = " << (mod_idx + 1) << ";\n";
    head << "let l_val = " << (mod_idx + 10) << ";\n";
    head << "function fn() { return l_val; }\n";
    head << "export * from '" << SourceTargetName(mod_idx + 1, num_source)
         << "';\n";
  } else {
    head << "export const val = " << (mod_idx + 1) << ";\n";
    head << "export let l_val = " << (mod_idx + 10) << ";\n";
    head << "export function fn() { return l_val; }\n";
    head << "export { val as \"quoted name\", l_val as \"another one\" };\n";
  }

  // Edges (must be generated first so ns_vars is populated for
  // then/statements).
  size_t eid = 0;
  bool has_circular_export = false;
  for (const EdgeSpec& edge : m.edges) {
    ++eid;
    std::string dst = TargetName(edge.target);
    bool is_synthetic = dst.find(".syn") != std::string::npos;
    switch (edge.kind) {
      case EdgeKind::kStaticNamed:
        head << "import { val as val_" << eid << " } from '" << dst << "';\n";
        body << "try { val_" << eid << "; } catch (e) {}\n";
        break;
      case EdgeKind::kStaticLet:
        if (is_synthetic) {
          head << "import { val as lv_" << eid << " } from '" << dst << "';\n";
        } else {
          head << "import { l_val as lv_" << eid << " } from '" << dst
               << "';\n";
        }
        body << "try { lv_" << eid << "; } catch (e) {}\n";
        break;
      case EdgeKind::kStaticDefault:
        head << "import def_" << eid << " from '" << dst << "';\n";
        body << "try { typeof def_" << eid << " === 'function' ? def_" << eid
             << "() : def_" << eid << "; } catch (e) {}\n";
        break;
      case EdgeKind::kStaticNamespace:
        head << "import * as ns_" << eid << " from '" << dst << "';\n";
        ns_vars.push_back("ns_" + std::to_string(eid));
        break;
      case EdgeKind::kStaticDefaultAndNamespace:
        head << "import def_" << eid << ", * as ns_" << eid << " from '" << dst
             << "';\n";
        ns_vars.push_back("ns_" + std::to_string(eid));
        body << "try { def_" << eid << "; } catch (e) {}\n";
        break;
      case EdgeKind::kStaticDefaultAndNamed:
        if (is_synthetic) {
          head << "import def_" << eid << ", { val as val_" << eid
               << " } from '" << dst << "';\n";
          body << "try { val_" << eid << "; } catch (e) {}\n";
        } else {
          head << "import def_" << eid << ", { val as val_" << eid
               << ", fn as fn_" << eid << " } from '" << dst << "';\n";
          body << "try { fn_" << eid << "(); val_" << eid
               << "; } catch (e) {}\n";
        }
        break;
      case EdgeKind::kStaticEmpty:
        head << "import {} from '" << dst << "';\n";
        break;
      case EdgeKind::kStaticQuoted:
        if (is_synthetic) {
          head << "import { val as q_" << eid << " } from '" << dst << "';\n";
          body << "try { q_" << eid << "; } catch (e) {}\n";
        } else {
          head << "import { \"quoted name\" as q_" << eid
               << ", \"another one\" as q2_" << eid << " } from '" << dst
               << "';\n";
          body << "try { q_" << eid << "; q2_" << eid << "; } catch (e) {}\n";
        }
        break;
      case EdgeKind::kStaticBare:
        head << "import '" << dst << "';\n";
        break;
      case EdgeKind::kDeferNamespace:
        head << "import defer * as defNs_" << eid << " from '" << dst << "';\n";
        ns_vars.push_back("defNs_" + std::to_string(eid));
        break;
      case EdgeKind::kSourceStatic:
        head << "import source src_" << eid << " from '"
             << ((edge.target & 7) == 7
                     ? dst
                     : SourceTargetName(edge.target, num_source))
             << "';\n";
        body << "try { src_" << eid << "; } catch (e) {}\n";
        break;
      case EdgeKind::kExportStar:
        head << "export * from '" << dst << "';\n";
        break;
      case EdgeKind::kExportNamed:
        head << "export { val as re_val_" << eid << " } from '" << dst
             << "';\n";
        break;
      case EdgeKind::kExportNamespace:
        head << "export * as re_ns_" << eid << " from '" << dst << "';\n";
        break;
      case EdgeKind::kExportMissing:
        if ((edge.target & 3) == 0) {
          head << "export { missing_symbol_" << eid << " } from '" << dst
               << "';\n";
        } else {
          head << "export { default as re_def_" << eid << " } from '" << dst
               << "';\n";
        }
        break;
      case EdgeKind::kExportCircular:
        if (!has_circular_export) {
          has_circular_export = true;
          head << "export { circular_x } from '"
               << SourceTargetName(edge.target, num_source) << "';\n";
        } else {
          head << "export { val as re_val_" << eid << " } from '" << dst
               << "';\n";
        }
        break;
    }
  }

  if (!has_circular_export && !(m.extra_export_flags & kOmitBaseValExports)) {
    head << "export const circular_x = " << (mod_idx + 1) << ";\n";
  }

  // Composable thenable namespace export.
  if (m.then_export.settle != ThenSettleKind::kNone) {
    head << "export function then(resolve, reject) { "
         << "if ((globalThis.__thenDepth = (globalThis.__thenDepth || 0) + 1) "
            "<= 4) { try { "
         << EmitOp(m.then_export.side_effect, mod_idx, 0, m.then_export.target,
                   ns_vars, /*allow_await=*/false)
         << " } finally { --globalThis.__thenDepth; } } else { "
            "--globalThis.__thenDepth; } ";
    switch (m.then_export.settle) {
      case ThenSettleKind::kNone:
        UNREACHABLE();
      case ThenSettleKind::kResolve:
        head << "if (typeof resolve === 'function') resolve(42); }\n";
        break;
      case ThenSettleKind::kReject:
        head << "if (typeof reject === 'function') reject(new Error('then "
                "rejects')); }\n";
        break;
      case ThenSettleKind::kAsyncResolve:
        head << "Promise.resolve().then(() => { if (typeof resolve === "
                "'function') resolve(42); }); }\n";
        break;
    }
  }

  // Default export.
  switch (m.default_export) {
    case DefaultExportKind::kFunction:
      head << "export default function defFn() { return 'default_m" << mod_idx
           << "'; }\n";
      break;
    case DefaultExportKind::kClass:
      head << "export default class { static v = 1; }\n";
      break;
    case DefaultExportKind::kAsyncFunction:
      head << "export default async function () { await 0; return 1; }\n";
      break;
    case DefaultExportKind::kGenerator:
      head << "export default function* () { yield 1; }\n";
      break;
    case DefaultExportKind::kIIFE:
      head << "export default (() => ({ defVal: 99 }))();\n";
      break;
    case DefaultExportKind::kConditionalThrowIIFE:
      head << "export default (() => { if (globalThis.__throwDef) throw new "
              "Error('default throws'); return 1; })();\n";
      break;
    case DefaultExportKind::kAliasFn:
      head << "export { fn as default };\n";
      break;
    case DefaultExportKind::kObjectLiteral:
      head << "export default { defVal: 99 };\n";
      break;
  }

  // Extra export declarations.
  if (m.extra_export_flags & kExtraQuotedNames) {
    head << "export { fn as \"extra quoted\" };\n";
  }
  if (m.extra_export_flags & kExtraHoistedAndClass) {
    head << "export var hoisted = 1;\nexport class Klass { static s = 1; }\n";
  }
  if (m.extra_export_flags & kExtraAsyncAndGenerator) {
    head << "export async function afn() { await 0; }\n"
         << "export function* gen() { yield 1; }\n";
  }
  if (m.extra_export_flags & kExtraLateLetAndDestructuring) {
    head << "export let late; late = 1;\n"
         << "export const { d1, d2 = 2 } = { d1: 1 };\n";
  }

  // Composable statements.
  for (size_t i = 0; i < m.statements.size(); ++i) {
    body << EmitStmt(m.statements[i], mod_idx, i + 1, ns_vars);
  }

  return head.str() + body.str();
}

bool ModuleGraphTest::EnsureInstantiatedAndBindNamespace(
    Local<Context> context, Local<v8::Module> target_mod,
    ModuleImportPhase phase, const char* global_name) {
  v8::TryCatch try_catch(isolate());
  if (target_mod->GetStatus() < v8::Module::kInstantiated) {
    USE(target_mod->InstantiateModule(context, ResolveModuleCallback,
                                      ResolveModuleSourceCallback));
  }
  if (target_mod->GetStatus() < v8::Module::kInstantiated ||
      isolate()->IsExecutionTerminating()) {
    return false;
  }
  Local<Value> ns = target_mod->GetModuleNamespace(phase);
  Local<v8::String> key =
      v8::String::NewFromUtf8(isolate(), global_name).ToLocalChecked();
  return context->Global()->Set(context, key, ns).FromMaybe(false) &&
         !isolate()->IsExecutionTerminating();
}

void ModuleGraphTest::VerifyAllModules() const {
  bool any_new_ns = false;
  for (const auto& pair : module_map_) {
    Local<v8::Module> mod = pair.second.Get(isolate());
    i::DirectHandle<i::Module> i_mod = v8::Utils::OpenDirectHandle(*mod);
    if (i_mod->status() < i::Module::kLinked) continue;
    bool has_eval_ns =
        !i::IsUndefined(i_mod->module_namespace()) &&
        i::IsJSModuleNamespace(
            i::Cast<i::Cell>(i_mod->module_namespace())->value());
    bool has_defer_ns =
        !i::IsUndefined(i_mod->deferred_module_namespace()) &&
        i::IsJSModuleNamespace(
            i::Cast<i::Cell>(i_mod->deferred_module_namespace())->value());
    auto it = export_snapshots_.find(pair.first);
    if (it == export_snapshots_.end()) {
      if (has_eval_ns || has_defer_ns) any_new_ns = true;
    } else {
      if ((has_eval_ns && !it->second.has_eval_ns) ||
          (has_defer_ns && !it->second.has_defer_ns)) {
        any_new_ns = true;
      }
    }
  }

  for (const auto& pair : module_map_) {
    Local<v8::Module> mod = pair.second.Get(isolate());
    v8::Module::Status status = mod->GetStatus();
    CHECK_NE(status, v8::Module::kInstantiating);
    if (status == v8::Module::kErrored) {
      CHECK(!mod->GetException().IsEmpty());
    }
    i::DirectHandle<i::Module> i_mod = v8::Utils::OpenDirectHandle(*mod);
    if (i_mod->status() >= i::Module::kLinked) {
      int nof = i_mod->exports()->NumberOfElements();
      bool has_eval_ns =
          !i::IsUndefined(i_mod->module_namespace()) &&
          i::IsJSModuleNamespace(
              i::Cast<i::Cell>(i_mod->module_namespace())->value());
      bool has_defer_ns =
          !i::IsUndefined(i_mod->deferred_module_namespace()) &&
          i::IsJSModuleNamespace(
              i::Cast<i::Cell>(i_mod->deferred_module_namespace())->value());
      auto [it, inserted] = export_snapshots_.emplace(
          pair.first, ExportSnapshot{nof, has_eval_ns, has_defer_ns});
      if (!inserted) {
        if (it->second.finalized() || !any_new_ns) {
          CHECK_EQ(nof, it->second.num_elements);
        }
        it->second.num_elements = nof;
        it->second.has_eval_ns = it->second.has_eval_ns || has_eval_ns;
        it->second.has_defer_ns = it->second.has_defer_ns || has_defer_ns;
      }
    }
#ifdef VERIFY_HEAP
    i::Object::ObjectVerify(*i_mod, i_isolate());
#endif
  }
}

void ModuleGraphTest::RunScriptIgnoringExceptions(Local<Context> context,
                                                  const std::string& source) {
  if (isolate()->IsExecutionTerminating()) return;
  v8::TryCatch try_catch(isolate());
  Local<v8::String> src_str;
  if (!v8::String::NewFromUtf8(isolate(), source.c_str()).ToLocal(&src_str)) {
    return;
  }
  if (isolate()->IsExecutionTerminating()) return;
  Local<v8::Script> script;
  if (!v8::Script::Compile(context, src_str).ToLocal(&script)) {
    return;
  }
  if (isolate()->IsExecutionTerminating()) return;
  USE(script->Run(context));
}

void ModuleGraphTest::FuzzModuleGraph(const ModuleGraphSpec& spec) {
  HandleScope handle_scope(isolate());

  active_instance_ = this;
  allow_terminate_ = true;
  dynamic_import_count_ = 0;
  module_names_.clear();
  module_map_.clear();
  export_snapshots_.clear();

  Local<v8::ObjectTemplate> global_template =
      v8::ObjectTemplate::New(isolate());
  global_template->Set(isolate(), "terminate",
                       v8::FunctionTemplate::New(isolate(), TerminateCallback));

  Local<Context> context = Context::New(isolate(), nullptr, global_template);
  Context::Scope context_scope(context);

  RunScriptIgnoringExceptions(
      context,
      "globalThis.__nsReaders = {};"
      "globalThis.__accessNs = function(ns, id) {"
      "  const fn = (globalThis.__nsReaders[id] ??= new Function('ns',"
      "    'for (let i = 0; i < 4; i++) {' +"
      "    '  try { ns.val; } catch (e) {}' +"
      "    '  try { ns.l_val; } catch (e) {}' +"
      "    '  try { ns.fn; } catch (e) {}' +"
      "    '  try { ns.default; } catch (e) {}' +"
      "    '  try { ns.then; } catch (e) {}' +"
      "    '  try { ns[\"quoted name\"]; } catch (e) {}' +"
      "    '  try { ns[\"another one\"]; } catch (e) {}' +"
      "    '  try { ns.hoisted; } catch (e) {}' +"
      "    '  try { ns.Klass; } catch (e) {}' +"
      "    '  try { ns.afn; } catch (e) {}' +"
      "    '  try { ns.gen; } catch (e) {}' +"
      "    '  try { ns.late; } catch (e) {}' +"
      "    '  try { ns.d1; } catch (e) {}' +"
      "    '  try { ns.d2; } catch (e) {}' +"
      "    '}'));"
      "  fn(ns);"
      "  for (let i = 0; i < 4; i++) {"
      "    try { ns.val; } catch (e) {}"
      "    try { ns.l_val; } catch (e) {}"
      "    try { ns.default; } catch (e) {}"
      "    try { ns.then; } catch (e) {}"
      "    try { ns['quoted name']; } catch (e) {}"
      "    try { ns.hoisted; } catch (e) {}"
      "  }"
      "};");

  const size_t num_source = spec.source_modules.size();
  const size_t num_synth = spec.synthetic_modules.size();
  if (num_source == 0) return;

  for (size_t i = 0; i < num_source; ++i) {
    module_names_.push_back("./m" + std::to_string(i) + ".mjs");
  }
  for (size_t j = 0; j < num_synth; ++j) {
    module_names_.push_back("./s" + std::to_string(j) + ".syn");
  }

  // 1. Compile all source text modules.
  for (size_t i = 0; i < num_source; ++i) {
    std::string name = "./m" + std::to_string(i) + ".mjs";
    std::string source_code =
        BuildModuleSource(i, spec.source_modules[i], num_source);

    v8::TryCatch try_catch(isolate());
    Local<v8::String> url_str =
        v8::String::NewFromUtf8(isolate(), name.c_str()).ToLocalChecked();
    Local<v8::String> src_str;
    if (!v8::String::NewFromUtf8(isolate(), source_code.c_str())
             .ToLocal(&src_str)) {
      continue;
    }
    ScriptOrigin origin(url_str, 0, 0, false, -1, Local<Value>(), false, false,
                        true);
    ScriptCompiler::Source source(src_str, origin);
    Local<v8::Module> mod;
    if (ScriptCompiler::CompileModule(isolate(), &source).ToLocal(&mod)) {
      module_map_.emplace(name, Global<v8::Module>(isolate(), mod));
    }
  }

  // 2. Create synthetic modules.
  Local<v8::String> default_export_name =
      v8::String::NewFromUtf8Literal(isolate(), "default");
  Local<v8::String> val_export_name =
      v8::String::NewFromUtf8Literal(isolate(), "val");
  std::vector<Local<v8::String>> synth_exports = {default_export_name,
                                                  val_export_name};
  for (size_t j = 0; j < num_synth; ++j) {
    std::string name = "./s" + std::to_string(j) + ".syn";
    Local<v8::String> name_str =
        v8::String::NewFromUtf8(isolate(), name.c_str()).ToLocalChecked();
    Local<Data> host_options =
        v8::Int32::New(isolate(), static_cast<int>(spec.synthetic_modules[j]));
    Local<v8::Module> synth = v8::Module::CreateSyntheticModule(
        isolate(), name_str, synth_exports, SyntheticModuleEvaluationSteps,
        host_options);
    module_map_.emplace(name, Global<v8::Module>(isolate(), synth));
  }

  if (module_map_.empty()) return;

  // 3. Execute multi-step driver actions.
  for (const DriverActionSpec& action : spec.actions) {
    std::string target_name = TargetName(action.target);
    auto it = module_map_.find(target_name);
    if (it == module_map_.end()) continue;
    Local<v8::Module> target_mod = it->second.Get(isolate());

    switch (action.kind) {
      case DriverActionKind::kInstantiate: {
        v8::TryCatch try_catch(isolate());
        USE(target_mod->InstantiateModule(context, ResolveModuleCallback,
                                          ResolveModuleSourceCallback));
        break;
      }
      case DriverActionKind::kEvaluate: {
        v8::TryCatch try_catch(isolate());
        if (target_mod
                ->InstantiateModule(context, ResolveModuleCallback,
                                    ResolveModuleSourceCallback)
                .FromMaybe(false) &&
            !isolate()->IsExecutionTerminating()) {
          USE(target_mod->Evaluate(context));
        }
        break;
      }
      case DriverActionKind::kEvaluateForImportDefer: {
        v8::TryCatch try_catch(isolate());
        if (target_mod
                ->InstantiateModule(context, ResolveModuleCallback,
                                    ResolveModuleSourceCallback)
                .FromMaybe(false) &&
            !isolate()->IsExecutionTerminating()) {
          USE(target_mod->EvaluateForImportDefer(context));
        }
        break;
      }
      case DriverActionKind::kEvaluateWithMicrotaskTerminate: {
        v8::TryCatch try_catch(isolate());
        if (target_mod
                ->InstantiateModule(context, ResolveModuleCallback,
                                    ResolveModuleSourceCallback)
                .FromMaybe(false) &&
            !isolate()->IsExecutionTerminating()) {
          RunScriptIgnoringExceptions(
              context, "Promise.resolve().then(() => { terminate(); });");
          if (!isolate()->IsExecutionTerminating()) {
            USE(target_mod->Evaluate(context));
          }
        }
        break;
      }
      case DriverActionKind::kGetNamespaceAndAccess: {
        if (EnsureInstantiatedAndBindNamespace(
                context, target_mod, ModuleImportPhase::kEvaluation, "__ns")) {
          RunScriptIgnoringExceptions(
              context,
              "try { globalThis.__accessNs(__ns, '" + target_name +
                  "'); } catch (e) {} "
                  "try { for (const k of Reflect.ownKeys(__ns)) { try { "
                  "__ns[k]; } catch (e) {} try { "
                  "Object.getOwnPropertyDescriptor(__ns, k); } catch (e) {} } "
                  "} catch (e) {} "
                  "try { Object.keys(__ns); } catch (e) {}");
        }
        break;
      }
      case DriverActionKind::kGetDeferNamespaceAndAccess: {
        if (EnsureInstantiatedAndBindNamespace(
                context, target_mod, ModuleImportPhase::kDefer, "__def_ns")) {
          RunScriptIgnoringExceptions(
              context,
              "try { __def_ns.then; } catch (e) {} "
              "try { __def_ns[Symbol.toStringTag]; } catch (e) {} "
              "try { globalThis.__accessNs(__def_ns, 'defer_" +
                  target_name +
                  "'); } catch (e) {} "
                  "try { for (const k of Reflect.ownKeys(__def_ns)) { try { "
                  "__def_ns[k]; } catch (e) {} try { "
                  "Object.getOwnPropertyDescriptor(__def_ns, k); } catch (e) "
                  "{} } } catch (e) {} "
                  "try { Object.keys(__def_ns); } catch (e) {}");
        }
        break;
      }
      case DriverActionKind::kGetDeferNamespaceAndAccessWithTerminate: {
        if (EnsureInstantiatedAndBindNamespace(
                context, target_mod, ModuleImportPhase::kDefer, "__def_ns")) {
          RunScriptIgnoringExceptions(
              context,
              "Promise.resolve().then(() => { terminate(); }); "
              "try { __def_ns.val; } catch (e) {}");
        }
        break;
      }
      case DriverActionKind::kDynamicImportJS: {
        RunScriptIgnoringExceptions(
            context,
            "import('" + target_name + "').then(m => m.val, e => {});");
        break;
      }
      case DriverActionKind::kDynamicImportDeferJS: {
        RunScriptIgnoringExceptions(
            context,
            "try { import.defer('" + target_name +
                "').then(d => { try { d.val; } catch (e) {} }, e => {}); } "
                "catch (e) {}");
        break;
      }
      case DriverActionKind::kDynamicImportSourceJS: {
        RunScriptIgnoringExceptions(
            context, "try { import.source('" + target_name +
                         "').then(() => {}, e => {}); } catch (e) {}");
        break;
      }
      case DriverActionKind::kResumeTla: {
        RunScriptIgnoringExceptions(
            context,
            "if (Array.isArray(globalThis.__resumables)) { "
            "for (const r of globalThis.__resumables.splice(0)) { "
            "try { r(); } catch (e) {} } }");
        break;
      }
      case DriverActionKind::kDrainMicrotasks: {
        v8::TryCatch try_catch(isolate());
        isolate()->PerformMicrotaskCheckpoint();
        break;
      }
      case DriverActionKind::kCheckStalledTlaAndGC: {
        if (target_mod->IsSourceTextModule() &&
            target_mod->GetStatus() >= v8::Module::kInstantiated) {
          USE(target_mod->GetStalledTopLevelAwaitMessages(isolate()));
        }
        InvokeMinorGC();
        InvokeMajorGC();
        break;
      }
    }

    isolate()->CancelTerminateExecution();
    VerifyAllModules();
  }

  // Final cleanup: disable termination and drain any remaining microtasks.
  allow_terminate_ = false;
  isolate()->CancelTerminateExecution();
  {
    v8::TryCatch try_catch(isolate());
    isolate()->PerformMicrotaskCheckpoint();
  }
  isolate()->CancelTerminateExecution();
  VerifyAllModules();
  module_map_.clear();
}

// FuzzTest domain construction.

auto AnyEdgeKind() {
  return fuzztest::ElementOf({
      EdgeKind::kStaticNamed,
      EdgeKind::kStaticLet,
      EdgeKind::kStaticDefault,
      EdgeKind::kStaticNamespace,
      EdgeKind::kStaticDefaultAndNamespace,
      EdgeKind::kStaticDefaultAndNamed,
      EdgeKind::kStaticEmpty,
      EdgeKind::kStaticQuoted,
      EdgeKind::kStaticBare,
      EdgeKind::kDeferNamespace,
      EdgeKind::kSourceStatic,
      EdgeKind::kExportStar,
      EdgeKind::kExportNamed,
      EdgeKind::kExportNamespace,
      EdgeKind::kExportMissing,
      EdgeKind::kExportCircular,
  });
}

auto AnyEdgeSpec() {
  return fuzztest::StructOf<EdgeSpec>(fuzztest::Arbitrary<uint8_t>(),
                                      AnyEdgeKind());
}

auto AnyDefaultExportKind() {
  return fuzztest::ElementOf({
      DefaultExportKind::kFunction,
      DefaultExportKind::kClass,
      DefaultExportKind::kAsyncFunction,
      DefaultExportKind::kGenerator,
      DefaultExportKind::kIIFE,
      DefaultExportKind::kConditionalThrowIIFE,
      DefaultExportKind::kAliasFn,
      DefaultExportKind::kObjectLiteral,
  });
}

auto AnyExtraExportFlags() {
  return fuzztest::InRange<uint8_t>(
      0, kExtraQuotedNames | kExtraHoistedAndClass | kExtraAsyncAndGenerator |
             kExtraLateLetAndDestructuring | kOmitBaseValExports);
}

auto AnyWrapperKind() {
  return fuzztest::ElementOf({
      WrapperKind::kNone,
      WrapperKind::kTryCatch,
      WrapperKind::kTryFinally,
      WrapperKind::kTryCatchFinally,
      WrapperKind::kUsing,
      WrapperKind::kAwaitUsing,
      WrapperKind::kMicrotask,
      WrapperKind::kForAwait,
      WrapperKind::kOptimizedFn,
      WrapperKind::kStaticBlock,
      WrapperKind::kEval,
      WrapperKind::kPrepareStackTrace,
      WrapperKind::kObjectProtoThen,
      WrapperKind::kPromiseThenPatch,
  });
}

auto AnyOpKind() {
  return fuzztest::ElementOf({
      OpKind::kNop,
      OpKind::kAwaitZero,
      OpKind::kAwaitPromiseResolve,
      OpKind::kAwaitPromiseReject,
      OpKind::kAwaitPending,
      OpKind::kAwaitResumable,
      OpKind::kAwaitThenable,
      OpKind::kAwaitDynamicImport,
      OpKind::kAwaitDynamicImportDefer,
      OpKind::kThrowError,
      OpKind::kThrowDefaultFlag,
      OpKind::kTerminate,
      OpKind::kStackOverflow,
      OpKind::kDynamicImport,
      OpKind::kDynamicImportDefer,
      OpKind::kDynamicImportSource,
      OpKind::kBadDynamicImport,
      OpKind::kNsGetVal,
      OpKind::kNsReflectAndKeys,
      OpKind::kNsMutateAndIntegrity,
      OpKind::kNsSpreadAndProxy,
      OpKind::kImportMeta,
      OpKind::kTriggerGC,
  });
}

auto AnyStmtSpec() {
  return fuzztest::StructOf<StmtSpec>(AnyWrapperKind(), AnyOpKind(),
                                      AnyOpKind(),
                                      fuzztest::Arbitrary<uint8_t>());
}

auto AnyThenExportSpec() {
  return fuzztest::StructOf<ThenExportSpec>(fuzztest::ElementOf({
                                                ThenSettleKind::kNone,
                                                ThenSettleKind::kResolve,
                                                ThenSettleKind::kReject,
                                                ThenSettleKind::kAsyncResolve,
                                            }),
                                            AnyOpKind(),
                                            fuzztest::Arbitrary<uint8_t>());
}

auto AnySourceModuleSpec() {
  return fuzztest::StructOf<SourceModuleSpec>(
      fuzztest::VectorOf(AnyEdgeSpec()).WithMinSize(0).WithMaxSize(4),
      AnyDefaultExportKind(), AnyExtraExportFlags(), AnyThenExportSpec(),
      fuzztest::VectorOf(AnyStmtSpec()).WithMinSize(1).WithMaxSize(4));
}

auto AnySyntheticModuleKind() {
  return fuzztest::ElementOf({
      SyntheticModuleKind::kFulfill,
      SyntheticModuleKind::kThrow,
      SyntheticModuleKind::kTerminate,
      SyntheticModuleKind::kReentrantDeferAccess,
  });
}

auto AnyDriverActionSpec() {
  return fuzztest::StructOf<DriverActionSpec>(
      fuzztest::ElementOf({
          DriverActionKind::kInstantiate,
          DriverActionKind::kEvaluate,
          DriverActionKind::kEvaluateForImportDefer,
          DriverActionKind::kEvaluateWithMicrotaskTerminate,
          DriverActionKind::kGetNamespaceAndAccess,
          DriverActionKind::kGetDeferNamespaceAndAccess,
          DriverActionKind::kGetDeferNamespaceAndAccessWithTerminate,
          DriverActionKind::kDynamicImportJS,
          DriverActionKind::kDynamicImportDeferJS,
          DriverActionKind::kDynamicImportSourceJS,
          DriverActionKind::kResumeTla,
          DriverActionKind::kDrainMicrotasks,
          DriverActionKind::kCheckStalledTlaAndGC,
      }),
      fuzztest::Arbitrary<uint8_t>());
}

auto AnyModuleGraphSpec() {
  return fuzztest::StructOf<ModuleGraphSpec>(
      fuzztest::VectorOf(AnySourceModuleSpec()).WithMinSize(2).WithMaxSize(8),
      fuzztest::VectorOf(AnySyntheticModuleKind())
          .WithMinSize(0)
          .WithMaxSize(2),
      fuzztest::VectorOf(AnyDriverActionSpec()).WithMinSize(1).WithMaxSize(6));
}

V8_FUZZ_SUITE(ModuleGraphFuzzTest, ModuleGraphTest);

V8_FUZZ_TEST_F(ModuleGraphFuzzTest, FuzzModuleGraph)
    .WithDomains(AnyModuleGraphSpec());

}  // namespace
}  // namespace v8
