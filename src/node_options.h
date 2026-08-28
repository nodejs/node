#ifndef SRC_NODE_OPTIONS_H_
#define SRC_NODE_OPTIONS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "node_constants.h"
#include "node_mutex.h"
#include "util.h"

#if HAVE_OPENSSL
#include "openssl/opensslv.h"
#include "quic/guard.h"
#endif

namespace node {

// Helper macro for defining a 1-bit boolean field in a struct.
#define DEFINE_BOOL_FIELD(field) bool field : 1

class HostPort {
 public:
  HostPort(const std::string& host_name, int port)
      : host_name_(host_name), port_(port) {}
  HostPort(const HostPort&) = default;
  HostPort& operator=(const HostPort&) = default;
  HostPort(HostPort&&) = default;
  HostPort& operator=(HostPort&&) = default;

  void set_host(const std::string& host) { host_name_ = host; }

  void set_port(uint16_t port) { port_ = port; }

  const std::string& host() const { return host_name_; }

  uint16_t port() const { return port_; }

  void Update(const HostPort& other) {
    if (!other.host_name_.empty()) host_name_ = other.host_name_;
    port_ = other.port_;
  }

 private:
  std::string host_name_;
  uint16_t port_;
};

class Options {
 public:
  virtual void CheckOptions(std::vector<std::string>* errors,
                            std::vector<std::string>* argv) {}
  virtual ~Options() = default;
};

struct InspectPublishUid {
  DEFINE_BOOL_FIELD(console);
  DEFINE_BOOL_FIELD(http);
};

// These options are currently essentially per-Environment, but it can be nice
// to keep them separate since they are a group of options applying to a very
// specific part of Node. It might also make more sense for them to be
// per-Isolate, rather than per-Environment.
class DebugOptions : public Options {
 public:
  DebugOptions() = default;
  DebugOptions(const DebugOptions&) = default;
  DebugOptions& operator=(const DebugOptions&) = default;
  DebugOptions(DebugOptions&&) = default;
  DebugOptions& operator=(DebugOptions&&) = default;

  // --inspect-publish-uid
  std::string inspect_publish_uid_string = "stderr,http";

  DEFINE_BOOL_FIELD(allow_attaching_debugger) = true;
  // --inspect
  DEFINE_BOOL_FIELD(inspector_enabled) = false;
  // --inspect-wait
  DEFINE_BOOL_FIELD(inspect_wait) = false;
  // --debug
  DEFINE_BOOL_FIELD(deprecated_debug) = false;
  // --inspect-brk
  DEFINE_BOOL_FIELD(break_first_line) = false;
  // --inspect-brk-node
  DEFINE_BOOL_FIELD(break_node_first_line) = false;

  InspectPublishUid inspect_publish_uid;

  enum { kDefaultInspectorPort = 9229 };

  HostPort host_port{"127.0.0.1", kDefaultInspectorPort};

  // Used to patch the options as if --inspect-brk is passed.
  void EnableBreakFirstLine() {
    inspector_enabled = true;
    break_first_line = true;
  }

  void DisableWaitOrBreakFirstLine() {
    inspect_wait = false;
    break_first_line = false;
  }

  bool wait_for_connect() const {
    return break_first_line || break_node_first_line || inspect_wait;
  }

  bool should_break_first_line() const {
    return break_first_line || break_node_first_line;
  }

  void CheckOptions(std::vector<std::string>* errors,
                    std::vector<std::string>* argv) override;
};

#ifndef EXPERIMENTALS_DEFAULT_VALUE
#define EXPERIMENTALS_DEFAULT_VALUE false
#endif

class EnvironmentOptions : public Options {
 public:
#if HAVE_INSPECTOR
  static constexpr uint64_t kDefaultCpuProfInterval = 1000;
  static constexpr uint64_t kDefaultHeapProfInterval = 512 * 1024;
#endif
  static constexpr bool IS_DEBUG =
#ifdef DEBUG
      true;
#else
      false;
#endif  // DEBUG

