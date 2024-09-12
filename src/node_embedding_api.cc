#define NAPI_EXPERIMENTAL
#include "node_embedding_api.h"

#include "env-inl.h"
#include "js_native_api_v8.h"
#include "node_api_internals.h"
#include "util-inl.h"

#include <mutex>
#include <string>

// Use macros to handle errors since they can record the failing argument name
// or expression and their location in the source code.

#define EMBEDDED_PLATFORM(platform)                                            \
  ((platform) == nullptr)                                                      \
      ? v8impl::EmbeddedErrorHandling::HandleError(                            \
            "Argument must not be null: " #platform,                           \
            __FILE__,                                                          \
            __LINE__,                                                          \
            node_embedding_exit_code_generic_user_error)                       \
      : reinterpret_cast<v8impl::EmbeddedPlatform*>(platform)

#define EMBEDDED_RUNTIME(runtime)                                              \
  (runtime) == nullptr ? v8impl::EmbeddedErrorHandling::HandleError(           \
                             "Argument must not be null: " #runtime,           \
                             __FILE__,                                         \
                             __LINE__,                                         \
                             node_embedding_exit_code_generic_user_error)      \
                       : reinterpret_cast<v8impl::EmbeddedRuntime*>(runtime)

#define ARG_NOT_NULL(arg)                                                      \
  do {                                                                         \
    if ((arg) == nullptr) {                                                    \
      return v8impl::EmbeddedErrorHandling::HandleError(                       \
          "Argument must not be null: " #arg,                                  \
          __FILE__,                                                            \
          __LINE__,                                                            \
          node_embedding_exit_code_generic_user_error);                        \
    }                                                                          \
  } while (false)

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      return v8impl::EmbeddedErrorHandling::HandleError(                       \
          "Expression returned false: " #expr,                                 \
          __FILE__,                                                            \
          __LINE__,                                                            \
          node_embedding_exit_code_generic_user_error);                        \
    }                                                                          \
  } while (false)

namespace node {

// Declare functions implemented in embed_helpers.cc
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(Environment* env);
v8::Maybe<ExitCode> SpinEventLoopWithoutCleanup(
    Environment* env,
    uv_run_mode run_mode,
    const std::function<bool(bool)>& shouldContinue);

}  // end of namespace node

namespace v8impl {

napi_env NewEnv(v8::Local<v8::Context> context,
                const std::string& module_filename,
                int32_t module_api_version);
namespace {

// A helper class to convert std::vector<std::string> to an array of C strings.
// If the number of strings is less than kInplaceBufferSize, the strings are
// stored in the inplace_buffer_ array. Otherwise, the strings are stored in the
// allocated_buffer_ array.
// Ideally the class must be allocated on the stack.
// In any case it must not outlive the passed vector since it keeps only the
// string pointers returned by std::string::c_str() method.
template <size_t kInplaceBufferSize = 32>
class CStringArray {
 public:
  explicit CStringArray(const std::vector<std::string>& strings) noexcept
      : size_(strings.size()) {
    if (size_ <= inplace_buffer_.size()) {
      c_strs_ = inplace_buffer_.data();
    } else {
      allocated_buffer_ = std::make_unique<const char*[]>(size_);
      c_strs_ = allocated_buffer_.get();
    }
    for (size_t i = 0; i < size_; ++i) {
      c_strs_[i] = strings[i].c_str();
    }
  }

  CStringArray(const CStringArray&) = delete;
  CStringArray& operator=(const CStringArray&) = delete;

  const char** c_strs() const { return c_strs_; }
  size_t size() const { return size_; }

  const char** argv() const { return c_strs_; }
  int32_t argc() const { return static_cast<int32_t>(size_); }

 private:
  const char** c_strs_{};
  size_t size_{};
  std::array<const char*, kInplaceBufferSize> inplace_buffer_;
  std::unique_ptr<const char*[]> allocated_buffer_;
};

class EmbeddedErrorHandling {
 public:
  static node_embedding_exit_code SetErrorHandler(
      node_embedding_error_handler error_handler, void* error_handler_data);

  static node_embedding_exit_code HandleError(
      const std::string& message, node_embedding_exit_code exit_code);

  static node_embedding_exit_code HandleError(
      const std::vector<std::string>& messages,
      node_embedding_exit_code exit_code);

  static node_embedding_exit_code HandleError(
      const char* message,
      const char* filename,
      int32_t line,
      node_embedding_exit_code exit_code);

  static std::string FormatString(const char* format, ...);

  static node_embedding_exit_code DefaultErrorHandler(
      void* handler_data,
      const char* messages[],
      size_t messages_size,
      node_embedding_exit_code exit_code);

  static node_embedding_error_handler error_handler() {
    return error_handler_ ? error_handler_ : DefaultErrorHandler;
  }

 private:
  static node_embedding_error_handler error_handler_;
  static void* error_handler_data_;
};

node_embedding_error_handler EmbeddedErrorHandling::error_handler_{};
void* EmbeddedErrorHandling::error_handler_data_{};

class EmbeddedPlatform {
 public:
  explicit EmbeddedPlatform(int32_t api_version) noexcept
      : api_version_(api_version) {}

