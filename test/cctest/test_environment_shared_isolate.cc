// Several node::Environments on one v8::Isolate that the embedder created,
// registered with the platform and gave a CppHeap, each in its own
// embedder-created v8::Context, driven by one event loop and freed one at a
// time while the others keep running. doc/api/embedding.md allows this and
// embedders that host Node.js inside an existing JS runtime depend on it.

#include "cppgc/allocation.h"
#include "cppgc/garbage-collected.h"
#include "env-inl.h"
#include "node_test_fixture.h"
#include "v8-cppgc.h"
#if HAVE_OPENSSL
#include "crypto/crypto_context.h"
#endif

#include <string>
#include <vector>

using node::Environment;
using node::IsolateData;
using v8::Context;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Value;
namespace EnvironmentFlags = node::EnvironmentFlags;

namespace {

class EmbedderObject final : public v8::Object::Wrappable {
 public:
  static int alive;
  EmbedderObject() { alive++; }
  ~EmbedderObject() override { alive--; }
};
int EmbedderObject::alive = 0;

void SetFlag(void* flag) {
  *static_cast<bool*>(flag) = true;
}

int cleanup_hook_runs = 0;
void CountCleanupHook(void* arg) {
  (*static_cast<int*>(arg))++;
}

enum class IsolateDataMode {
  // One IsolateData created with the platform, shared by every Environment.
  kShared,
  // One IsolateData per Environment, created without a platform and freed
  // together with its Environment.
  kPerEnvironmentWithoutPlatform,
};

struct Instance {
  int id;
  v8::Global<Context> context;
  IsolateData* isolate_data = nullptr;
  Environment* env = nullptr;
  bool exit_handler_called = false;
  int exit_code = -1;
  node::StopFlags::Flags stop_flags = node::StopFlags::kDoNotTerminateIsolate;
};

}  // namespace