  std::string dns_result_order;
  std::string localstorage_file;
  std::string input_type;  // Value of --input-type
  std::string heap_snapshot_signal;
  std::string redirect_warnings;
  std::string diagnostic_dir;
  std::string test_rerun_failures_path;
  std::string test_global_setup_path;
  std::string test_isolation = "process";
  std::string test_shard;
  std::string trace_require_module;
  std::string unhandled_rejections;
  std::string watch_mode_kill_signal = "SIGTERM";
  std::string eval_string;
  std::string tls_keylog;
  std::string experimental_config_file_path;
  std::string experimental_package_map_path;
#if HAVE_INSPECTOR
  std::string cpu_prof_dir;
  std::string cpu_prof_name;
  std::string heap_prof_dir;
  std::string heap_prof_name;
#endif  // HAVE_INSPECTOR

  std::vector<std::string> conditions;
  std::vector<std::string> allow_fs_read;
  std::vector<std::string> allow_fs_write;
  std::vector<std::string> disable_warnings;
  std::vector<std::string> env_file;
  std::vector<std::string> optional_env_file;
  std::vector<std::string> test_name_pattern;
  std::vector<std::string> test_reporter;
  std::vector<std::string> test_reporter_destination;
  std::vector<std::string> test_skip_pattern;
  std::vector<std::string> experimental_test_tag_filter;
  std::vector<std::string> coverage_include_pattern;
  std::vector<std::string> coverage_exclude_pattern;
  std::vector<std::string> userland_loaders;
  std::vector<std::string> watch_mode_paths;
  std::vector<std::string> preload_cjs_modules;
  std::vector<std::string> preload_esm_modules;
  std::vector<std::string> user_argv;

  int64_t heap_snapshot_near_heap_limit = 0;
  uint64_t network_family_autoselection_attempt_timeout = 500;
  uint64_t max_http_header_size = 16 * 1024;
  uint64_t test_runner_concurrency = 0;
  uint64_t test_runner_timeout = 0;
  uint64_t test_coverage_branches = 0;
  uint64_t test_coverage_functions = 0;
  uint64_t test_coverage_lines = 0;
  uint64_t test_random_seed = 0;
#if HAVE_INSPECTOR
  uint64_t cpu_prof_interval = kDefaultCpuProfInterval;
  uint64_t heap_prof_interval = kDefaultHeapProfInterval;
#endif  // HAVE_INSPECTOR

  // These are intentionally not bools. They are 1 bit packed flags in
  // order to reduce the overall size of the struct.

