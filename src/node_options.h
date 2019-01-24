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

namespace node {

class HostPort {
 public:
  HostPort(const std::string& host_name, int port)
      : host_name_(host_name), port_(port) {}
  HostPort(const HostPort&) = default;
  HostPort& operator=(const HostPort&) = default;
  HostPort(HostPort&&) = default;
  HostPort& operator=(HostPort&&) = default;

  void set_host(const std::string& host) { host_name_ = host; }

  void set_port(int port) { port_ = port; }

  const std::string& host() const { return host_name_; }

  int port() const {
    // TODO(joyeecheung): make port a uint16_t
    CHECK_GE(port_, 0);
    return port_;
  }

  void Update(const HostPort& other) {
    if (!other.host_name_.empty()) host_name_ = other.host_name_;
    if (other.port_ >= 0) port_ = other.port_;
  }

 private:
  std::string host_name_;
  int port_;
};

class Options {
 public:
  virtual void CheckOptions(std::vector<std::string>* errors) {}
  virtual ~Options() {}
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

  // --inspect
  bool inspector_enabled = false;
  // --debug
  bool deprecated_debug = false;
  // --inspect-brk
  bool break_first_line = false;
  // --inspect-brk-node
  bool break_node_first_line = false;

  enum { kDefaultInspectorPort = 9229 };

  HostPort host_port{"127.0.0.1", kDefaultInspectorPort};

  bool deprecated_invocation() const {
    return deprecated_debug &&
      inspector_enabled &&
      break_first_line;
  }

  bool invalid_invocation() const {
    return deprecated_debug && !inspector_enabled;
  }

  bool wait_for_connect() const {
    return break_first_line || break_node_first_line;
  }

  void CheckOptions(std::vector<std::string>* errors) override;
};

class EnvironmentOptions : public Options {
 public:
  bool abort_on_uncaught_exception = false;
  bool experimental_modules = false;
  std::string experimental_policy;
  bool experimental_repl_await = false;
  bool experimental_vm_modules = false;
  bool expose_internals = false;
  bool frozen_intrinsics = false;
  std::string http_parser =
#ifdef NODE_EXPERIMENTAL_HTTP_DEFAULT
    "llhttp";
#else
    "legacy";
#endif
  bool no_deprecation = false;
  bool no_force_async_hooks_checks = false;
  bool no_warnings = false;
  bool pending_deprecation = false;
  bool preserve_symlinks = false;
  bool preserve_symlinks_main = false;
  bool prof_process = false;
  std::string redirect_warnings;
  bool throw_deprecation = false;
  bool trace_deprecation = false;
  bool trace_sync_io = false;
  bool trace_warnings = false;
  std::string userland_loader;

  bool syntax_check_only = false;
  bool has_eval_string = false;
#ifdef NODE_REPORT
  bool experimental_report = false;
#endif  //  NODE_REPORT
  std::string eval_string;
  bool print_eval = false;
  bool force_repl = false;

  std::vector<std::string> preload_modules;

  std::vector<std::string> user_argv;

  inline DebugOptions* get_debug_options();
  inline const DebugOptions& debug_options() const;
  void CheckOptions(std::vector<std::string>* errors) override;

 private:
  DebugOptions debug_options_;
};

class PerIsolateOptions : public Options {
 public:
  std::shared_ptr<EnvironmentOptions> per_env { new EnvironmentOptions() };
  bool track_heap_objects = false;

#ifdef NODE_REPORT
  bool report_uncaught_exception = false;
  bool report_on_signal = false;
  bool report_on_fatalerror = false;
  std::string report_signal;
  std::string report_filename;
  std::string report_directory;
#endif  //  NODE_REPORT
  inline EnvironmentOptions* get_per_env_options();
  void CheckOptions(std::vector<std::string>* errors) override;
};

class PerProcessOptions : public Options {
 public:
  std::shared_ptr<PerIsolateOptions> per_isolate { new PerIsolateOptions() };

  std::string title;
  std::string trace_event_categories;
  std::string trace_event_file_pattern = "node_trace.${rotation}.log";
  uint64_t max_http_header_size = 8 * 1024;
  int64_t v8_thread_pool_size = 4;
  bool zero_fill_all_buffers = false;
  bool debug_arraybuffer_allocations = false;

  std::vector<std::string> security_reverts;
  bool print_bash_completion = false;
  bool print_help = false;
  bool print_v8_help = false;
  bool print_version = false;

#ifdef NODE_HAVE_I18N_SUPPORT
  std::string icu_data_dir;
#endif

  // TODO(addaleax): Some of these could probably be per-Environment.
#if HAVE_OPENSSL
  std::string openssl_config;
  std::string tls_cipher_list = DEFAULT_CIPHER_LIST_CORE;
#ifdef NODE_OPENSSL_CERT_STORE
  bool ssl_openssl_cert_store = true;
#else
  bool ssl_openssl_cert_store = false;
#endif
  bool use_openssl_ca = false;
  bool use_bundled_ca = false;
#if NODE_FIPS_MODE
  bool enable_fips_crypto = false;
  bool force_fips_crypto = false;
#endif
#endif

#ifdef NODE_REPORT
  std::vector<std::string> cmdline;
#endif  //  NODE_REPORT