class SharedIsolateTest
    : public NodeZeroIsolateTestFixture,
      public ::testing::WithParamInterface<IsolateDataMode> {
 protected:
  Isolate* isolate_ = nullptr;
  v8::CppHeap* cpp_heap_ = nullptr;
  IsolateData* shared_isolate_data_ = nullptr;

  void SetUp() override {
    NodeZeroIsolateTestFixture::SetUp();

    Isolate::CreateParams params;
    params.array_buffer_allocator = allocator.get();
    params.cpp_heap =
        v8::CppHeap::Create(platform.get(), v8::CppHeapCreateParams{{}})
            .release();
    cpp_heap_ = params.cpp_heap;

    isolate_ = Isolate::Allocate();
    CHECK_NOT_NULL(isolate_);
    platform->RegisterIsolate(isolate_, &current_loop);
    Isolate::Initialize(isolate_, params);
    node::IsolateSettings settings;
    settings.flags |=
        node::IsolateSettingsFlags::SHOULD_NOT_SET_PREPARE_STACK_TRACE_CALLBACK;
    node::SetIsolateUpForNode(isolate_, settings);
    isolate_->Enter();

    if (GetParam() == IsolateDataMode::kShared) {
      HandleScope handle_scope(isolate_);
      shared_isolate_data_ =
          node::CreateIsolateData(isolate_, &current_loop, platform.get());
      CHECK_NOT_NULL(shared_isolate_data_);
    }
  }

  void TearDown() override {
    if (shared_isolate_data_ != nullptr) {
      node::FreeIsolateData(shared_isolate_data_);
      shared_isolate_data_ = nullptr;
    }
    EXPECT_EQ(isolate_->GetCppHeap(), cpp_heap_);
    platform->DrainTasks(isolate_);
    isolate_->Exit();
    bool platform_finished = false;
    platform->AddIsolateFinishedCallback(isolate_, SetFlag, &platform_finished);
    isolate_->Dispose();
    platform->UnregisterIsolate(isolate_);
    while (!platform_finished) uv_run(&current_loop, UV_RUN_ONCE);
    isolate_ = nullptr;
  }

  std::unique_ptr<Instance> CreateInstance(int id,
                                           EnvironmentFlags::Flags flags,
                                           bool throw_after_bootstrap = false) {
    auto instance = std::make_unique<Instance>();
    instance->id = id;
    HandleScope handle_scope(isolate_);
    Local<Context> context = Context::New(isolate_);
    CHECK(node::InitializeContext(context).FromJust());
    instance->context.Reset(isolate_, context);
    Context::Scope context_scope(context);

    if (GetParam() == IsolateDataMode::kShared) {
      instance->isolate_data = shared_isolate_data_;
    } else {
      instance->isolate_data =
          node::CreateIsolateData(isolate_, &current_loop, nullptr);
      CHECK_NOT_NULL(instance->isolate_data);
    }

    std::vector<std::string> args{"node"};
    if (throw_after_bootstrap) args.push_back("--throw");
    std::vector<std::string> exec_args;
    instance->env = node::CreateEnvironment(
        instance->isolate_data, context, args, exec_args, flags);
    CHECK_NOT_NULL(instance->env);

    Instance* raw = instance.get();
    if (throw_after_bootstrap) instance->stop_flags = node::StopFlags::kNoFlags;
    node::SetProcessExitHandler(instance->env,
                                [raw](Environment* env, int exit_code) {
                                  raw->exit_handler_called = true;
                                  raw->exit_code = exit_code;
                                  node::Stop(env, raw->stop_flags);
                                });

    std::string script =
        "const id = " + std::to_string(id) +
        ";\n"
        "const vm = require('vm');\n"
        "const { setInterval, clearInterval, setImmediate } ="
        " require('timers');\n"
        "require('net');\n"
        "globalThis.state = { id, ticks: 0, immediates: 0, events: [] };\n"
        "const interval = setInterval(() => {\n"
        "  state.ticks++;\n"
        "  new vm.Script('1 + 1');\n"
        "}, 1);\n"
        "(function again() {\n"
        "  state.immediates++;\n"
        "  setImmediate(again);\n"
        "})();\n"
        "process.on('beforeExit', () => state.events.push('beforeExit'));\n"
        "process.on('exit', () => {\n"
        "  state.events.push('exit');\n"
        "  clearInterval(interval);\n"
        "});\n"
        "if (process.argv.includes('--throw')) throw new Error('uncaught');\n"
        "return id;\n";
    v8::MaybeLocal<Value> result =
        node::LoadEnvironment(instance->env, script.c_str());
    if (throw_after_bootstrap) {
      CHECK(result.IsEmpty());
      CHECK(instance->exit_handler_called);
    } else {
      CHECK_EQ(result.ToLocalChecked()->Int32Value(context).FromJust(), id);
    }

    node::AddEnvironmentCleanupHook(
        isolate_, CountCleanupHook, &cleanup_hook_runs);
    return instance;
  }

  void FreeInstance(std::unique_ptr<Instance> instance) {
    node::FreeEnvironment(instance->env);
    if (GetParam() == IsolateDataMode::kPerEnvironmentWithoutPlatform) {
      node::FreeIsolateData(instance->isolate_data);
    }
    instance->context.Reset();
  }

  Local<Value> Evaluate(Instance* instance, const char* source) {
    v8::EscapableHandleScope handle_scope(isolate_);
    Local<Context> context = instance->context.Get(isolate_);
    Context::Scope context_scope(context);
    Local<v8::Script> script =
        v8::Script::Compile(
            context, v8::String::NewFromUtf8(isolate_, source).ToLocalChecked())
            .ToLocalChecked();
    return handle_scope.Escape(script->Run(context).ToLocalChecked());
  }

  bool TryEvaluate(Instance* instance, const char* source) {
    HandleScope handle_scope(isolate_);
    Local<Context> context = instance->context.Get(isolate_);
    Context::Scope context_scope(context);
    v8::TryCatch try_catch(isolate_);
    Local<v8::Script> script =
        v8::Script::Compile(
            context, v8::String::NewFromUtf8(isolate_, source).ToLocalChecked())
            .ToLocalChecked();
    return !script->Run(context).IsEmpty() && !try_catch.HasTerminated();
  }

  int EvaluateInt(Instance* instance, const char* source) {
    HandleScope handle_scope(isolate_);
    Local<Context> context = instance->context.Get(isolate_);
    return Evaluate(instance, source)->Int32Value(context).FromJust();
  }

  std::string EvaluateString(Instance* instance, const char* source) {
    HandleScope handle_scope(isolate_);
    v8::String::Utf8Value utf8(isolate_, Evaluate(instance, source));
    return *utf8;
  }

  // What an embedder's message pump does: a bounded number of loop turns and
  // foreground task flushes, never SpinEventLoop() on one Environment.
  void PumpLoop(int turns) {
    for (int i = 0; i < turns; i++) {
      uv_run(&current_loop, UV_RUN_NOWAIT);
      platform->DrainTasks(isolate_);
    }
  }

  // Runs the loop until every instance's interval timer and immediate chain
  // have both run again; a callback lost to a stray termination or exception
  // breaks its chain and shows up here.
  void PumpUntilAllTicked(const std::vector<Instance*>& instances) {
    std::vector<int> ticks, immediates;
    for (Instance* instance : instances) {
      ticks.push_back(EvaluateInt(instance, "state.ticks"));
      immediates.push_back(EvaluateInt(instance, "state.immediates"));
    }
    for (int turn = 0; turn < 10000; turn++) {
      uv_run(&current_loop, UV_RUN_ONCE);
      platform->DrainTasks(isolate_);
      bool all_progressed = true;
      for (size_t i = 0; i < instances.size(); i++) {
        if (EvaluateInt(instances[i], "state.ticks") <= ticks[i] ||
            EvaluateInt(instances[i], "state.immediates") <= immediates[i]) {
          all_progressed = false;
        }
      }
      if (all_progressed) return;
    }
    FAIL() << "an Environment stopped running its timers or immediates";
  }
};