  DEFINE_BOOL_FIELD(abort_on_uncaught_exception) = false;
  DEFINE_BOOL_FIELD(detect_module) = true;
  DEFINE_BOOL_FIELD(disable_sigusr1) = false;
  DEFINE_BOOL_FIELD(print_required_tla) = false;
  DEFINE_BOOL_FIELD(require_module) = true;
  DEFINE_BOOL_FIELD(enable_source_maps) = false;
  DEFINE_BOOL_FIELD(experimental_addon_modules) = true;
  DEFINE_BOOL_FIELD(experimental_eventsource) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_ffi) = HAVE_FFI;
  DEFINE_BOOL_FIELD(experimental_web_worker) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_websocket) = true;
  DEFINE_BOOL_FIELD(experimental_sqlite) = HAVE_SQLITE;
  DEFINE_BOOL_FIELD(experimental_stream_iter) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_vfs) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(webstorage) = HAVE_SQLITE;
  DEFINE_BOOL_FIELD(experimental_dtls) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_quic) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_global_navigator) = true;
  DEFINE_BOOL_FIELD(experimental_global_web_crypto) = true;
  DEFINE_BOOL_FIELD(experimental_import_text) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_import_meta_resolve) =
      EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(entry_is_url) = false;
  DEFINE_BOOL_FIELD(permission) = false;
  DEFINE_BOOL_FIELD(permission_audit) = false;
  DEFINE_BOOL_FIELD(allow_addons) = false;
  DEFINE_BOOL_FIELD(allow_inspector) = false;
  DEFINE_BOOL_FIELD(allow_child_process) = false;
  DEFINE_BOOL_FIELD(allow_net) = false;
  DEFINE_BOOL_FIELD(allow_wasi) = false;
  DEFINE_BOOL_FIELD(allow_ffi) = false;
  DEFINE_BOOL_FIELD(allow_openssl_store) = false;
  DEFINE_BOOL_FIELD(allow_worker_threads) = false;
  DEFINE_BOOL_FIELD(experimental_repl_await) = true;
  DEFINE_BOOL_FIELD(experimental_vm_modules) = EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(async_context_frame) = true;
  DEFINE_BOOL_FIELD(expose_internals) = false;
  DEFINE_BOOL_FIELD(force_node_api_uncaught_exceptions_policy) = false;
  DEFINE_BOOL_FIELD(frozen_intrinsics) = false;
  DEFINE_BOOL_FIELD(network_family_autoselection) = true;
  DEFINE_BOOL_FIELD(deprecation) = true;
  DEFINE_BOOL_FIELD(force_async_hooks_checks) = true;
  DEFINE_BOOL_FIELD(allow_native_addons) = true;
  DEFINE_BOOL_FIELD(global_search_paths) = true;
  DEFINE_BOOL_FIELD(warnings) = true;
  DEFINE_BOOL_FIELD(force_context_aware) = false;
  DEFINE_BOOL_FIELD(pending_deprecation) = false;
  DEFINE_BOOL_FIELD(preserve_symlinks) = false;
  DEFINE_BOOL_FIELD(preserve_symlinks_main) = false;
  DEFINE_BOOL_FIELD(prof_process) = false;
  DEFINE_BOOL_FIELD(has_env_file_string) = false;
  DEFINE_BOOL_FIELD(test_runner) = false;
  DEFINE_BOOL_FIELD(test_runner_coverage) = false;
  DEFINE_BOOL_FIELD(test_runner_force_exit) = false;
  DEFINE_BOOL_FIELD(test_runner_module_mocks) = false;
  DEFINE_BOOL_FIELD(test_runner_update_snapshots) = false;
  DEFINE_BOOL_FIELD(test_randomize) = false;
  DEFINE_BOOL_FIELD(has_test_random_seed) = false;
  DEFINE_BOOL_FIELD(test_only) = false;
  DEFINE_BOOL_FIELD(test_udp_no_try_send) = false;
  DEFINE_BOOL_FIELD(coverage_include_all) = false;
  DEFINE_BOOL_FIELD(throw_deprecation) = false;
  DEFINE_BOOL_FIELD(trace_deprecation) = false;
  DEFINE_BOOL_FIELD(trace_exit) = false;
  DEFINE_BOOL_FIELD(trace_sync_io) = false;
  DEFINE_BOOL_FIELD(trace_tls) = false;
  DEFINE_BOOL_FIELD(trace_uncaught) = false;
  DEFINE_BOOL_FIELD(trace_warnings) = false;
  DEFINE_BOOL_FIELD(trace_promises) = false;
  DEFINE_BOOL_FIELD(trace_env) = false;
  DEFINE_BOOL_FIELD(trace_env_js_stack) = false;
  DEFINE_BOOL_FIELD(trace_env_native_stack) = false;
  DEFINE_BOOL_FIELD(use_system_ca) = false;
  DEFINE_BOOL_FIELD(extra_info_on_fatal_exception) = true;
  DEFINE_BOOL_FIELD(verify_base_objects) = IS_DEBUG;
  DEFINE_BOOL_FIELD(watch_mode) = false;
  DEFINE_BOOL_FIELD(watch_mode_report_to_parent) = false;
  DEFINE_BOOL_FIELD(watch_mode_preserve_output) = false;
  DEFINE_BOOL_FIELD(syntax_check_only) = false;
  DEFINE_BOOL_FIELD(has_eval_string) = false;
  DEFINE_BOOL_FIELD(print_eval) = false;
  DEFINE_BOOL_FIELD(force_repl) = false;
  DEFINE_BOOL_FIELD(insecure_http_parser) = false;
  DEFINE_BOOL_FIELD(use_env_proxy) = false;
  DEFINE_BOOL_FIELD(tls_min_v1_0) = false;
  DEFINE_BOOL_FIELD(tls_min_v1_1) = false;
  DEFINE_BOOL_FIELD(tls_min_v1_2) = false;
  DEFINE_BOOL_FIELD(tls_min_v1_3) = false;
  DEFINE_BOOL_FIELD(tls_max_v1_2) = false;
  DEFINE_BOOL_FIELD(tls_max_v1_3) = false;
  DEFINE_BOOL_FIELD(strip_types) = HAVE_AMARO;
  DEFINE_BOOL_FIELD(report_exclude_env) = false;
  DEFINE_BOOL_FIELD(report_exclude_network) = false;
