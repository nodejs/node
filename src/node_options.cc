#include "node_options.h"  // NOLINT(build/include_inline)
#include "node_options-inl.h"

#include "env-inl.h"
#include "node_binding.h"
#include "node_external_reference.h"
#include "node_internals.h"
#if HAVE_OPENSSL
#include "openssl/opensslv.h"
#endif

#include <errno.h>
#include <sstream>
#include <limits>
#include <algorithm>
#include <cstdlib>  // strtoul, errno

using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Map;
using v8::Number;
using v8::Object;
using v8::Undefined;
using v8::Value;

namespace node {

namespace per_process {
Mutex cli_options_mutex;
std::shared_ptr<PerProcessOptions> cli_options{new PerProcessOptions()};
}  // namespace per_process

void DebugOptions::CheckOptions(std::vector<std::string>* errors) {
#if !NODE_USE_V8_PLATFORM && !HAVE_INSPECTOR
  if (inspector_enabled) {
    errors->push_back("Inspector is not available when Node is compiled "
                      "--without-v8-platform and --without-inspector.");
  }
#endif

  if (deprecated_debug) {
    errors->push_back("[DEP0062]: `node --debug` and `node --debug-brk` "
                      "are invalid. Please use `node --inspect` and "
                      "`node --inspect-brk` instead.");
  }

  std::vector<std::string> destinations =
      SplitString(inspect_publish_uid_string, ',');
  inspect_publish_uid.console = false;
  inspect_publish_uid.http = false;
  for (const std::string& destination : destinations) {
    if (destination == "stderr") {
      inspect_publish_uid.console = true;
    } else if (destination == "http") {
      inspect_publish_uid.http = true;
    } else {
      errors->push_back("--inspect-publish-uid destination can be "
                        "stderr or http");
    }
  }
}

void PerProcessOptions::CheckOptions(std::vector<std::string>* errors) {
#if HAVE_OPENSSL
  if (use_openssl_ca && use_bundled_ca) {
    errors->push_back("either --use-openssl-ca or --use-bundled-ca can be "
                      "used, not both");
  }

  // Any value less than 2 disables use of the secure heap.
  if (secure_heap >= 2) {
    if ((secure_heap & (secure_heap - 1)) != 0)
      errors->push_back("--secure-heap must be a power of 2");
    secure_heap_min =
        std::min({
            secure_heap,
            secure_heap_min,
            static_cast<int64_t>(std::numeric_limits<int>::max())});
    secure_heap_min = std::max(static_cast<int64_t>(2), secure_heap_min);
    if ((secure_heap_min & (secure_heap_min - 1)) != 0)
      errors->push_back("--secure-heap-min must be a power of 2");
  }
#endif  // HAVE_OPENSSL

  if (use_largepages != "off" &&
      use_largepages != "on" &&
      use_largepages != "silent") {
    errors->push_back("invalid value for --use-largepages");
  }
  per_isolate->CheckOptions(errors);
}

void PerIsolateOptions::CheckOptions(std::vector<std::string>* errors) {
  per_env->CheckOptions(errors);
}

void EnvironmentOptions::CheckOptions(std::vector<std::string>* errors) {
  if (has_policy_integrity_string && experimental_policy.empty()) {
    errors->push_back("--policy-integrity requires "
                      "--experimental-policy be enabled");
  }
  if (has_policy_integrity_string && experimental_policy_integrity.empty()) {
    errors->push_back("--policy-integrity cannot be empty");
  }

  if (!module_type.empty()) {
    if (module_type != "commonjs" && module_type != "module") {
      errors->push_back("--input-type must be \"module\" or \"commonjs\"");
    }
  }

  if (!experimental_specifier_resolution.empty()) {
    if (experimental_specifier_resolution != "node" &&
        experimental_specifier_resolution != "explicit") {
      errors->push_back(
        "invalid value for --experimental-specifier-resolution");
    }
  }

  if (syntax_check_only && has_eval_string) {
    errors->push_back("either --check or --eval can be used, not both");
  }

  if (!unhandled_rejections.empty() &&
      unhandled_rejections != "warn-with-error-code" &&
      unhandled_rejections != "throw" &&
      unhandled_rejections != "strict" &&
      unhandled_rejections != "warn" &&
      unhandled_rejections != "none") {
    errors->push_back("invalid value for --unhandled-rejections");
  }

  if (tls_min_v1_3 && tls_max_v1_2) {
    errors->push_back("either --tls-min-v1.3 or --tls-max-v1.2 can be "
                      "used, not both");
  }

  if (heap_snapshot_near_heap_limit < 0) {
    errors->push_back("--heap-snapshot-near-heap-limit must not be negative");
  }

  if (test_runner) {
    if (syntax_check_only) {
      errors->push_back("either --test or --check can be used, not both");
    }

    if (has_eval_string) {
      errors->push_back("either --test or --eval can be used, not both");
    }

    if (force_repl) {
      errors->push_back("either --test or --interactive can be used, not both");
    }

    if (debug_options_.inspector_enabled) {
      errors->push_back("the inspector cannot be used with --test");
    }
  }

#if HAVE_INSPECTOR
  if (!cpu_prof) {
    if (!cpu_prof_name.empty()) {
      errors->push_back("--cpu-prof-name must be used with --cpu-prof");
    }
    if (!cpu_prof_dir.empty()) {
      errors->push_back("--cpu-prof-dir must be used with --cpu-prof");
    }
    // We can't catch the case where the value passed is the default value,
    // then the option just becomes a noop which is fine.
    if (cpu_prof_interval != kDefaultCpuProfInterval) {
      errors->push_back("--cpu-prof-interval must be used with --cpu-prof");
    }
  }

  if (cpu_prof && cpu_prof_dir.empty() && !diagnostic_dir.empty()) {
      cpu_prof_dir = diagnostic_dir;
    }

  if (!heap_prof) {
    if (!heap_prof_name.empty()) {
      errors->push_back("--heap-prof-name must be used with --heap-prof");
    }
    if (!heap_prof_dir.empty()) {
      errors->push_back("--heap-prof-dir must be used with --heap-prof");
    }
    // We can't catch the case where the value passed is the default value,
    // then the option just becomes a noop which is fine.
    if (heap_prof_interval != kDefaultHeapProfInterval) {
      errors->push_back("--heap-prof-interval must be used with --heap-prof");
    }
  }

  if (heap_prof && heap_prof_dir.empty() && !diagnostic_dir.empty()) {
    heap_prof_dir = diagnostic_dir;
  }

  debug_options_.CheckOptions(errors);
#endif  // HAVE_INSPECTOR
}

namespace options_parser {

class DebugOptionsParser : public OptionsParser<DebugOptions> {
 public:
  DebugOptionsParser();
};

class EnvironmentOptionsParser : public OptionsParser<EnvironmentOptions> {
 public:
  EnvironmentOptionsParser();
  explicit EnvironmentOptionsParser(const DebugOptionsParser& dop)
    : EnvironmentOptionsParser() {
    Insert(dop, &EnvironmentOptions::get_debug_options);
  }
};

class PerIsolateOptionsParser : public OptionsParser<PerIsolateOptions> {
 public:
  PerIsolateOptionsParser() = delete;
  explicit PerIsolateOptionsParser(const EnvironmentOptionsParser& eop);
};

class PerProcessOptionsParser : public OptionsParser<PerProcessOptions> {
 public:
  PerProcessOptionsParser() = delete;
  explicit PerProcessOptionsParser(const PerIsolateOptionsParser& iop);
};

#if HAVE_INSPECTOR
const DebugOptionsParser _dop_instance{};
const EnvironmentOptionsParser _eop_instance{_dop_instance};

// This Parse is not dead code. It is used by embedders (e.g., Electron).
template <>
void Parse(
  StringVector* const args, StringVector* const exec_args,
  StringVector* const v8_args,
  DebugOptions* const options,
  OptionEnvvarSettings required_env_settings, StringVector* const errors) {
  _dop_instance.Parse(
    args, exec_args, v8_args, options, required_env_settings, errors);
}
#else
const EnvironmentOptionsParser _eop_instance{};
#endif  // HAVE_INSPECTOR
const PerIsolateOptionsParser _piop_instance{_eop_instance};
const PerProcessOptionsParser _ppop_instance{_piop_instance};

template <>
void Parse(
  StringVector* const args, StringVector* const exec_args,
  StringVector* const v8_args,
  PerIsolateOptions* const options,
  OptionEnvvarSettings required_env_settings, StringVector* const errors) {
  _piop_instance.Parse(
    args, exec_args, v8_args, options, required_env_settings, errors);
}

template <>
void Parse(
  StringVector* const args, StringVector* const exec_args,
  StringVector* const v8_args,
  PerProcessOptions* const options,
  OptionEnvvarSettings required_env_settings, StringVector* const errors) {
  _ppop_instance.Parse(
    args, exec_args, v8_args, options, required_env_settings, errors);
}

// XXX: If you add an option here, please also add it to doc/node.1 and
// doc/api/cli.md
// TODO(addaleax): Make that unnecessary.

DebugOptionsParser::DebugOptionsParser() {
  AddOption("--inspect-port",
            "set host:port for inspector",
            &DebugOptions::host_port,
            kAllowedInEnvironment);
  AddAlias("--debug-port", "--inspect-port");

  AddOption("--inspect",
            "activate inspector on host:port (default: 127.0.0.1:9229)",
            &DebugOptions::inspector_enabled,
            kAllowedInEnvironment);
  AddAlias("--inspect=", { "--inspect-port", "--inspect" });

  AddOption("--debug", "", &DebugOptions::deprecated_debug);
  AddAlias("--debug=", "--debug");
  AddOption("--debug-brk", "", &DebugOptions::deprecated_debug);
  AddAlias("--debug-brk=", "--debug-brk");

  AddOption("--inspect-brk",
            "activate inspector on host:port and break at start of user script",
            &DebugOptions::break_first_line,
            kAllowedInEnvironment);
  Implies("--inspect-brk", "--inspect");
  AddAlias("--inspect-brk=", { "--inspect-port", "--inspect-brk" });

  AddOption("--inspect-brk-node", "", &DebugOptions::break_node_first_line);
  Implies("--inspect-brk-node", "--inspect");
  AddAlias("--inspect-brk-node=", { "--inspect-port", "--inspect-brk-node" });

  AddOption("--inspect-publish-uid",
            "comma separated list of destinations for inspector uid"
            "(default: stderr,http)",
            &DebugOptions::inspect_publish_uid_string,
            kAllowedInEnvironment);
}

EnvironmentOptionsParser::EnvironmentOptionsParser() {
  AddOption("--conditions",
            "additional user conditions for conditional exports and imports",
            &EnvironmentOptions::conditions,
            kAllowedInEnvironment);
  AddAlias("-C", "--conditions");
  AddOption("--diagnostic-dir",
            "set dir for all output files"
            " (default: current working directory)",
            &EnvironmentOptions::diagnostic_dir,
            kAllowedInEnvironment);
  AddOption("--dns-result-order",
            "set default value of verbatim in dns.lookup. Options are "
            "'ipv4first' (IPv4 addresses are placed before IPv6 addresses) "
            "'verbatim' (addresses are in the order the DNS resolver "
            "returned)",
            &EnvironmentOptions::dns_result_order,
            kAllowedInEnvironment);
  AddOption("--enable-source-maps",
            "Source Map V3 support for stack traces",
            &EnvironmentOptions::enable_source_maps,
            kAllowedInEnvironment);
  AddOption("--experimental-abortcontroller", "",
            NoOp{}, kAllowedInEnvironment);
  AddOption("--experimental-fetch",
            "experimental Fetch API",
            &EnvironmentOptions::experimental_fetch,
            kAllowedInEnvironment,
            true);
  AddOption("--experimental-global-webcrypto",
            "expose experimental Web Crypto API on the global scope",
            &EnvironmentOptions::experimental_global_web_crypto,
            kAllowedInEnvironment);
  AddOption("--experimental-json-modules", "", NoOp{}, kAllowedInEnvironment);
  AddOption("--experimental-loader",
            "use the specified module as a custom loader",
            &EnvironmentOptions::userland_loaders,
            kAllowedInEnvironment);
  AddAlias("--loader", "--experimental-loader");
  AddOption("--experimental-modules", "", NoOp{}, kAllowedInEnvironment);
  AddOption("--experimental-network-imports",
            "experimental https: support for the ES Module loader",
            &EnvironmentOptions::experimental_https_modules,
            kAllowedInEnvironment);
  AddOption("--experimental-wasm-modules",
            "experimental ES Module support for webassembly modules",
            &EnvironmentOptions::experimental_wasm_modules,
            kAllowedInEnvironment);
  AddOption("--experimental-import-meta-resolve",
            "experimental ES Module import.meta.resolve() support",
            &EnvironmentOptions::experimental_import_meta_resolve,
            kAllowedInEnvironment);
  AddOption("--experimental-policy",
            "use the specified file as a "
            "security policy",
            &EnvironmentOptions::experimental_policy,
            kAllowedInEnvironment);
  AddOption("[has_policy_integrity_string]",
            "",
            &EnvironmentOptions::has_policy_integrity_string);
  AddOption("--policy-integrity",
            "ensure the security policy contents match "
            "the specified integrity",
            &EnvironmentOptions::experimental_policy_integrity,
            kAllowedInEnvironment);
  Implies("--policy-integrity", "[has_policy_integrity_string]");
  AddOption("--experimental-repl-await",
            "experimental await keyword support in REPL",
            &EnvironmentOptions::experimental_repl_await,
            kAllowedInEnvironment,
            true);
  AddOption("--experimental-vm-modules",
            "experimental ES Module support in vm module",
            &EnvironmentOptions::experimental_vm_modules,
            kAllowedInEnvironment);
  AddOption("--experimental-worker", "", NoOp{}, kAllowedInEnvironment);
  AddOption("--experimental-report", "", NoOp{}, kAllowedInEnvironment);
  AddOption("--experimental-wasi-unstable-preview1",
            "experimental WASI support",
            &EnvironmentOptions::experimental_wasi,
            kAllowedInEnvironment);
  AddOption("--expose-internals", "", &EnvironmentOptions::expose_internals);
  AddOption("--frozen-intrinsics",
            "experimental frozen intrinsics support",
            &EnvironmentOptions::frozen_intrinsics,
            kAllowedInEnvironment);
  AddOption("--heapsnapshot-signal",
            "Generate heap snapshot on specified signal",
            &EnvironmentOptions::heap_snapshot_signal,
            kAllowedInEnvironment);
  AddOption("--heapsnapshot-near-heap-limit",
            "Generate heap snapshots whenever V8 is approaching "
            "the heap limit. No more than the specified number of "
            "heap snapshots will be generated.",
            &EnvironmentOptions::heap_snapshot_near_heap_limit,
            kAllowedInEnvironment);
  AddOption("--http-parser", "", NoOp{}, kAllowedInEnvironment);
  AddOption("--insecure-http-parser",
            "use an insecure HTTP parser that accepts invalid HTTP headers",
            &EnvironmentOptions::insecure_http_parser,
            kAllowedInEnvironment);
  AddOption("--input-type",
            "set module type for string input",
            &EnvironmentOptions::module_type,
            kAllowedInEnvironment);
  AddOption("--experimental-specifier-resolution",
            "Select extension resolution algorithm for es modules; "
            "either 'explicit' (default) or 'node'",
            &EnvironmentOptions::experimental_specifier_resolution,
            kAllowedInEnvironment);
  AddAlias("--es-module-specifier-resolution",
           "--experimental-specifier-resolution");
  AddOption("--deprecation",
            "silence deprecation warnings",
            &EnvironmentOptions::deprecation,
            kAllowedInEnvironment,
            true);
  AddOption("--force-async-hooks-checks",
            "disable checks for async_hooks",
            &EnvironmentOptions::force_async_hooks_checks,
            kAllowedInEnvironment,
            true);
  AddOption(
      "--force-node-api-uncaught-exceptions-policy",
      "enforces 'uncaughtException' event on Node API asynchronous callbacks",
      &EnvironmentOptions::force_node_api_uncaught_exceptions_policy,
      kAllowedInEnvironment,
      false);
  AddOption("--addons",
            "disable loading native addons",
            &EnvironmentOptions::allow_native_addons,
            kAllowedInEnvironment,
            true);
  AddOption("--global-search-paths",
            "disable global module search paths",
            &EnvironmentOptions::global_search_paths,
            kAllowedInEnvironment,
            true);
  AddOption("--warnings",
            "silence all process warnings",
            &EnvironmentOptions::warnings,
            kAllowedInEnvironment,
            true);
  AddOption("--force-context-aware",
            "disable loading non-context-aware addons",
            &EnvironmentOptions::force_context_aware,
            kAllowedInEnvironment);
  AddOption("--pending-deprecation",
            "emit pending deprecation warnings",
            &EnvironmentOptions::pending_deprecation,
            kAllowedInEnvironment);
  AddOption("--preserve-symlinks",
            "preserve symbolic links when resolving",
            &EnvironmentOptions::preserve_symlinks,
            kAllowedInEnvironment);
  AddOption("--preserve-symlinks-main",
            "preserve symbolic links when resolving the main module",
            &EnvironmentOptions::preserve_symlinks_main,
            kAllowedInEnvironment);
  AddOption("--prof",
            "Generate V8 profiler output.",
            V8Option{});
  AddOption("--prof-process",
            "process V8 profiler output generated using --prof",
            &EnvironmentOptions::prof_process);
  // Options after --prof-process are passed through to the prof processor.
  AddAlias("--prof-process", { "--prof-process", "--" });
#if HAVE_INSPECTOR
  AddOption("--cpu-prof",
            "Start the V8 CPU profiler on start up, and write the CPU profile "
            "to disk before exit. If --cpu-prof-dir is not specified, write "
            "the profile to the current working directory.",
            &EnvironmentOptions::cpu_prof);
  AddOption("--cpu-prof-name",
            "specified file name of the V8 CPU profile generated with "
            "--cpu-prof",
            &EnvironmentOptions::cpu_prof_name);
  AddOption("--cpu-prof-interval",
            "specified sampling interval in microseconds for the V8 CPU "
            "profile generated with --cpu-prof. (default: 1000)",
            &EnvironmentOptions::cpu_prof_interval);
  AddOption("--cpu-prof-dir",
            "Directory where the V8 profiles generated by --cpu-prof will be "
            "placed. Does not affect --prof.",
            &EnvironmentOptions::cpu_prof_dir);
  AddOption(
      "--heap-prof",
      "Start the V8 heap profiler on start up, and write the heap profile "
      "to disk before exit. If --heap-prof-dir is not specified, write "
      "the profile to the current working directory.",
      &EnvironmentOptions::heap_prof);
  AddOption("--heap-prof-name",
            "specified file name of the V8 heap profile generated with "
            "--heap-prof",
            &EnvironmentOptions::heap_prof_name);
  AddOption("--heap-prof-dir",
            "Directory where the V8 heap profiles generated by --heap-prof "
            "will be placed.",
            &EnvironmentOptions::heap_prof_dir);
  AddOption("--heap-prof-interval",
            "specified sampling interval in bytes for the V8 heap "
            "profile generated with --heap-prof. (default: 512 * 1024)",
            &EnvironmentOptions::heap_prof_interval);
#endif  // HAVE_INSPECTOR
  AddOption("--max-http-header-size",
            "set the maximum size of HTTP headers (default: 16384 (16KB))",
            &EnvironmentOptions::max_http_header_size,
            kAllowedInEnvironment);
  AddOption("--redirect-warnings",
            "write warnings to file instead of stderr",
            &EnvironmentOptions::redirect_warnings,
            kAllowedInEnvironment);
  AddOption("--test",
            "launch test runner on startup",
            &EnvironmentOptions::test_runner);
  AddOption("--test-only",
            "run tests with 'only' option set",
            &EnvironmentOptions::test_only,
            kAllowedInEnvironment);
  AddOption("--test-udp-no-try-send", "",  // For testing only.
            &EnvironmentOptions::test_udp_no_try_send);
  AddOption("--throw-deprecation",
            "throw an exception on deprecations",
            &EnvironmentOptions::throw_deprecation,
            kAllowedInEnvironment);
  AddOption("--trace-atomics-wait",
            "trace Atomics.wait() operations",
            &EnvironmentOptions::trace_atomics_wait,
            kAllowedInEnvironment);
  AddOption("--trace-deprecation",
            "show stack traces on deprecations",
            &EnvironmentOptions::trace_deprecation,
            kAllowedInEnvironment);
  AddOption("--trace-exit",
            "show stack trace when an environment exits",
            &EnvironmentOptions::trace_exit,
            kAllowedInEnvironment);
  AddOption("--trace-sync-io",
            "show stack trace when use of sync IO is detected after the "
            "first tick",
            &EnvironmentOptions::trace_sync_io,
            kAllowedInEnvironment);
  AddOption("--trace-tls",
            "prints TLS packet trace information to stderr",
            &EnvironmentOptions::trace_tls,
            kAllowedInEnvironment);
  AddOption("--trace-uncaught",
            "show stack traces for the `throw` behind uncaught exceptions",
            &EnvironmentOptions::trace_uncaught,
            kAllowedInEnvironment);
  AddOption("--trace-warnings",
            "show stack traces on process warnings",
            &EnvironmentOptions::trace_warnings,
            kAllowedInEnvironment);
  AddOption("--extra-info-on-fatal-exception",
            "hide extra information on fatal exception that causes exit",
            &EnvironmentOptions::extra_info_on_fatal_exception,
            kAllowedInEnvironment,
            true);
  AddOption("--unhandled-rejections",
            "define unhandled rejections behavior. Options are 'strict' "
            "(always raise an error), 'throw' (raise an error unless "
            "'unhandledRejection' hook is set), 'warn' (log a warning), 'none' "
            "(silence warnings), 'warn-with-error-code' (log a warning and set "
            "exit code 1 unless 'unhandledRejection' hook is set). (default: "
            "throw)",
            &EnvironmentOptions::unhandled_rejections,
            kAllowedInEnvironment);
  AddOption("--verify-base-objects",
            "", /* undocumented, only for debugging */
            &EnvironmentOptions::verify_base_objects,
            kAllowedInEnvironment);

  AddOption("--check",
            "syntax check script without executing",
            &EnvironmentOptions::syntax_check_only);
  AddAlias("-c", "--check");
  // This option is only so that we can tell --eval with an empty string from
  // no eval at all. Having it not start with a dash makes it inaccessible
  // from the parser itself, but available for using Implies().
  // TODO(addaleax): When moving --help over to something generated from the
  // programmatic descriptions, this will need some special care.
  // (See also [ssl_openssl_cert_store] below.)
  AddOption("[has_eval_string]", "", &EnvironmentOptions::has_eval_string);
  AddOption("--eval", "evaluate script", &EnvironmentOptions::eval_string);
  Implies("--eval", "[has_eval_string]");
  AddOption("--print",
            "evaluate script and print result",
            &EnvironmentOptions::print_eval);
  AddAlias("-e", "--eval");
  AddAlias("--print <arg>", "-pe");
  AddAlias("-pe", { "--print", "--eval" });
  AddAlias("-p", "--print");
  AddOption("--require",
            "module to preload (option can be repeated)",
            &EnvironmentOptions::preload_modules,
            kAllowedInEnvironment);
  AddAlias("-r", "--require");
  AddOption("--interactive",
            "always enter the REPL even if stdin does not appear "
            "to be a terminal",
            &EnvironmentOptions::force_repl);
  AddAlias("-i", "--interactive");

  AddOption("--napi-modules", "", NoOp{}, kAllowedInEnvironment);

  AddOption("--tls-keylog",
            "log TLS decryption keys to named file for traffic analysis",
            &EnvironmentOptions::tls_keylog, kAllowedInEnvironment);

  AddOption("--tls-min-v1.0",
            "set default TLS minimum to TLSv1.0 (default: TLSv1.2)",
            &EnvironmentOptions::tls_min_v1_0,
            kAllowedInEnvironment);
  AddOption("--tls-min-v1.1",
            "set default TLS minimum to TLSv1.1 (default: TLSv1.2)",
            &EnvironmentOptions::tls_min_v1_1,
            kAllowedInEnvironment);
  AddOption("--tls-min-v1.2",
            "set default TLS minimum to TLSv1.2 (default: TLSv1.2)",
            &EnvironmentOptions::tls_min_v1_2,
            kAllowedInEnvironment);
  AddOption("--tls-min-v1.3",
            "set default TLS minimum to TLSv1.3 (default: TLSv1.2)",
            &EnvironmentOptions::tls_min_v1_3,
            kAllowedInEnvironment);
  AddOption("--tls-max-v1.2",
            "set default TLS maximum to TLSv1.2 (default: TLSv1.3)",
            &EnvironmentOptions::tls_max_v1_2,
            kAllowedInEnvironment);
  // Current plan is:
  // - 11.x and below: TLS1.3 is opt-in with --tls-max-v1.3
  // - 12.x: TLS1.3 is opt-out with --tls-max-v1.2
  // In either case, support both options they are uniformly available.
  AddOption("--tls-max-v1.3",
            "set default TLS maximum to TLSv1.3 (default: TLSv1.3)",
            &EnvironmentOptions::tls_max_v1_3,
            kAllowedInEnvironment);
}

PerIsolateOptionsParser::PerIsolateOptionsParser(
  const EnvironmentOptionsParser& eop) {
  AddOption("--track-heap-objects",
            "track heap object allocations for heap snapshots",
            &PerIsolateOptions::track_heap_objects,
            kAllowedInEnvironment);

  // Explicitly add some V8 flags to mark them as allowed in NODE_OPTIONS.
  AddOption("--abort-on-uncaught-exception",
            "aborting instead of exiting causes a core file to be generated "
            "for analysis",
            V8Option{},
            kAllowedInEnvironment);
  AddOption("--interpreted-frames-native-stack",
            "help system profilers to translate JavaScript interpreted frames",
            V8Option{}, kAllowedInEnvironment);
  AddOption("--max-old-space-size", "", V8Option{}, kAllowedInEnvironment);
  AddOption("--perf-basic-prof", "", V8Option{}, kAllowedInEnvironment);
  AddOption("--perf-basic-prof-only-functions",
            "",
            V8Option{},
            kAllowedInEnvironment);
  AddOption("--perf-prof", "", V8Option{}, kAllowedInEnvironment);
  AddOption("--perf-prof-unwinding-info",
            "",
            V8Option{},
            kAllowedInEnvironment);
  AddOption("--stack-trace-limit", "", V8Option{}, kAllowedInEnvironment);
  AddOption("--disallow-code-generation-from-strings",
            "disallow eval and friends",
            V8Option{},
            kAllowedInEnvironment);
  AddOption("--huge-max-old-generation-size",
             "increase default maximum heap size on machines with 16GB memory "
             "or more",
             V8Option{},
             kAllowedInEnvironment);
  AddOption("--jitless",
             "disable runtime allocation of executable memory",
             V8Option{},
             kAllowedInEnvironment);
  AddOption("--report-uncaught-exception",
            "generate diagnostic report on uncaught exceptions",
            &PerIsolateOptions::report_uncaught_exception,
            kAllowedInEnvironment);
  AddOption("--report-on-signal",
            "generate diagnostic report upon receiving signals",
            &PerIsolateOptions::report_on_signal,
            kAllowedInEnvironment);
  AddOption("--report-signal",
            "causes diagnostic report to be produced on provided signal,"
            " unsupported in Windows. (default: SIGUSR2)",
            &PerIsolateOptions::report_signal,
            kAllowedInEnvironment);
  Implies("--report-signal", "--report-on-signal");

  AddOption(
      "--experimental-top-level-await", "", NoOp{}, kAllowedInEnvironment);

  AddOption("--experimental-shadow-realm",
            "",
            &PerIsolateOptions::experimental_shadow_realm,
            kAllowedInEnvironment);
  AddOption("--harmony-shadow-realm", "", V8Option{});
  Implies("--experimental-shadow-realm", "--harmony-shadow-realm");
  Implies("--harmony-shadow-realm", "--experimental-shadow-realm");
  ImpliesNot("--no-harmony-shadow-realm", "--experimental-shadow-realm");

  Insert(eop, &PerIsolateOptions::get_per_env_options);
}

PerProcessOptionsParser::PerProcessOptionsParser(
  const PerIsolateOptionsParser& iop) {
  AddOption("--title",
            "the process title to use on startup",
            &PerProcessOptions::title,
            kAllowedInEnvironment);
  AddOption("--trace-event-categories",
            "comma separated list of trace event categories to record",
            &PerProcessOptions::trace_event_categories,
            kAllowedInEnvironment);
  AddOption("--trace-event-file-pattern",
            "Template string specifying the filepath for the trace-events "
            "data, it supports ${rotation} and ${pid}.",
            &PerProcessOptions::trace_event_file_pattern,
            kAllowedInEnvironment);
  AddAlias("--trace-events-enabled", {
    "--trace-event-categories", "v8,node,node.async_hooks" });
  AddOption("--v8-pool-size",
            "set V8's thread pool size",
            &PerProcessOptions::v8_thread_pool_size,
            kAllowedInEnvironment);
  AddOption("--zero-fill-buffers",
            "automatically zero-fill all newly allocated Buffer and "
            "SlowBuffer instances",
            &PerProcessOptions::zero_fill_all_buffers,
            kAllowedInEnvironment);
  AddOption("--debug-arraybuffer-allocations",
            "", /* undocumented, only for debugging */
            &PerProcessOptions::debug_arraybuffer_allocations,
            kAllowedInEnvironment);
  AddOption("--disable-proto",
            "disable Object.prototype.__proto__",
            &PerProcessOptions::disable_proto,
            kAllowedInEnvironment);
  AddOption("--build-snapshot",
            "Generate a snapshot blob when the process exits."
            "Currently only supported in the node_mksnapshot binary.",
            &PerProcessOptions::build_snapshot,
            kDisallowedInEnvironment);
  AddOption("--node-snapshot",
            "",  // It's a debug-only option.
            &PerProcessOptions::node_snapshot,
            kAllowedInEnvironment);
  // 12.x renamed this inadvertently, so alias it for consistency within the
  // release line, while using the original name for consistency with older
  // release lines.
  AddOption("--security-revert", "", &PerProcessOptions::security_reverts);
  AddAlias("--security-reverts", "--security-revert");
  AddOption("--completion-bash",
            "print source-able bash completion script",
            &PerProcessOptions::print_bash_completion);
  AddOption("--help",
            "print node command line options",
            &PerProcessOptions::print_help);
  AddAlias("-h", "--help");
  AddOption(
      "--version", "print Node.js version", &PerProcessOptions::print_version);
  AddAlias("-v", "--version");
  AddOption("--v8-options",
            "print V8 command line options",
            &PerProcessOptions::print_v8_help);
  AddOption("--report-compact",
            "output compact single-line JSON",
            &PerProcessOptions::report_compact,
            kAllowedInEnvironment);
  AddOption("--report-dir",
            "define custom report pathname."
            " (default: current working directory)",
            &PerProcessOptions::report_directory,
            kAllowedInEnvironment);
  AddAlias("--report-directory", "--report-dir");
  AddOption("--report-filename",
            "define custom report file name."
            " (default: YYYYMMDD.HHMMSS.PID.SEQUENCE#.txt)",
            &PerProcessOptions::report_filename,
            kAllowedInEnvironment);
  AddOption("--report-on-fatalerror",
              "generate diagnostic report on fatal (internal) errors",
              &PerProcessOptions::report_on_fatalerror,
              kAllowedInEnvironment);

#ifdef NODE_HAVE_I18N_SUPPORT
  AddOption("--icu-data-dir",
            "set ICU data load path to dir (overrides NODE_ICU_DATA)"
#ifndef NODE_HAVE_SMALL_ICU
            " (note: linked-in ICU data is present)"
#endif
            ,
            &PerProcessOptions::icu_data_dir,
            kAllowedInEnvironment);
#endif

#if HAVE_OPENSSL
  AddOption("--openssl-config",
            "load OpenSSL configuration from the specified file "
            "(overrides OPENSSL_CONF)",
            &PerProcessOptions::openssl_config,
            kAllowedInEnvironment);
  AddOption("--tls-cipher-list",
            "use an alternative default TLS cipher list",
            &PerProcessOptions::tls_cipher_list,
            kAllowedInEnvironment);
  AddOption("--use-openssl-ca",
            "use OpenSSL's default CA store"
#if defined(NODE_OPENSSL_CERT_STORE)
            " (default)"
#endif
            ,
            &PerProcessOptions::use_openssl_ca,
            kAllowedInEnvironment);
  AddOption("--use-bundled-ca",
            "use bundled CA store"
#if !defined(NODE_OPENSSL_CERT_STORE)
            " (default)"
#endif
            ,
            &PerProcessOptions::use_bundled_ca,
            kAllowedInEnvironment);
  // Similar to [has_eval_string] above, except that the separation between
  // this and use_openssl_ca only exists for option validation after parsing.
  // This is not ideal.
  AddOption("[ssl_openssl_cert_store]",
            "",
            &PerProcessOptions::ssl_openssl_cert_store);
  Implies("--use-openssl-ca", "[ssl_openssl_cert_store]");
  ImpliesNot("--use-bundled-ca", "[ssl_openssl_cert_store]");
  AddOption("--enable-fips",
            "enable FIPS crypto at startup",
            &PerProcessOptions::enable_fips_crypto,
            kAllowedInEnvironment);
  AddOption("--force-fips",
            "force FIPS crypto (cannot be disabled)",
            &PerProcessOptions::force_fips_crypto,
            kAllowedInEnvironment);
  AddOption("--secure-heap",
            "total size of the OpenSSL secure heap",
            &PerProcessOptions::secure_heap,
            kAllowedInEnvironment);
  AddOption("--secure-heap-min",
            "minimum allocation size from the OpenSSL secure heap",
            &PerProcessOptions::secure_heap_min,
            kAllowedInEnvironment);
#endif  // HAVE_OPENSSL
#if OPENSSL_VERSION_MAJOR >= 3
  AddOption("--openssl-legacy-provider",
            "enable OpenSSL 3.0 legacy provider",
            &PerProcessOptions::openssl_legacy_provider,
            kAllowedInEnvironment);
  AddOption("--openssl-shared-config",
            "enable OpenSSL shared configuration",
            &PerProcessOptions::openssl_shared_config,
            kAllowedInEnvironment);

#endif  // OPENSSL_VERSION_MAJOR
  AddOption("--use-largepages",
            "Map the Node.js static code to large pages. Options are "
            "'off' (the default value, meaning do not map), "
            "'on' (map and ignore failure, reporting it to stderr), "
            "or 'silent' (map and silently ignore failure)",
            &PerProcessOptions::use_largepages,
            kAllowedInEnvironment);

  AddOption("--trace-sigint",
            "enable printing JavaScript stacktrace on SIGINT",
            &PerProcessOptions::trace_sigint,
            kAllowedInEnvironment);

  Insert(iop, &PerProcessOptions::get_per_isolate_options);

  AddOption("--node-memory-debug",
            "Run with extra debug checks for memory leaks in Node.js itself",
            NoOp{}, kAllowedInEnvironment);
  Implies("--node-memory-debug", "--debug-arraybuffer-allocations");
  Implies("--node-memory-debug", "--verify-base-objects");
}

inline std::string RemoveBrackets(const std::string& host) {
  if (!host.empty() && host.front() == '[' && host.back() == ']')
    return host.substr(1, host.size() - 2);
  else
    return host;
}

inline int ParseAndValidatePort(const std::string& port,
                                std::vector<std::string>* errors) {
  char* endptr;
  errno = 0;
  const unsigned long result =                 // NOLINT(runtime/int)
    strtoul(port.c_str(), &endptr, 10);
  if (errno != 0 || *endptr != '\0'||
      (result != 0 && result < 1024) || result > 65535) {
    errors->push_back(" must be 0 or in range 1024 to 65535.");
  }
  return static_cast<int>(result);
}

HostPort SplitHostPort(const std::string& arg,
                      std::vector<std::string>* errors) {
  // remove_brackets only works if no port is specified
  // so if it has an effect only an IPv6 address was specified.
  std::string host = RemoveBrackets(arg);
  if (host.length() < arg.length())
    return HostPort{host, DebugOptions::kDefaultInspectorPort};

  size_t colon = arg.rfind(':');
  if (colon == std::string::npos) {
    // Either a port number or a host name.  Assume that
    // if it's not all decimal digits, it's a host name.
    for (char c : arg) {
      if (c < '0' || c > '9') {
        return HostPort{arg, DebugOptions::kDefaultInspectorPort};
      }
    }
    return HostPort { "", ParseAndValidatePort(arg, errors) };
  }
  // Host and port found:
  return HostPort { RemoveBrackets(arg.substr(0, colon)),
                    ParseAndValidatePort(arg.substr(colon + 1), errors) };
}

std::string GetBashCompletion() {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  const auto& parser = _ppop_instance;

  std::ostringstream out;

  out << "_node_complete() {\n"
         "  local cur_word options\n"
         "  cur_word=\"${COMP_WORDS[COMP_CWORD]}\"\n"
         "  if [[ \"${cur_word}\" == -* ]] ; then\n"
         "    COMPREPLY=( $(compgen -W '";

  for (const auto& item : parser.options_) {
    if (item.first[0] != '[') {
      out << item.first << " ";
    }
  }
  for (const auto& item : parser.aliases_) {
    if (item.first[0] != '[') {
      out << item.first << " ";
    }
  }
  if (parser.aliases_.size() > 0) {
    out.seekp(-1, out.cur);  // Strip the trailing space
  }

  out << "' -- \"${cur_word}\") )\n"
         "    return 0\n"
         "  else\n"
         "    COMPREPLY=( $(compgen -f \"${cur_word}\") )\n"
         "    return 0\n"
         "  fi\n"
         "}\n"
         "complete -o filenames -o nospace -o bashdefault "
         "-F _node_complete node node_g";
  return out.str();
}

// Return a map containing all the options and their metadata as well
// as the aliases
void GetCLIOptions(const FunctionCallbackInfo<Value>& args) {
  Mutex::ScopedLock lock(per_process::cli_options_mutex);
  Environment* env = Environment::GetCurrent(args);
  if (!env->has_run_bootstrapping_code()) {
    // No code because this is an assertion.
    return env->ThrowError(
        "Should not query options before bootstrapping is done");
  }
  env->set_has_serialized_options(true);

  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();

  // Temporarily act as if the current Environment's/IsolateData's options were
  // the default options, i.e. like they are the ones we'd access for global
  // options parsing, so that all options are available from the main parser.
  auto original_per_isolate = per_process::cli_options->per_isolate;
  per_process::cli_options->per_isolate = env->isolate_data()->options();
  auto original_per_env = per_process::cli_options->per_isolate->per_env;
  per_process::cli_options->per_isolate->per_env = env->options();
  auto on_scope_leave = OnScopeLeave([&]() {
    per_process::cli_options->per_isolate->per_env = original_per_env;
    per_process::cli_options->per_isolate = original_per_isolate;
  });

  Local<Map> options = Map::New(isolate);
  if (options
          ->SetPrototype(context, env->primordials_safe_map_prototype_object())
          .IsNothing()) {
    return;
  }

  for (const auto& item : _ppop_instance.options_) {
    Local<Value> value;
    const auto& option_info = item.second;
    auto field = option_info.field;
    PerProcessOptions* opts = per_process::cli_options.get();
    switch (option_info.type) {
      case kNoOp:
      case kV8Option:
        // Special case for --abort-on-uncaught-exception which is also
        // respected by Node.js internals
        if (item.first == "--abort-on-uncaught-exception") {
          value = Boolean::New(
            isolate, original_per_env->abort_on_uncaught_exception);
        } else {
          value = Undefined(isolate);
        }
        break;
      case kBoolean:
        value = Boolean::New(isolate,
                             *_ppop_instance.Lookup<bool>(field, opts));
        break;
      case kInteger:
        value = Number::New(
            isolate,
            static_cast<double>(*_ppop_instance.Lookup<int64_t>(field, opts)));
        break;
      case kUInteger:
        value = Number::New(
            isolate,
            static_cast<double>(*_ppop_instance.Lookup<uint64_t>(field, opts)));
        break;
      case kString:
        if (!ToV8Value(context,
                       *_ppop_instance.Lookup<std::string>(field, opts))
                 .ToLocal(&value)) {
          return;
        }
        break;
      case kStringList:
        if (!ToV8Value(context,
                       *_ppop_instance.Lookup<StringVector>(field, opts))
                 .ToLocal(&value)) {
          return;
        }
        break;
      case kHostPort: {
        const HostPort& host_port =
          *_ppop_instance.Lookup<HostPort>(field, opts);
        Local<Object> obj = Object::New(isolate);
        Local<Value> host;
        if (!ToV8Value(context, host_port.host()).ToLocal(&host) ||
            obj->Set(context, env->host_string(), host).IsNothing() ||
            obj->Set(context,
                     env->port_string(),
                     Integer::New(isolate, host_port.port()))
                .IsNothing()) {
          return;
        }
        value = obj;
        break;
      }
      default:
        UNREACHABLE();
    }
    CHECK(!value.IsEmpty());

    Local<Value> name = ToV8Value(context, item.first).ToLocalChecked();
    Local<Object> info = Object::New(isolate);
    Local<Value> help_text;
    if (!ToV8Value(context, option_info.help_text).ToLocal(&help_text) ||
        !info->Set(context, env->help_text_string(), help_text)
             .FromMaybe(false) ||
        !info->Set(context,
                   env->env_var_settings_string(),
                   Integer::New(isolate,
                                static_cast<int>(option_info.env_setting)))
             .FromMaybe(false) ||
        !info->Set(context,
                   env->type_string(),
                   Integer::New(isolate, static_cast<int>(option_info.type)))
             .FromMaybe(false) ||
        !info->Set(context,
                   env->default_is_true_string(),
                   Boolean::New(isolate, option_info.default_is_true))
             .FromMaybe(false) ||
        info->Set(context, env->value_string(), value).IsNothing() ||
        options->Set(context, name, info).IsEmpty()) {
      return;
    }
  }

  Local<Value> aliases;
  if (!ToV8Value(context, _ppop_instance.aliases_).ToLocal(&aliases)) return;

  if (aliases.As<Object>()
          ->SetPrototype(context, env->primordials_safe_map_prototype_object())
          .IsNothing()) {
    return;
  }

  Local<Object> ret = Object::New(isolate);
  if (ret->Set(context, env->options_string(), options).IsNothing() ||
      ret->Set(context, env->aliases_string(), aliases).IsNothing()) {
    return;
  }

  args.GetReturnValue().Set(ret);
}

void GetEmbedderOptions(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (!env->has_run_bootstrapping_code()) {
    // No code because this is an assertion.
    return env->ThrowError(
        "Should not query options before bootstrapping is done");
  }
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = env->context();
  Local<Object> ret = Object::New(isolate);

  if (ret->Set(context,
           FIXED_ONE_BYTE_STRING(env->isolate(), "shouldNotRegisterESMLoader"),
           Boolean::New(isolate, env->should_not_register_esm_loader()))
      .IsNothing()) return;

  if (ret->Set(context,
           FIXED_ONE_BYTE_STRING(env->isolate(), "noGlobalSearchPaths"),
           Boolean::New(isolate, env->no_global_search_paths()))
      .IsNothing()) return;

  args.GetReturnValue().Set(ret);
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  env->SetMethodNoSideEffect(target, "getCLIOptions", GetCLIOptions);
  env->SetMethodNoSideEffect(target, "getEmbedderOptions", GetEmbedderOptions);

  Local<Object> env_settings = Object::New(isolate);
  NODE_DEFINE_CONSTANT(env_settings, kAllowedInEnvironment);
  NODE_DEFINE_CONSTANT(env_settings, kDisallowedInEnvironment);
  target
      ->Set(
          context, FIXED_ONE_BYTE_STRING(isolate, "envSettings"), env_settings)
      .Check();

  Local<Object> types = Object::New(isolate);
  NODE_DEFINE_CONSTANT(types, kNoOp);
  NODE_DEFINE_CONSTANT(types, kV8Option);
  NODE_DEFINE_CONSTANT(types, kBoolean);
  NODE_DEFINE_CONSTANT(types, kInteger);
  NODE_DEFINE_CONSTANT(types, kUInteger);
  NODE_DEFINE_CONSTANT(types, kString);
  NODE_DEFINE_CONSTANT(types, kHostPort);
  NODE_DEFINE_CONSTANT(types, kStringList);
  target->Set(context, FIXED_ONE_BYTE_STRING(isolate, "types"), types)
      .Check();
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(GetCLIOptions);
  registry->Register(GetEmbedderOptions);
}
}  // namespace options_parser

void HandleEnvOptions(std::shared_ptr<EnvironmentOptions> env_options) {
  HandleEnvOptions(env_options, [](const char* name) {
    std::string text;
    return credentials::SafeGetenv(name, &text) ? text : "";
  });
}

void HandleEnvOptions(std::shared_ptr<EnvironmentOptions> env_options,
                      std::function<std::string(const char*)> opt_getter) {
  env_options->pending_deprecation =
      opt_getter("NODE_PENDING_DEPRECATION") == "1";

  env_options->preserve_symlinks = opt_getter("NODE_PRESERVE_SYMLINKS") == "1";

  env_options->preserve_symlinks_main =
      opt_getter("NODE_PRESERVE_SYMLINKS_MAIN") == "1";

  if (env_options->redirect_warnings.empty())
    env_options->redirect_warnings = opt_getter("NODE_REDIRECT_WARNINGS");
}

std::vector<std::string> ParseNodeOptionsEnvVar(
    const std::string& node_options, std::vector<std::string>* errors) {
  std::vector<std::string> env_argv;

  bool is_in_string = false;
  bool will_start_new_arg = true;
  for (std::string::size_type index = 0; index < node_options.size(); ++index) {
    char c = node_options.at(index);

    // Backslashes escape the following character
    if (c == '\\' && is_in_string) {
      if (index + 1 == node_options.size()) {
        errors->push_back("invalid value for NODE_OPTIONS "
                          "(invalid escape)\n");
        return env_argv;
      } else {
        c = node_options.at(++index);
      }
    } else if (c == ' ' && !is_in_string) {
      will_start_new_arg = true;
      continue;
    } else if (c == '"') {
      is_in_string = !is_in_string;
      continue;
    }

    if (will_start_new_arg) {
      env_argv.emplace_back(std::string(1, c));
      will_start_new_arg = false;
    } else {
      env_argv.back() += c;
    }
  }

  if (is_in_string) {
    errors->push_back("invalid value for NODE_OPTIONS "
                      "(unterminated string)\n");
  }
  return env_argv;
}
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(options, node::options_parser::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(options,
                               node::options_parser::RegisterExternalReferences)