TEST_P(SharedIsolateTest, EnvironmentsComeAndGoWhileSiblingsRun) {
  const HandleScope handle_scope(isolate_);
  cleanup_hook_runs = 0;

  const auto sibling_flags = static_cast<EnvironmentFlags::Flags>(
      EnvironmentFlags::kNoCreateInspector |
      EnvironmentFlags::kNoBrowserGlobals |
      EnvironmentFlags::kNoRegisterESMLoader |
      EnvironmentFlags::kNoGlobalSearchPaths);

  std::unique_ptr<Instance> owner =
      CreateInstance(0, EnvironmentFlags::kDefaultFlags);
  std::unique_ptr<Instance> second = CreateInstance(1, sibling_flags);
  std::unique_ptr<Instance> third = CreateInstance(2, sibling_flags);
  EXPECT_EQ(isolate_->GetCppHeap(), cpp_heap_);

  PumpUntilAllTicked({owner.get(), second.get(), third.get()});

  // The embedder's own cppgc objects live on the same heap as Node's.
  v8::Global<v8::Object> embedder_holder;
  {
    HandleScope inner(isolate_);
    Local<Context> context = second->context.Get(isolate_);
    Context::Scope context_scope(context);
    Local<v8::Object> holder = v8::FunctionTemplate::New(isolate_)
                                   ->InstanceTemplate()
                                   ->NewInstance(context)
                                   .ToLocalChecked();
    v8::Object::Wrap<v8::CppHeapPointerTag::kDefaultTag>(
        isolate_,
        holder,
        cppgc::MakeGarbageCollected<EmbedderObject>(
            isolate_->GetCppHeap()->GetAllocationHandle()));
    embedder_holder.Reset(isolate_, holder);
  }
  isolate_->LowMemoryNotification();
  EXPECT_EQ(EmbedderObject::alive, 1);

#if HAVE_INSPECTOR
  EXPECT_EQ(EvaluateInt(
                owner.get(),
                "(() => {\n"
                "  const { Session } = process.getBuiltinModule('inspector');\n"
                "  const session = new Session();\n"
                "  session.connect();\n"
                "  let value = -1;\n"
                "  session.post('Runtime.evaluate', { expression: 'state.id + "
                "42' },\n"
                "               (err, res) => { value = err ? -2 : "
                "res.result.value; });\n"
                "  console.time('session'); console.timeEnd('session');\n"
                "  session.disconnect();\n"
                "  return value;\n"
                "})()"),
            42);
  // A sibling without an inspector of its own gets an exception, not an abort.
  EXPECT_EQ(
      EvaluateString(
          second.get(),
          "(() => {\n"
          "  try {\n"
          "    const { Session } = process.getBuiltinModule('inspector');\n"
          "    const session = new Session();\n"
          "    session.connect();\n"
          "    session.disconnect();\n"
          "    return 'connected';\n"
          "  } catch (e) { return String(e.code); }\n"
          "})()"),
      "ERR_INSPECTOR_NOT_AVAILABLE");
#endif  // HAVE_INSPECTOR

  if (GetParam() == IsolateDataMode::kShared) {
    // Workers need a platform; their exit must not disturb the siblings.
    EXPECT_EQ(
        EvaluateInt(third.get(),
                    "(() => {\n"
                    "  const { Worker } ="
                    " process.getBuiltinModule('worker_threads');\n"
                    "  state.workerExit = -1;\n"
                    "  new Worker('process.exit(7)', { eval: true })\n"
                    "    .on('exit', (code) => { state.workerExit = code; });\n"
                    "  return 0;\n"
                    "})()"),
        0);
    for (int turn = 0; turn < 10000; turn++) {
      PumpLoop(1);
      if (EvaluateInt(third.get(), "state.workerExit") == 7) break;
    }
    EXPECT_EQ(EvaluateInt(third.get(), "state.workerExit"), 7);
  }

  PumpUntilAllTicked({owner.get(), second.get(), third.get()});

  // Free one Environment while its siblings have timers and immediates due.
  const int owner_ticks = EvaluateInt(owner.get(), "state.ticks");
  FreeInstance(std::move(second));
  EXPECT_EQ(cleanup_hook_runs, 1);
  EXPECT_FALSE(owner->exit_handler_called);
  EXPECT_FALSE(third->exit_handler_called);
  PumpUntilAllTicked({owner.get(), third.get()});
  EXPECT_GT(EvaluateInt(owner.get(), "state.ticks"), owner_ticks);

  // The embedder's object outlives the Environment whose context wrapped it.
  isolate_->LowMemoryNotification();
  EXPECT_EQ(EmbedderObject::alive, 1);
  embedder_holder.Reset();

  // An uncaught exception whose exit handler calls Stop() without
  // kDoNotTerminateIsolate, then free: the termination it requests must not
  // leak into the sibling's next script.
  third->stop_flags = node::StopFlags::kNoFlags;
  EXPECT_EQ(EvaluateInt(third.get(),
                        "process.getBuiltinModule('timers').setImmediate("
                        "() => { throw new Error('uncaught'); }), 0"),
            0);
  for (int turn = 0; turn < 100 && !third->exit_handler_called; turn++) {
    PumpLoop(1);
  }
  EXPECT_TRUE(third->exit_handler_called);
  EXPECT_EQ(third->exit_code, 1);
  FreeInstance(std::move(third));
  EXPECT_EQ(cleanup_hook_runs, 2);
  EXPECT_TRUE(TryEvaluate(owner.get(), "state.ticks"));
  PumpUntilAllTicked({owner.get()});
  EXPECT_FALSE(owner->exit_handler_called);

  // The same when the exception is thrown by the bootstrap script itself and
  // no JavaScript of that Environment runs after its exit handler.
  std::unique_ptr<Instance> failed = CreateInstance(4, sibling_flags, true);
  EXPECT_EQ(failed->exit_code, 1);
  FreeInstance(std::move(failed));
  EXPECT_EQ(cleanup_hook_runs, 3);
  EXPECT_TRUE(TryEvaluate(owner.get(), "state.ticks"));
  PumpUntilAllTicked({owner.get()});

  // A new sibling can join after others left, here a second one that owns an
  // inspector and debug signal handler of its own.
  std::unique_ptr<Instance> late =
      CreateInstance(3, EnvironmentFlags::kDefaultFlags);
  PumpUntilAllTicked({owner.get(), late.get()});

  // beforeExit / exit are per Environment.
  {
    HandleScope inner(isolate_);
    Context::Scope context_scope(late->context.Get(isolate_));
    node::EmitProcessBeforeExit(late->env).Check();
  }
  EXPECT_EQ(EvaluateString(late.get(), "state.events.join()"), "beforeExit");
  EXPECT_EQ(EvaluateString(owner.get(), "state.events.join()"), "");
  {
    HandleScope inner(isolate_);
    Context::Scope context_scope(late->context.Get(isolate_));
    EXPECT_EQ(node::EmitProcessExit(late->env).FromJust(), 0);
  }
  EXPECT_EQ(EvaluateString(late.get(), "state.events.join()"),
            "beforeExit,exit");
  EXPECT_EQ(EvaluateString(owner.get(), "state.events.join()"), "");
  node::Stop(late->env, node::StopFlags::kDoNotTerminateIsolate);
  PumpUntilAllTicked({owner.get()});
  EXPECT_EQ(EvaluateInt(late.get(), "state.events.length"), 2);
  FreeInstance(std::move(late));
  EXPECT_EQ(cleanup_hook_runs, 4);

  PumpUntilAllTicked({owner.get()});
  node::Stop(owner->env, node::StopFlags::kDoNotTerminateIsolate);
  FreeInstance(std::move(owner));
  EXPECT_EQ(cleanup_hook_runs, 5);

  // Nothing the Environments left behind keeps the shared loop busy.
  PumpLoop(2);
  EXPECT_EQ(uv_loop_alive(&current_loop), 0);
}