#if HAVE_INSPECTOR
  DEFINE_BOOL_FIELD(cpu_prof) = false;
  DEFINE_BOOL_FIELD(experimental_network_inspection) =
      EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_worker_inspection) =
      EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_storage_inspection) =
      EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(experimental_inspector_network_resource) =
      EXPERIMENTALS_DEFAULT_VALUE;
  DEFINE_BOOL_FIELD(heap_prof) = false;
#endif  // HAVE_INSPECTOR

  inline DebugOptions* get_debug_options() { return &debug_options_; }
  inline const DebugOptions& debug_options() const { return debug_options_; }

  void CheckOptions(std::vector<std::string>* errors,
                    std::vector<std::string>* argv) override;

 private:
  DebugOptions debug_options_;
};

class PerIsolateOptions : public Options {
 public:
  PerIsolateOptions() = default;
  PerIsolateOptions(PerIsolateOptions&&) = default;

  std::shared_ptr<EnvironmentOptions> per_env{new EnvironmentOptions()};

  std::string max_old_space_size_percentage;
  std::string max_old_space_size;
  std::string report_signal = "SIGUSR2";
  std::string build_snapshot_config;

  int64_t stack_trace_limit = 10;

  DEFINE_BOOL_FIELD(track_heap_objects) = false;
  DEFINE_BOOL_FIELD(report_uncaught_exception) = false;
  DEFINE_BOOL_FIELD(report_on_signal) = false;
  DEFINE_BOOL_FIELD(experimental_shadow_realm) = false;
  DEFINE_BOOL_FIELD(build_snapshot) = false;

  inline EnvironmentOptions* get_per_env_options();
  void CheckOptions(std::vector<std::string>* errors,
                    std::vector<std::string>* argv) override;
  void HandleMaxOldSpaceSizePercentage(std::vector<std::string>* errors,
                                       std::string* max_old_space_size);

  inline std::shared_ptr<PerIsolateOptions> Clone() const;

 private:
  PerIsolateOptions(const PerIsolateOptions&) = default;
};

class PerProcessOptions : public Options {
 public:
  // Options shouldn't be here unless they affect the entire process scope, and
  // that should avoided when possible.
  //
  // When an option is used during process initialization, it does not need
  // protection, but any use after that will likely require synchronization
  // using the node::per_process::cli_options_mutex, typically:
  //
  //     Mutex::ScopedLock lock(node::per_process::cli_options_mutex);
  std::shared_ptr<PerIsolateOptions> per_isolate{new PerIsolateOptions()};

  std::string title;
  std::string trace_event_categories;
#if defined(V8_USE_PERFETTO)
  std::string trace_event_file_pattern = "node_trace.${rotation}.pftrace";
#else
  std::string trace_event_file_pattern = "node_trace.${rotation}.log";
#endif
  std::string disable_proto;
  std::string snapshot_blob;
  std::string experimental_sea_config;
  std::string run;
  std::string build_sea;
#ifdef NODE_HAVE_I18N_SUPPORT
  std::string icu_data_dir;
#endif
// Per-process because they affect singleton OpenSSL shared library state,
// or are used once during process initialization.
#if HAVE_OPENSSL
  std::string openssl_config;
  std::string tls_cipher_list = DEFAULT_CIPHER_LIST_CORE;
#endif  // HAVE_OPENSSL
  std::string report_directory;
  std::string report_filename;
  // TODO(addaleax): Some of these could probably be per-Environment.
  std::string use_largepages = "off";

  std::vector<std::string> security_reverts;
  std::vector<std::string> cmdline;

  int64_t v8_thread_pool_size = 4;
#if HAVE_OPENSSL
  int64_t secure_heap = 0;
  int64_t secure_heap_min = 2;
#endif  // HAVE_OPENSSL

  DEFINE_BOOL_FIELD(zero_fill_all_buffers) = false;
  DEFINE_BOOL_FIELD(debug_arraybuffer_allocations) = false;