  EmbeddedPlatform(const EmbeddedPlatform&) = delete;
  EmbeddedPlatform& operator=(const EmbeddedPlatform&) = delete;

  node_embedding_exit_code DeleteMe();

  node_embedding_exit_code IsInitialized(bool* result);

  node_embedding_exit_code SetFlags(node_embedding_platform_flags flags);

  node_embedding_exit_code SetArgs(int32_t argc, char* argv[]);

  node_embedding_exit_code Initialize(bool* early_return);

  node_embedding_exit_code GetArgs(node_embedding_get_args_callback get_args,
                                   void* get_args_data);

  node_embedding_exit_code GetExecArgs(
      node_embedding_get_args_callback get_args, void* get_args_data);

  node_embedding_exit_code CreateRuntime(node_embedding_runtime* result);

  node::InitializationResult* init_result() { return init_result_.get(); }

  node::MultiIsolatePlatform* get_v8_platform() { return v8_platform_.get(); }

 private:
  static node::ProcessInitializationFlags::Flags GetProcessInitializationFlags(
      node_embedding_platform_flags flags);

 private:
  int32_t api_version_{1};
  bool is_initialized_{false};
  bool v8_is_initialized_{false};
  bool v8_is_uninitialized_{false};
  node_embedding_platform_flags flags_;
  std::vector<std::string> args_;
  struct {
    bool flags : 1;
    bool args : 1;
  } optional_bits_{};

  std::shared_ptr<node::InitializationResult> init_result_;
  std::unique_ptr<node::MultiIsolatePlatform> v8_platform_;
};

struct IsolateLocker {
  IsolateLocker(node::CommonEnvironmentSetup* env_setup)
      : v8_locker_(env_setup->isolate()),
        isolate_scope_(env_setup->isolate()),
        handle_scope_(env_setup->isolate()),
        context_scope_(env_setup->context()) {}

  bool IsLocked() const {
    return v8::Locker::IsLocked(v8::Isolate::GetCurrent());
  }

  void IncrementLockCount() { ++lock_count_; }

  bool DecrementLockCount() { return --lock_count_ == 0; }

 private:
  int32_t lock_count_ = 1;
  v8::Locker v8_locker_;
  v8::Isolate::Scope isolate_scope_;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
};

class EmbeddedRuntime {
 public:
  explicit EmbeddedRuntime(EmbeddedPlatform* platform);

  node_embedding_exit_code DeleteMe();

  node_embedding_exit_code IsInitialized(bool* result);

  node_embedding_exit_code SetFlags(node_embedding_runtime_flags flags);

  node_embedding_exit_code SetArgs(int32_t argc, const char* argv[]);

  node_embedding_exit_code SetExecArgs(int32_t argc, const char* argv[]);

  node_embedding_exit_code SetPreloadCallback(
      node_embedding_preload_callback preload_cb, void* preload_cb_data);

  node_embedding_exit_code SetSnapshotBlob(const uint8_t* snapshot,
                                           size_t size);

  node_embedding_exit_code OnCreateSnapshotBlob(
      node_embedding_store_blob_callback store_blob_cb,
      void* store_blob_cb_data,
      node_embedding_snapshot_flags snapshot_flags);

  node_embedding_exit_code AddModule(
      const char* module_name,
      node_embedding_initialize_module_callback init_module_cb,
      void* init_module_cb_data,
      int32_t module_node_api_version);

  node_embedding_exit_code Initialize(const char* main_script);

  node_embedding_exit_code RunEventLoop();

  node_embedding_exit_code RunEventLoopWhile(
      node_embedding_event_loop_predicate predicate,
      void* predicate_data,
      node_embedding_event_loop_run_mode run_mode,
      bool* has_more_work);

  node_embedding_exit_code AwaitPromise(napi_value promise,
                                        node_embedding_promise_state* state,
                                        napi_value* result,
                                        bool* has_more_work);

  node_embedding_exit_code SetNodeApiVersion(int32_t node_api_version);

  node_embedding_exit_code InvokeNodeApi(
      node_embedding_node_api_callback node_api_cb, void* node_api_cb_data);

  bool IsScopeOpened() const;

  static napi_env GetOrCreateNodeApiEnv(node::Environment* node_env,
                                        const std::string& module_filename,
                                        int32_t node_api_version);

 private:
  static node::EnvironmentFlags::Flags GetEnvironmentFlags(
      node_embedding_runtime_flags flags);

  void RegisterModules();

  static void RegisterModule(v8::Local<v8::Object> exports,
                             v8::Local<v8::Value> module,
                             v8::Local<v8::Context> context,
                             void* priv);

 private:
  struct ModuleInfo {
    std::string module_name;
    node_embedding_initialize_module_callback init_module_cb;
    void* init_module_cb_data;
    int32_t module_node_api_version;
  };

  struct SharedData {
    std::mutex mutex;
    std::unordered_map<node::Environment*, napi_env> node_env_to_node_api_env;

    static SharedData& Get() {
      static SharedData shared_data;
      return shared_data;
    }
  };