TEST_P(SharedIsolateTest, FreeIsolateDataBeforeItsEnvironmentAsserts) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  const HandleScope handle_scope(isolate_);
  std::unique_ptr<Instance> instance =
      CreateInstance(0, EnvironmentFlags::kNoCreateInspector);
  IsolateData* isolate_data = instance->isolate_data;
  EXPECT_DEATH_IF_SUPPORTED(node::FreeIsolateData(isolate_data),
                            "environment_count_");
  node::Stop(instance->env, node::StopFlags::kDoNotTerminateIsolate);
  FreeInstance(std::move(instance));
}

#if HAVE_OPENSSL
TEST_P(SharedIsolateTest, RootCertStoreIsPerEnvironment) {
  const HandleScope handle_scope(isolate_);
  std::unique_ptr<Instance> first =
      CreateInstance(0, EnvironmentFlags::kNoCreateInspector);
  std::unique_ptr<Instance> second =
      CreateInstance(1, EnvironmentFlags::kNoCreateInspector);
  auto store_size = [](Instance* instance) {
    return sk_X509_OBJECT_num(X509_STORE_get0_objects(
        node::crypto::GetOrCreateRootCertStore(instance->env)));
  };
  const int default_size = store_size(second.get());
  EXPECT_GT(default_size, 0);

  Evaluate(first.get(),
           "process.getBuiltinModule('tls').setDefaultCACertificates([])");
  EXPECT_EQ(store_size(first.get()), 0);
  EXPECT_EQ(store_size(second.get()), default_size);

  FreeInstance(std::move(first));
  EXPECT_EQ(store_size(second.get()), default_size);
  FreeInstance(std::move(second));
}
#endif  // HAVE_OPENSSL

INSTANTIATE_TEST_SUITE_P(
    EnvironmentTest,
    SharedIsolateTest,
    ::testing::Values(IsolateDataMode::kShared,
                      IsolateDataMode::kPerEnvironmentWithoutPlatform),
    [](const ::testing::TestParamInfo<IsolateDataMode>& info) {
      return info.param == IsolateDataMode::kShared
                 ? "SharedIsolateData"
                 : "IsolateDataPerEnvironmentWithoutPlatform";
    });