  // We enable the shared read-only heap which currently requires that the
  // snapshot used in different isolates in the same process to be the same.
  // Therefore --node-snapshot is a per-process option.
  DEFINE_BOOL_FIELD(node_snapshot) = true;

  DEFINE_BOOL_FIELD(print_bash_completion) = false;
  DEFINE_BOOL_FIELD(print_help) = false;
  DEFINE_BOOL_FIELD(print_v8_help) = false;
  DEFINE_BOOL_FIELD(print_version) = false;

#if HAVE_OPENSSL
#ifdef NODE_OPENSSL_CERT_STORE
  DEFINE_BOOL_FIELD(ssl_openssl_cert_store) = true;
#else
  DEFINE_BOOL_FIELD(ssl_openssl_cert_store) = false;
#endif  // NODE_OPENSSL_CERT_STORE
  DEFINE_BOOL_FIELD(use_openssl_ca) = false;
  DEFINE_BOOL_FIELD(use_bundled_ca) = false;
  DEFINE_BOOL_FIELD(enable_fips_crypto) = false;
  DEFINE_BOOL_FIELD(force_fips_crypto) = false;
#endif  // HAVE_OPENSSL
#if OPENSSL_VERSION_MAJOR >= 3
  DEFINE_BOOL_FIELD(openssl_legacy_provider) = false;
  DEFINE_BOOL_FIELD(openssl_shared_config) = false;
#endif

  DEFINE_BOOL_FIELD(disable_wasm_trap_handler) = false;
  DEFINE_BOOL_FIELD(report_on_fatalerror) = false;
  DEFINE_BOOL_FIELD(report_compact) = false;
  DEFINE_BOOL_FIELD(trace_sigint) = false;
  // Tracks whether `--run` was passed, since an empty `run` is ambiguous
  // between "not passed" and "passed without a script name" (lists scripts).
  DEFINE_BOOL_FIELD(has_run) = false;

  inline PerIsolateOptions* get_per_isolate_options();
  void CheckOptions(std::vector<std::string>* errors,
                    std::vector<std::string>* argv) override;
};

// The actual options parser, as opposed to the structs containing them:

namespace options_parser {

HostPort SplitHostPort(const std::string& arg,
                       std::vector<std::string>* errors);
void GetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
std::string GetBashCompletion();

enum OptionType {
  kNoOp,
  kV8Option,
  kBoolean,
  kInteger,
  kUInteger,
  kString,
  kHostPort,
  kStringList,
};
struct OptionMappingDetails {
  OptionType type;
  std::string help_text;
};
std::unordered_map<std::string, OptionMappingDetails>
MapEnvOptionsFlagInputType();
std::unordered_map<std::string, OptionMappingDetails> MapOptionsByNamespace(
    std::string namespace_name);
std::unordered_map<
    std::string,
    std::unordered_map<std::string, options_parser::OptionMappingDetails>>
MapNamespaceOptionsAssociations();
std::vector<std::string> MapAvailableNamespaces();

// Define all namespace entries
#define OPTION_NAMESPACE_LIST(V)                                               \
  V(kNoNamespace, "")                                                          \
  V(kTestRunnerNamespace, "test")                                              \
  V(kWatchNamespace, "watch")                                                  \
  V(kPermissionNamespace, "permission")

enum class OptionNamespaces {
#define V(name, _) name,
  OPTION_NAMESPACE_LIST(V)
#undef V
};

inline const std::string NamespaceEnumToString(OptionNamespaces ns) {
  switch (ns) {
#define V(name, string_value)                                                  \
  case OptionNamespaces::name:                                                 \
    return string_value;
    OPTION_NAMESPACE_LIST(V)
#undef V
    default:
      return "";
  }
}

inline constexpr auto AllNamespaces() {
  return std::array{
#define V(name, _) OptionNamespaces::name,
      OPTION_NAMESPACE_LIST(V)
#undef V
  };
}

template <typename Options>
class OptionsParser {
 public:
  virtual ~OptionsParser() = default;

  typedef Options TargetType;

  struct NoOp {};
  struct V8Option {};

  // These methods add a single option to the parser. Optionally, it can be
  // specified whether the option should be allowed from environment variable
  // sources (i.e. NODE_OPTIONS).