 private:
  EmbeddedPlatform* platform_;
  bool is_initialized_{false};
  node_embedding_runtime_flags flags_{node_embedding_runtime_default_flags};
  std::vector<std::string> args_;
  std::vector<std::string> exec_args_;
  node::EmbedderPreloadCallback preload_cb_{};
  node::EmbedderSnapshotData::Pointer snapshot_;
  std::function<void(const node::EmbedderSnapshotData*)> create_snapshot_;
  node::SnapshotConfig snapshot_config_{};
  int32_t node_api_version_{0};
  napi_env node_api_env_{};

  struct {
    bool flags : 1;
    bool args : 1;
    bool exec_args : 1;
  } optional_bits_{};

  std::unordered_map<std::string, ModuleInfo> modules_;

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup_;
  std::optional<IsolateLocker> isolate_locker_;
};

//-----------------------------------------------------------------------------
// EmbeddedErrorHandling implementation.
//-----------------------------------------------------------------------------

node_embedding_exit_code EmbeddedErrorHandling::SetErrorHandler(
    node_embedding_error_handler error_handler, void* error_handler_data) {
  error_handler_ = error_handler;
  error_handler_data_ = error_handler_data;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedErrorHandling::HandleError(
    const std::string& message, node_embedding_exit_code exit_code) {
  const char* message_c_str = message.c_str();
  return error_handler()(error_handler_data_, &message_c_str, 1, exit_code);
}

node_embedding_exit_code EmbeddedErrorHandling::HandleError(
    const std::vector<std::string>& messages,
    node_embedding_exit_code exit_code) {
  CStringArray message_arr(messages);
  return error_handler()(
      error_handler_data_, message_arr.c_strs(), message_arr.size(), exit_code);
}

node_embedding_exit_code EmbeddedErrorHandling::HandleError(
    const char* message,
    const char* filename,
    int32_t line,
    node_embedding_exit_code exit_code) {
  return HandleError(
      FormatString("Error: %s at %s:%d", message, filename, line), exit_code);
}

node_embedding_exit_code EmbeddedErrorHandling::DefaultErrorHandler(
    void* /*handler_data*/,
    const char* messages[],
    size_t messages_size,
    node_embedding_exit_code exit_code) {
  if (exit_code != 0) {
    for (size_t i = 0; i < messages_size; ++i) {
      fprintf(stderr, "%s\n", messages[i]);
    }
    fflush(stderr);
    node::Exit(static_cast<node::ExitCode>(exit_code));
  } else {
    for (size_t i = 0; i < messages_size; ++i) {
      fprintf(stdout, "%s\n", messages[i]);
    }
    fflush(stdout);
  }
  return exit_code;
}

std::string EmbeddedErrorHandling::FormatString(const char* format, ...) {
  va_list args1;
  va_start(args1, format);
  va_list args2;
  va_copy(args2, args1);  // Required for some compilers like GCC.
  std::string result(std::vsnprintf(nullptr, 0, format, args1), '\0');
  va_end(args1);
  std::vsnprintf(&result[0], result.size() + 1, format, args2);
  va_end(args2);
  return result;
}

//-----------------------------------------------------------------------------
// EmbeddedPlatform implementation.
//-----------------------------------------------------------------------------

node_embedding_exit_code EmbeddedPlatform::DeleteMe() {
  if (v8_is_initialized_ && !v8_is_uninitialized_) {
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    node::TearDownOncePerProcess();
    v8_is_uninitialized_ = false;
  }

  delete this;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::IsInitialized(bool* result) {
  ARG_NOT_NULL(result);
  *result = is_initialized_;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::SetFlags(
    node_embedding_platform_flags flags) {
  ASSERT(!is_initialized_);
  flags_ = flags;
  optional_bits_.flags = true;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::SetArgs(int32_t argc, char* argv[]) {
  ARG_NOT_NULL(argv);
  ASSERT(!is_initialized_);
  args_.assign(argv, argv + argc);
  optional_bits_.args = true;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::Initialize(bool* early_return) {
  ASSERT(!is_initialized_);

  is_initialized_ = true;

  // TODO(vmoroz): default initialize args_.

  if (!optional_bits_.flags) {
    flags_ = node_embedding_platform_no_flags;
  }

  init_result_ = node::InitializeOncePerProcess(
      args_, GetProcessInitializationFlags(flags_));

  if (init_result_->exit_code() != 0 || !init_result_->errors().empty()) {
    return EmbeddedErrorHandling::HandleError(
        init_result_->errors(),
        static_cast<node_embedding_exit_code>(init_result_->exit_code()));
  }

  if (early_return != nullptr) {
    *early_return = init_result_->early_return();
  } else if (init_result_->early_return()) {
    exit(init_result_->exit_code());
  }

  if (init_result_->early_return()) {
    return static_cast<node_embedding_exit_code>(init_result_->exit_code());
  }

  int32_t thread_pool_size =
      static_cast<int32_t>(node::per_process::cli_options->v8_thread_pool_size);
  v8_platform_ = node::MultiIsolatePlatform::Create(thread_pool_size);
  v8::V8::InitializePlatform(v8_platform_.get());
  v8::V8::Initialize();

  v8_is_initialized_ = true;

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::GetArgs(
    node_embedding_get_args_callback get_args_cb, void* get_args_cb_data) {
  ARG_NOT_NULL(get_args_cb);
  ASSERT(is_initialized_);

  v8impl::CStringArray args(init_result_->args());
  get_args_cb(get_args_cb_data, args.argc(), args.argv());

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::GetExecArgs(
    node_embedding_get_args_callback get_args_cb, void* get_args_cb_data) {
  ARG_NOT_NULL(get_args_cb);
  ASSERT(is_initialized_);

  v8impl::CStringArray args(init_result_->exec_args());
  get_args_cb(get_args_cb_data, args.argc(), args.argv());

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedPlatform::CreateRuntime(
    node_embedding_runtime* result) {
  ARG_NOT_NULL(result);
  ASSERT(is_initialized_);
  ASSERT(v8_is_initialized_);

  std::unique_ptr<EmbeddedRuntime> runtime =
      std::make_unique<EmbeddedRuntime>(this);

  *result = reinterpret_cast<node_embedding_runtime>(runtime.release());

  return node_embedding_exit_code_ok;
}

node::ProcessInitializationFlags::Flags
EmbeddedPlatform::GetProcessInitializationFlags(
    node_embedding_platform_flags flags) {
  uint32_t result = node::ProcessInitializationFlags::kNoFlags;
  if ((flags & node_embedding_platform_enable_stdio_inheritance) != 0) {
    result |= node::ProcessInitializationFlags::kEnableStdioInheritance;
  }
  if ((flags & node_embedding_platform_disable_node_options_env) != 0) {
    result |= node::ProcessInitializationFlags::kDisableNodeOptionsEnv;
  }
  if ((flags & node_embedding_platform_disable_cli_options) != 0) {
    result |= node::ProcessInitializationFlags::kDisableCLIOptions;
  }
  if ((flags & node_embedding_platform_no_icu) != 0) {
    result |= node::ProcessInitializationFlags::kNoICU;
  }
  if ((flags & node_embedding_platform_no_stdio_initialization) != 0) {
    result |= node::ProcessInitializationFlags::kNoStdioInitialization;
  }
  if ((flags & node_embedding_platform_no_default_signal_handling) != 0) {
    result |= node::ProcessInitializationFlags::kNoDefaultSignalHandling;
  }
  result |= node::ProcessInitializationFlags::kNoInitializeV8;
  result |= node::ProcessInitializationFlags::kNoInitializeNodeV8Platform;
  if ((flags & node_embedding_platform_no_init_openssl) != 0) {
    result |= node::ProcessInitializationFlags::kNoInitOpenSSL;
  }
  if ((flags & node_embedding_platform_no_parse_global_debug_variables) != 0) {
    result |= node::ProcessInitializationFlags::kNoParseGlobalDebugVariables;
  }
  if ((flags & node_embedding_platform_no_adjust_resource_limits) != 0) {
    result |= node::ProcessInitializationFlags::kNoAdjustResourceLimits;
  }
  if ((flags & node_embedding_platform_no_use_large_pages) != 0) {
    result |= node::ProcessInitializationFlags::kNoUseLargePages;
  }
  if ((flags & node_embedding_platform_no_print_help_or_version_output) != 0) {
    result |= node::ProcessInitializationFlags::kNoPrintHelpOrVersionOutput;
  }
  if ((flags & node_embedding_platform_generate_predictable_snapshot) != 0) {
    result |= node::ProcessInitializationFlags::kGeneratePredictableSnapshot;
  }
  return static_cast<node::ProcessInitializationFlags::Flags>(result);
}

//-----------------------------------------------------------------------------
// EmbeddedRuntime implementation.
//-----------------------------------------------------------------------------

EmbeddedRuntime::EmbeddedRuntime(EmbeddedPlatform* platform)
    : platform_(platform) {}

node_embedding_exit_code EmbeddedRuntime::DeleteMe() {
  ASSERT(!IsScopeOpened());

  {
    v8impl::IsolateLocker isolate_locker(env_setup_.get());

    node_embedding_exit_code exit_code = static_cast<node_embedding_exit_code>(
        node::SpinEventLoop(env_setup_->env()).FromMaybe(1));
    if (exit_code != node_embedding_exit_code_ok) {
      return EmbeddedErrorHandling::HandleError(
          "Failed while closing the runtime", exit_code);
    }
  }

  std::unique_ptr<node::CommonEnvironmentSetup> env_setup =
      std::move(env_setup_);

  if (create_snapshot_) {
    node::EmbedderSnapshotData::Pointer snapshot = env_setup->CreateSnapshot();
    ASSERT(snapshot);
    // TODO(vmoroz): handle error conditions.
    create_snapshot_(snapshot.get());
  }

  node::Stop(env_setup->env());

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::IsInitialized(bool* result) {
  ARG_NOT_NULL(result);
  *result = is_initialized_;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::SetFlags(
    node_embedding_runtime_flags flags) {
  ASSERT(!is_initialized_);
  flags_ = flags;
  optional_bits_.flags = true;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::SetArgs(int32_t argc,
                                                  const char* argv[]) {
  ARG_NOT_NULL(argv);
  ASSERT(!is_initialized_);
  args_.assign(argv, argv + argc);
  optional_bits_.args = true;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::SetExecArgs(int32_t argc,
                                                      const char* argv[]) {
  ARG_NOT_NULL(argv);
  ASSERT(!is_initialized_);
  exec_args_.assign(argv, argv + argc);
  optional_bits_.exec_args = true;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::SetPreloadCallback(
    node_embedding_preload_callback preload_cb, void* preload_cb_data) {
  ASSERT(!is_initialized_);

  if (preload_cb != nullptr) {
    preload_cb_ = node::EmbedderPreloadCallback(
        [preload_cb, preload_cb_data, node_api_version = node_api_version_](
            node::Environment* node_env,
            v8::Local<v8::Value> process,
            v8::Local<v8::Value> require) {
          napi_env env = GetOrCreateNodeApiEnv(
              node_env, "<worker thread>", node_api_version);
          env->CallIntoModule([&](napi_env env) {
            napi_value process_value = v8impl::JsValueFromV8LocalValue(process);
            napi_value require_value = v8impl::JsValueFromV8LocalValue(require);
            preload_cb(preload_cb_data, env, process_value, require_value);
          });
        });
  } else {
    preload_cb_ = {};
  }

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::SetSnapshotBlob(
    const uint8_t* snapshot, size_t size) {
  ARG_NOT_NULL(snapshot);
  ASSERT(!is_initialized_);

  snapshot_ = node::EmbedderSnapshotData::FromBlob(
      std::string_view(reinterpret_cast<const char*>(snapshot), size));
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::OnCreateSnapshotBlob(
    node_embedding_store_blob_callback store_blob_cb,
    void* store_blob_cb_data,
    node_embedding_snapshot_flags snapshot_flags) {
  ARG_NOT_NULL(store_blob_cb);
  ASSERT(!is_initialized_);

  create_snapshot_ = [store_blob_cb, store_blob_cb_data](
                         const node::EmbedderSnapshotData* snapshot) {
    std::vector<char> blob = snapshot->ToBlob();
    store_blob_cb(store_blob_cb_data,
                  reinterpret_cast<const uint8_t*>(blob.data()),
                  blob.size());
  };

  if ((snapshot_flags & node_embedding_snapshot_no_code_cache) != 0) {
    snapshot_config_.flags = static_cast<node::SnapshotFlags>(
        static_cast<uint32_t>(snapshot_config_.flags) |
        static_cast<uint32_t>(node::SnapshotFlags::kWithoutCodeCache));
  }

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::AddModule(
    const char* module_name,
    node_embedding_initialize_module_callback init_module_cb,
    void* init_module_cb_data,
    int32_t module_node_api_version) {
  ARG_NOT_NULL(module_name);
  ARG_NOT_NULL(init_module_cb);
  ASSERT(!is_initialized_);

  auto insert_result = modules_.try_emplace(module_name,
                                            module_name,
                                            init_module_cb,
                                            init_module_cb_data,
                                            module_node_api_version);
  if (!insert_result.second) {
    return EmbeddedErrorHandling::HandleError(
        EmbeddedErrorHandling::FormatString(
            "Module with name '%s' is already added.", module_name),
        node_embedding_exit_code_generic_user_error);
  }

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::Initialize(const char* main_script) {
  ASSERT(!is_initialized_);

  is_initialized_ = true;

  node::MultiIsolatePlatform* platform = platform_->get_v8_platform();

  node::EnvironmentFlags::Flags flags = GetEnvironmentFlags(
      optional_bits_.flags ? flags_ : node_embedding_runtime_default_flags);

  const std::vector<std::string>& args =
      optional_bits_.args ? args_ : platform_->init_result()->args();

  const std::vector<std::string>& exec_args =
      optional_bits_.exec_args ? exec_args_
                               : platform_->init_result()->exec_args();

  std::vector<std::string> errors;
  if (snapshot_) {
    env_setup_ = node::CommonEnvironmentSetup::CreateFromSnapshot(
        platform, &errors, snapshot_.get(), args, exec_args, flags);
  } else if (create_snapshot_) {
    env_setup_ = node::CommonEnvironmentSetup::CreateForSnapshotting(
        platform, &errors, args, exec_args, snapshot_config_);
  } else {
    env_setup_ = node::CommonEnvironmentSetup::Create(
        platform, &errors, args, exec_args, flags);
  }

  if (env_setup_ == nullptr || !errors.empty()) {
    return EmbeddedErrorHandling::HandleError(
        errors, node_embedding_exit_code_generic_user_error);
  }

  v8impl::IsolateLocker isolate_locker(env_setup_.get());

  std::string filename = args_.size() > 1 ? args_[1] : "<internal>";
  node_api_env_ =
      GetOrCreateNodeApiEnv(env_setup_->env(), filename, node_api_version_);

  node::Environment* node_env = env_setup_->env();

  RegisterModules();

  v8::MaybeLocal<v8::Value> ret =
      snapshot_
          ? node::LoadEnvironment(node_env, node::StartExecutionCallback{})
          : node::LoadEnvironment(
                node_env, std::string_view(main_script), preload_cb_);

  if (ret.IsEmpty()) return node_embedding_exit_code_generic_user_error;

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::RunEventLoop() {
  ASSERT(is_initialized_);

  IsolateLocker isolate_locker(env_setup_.get());

  if (node::SpinEventLoopWithoutCleanup(env_setup_->env()).IsNothing()) {
    return node_embedding_exit_code_generic_user_error;
  }

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::RunEventLoopWhile(
    node_embedding_event_loop_predicate predicate,
    void* predicate_data,
    node_embedding_event_loop_run_mode run_mode,
    bool* has_more_work) {
  ARG_NOT_NULL(predicate);
  ASSERT(is_initialized_);

  IsolateLocker isolate_locker(env_setup_.get());

  if (predicate(predicate_data,
                uv_loop_alive(env_setup_->env()->event_loop()))) {
    if (node::SpinEventLoopWithoutCleanup(
            env_setup_->env(),
            static_cast<uv_run_mode>(run_mode),
            [predicate, predicate_data](bool has_work) {
              return predicate(predicate_data, has_work);
            })
            .IsNothing()) {
      return node_embedding_exit_code_generic_user_error;
    }
  }

  if (has_more_work != nullptr) {
    *has_more_work = uv_loop_alive(env_setup_->env()->event_loop());
  }

  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::AwaitPromise(
    napi_value promise,
    node_embedding_promise_state* state,
    napi_value* result,
    bool* has_more_work) {
  napi_status status = [&]() {
    NAPI_PREAMBLE(node_api_env_);
    CHECK_ARG(node_api_env_, state);
    CHECK_ARG(node_api_env_, result);

    v8::EscapableHandleScope scope(node_api_env_->isolate);

    v8::Local<v8::Value> promise_value =
        v8impl::V8LocalValueFromJsValue(promise);
    if (promise_value.IsEmpty() || !promise_value->IsPromise())
      return napi_invalid_arg;
    v8::Local<v8::Promise> promise_object = promise_value.As<v8::Promise>();

    v8::Local<v8::Value> rejected =
        v8::Boolean::New(node_api_env_->isolate, false);
    v8::Local<v8::Function> err_handler =
        v8::Function::New(
            node_api_env_->context(),
            [](const v8::FunctionCallbackInfo<v8::Value>& info) { return; },
            rejected)
            .ToLocalChecked();

    if (promise_object->Catch(node_api_env_->context(), err_handler).IsEmpty())
      return napi_pending_exception;

    if (node::SpinEventLoopWithoutCleanup(
            env_setup_->env(),
            UV_RUN_ONCE,
            [&promise_object](bool /*has_work*/) {
              return promise_object->State() ==
                     v8::Promise::PromiseState::kPending;
            })
            .IsNothing())
      return napi_closing;

    *state = static_cast<node_embedding_promise_state>(promise_object->State());
    *result =
        v8impl::JsValueFromV8LocalValue(scope.Escape(promise_object->Result()));

    if (has_more_work != nullptr) {
      *has_more_work = uv_loop_alive(env_setup_->env()->event_loop());
    }

    return napi_ok;
  }();
  return (status == napi_ok) ? node_embedding_exit_code_ok
                             : node_embedding_exit_code_generic_user_error;
}

node_embedding_exit_code EmbeddedRuntime::SetNodeApiVersion(
    int32_t node_api_version) {
  ASSERT(!is_initialized_);
  node_api_version_ = node_api_version;
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code EmbeddedRuntime::InvokeNodeApi(
    node_embedding_node_api_callback node_api_cb, void* node_api_cb_data) {
  ARG_NOT_NULL(node_api_cb);
  if (isolate_locker_.has_value()) {
    ASSERT(isolate_locker_->IsLocked());
    isolate_locker_->IncrementLockCount();
  } else {
    isolate_locker_.emplace(env_setup_.get());
  }

  node_api_env_->CallIntoModule(
      [&](napi_env env) { node_api_cb(node_api_cb_data, env); },
      [](napi_env env, v8::Local<v8::Value> local_err) {
        node_napi_env__* node_napi_env = static_cast<node_napi_env__*>(env);
        if (node_napi_env->terminatedOrTerminating()) {
          return;
        }
        // If there was an unhandled exception while calling Node-API,
        // report it as a fatal exception. (There is no JavaScript on the
        // call stack that can possibly handle it.)
        node_napi_env->trigger_fatal_exception(local_err);
      });

  if (isolate_locker_->DecrementLockCount()) isolate_locker_.reset();
  return node_embedding_exit_code_ok;
}

bool EmbeddedRuntime::IsScopeOpened() const {
  return isolate_locker_.has_value();
}

napi_env EmbeddedRuntime::GetOrCreateNodeApiEnv(
    node::Environment* node_env,
    const std::string& module_filename,
    int32_t node_api_version) {
  SharedData& shared_data = SharedData::Get();

  {
    std::scoped_lock<std::mutex> lock(shared_data.mutex);
    auto it = shared_data.node_env_to_node_api_env.find(node_env);
    if (it != shared_data.node_env_to_node_api_env.end()) return it->second;
  }

  // Avoid creating the environment under the lock.
  napi_env env = NewEnv(node_env->context(), module_filename, node_api_version);

  std::scoped_lock<std::mutex> lock(shared_data.mutex);
  auto insert_result =
      shared_data.node_env_to_node_api_env.try_emplace(node_env, env);

  // Return either the inserted or the existing environment.
  return insert_result.first->second;
}

node::EnvironmentFlags::Flags EmbeddedRuntime::GetEnvironmentFlags(
    node_embedding_runtime_flags flags) {
  uint64_t result = node::EnvironmentFlags::kNoFlags;
  if ((flags & node_embedding_runtime_default_flags) != 0) {
    result |= node::EnvironmentFlags::kDefaultFlags;
  }
  if ((flags & node_embedding_runtime_owns_process_state) != 0) {
    result |= node::EnvironmentFlags::kOwnsProcessState;
  }
  if ((flags & node_embedding_runtime_owns_inspector) != 0) {
    result |= node::EnvironmentFlags::kOwnsInspector;
  }
  if ((flags & node_embedding_runtime_no_register_esm_loader) != 0) {
    result |= node::EnvironmentFlags::kNoRegisterESMLoader;
  }
  if ((flags & node_embedding_runtime_track_unmanaged_fds) != 0) {
    result |= node::EnvironmentFlags::kTrackUnmanagedFds;
  }
  if ((flags & node_embedding_runtime_hide_console_windows) != 0) {
    result |= node::EnvironmentFlags::kHideConsoleWindows;
  }
  if ((flags & node_embedding_runtime_no_native_addons) != 0) {
    result |= node::EnvironmentFlags::kNoNativeAddons;
  }
  if ((flags & node_embedding_runtime_no_global_search_paths) != 0) {
    result |= node::EnvironmentFlags::kNoGlobalSearchPaths;
  }
  if ((flags & node_embedding_runtime_no_browser_globals) != 0) {
    result |= node::EnvironmentFlags::kNoBrowserGlobals;
  }
  if ((flags & node_embedding_runtime_no_create_inspector) != 0) {
    result |= node::EnvironmentFlags::kNoCreateInspector;
  }
  if ((flags & node_embedding_runtime_no_start_debug_signal_handler) != 0) {
    result |= node::EnvironmentFlags::kNoStartDebugSignalHandler;
  }
  if ((flags & node_embedding_runtime_no_wait_for_inspector_frontend) != 0) {
    result |= node::EnvironmentFlags::kNoWaitForInspectorFrontend;
  }
  return static_cast<node::EnvironmentFlags::Flags>(result);
}

void EmbeddedRuntime::RegisterModules() {
  for (const auto& [module_name, module_info] : modules_) {
    node::node_module mod = {
        -1,                                     // nm_version for Node-API
        NM_F_LINKED,                            // nm_flags
        nullptr,                                // nm_dso_handle
        nullptr,                                // nm_filename
        nullptr,                                // nm_register_func
        RegisterModule,                         // nm_context_register_func
        module_name.c_str(),                    // nm_modname
        const_cast<ModuleInfo*>(&module_info),  // nm_priv
        nullptr                                 // nm_link
    };
    node::AddLinkedBinding(env_setup_->env(), mod);
  }
}

/*static*/ void EmbeddedRuntime::RegisterModule(v8::Local<v8::Object> exports,
                                                v8::Local<v8::Value> module,
                                                v8::Local<v8::Context> context,
                                                void* priv) {
  ModuleInfo* module_info = static_cast<ModuleInfo*>(priv);

  // Create a new napi_env for this specific module.
  napi_env env = v8impl::NewEnv(
      context, module_info->module_name, module_info->module_node_api_version);

  napi_value node_api_exports = nullptr;
  env->CallIntoModule([&](napi_env env) {
    node_api_exports =
        module_info->init_module_cb(module_info->init_module_cb_data,
                                    env,
                                    module_info->module_name.c_str(),
                                    v8impl::JsValueFromV8LocalValue(exports));
  });

  // If register function returned a non-null exports object different from
  // the exports object we passed it, set that as the "exports" property of
  // the module.
  if (node_api_exports != nullptr &&
      node_api_exports != v8impl::JsValueFromV8LocalValue(exports)) {
    napi_value node_api_module = v8impl::JsValueFromV8LocalValue(module);
    napi_set_named_property(env, node_api_module, "exports", node_api_exports);
  }
}

}  // end of anonymous namespace
}  // end of namespace v8impl

node_embedding_exit_code NAPI_CDECL
node_embedding_run_nodejs_main(int32_t argc, char* argv[]) {
  return static_cast<node_embedding_exit_code>(node::Start(argc, argv));
}

node_embedding_exit_code NAPI_CDECL node_embedding_on_error(
    node_embedding_error_handler error_handler, void* error_handler_data) {
  return v8impl::EmbeddedErrorHandling::SetErrorHandler(error_handler,
                                                        error_handler_data);
}

node_embedding_exit_code NAPI_CDECL node_embedding_create_platform(
    int32_t api_version, node_embedding_platform* result) {
  ARG_NOT_NULL(result);
  *result = reinterpret_cast<node_embedding_platform>(
      new v8impl::EmbeddedPlatform(api_version));
  return node_embedding_exit_code_ok;
}

node_embedding_exit_code NAPI_CDECL
node_embedding_delete_platform(node_embedding_platform platform) {
  return EMBEDDED_PLATFORM(platform)->DeleteMe();
}

node_embedding_exit_code NAPI_CDECL node_embedding_platform_is_initialized(
    node_embedding_platform platform, bool* result) {
  return EMBEDDED_PLATFORM(platform)->IsInitialized(result);
}

node_embedding_exit_code NAPI_CDECL node_embedding_platform_set_flags(
    node_embedding_platform platform, node_embedding_platform_flags flags) {
  return EMBEDDED_PLATFORM(platform)->SetFlags(flags);
}

node_embedding_exit_code NAPI_CDECL node_embedding_platform_set_args(
    node_embedding_platform platform, int32_t argc, char* argv[]) {
  return EMBEDDED_PLATFORM(platform)->SetArgs(argc, argv);
}

node_embedding_exit_code NAPI_CDECL node_embedding_platform_initialize(
    node_embedding_platform platform, bool* early_return) {
  return EMBEDDED_PLATFORM(platform)->Initialize(early_return);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_platform_get_args(node_embedding_platform platform,
                                 node_embedding_get_args_callback get_args,
                                 void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetArgs(get_args, get_args_data);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_platform_get_exec_args(node_embedding_platform platform,
                                      node_embedding_get_args_callback get_args,
                                      void* get_args_data) {
  return EMBEDDED_PLATFORM(platform)->GetExecArgs(get_args, get_args_data);
}

node_embedding_exit_code NAPI_CDECL node_embedding_create_runtime(
    node_embedding_platform platform, node_embedding_runtime* result) {
  return EMBEDDED_PLATFORM(platform)->CreateRuntime(result);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_delete_runtime(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->DeleteMe();
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_is_initialized(
    node_embedding_runtime runtime, bool* result) {
  return EMBEDDED_RUNTIME(runtime)->IsInitialized(result);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_set_flags(
    node_embedding_runtime runtime, node_embedding_runtime_flags flags) {
  return EMBEDDED_RUNTIME(runtime)->SetFlags(flags);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_set_args(
    node_embedding_runtime runtime, int32_t argc, const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetArgs(argc, argv);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_set_exec_args(
    node_embedding_runtime runtime, int32_t argc, const char* argv[]) {
  return EMBEDDED_RUNTIME(runtime)->SetExecArgs(argc, argv);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_on_preload(node_embedding_runtime runtime,
                                  node_embedding_preload_callback preload_cb,
                                  void* preload_cb_data) {
  return EMBEDDED_RUNTIME(runtime)->SetPreloadCallback(preload_cb,
                                                       preload_cb_data);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_use_snapshot(
    node_embedding_runtime runtime, const uint8_t* snapshot, size_t size) {
  return EMBEDDED_RUNTIME(runtime)->SetSnapshotBlob(snapshot, size);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_on_create_snapshot(
    node_embedding_runtime runtime,
    node_embedding_store_blob_callback store_blob_cb,
    void* store_blob_cb_data,
    node_embedding_snapshot_flags snapshot_flags) {
  return EMBEDDED_RUNTIME(runtime)->OnCreateSnapshotBlob(
      store_blob_cb, store_blob_cb_data, snapshot_flags);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_add_module(
    node_embedding_runtime runtime,
    const char* module_name,
    node_embedding_initialize_module_callback init_module_cb,
    void* init_module_cb_data,
    int32_t module_node_api_version) {
  return EMBEDDED_RUNTIME(runtime)->AddModule(module_name,
                                              init_module_cb,
                                              init_module_cb_data,
                                              module_node_api_version);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_initialize(
    node_embedding_runtime runtime, const char* main_script) {
  return EMBEDDED_RUNTIME(runtime)->Initialize(main_script);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_run_event_loop(node_embedding_runtime runtime) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoop();
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_run_event_loop_while(
    node_embedding_runtime runtime,
    node_embedding_event_loop_predicate predicate,
    void* predicate_data,
    node_embedding_event_loop_run_mode run_mode,
    bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->RunEventLoopWhile(
      predicate, predicate_data, run_mode, has_more_work);
}

node_embedding_exit_code NAPI_CDECL
node_embedding_runtime_await_promise(node_embedding_runtime runtime,
                                     napi_value promise,
                                     node_embedding_promise_state* state,
                                     napi_value* result,
                                     bool* has_more_work) {
  return EMBEDDED_RUNTIME(runtime)->AwaitPromise(
      promise, state, result, has_more_work);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_set_node_api_version(
    node_embedding_runtime runtime, int32_t node_api_version) {
  return EMBEDDED_RUNTIME(runtime)->SetNodeApiVersion(node_api_version);
}

node_embedding_exit_code NAPI_CDECL node_embedding_runtime_invoke_node_api(
    node_embedding_runtime runtime,
    node_embedding_node_api_callback node_api_cb,
    void* node_api_cb_data) {
  return EMBEDDED_RUNTIME(runtime)->InvokeNodeApi(node_api_cb,
                                                  node_api_cb_data);
}
