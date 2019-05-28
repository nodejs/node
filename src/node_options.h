#ifndef SRC_NODE_OPTIONS_H_
#define SRC_NODE_OPTIONS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "node_constants.h"

namespace node {

struct HostPort {
  std::string host_name;
  int port;

  void Update(const HostPort& other) {
    if (!other.host_name.empty()) host_name = other.host_name;
    if (other.port >= 0) port = other.port;
  }
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
  bool inspector_enabled = false;
  bool deprecated_debug = false;
  bool break_first_line = false;
  bool break_node_first_line = false;
  HostPort host_port = {"127.0.0.1", -1};
  enum { kDefaultInspectorPort = 9229 };

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

  const std::string& host() {
    return host_port.host_name;
  }

  int port() {
    return host_port.port < 0 ? kDefaultInspectorPort : host_port.port;
  }
};

class EnvironmentOptions : public Options {
 public:
  std::shared_ptr<DebugOptions> debug_options { new DebugOptions() };
  bool abort_on_uncaught_exception = false;
  bool experimental_modules = false;
  bool experimental_repl_await = false;
  bool experimental_vm_modules = false;
  bool experimental_worker = false;
  bool expose_internals = false;
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
  std::string unhandled_rejections;
  std::string userland_loader;

  bool syntax_check_only = false;
  bool has_eval_string = false;
  std::string eval_string;
  bool print_eval = false;
  bool force_repl = false;

  bool insecure_http_parser = false;
  bool tls_min_v1_0 = false;
  bool tls_min_v1_1 = false;
  bool tls_min_v1_2 = false;
  bool tls_max_v1_2 = false;

  std::vector<std::string> preload_modules;

  std::vector<std::string> user_argv;

  inline DebugOptions* get_debug_options();
  void CheckOptions(std::vector<std::string>* errors);
};

class PerIsolateOptions : public Options {
 public:
  std::shared_ptr<EnvironmentOptions> per_env { new EnvironmentOptions() };
  bool track_heap_objects = false;

  inline EnvironmentOptions* get_per_env_options();
  void CheckOptions(std::vector<std::string>* errors);
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

  inline PerIsolateOptions* get_per_isolate_options();
  void CheckOptions(std::vector<std::string>* errors);
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
  void Insert(OptionsParser<ChildOptions>* child_options_parser,
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
                     std::vector<std::string>* const errors);

 private:
  // We support the wide variety of different option types by remembering
  // how to access them, given a certain `Options` struct.

  // Represents a field within `Options`.
  class BaseOptionField {
   public:
    virtual ~BaseOptionField() {}
    virtual void* LookupImpl(Options* options) const = 0;
  };

  // Represents a field of type T within `Options`.
  template <typename T>
  class OptionField : public BaseOptionField {
   public:
    typedef T Type;

    T* Lookup(Options* options) const {
      return static_cast<T*>(this->LookupImpl(options));
    }
  };

  // Represents a field of type T within `Options` that can be looked up
  // as a C++ member field.
  template <typename T>
  class SimpleOptionField : public OptionField<T> {
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
    return std::static_pointer_cast<OptionField<T>>(field)->Lookup(options);
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
    std::shared_ptr<OptionField<bool>> target_field;
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

  static DebugOptionsParser instance;
};

class EnvironmentOptionsParser : public OptionsParser<EnvironmentOptions> {
 public:
  EnvironmentOptionsParser();

  static EnvironmentOptionsParser instance;
};

class PerIsolateOptionsParser : public OptionsParser<PerIsolateOptions> {
 public:
  PerIsolateOptionsParser();

  static PerIsolateOptionsParser instance;
};

class PerProcessOptionsParser : public OptionsParser<PerProcessOptions> {
 public:
  PerProcessOptionsParser();

  static PerProcessOptionsParser instance;
};

}  // namespace options_parser
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_OPTIONS_H_
