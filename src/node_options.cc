#include "node_options-inl.h"
#include <errno.h>

namespace node {

DebugOptionsParser::DebugOptionsParser() {
  AddOption("--inspect-port", &DebugOptions::host_port,
            kAllowedInEnvironment);
  AddAlias("--debug-port", "--inspect-port");

  AddOption("--inspect", &DebugOptions::inspector_enabled,
            kAllowedInEnvironment);
  AddAlias("--inspect=", { "--inspect-port", "--inspect" });

  AddOption("--debug", &DebugOptions::deprecated_debug);
  AddAlias("--debug=", { "--inspect-port", "--debug" });

  AddOption("--inspect-brk", &DebugOptions::break_first_line,
            kAllowedInEnvironment);
  Implies("--inspect-brk", "--inspect");
  AddAlias("--inspect-brk=", { "--inspect-port", "--inspect-brk" });

  AddOption("--inspect-brk-node", &DebugOptions::break_node_first_line);
  Implies("--inspect-brk-node", "--inspect");
  AddAlias("--inspect-brk-node=", { "--inspect-port", "--inspect-brk-node" });

  AddOption("--debug-brk", &DebugOptions::break_first_line);
  Implies("--debug-brk", "--debug");
  AddAlias("--debug-brk=", { "--inspect-port", "--debug-brk" });
}

DebugOptionsParser DebugOptionsParser::instance;

EnvironmentOptionsParser::EnvironmentOptionsParser() {
  AddOption("--experimental-modules", &EnvironmentOptions::experimental_modules,
            kAllowedInEnvironment);
  AddOption("--experimental-repl-await",
            &EnvironmentOptions::experimental_repl_await,
            kAllowedInEnvironment);
  AddOption("--experimental-vm-modules",
            &EnvironmentOptions::experimental_vm_modules,
            kAllowedInEnvironment);
  AddOption("--experimental-worker", &EnvironmentOptions::experimental_worker,
            kAllowedInEnvironment);
  AddOption("--expose-internals", &EnvironmentOptions::expose_internals);
  // TODO(addaleax): Remove this when adding -/_ canonicalization to the parser.
  AddAlias("--expose_internals", "--expose-internals");
  AddOption("--loader", &EnvironmentOptions::userland_loader,
            kAllowedInEnvironment);
  AddOption("--no-deprecation", &EnvironmentOptions::no_deprecation,
            kAllowedInEnvironment);
  AddOption("--no-force-async-hooks-checks",
            &EnvironmentOptions::no_force_async_hooks_checks,
            kAllowedInEnvironment);
  AddOption("--no-warnings", &EnvironmentOptions::no_warnings,
            kAllowedInEnvironment);
  AddOption("--pending-deprecation", &EnvironmentOptions::pending_deprecation,
            kAllowedInEnvironment);
  AddOption("--preserve-symlinks", &EnvironmentOptions::preserve_symlinks);
  AddOption("--preserve-symlinks-main",
            &EnvironmentOptions::preserve_symlinks_main);
  AddOption("--prof-process", &EnvironmentOptions::prof_process);
  AddOption("--redirect-warnings", &EnvironmentOptions::redirect_warnings,
            kAllowedInEnvironment);
  AddOption("--throw-deprecation", &EnvironmentOptions::throw_deprecation,
            kAllowedInEnvironment);
  AddOption("--trace-deprecation", &EnvironmentOptions::trace_deprecation,
            kAllowedInEnvironment);
  AddOption("--trace-sync-io", &EnvironmentOptions::trace_sync_io,
            kAllowedInEnvironment);
  AddOption("--trace-warnings", &EnvironmentOptions::trace_warnings,
            kAllowedInEnvironment);

  AddOption("--check", &EnvironmentOptions::syntax_check_only);
  AddAlias("-c", "--check");
  // This option is only so that we can tell --eval with an empty string from
  // no eval at all. Having it not start with a dash makes it inaccessible
  // from the parser itself, but available for using Implies().
  // TODO(addaleax): When moving --help over to something generated from the
  // programmatic descriptions, this will need some special care.
  // (See also [ssl_openssl_cert_store] below.)
  AddOption("[has_eval_string]", &EnvironmentOptions::has_eval_string);
  AddOption("--eval", &EnvironmentOptions::eval_string);
  Implies("--eval", "[has_eval_string]");
  AddOption("--print", &EnvironmentOptions::print_eval);
  AddAlias("-e", "--eval");
  AddAlias("--print <arg>", "-pe");
  AddAlias("-pe", { "--print", "--eval" });
  AddAlias("-p", "--print");
  AddOption("--require", &EnvironmentOptions::preload_modules,
            kAllowedInEnvironment);
  AddAlias("-r", "--require");
  AddOption("--interactive", &EnvironmentOptions::force_repl);
  AddAlias("-i", "--interactive");

  AddOption("--napi-modules", NoOp {}, kAllowedInEnvironment);
  AddOption("--expose-http2", NoOp {}, kAllowedInEnvironment);
  AddOption("--expose_http2", NoOp {}, kAllowedInEnvironment);

  Insert(&DebugOptionsParser::instance,
         &EnvironmentOptions::get_debug_options);
}

EnvironmentOptionsParser EnvironmentOptionsParser::instance;

PerIsolateOptionsParser::PerIsolateOptionsParser() {
  AddOption("--track-heap-objects", &PerIsolateOptions::track_heap_objects,
            kAllowedInEnvironment);

  // Explicitly add some V8 flags to mark them as allowed in NODE_OPTIONS.
  AddOption("--abort_on_uncaught_exception", V8Option {},
            kAllowedInEnvironment);
  AddOption("--max_old_space_size", V8Option {}, kAllowedInEnvironment);
  AddOption("--perf_basic_prof", V8Option {}, kAllowedInEnvironment);
  AddOption("--perf_prof", V8Option {}, kAllowedInEnvironment);
  AddOption("--stack_trace_limit", V8Option {}, kAllowedInEnvironment);

  Insert(&EnvironmentOptionsParser::instance,
         &PerIsolateOptions::get_per_env_options);
}

PerIsolateOptionsParser PerIsolateOptionsParser::instance;

PerProcessOptionsParser::PerProcessOptionsParser() {
  AddOption("--title", &PerProcessOptions::title, kAllowedInEnvironment);
  AddOption("--trace-event-categories",
            &PerProcessOptions::trace_event_categories,
            kAllowedInEnvironment);
  AddOption("--trace-event-file-pattern",
            &PerProcessOptions::trace_event_file_pattern,
            kAllowedInEnvironment);
  AddAlias("--trace-events-enabled", {
    "--trace-event-categories", "v8,node,node.async_hooks" });
  AddOption("--v8-pool-size", &PerProcessOptions::v8_thread_pool_size,
            kAllowedInEnvironment);
  AddOption("--zero-fill-buffers", &PerProcessOptions::zero_fill_all_buffers,
            kAllowedInEnvironment);

  AddOption("--security-reverts", &PerProcessOptions::security_reverts);
  AddOption("--help", &PerProcessOptions::print_help);
  AddAlias("-h", "--help");
  AddOption("--version", &PerProcessOptions::print_version);
  AddAlias("-v", "--version");
  AddOption("--v8-options", &PerProcessOptions::print_v8_help);

#ifdef NODE_HAVE_I18N_SUPPORT
  AddOption("--icu-data-dir", &PerProcessOptions::icu_data_dir,
            kAllowedInEnvironment);
#endif

#if HAVE_OPENSSL
  AddOption("--openssl-config", &PerProcessOptions::openssl_config,
            kAllowedInEnvironment);
  AddOption("--tls-cipher-list", &PerProcessOptions::tls_cipher_list,
            kAllowedInEnvironment);
  AddOption("--use-openssl-ca", &PerProcessOptions::use_openssl_ca,
            kAllowedInEnvironment);
  AddOption("--use-bundled-ca", &PerProcessOptions::use_bundled_ca,
            kAllowedInEnvironment);
  // Similar to [has_eval_string] above, except that the separation between
  // this and use_openssl_ca only exists for option validation after parsing.
  // This is not ideal.
  AddOption("[ssl_openssl_cert_store]",
            &PerProcessOptions::ssl_openssl_cert_store);
  Implies("--use-openssl-ca", "[ssl_openssl_cert_store]");
  ImpliesNot("--use-bundled-ca", "[ssl_openssl_cert_store]");
#if NODE_FIPS_MODE
  AddOption("--enable-fips", &PerProcessOptions::enable_fips_crypto,
            kAllowedInEnvironment);
  AddOption("--force-fips", &PerProcessOptions::force_fips_crypto,
            kAllowedInEnvironment);
#endif
#endif

  Insert(&PerIsolateOptionsParser::instance,
         &PerProcessOptions::get_per_isolate_options);
}

PerProcessOptionsParser PerProcessOptionsParser::instance;

inline std::string RemoveBrackets(const std::string& host) {
  if (!host.empty() && host.front() == '[' && host.back() == ']')
    return host.substr(1, host.size() - 2);
  else
    return host;
}

inline int ParseAndValidatePort(const std::string& port, std::string* error) {
  char* endptr;
  errno = 0;
  const long result = strtol(port.c_str(), &endptr, 10);  // NOLINT(runtime/int)
  if (errno != 0 || *endptr != '\0'||
      (result != 0 && result < 1024) || result > 65535) {
    *error = "Port must be 0 or in range 1024 to 65535.";
  }
  return static_cast<int>(result);
}

HostPort SplitHostPort(const std::string& arg, std::string* error) {
  // remove_brackets only works if no port is specified
  // so if it has an effect only an IPv6 address was specified.
  std::string host = RemoveBrackets(arg);
  if (host.length() < arg.length())
    return HostPort { host, -1 };

  size_t colon = arg.rfind(':');
  if (colon == std::string::npos) {
    // Either a port number or a host name.  Assume that
    // if it's not all decimal digits, it's a host name.
    for (char c : arg) {
      if (c < '0' || c > '9') {
        return HostPort { arg, -1 };
      }
    }
    return HostPort { "", ParseAndValidatePort(arg, error) };
  }
  // Host and port found:
  return HostPort { RemoveBrackets(arg.substr(0, colon)),
                    ParseAndValidatePort(arg.substr(colon + 1), error) };
}
}  // namespace node