  inline PerIsolateOptions* get_per_isolate_options();
  void CheckOptions(std::vector<std::string>* errors) override;
};

// The actual options parser, as opposed to the structs containing them:

namespace options_parser {

HostPort SplitHostPort(const std::string& arg,
    std::vector<std::string>* errors);
void GetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);

enum OptionEnvvarSettings {
  kAllowedInEnvironment,
  kDisallowedInEnvironment
};

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

template <typename Options>
class OptionsParser {
 public:
  virtual ~OptionsParser() {}

  typedef Options TargetType;

  struct NoOp {};
  struct V8Option {};

  // These methods add a single option to the parser. Optionally, it can be
  // specified whether the option should be allowed from environment variable
  // sources (i.e. NODE_OPTIONS).
  void AddOption(const char* name,
                 const char* help_text,
                 bool Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 uint64_t Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 int64_t Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 std::string Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 std::vector<std::string> Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 HostPort Options::* field,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 NoOp no_op_tag,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);
  void AddOption(const char* name,
                 const char* help_text,
                 V8Option v8_option_tag,
                 OptionEnvvarSettings env_setting = kDisallowedInEnvironment);

  // Adds aliases. An alias can be of the form "--option-a" -> "--option-b",
  // or have a more complex group expansion, like
  //   "--option-a" -> { "--option-b", "--harmony-foobar", "--eval", "42" }
  // If `from` has the form "--option-a=", the alias will only be expanded if
  // the option is presented in that form (i.e. with a '=').
  // If `from` has the form "--option-a <arg>", the alias will only be expanded
  // if the option has a non-option argument (not starting with -) following it.
  void AddAlias(const char* from, const char* to);
  void AddAlias(const char* from, const std::vector<std::string>& to);
  void AddAlias(const char* from,
                const std::initializer_list<std::string>& to);

  // Add implications from some arbitrary option to a boolean one, either
  // in a way that makes `from` set `to` to true or to false.
  void Implies(const char* from, const char* to);
  void ImpliesNot(const char* from, const char* to);

  // Insert options from another options parser into this one, along with
  // a method that yields the target options type from this parser's options
  // type.
  template <typename ChildOptions>
  void Insert(const OptionsParser<ChildOptions>* child_options_parser,
              ChildOptions* (Options::* get_child)());

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
  virtual void Parse(std::vector<std::string>* const args,
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
    virtual ~BaseOptionField() {}
    virtual void* LookupImpl(Options* options) const = 0;

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

  template <typename T>
  inline T* Lookup(std::shared_ptr<BaseOptionField> field,
                   Options* options) const {
    return field->template Lookup<T>(options);
  }

  // An option consists of:
  // - A type.
  // - A way to store/access the property value.
  // - The information of whether it may occur in an env var or not.
  struct OptionInfo {
    OptionType type;
    std::shared_ptr<BaseOptionField> field;
    OptionEnvvarSettings env_setting;
    std::string help_text;
  };

  // An implied option is composed of the information on where to store a
  // specific boolean value (if another specific option is encountered).
  struct Implication {
    std::shared_ptr<BaseOptionField> target_field;
    bool target_value;
  };

  // These are helpers that make `Insert()` support properties of other
  // options structs, if we know how to access them.
  template <typename OriginalField, typename ChildOptions>
  static auto Convert(
      std::shared_ptr<OriginalField> original,
      ChildOptions* (Options::* get_child)());
  template <typename ChildOptions>
  static auto Convert(
      typename OptionsParser<ChildOptions>::OptionInfo original,
      ChildOptions* (Options::* get_child)());
  template <typename ChildOptions>
  static auto Convert(
      typename OptionsParser<ChildOptions>::Implication original,
      ChildOptions* (Options::* get_child)());

  std::unordered_map<std::string, OptionInfo> options_;
  std::unordered_map<std::string, std::vector<std::string>> aliases_;
  std::unordered_multimap<std::string, Implication> implications_;

  template <typename OtherOptions>
  friend class OptionsParser;

  friend void GetOptions(const v8::FunctionCallbackInfo<v8::Value>& args);
};

class DebugOptionsParser : public OptionsParser<DebugOptions> {
 public:
  DebugOptionsParser();

  static const DebugOptionsParser instance;
};

class EnvironmentOptionsParser : public OptionsParser<EnvironmentOptions> {
 public:
  EnvironmentOptionsParser();

  static const EnvironmentOptionsParser instance;
};

class PerIsolateOptionsParser : public OptionsParser<PerIsolateOptions> {
 public:
  PerIsolateOptionsParser();

  static const PerIsolateOptionsParser instance;
};

class PerProcessOptionsParser : public OptionsParser<PerProcessOptions> {
 public:
  PerProcessOptionsParser();

  static const PerProcessOptionsParser instance;
};

}  // namespace options_parser

namespace per_process {

extern Mutex cli_options_mutex;
extern std::shared_ptr<PerProcessOptions> cli_options;

}  // namespace per_process
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_OPTIONS_H_