  // default_is_true is only a hint in printing help text, it does not
  // affect the default value of the option. Set the default value in the
  // Options struct instead.
  void AddOption(
      const char* name,
      const char* help_text,
      bool (*getter)(Options*),
      void (*setter)(Options*, bool),
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      bool default_is_true = false,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      uint64_t Options::*field,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      int64_t Options::*field,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      std::string Options::*field,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      std::vector<std::string> Options::*field,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      HostPort Options::*field,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      NoOp no_op_tag,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);
  void AddOption(
      const char* name,
      const char* help_text,
      V8Option v8_option_tag,
      OptionEnvvarSettings env_setting = kDisallowedInEnvvar,
      OptionNamespaces namespace_id = OptionNamespaces::kNoNamespace);

  // Adds aliases. An alias can be of the form "--option-a" -> "--option-b",
  // or have a more complex group expansion, like
  //   "--option-a" -> { "--option-b", "--harmony-foobar", "--eval", "42" }
  // If `from` has the form "--option-a=", the alias will only be expanded if
  // the option is presented in that form (i.e. with a '=').
  // If `from` has the form "--option-a <arg>", the alias will only be expanded
  // if the option has a non-option argument (not starting with -) following it.
  void AddAlias(const char* from, const char* to);
  void AddAlias(const char* from, const std::vector<std::string>& to);
  void AddAlias(const char* from, const std::initializer_list<std::string>& to);

  // Add implications from some arbitrary option to a boolean one, either
  // in a way that makes `from` set `to` to true or to false.
  void Implies(const char* from, const char* to);
  void ImpliesNot(const char* from, const char* to);

  // Insert options from another options parser into this one, along with
  // a method that yields the target options type from this parser's options
  // type.
  template <typename ChildOptions>
  void Insert(const OptionsParser<ChildOptions>& child_options_parser,
              ChildOptions* (Options::*get_child)());

  // Parse a sequence of options into an options struct, a list of
  // arguments that were parsed as options, a list of unknown/JS engine options,
  // and leave the remainder in the input `args` vector.
  //
  // For example, an `args` input of
  //
  //   node --foo --harmony-bar --fizzle=42 -- /path/to/cow moo
  //
  // expands as
  //
  // - `args` -> { "node", "/path/to/cow", "moo" }
  // - `exec_args` -> { "--foo", "--harmony-bar", "--fizzle=42" }
  // - `v8_args` -> `{ "node", "--harmony-bar" }
  // - `options->foo == true`, `options->fizzle == 42`.
  //
  // If `*error` is set, the result of the parsing should be discarded and the
  // contents of any of the argument vectors should be considered undefined.
  void Parse(std::vector<std::string>* const args,
             std::vector<std::string>* const exec_args,
             std::vector<std::string>* const v8_args,
             Options* const options,
             OptionEnvvarSettings required_env_settings,
             std::vector<std::string>* const errors) const;

 private:
  // We support the wide variety of different option types by remembering
  // how to access them, given a certain `Options` struct.

  // Represents a field within `Options`.
  class BaseOptionField {
   public:
    virtual ~BaseOptionField() = default;
    virtual void* LookupImpl(Options* options) const = 0;

    // For boolean bit-field options, we can't return a pointer so we
    // use virtual getter/setter methods instead. The default
    // implementations go through LookupImpl for non-bit-field types.
    virtual bool GetBool(Options* options) const {
      return *static_cast<bool*>(LookupImpl(options));
    }
    virtual void SetBool(Options* options, bool value) {
      *static_cast<bool*>(LookupImpl(options)) = value;
    }

    template <typename T>
    inline T* Lookup(Options* options) const {
      return static_cast<T*>(LookupImpl(options));
    }
  };

  // Represents a field of type T within `Options` that can be looked up
  // as a C++ member field.
  template <typename T>
  class SimpleOptionField : public BaseOptionField {
   public:
    explicit SimpleOptionField(T Options::* field) : field_(field) {}
    void* LookupImpl(Options* options) const override {
      return static_cast<void*>(&(options->*field_));
    }

   private:
    T Options::* field_;
  };

  // Represents a boolean bit-field within `Options`. Bit-fields cannot
  // have their address taken, so we use getter/setter function pointers
  // instead of a pointer-to-member.
  class BitFieldOptionField : public BaseOptionField {
   public:
    using Getter = bool (*)(Options*);
    using Setter = void (*)(Options*, bool);

    BitFieldOptionField(Getter get, Setter set) : get_(get), set_(set) {}

    void* LookupImpl(Options* options) const override { UNREACHABLE(); }

    bool GetBool(Options* options) const override { return get_(options); }
    void SetBool(Options* options, bool value) override {
      set_(options, value);
    }

   private:
    Getter get_;
    Setter set_;
  };

  template <typename T>
  inline T* Lookup(std::shared_ptr<BaseOptionField> field,
                   Options* options) const {
    return field->template Lookup<T>(options);
  }

  // An option consists of:
  // - A type.
  // - A way to store/access the property value.
  // - The information of whether it may occur in an env var or not.
  // - A default value (if applicable).
  // - A namespace ID (optional) to allow for namespacing of options.
  struct OptionInfo {
    OptionType type;
    std::shared_ptr<BaseOptionField> field;
    OptionEnvvarSettings env_setting;
    std::string help_text;
    bool default_is_true = false;
    std::string namespace_id;
  };

  // An implied option is composed of the information on where to store a
  // specific boolean value (if another specific option is encountered).
  struct Implication {
    OptionType type;
    std::string name;
    std::shared_ptr<BaseOptionField> target_field;
    bool target_value;
  };

  // These are helpers that make `Insert()` support properties of other
  // options structs, if we know how to access them.
  template <typename OriginalField, typename ChildOptions>
  static auto Convert(std::shared_ptr<OriginalField> original,
                      ChildOptions* (Options::*get_child)());
  template <typename ChildOptions>
  static auto Convert(typename OptionsParser<ChildOptions>::OptionInfo original,
                      ChildOptions* (Options::*get_child)());
  template <typename ChildOptions>
  static auto Convert(
      typename OptionsParser<ChildOptions>::Implication original,
      ChildOptions* (Options::*get_child)());

  std::unordered_map<std::string, OptionInfo> options_;
  std::unordered_map<std::string, std::vector<std::string>> aliases_;
  std::unordered_multimap<std::string, Implication> implications_;

  template <typename OtherOptions>
  friend class OptionsParser;

  friend void GetCLIOptionsValues(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  friend void GetCLIOptionsInfo(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  friend std::string GetBashCompletion();
  friend std::unordered_map<std::string, OptionMappingDetails>
  MapEnvOptionsFlagInputType();
  friend std::unordered_map<std::string, OptionMappingDetails>
  MapOptionsByNamespace(std::string namespace_name);
  friend std::vector<std::string> MapAvailableNamespaces();
  friend void GetEnvOptionsInputType(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  friend void GetOptionsAsFlags(
      const v8::FunctionCallbackInfo<v8::Value>& args);
};

using StringVector = std::vector<std::string>;
template <class OptionsType, class = Options>
void Parse(StringVector* const args,
           StringVector* const exec_args,
           StringVector* const v8_args,
           OptionsType* const options,
           OptionEnvvarSettings required_env_settings,
           StringVector* const errors);

}  // namespace options_parser

// Helper macro for passing bit-field members to AddOption(). Expands to a
// getter and setter function pointer pair that can read/write the named
// bit-field without requiring a pointer-to-member (which is illegal for
// bit-fields).
#define BOOL_FIELD(field)                                                      \
  +[](TargetType* opts) -> bool { return opts->field; },                       \
      +[](TargetType* opts, bool v) { opts->field = v; }

namespace per_process {

extern Mutex cli_options_mutex;
extern NODE_EXTERN_PRIVATE std::shared_ptr<PerProcessOptions> cli_options;

}  // namespace per_process

void HandleEnvOptions(std::shared_ptr<EnvironmentOptions> env_options);
void HandleEnvOptions(std::shared_ptr<EnvironmentOptions> env_options,
                      std::function<std::string(const char*)> opt_getter);

std::vector<std::string> ParseNodeOptionsEnvVar(
    const std::string& node_options, std::vector<std::string>* errors);

#undef DEFINE_BOOL_FIELD
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_OPTIONS_H_
