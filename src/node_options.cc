#include "node.h"
#include "node_internals.h"
#include "node_debug_options.h"
#include "node_options.h"
#include "node_revert.h"
#include "uv.h"

#include <stdlib.h>
#include <string>

#if !defined(_MSC_VER)
#include <unistd.h>  // setuid, getuid
#endif

namespace node {

bool NodeOptions::SafeGetenv(const char* key, std::string* text) {
#if !defined(__CloudABI__) && !defined(_WIN32)
  if (IsSet(NODE_OPTION_AT_SECURE) ||
      getuid() != geteuid() ||
      getgid() != getegid()) {
    goto fail;
  }
#endif
  if (const char* value = getenv(key)) {
    *text = value;
    return true;
  }

fail:
  text->clear();
  return false;
}

void NodeOptions::SetString(NodeOptionsStrings option, const char* str) {
  CHECK_LT(option, NODE_OPTION_STRING_COUNT);
  strings_[option] = str;
}

void NodeOptions::Set(NodeOptionsFlags flag) {
  flags_ |= 1 << flag;
}

void NodeOptions::Unset(NodeOptionsFlags flag) {
  flags_ &= ~(1 << flag);
}

bool NodeOptions::IsSet(NodeOptionsFlags flag) const {
  return flags_ & (1 << flag);
}

const std::string& NodeOptions::GetString(NodeOptionsStrings option) const {
  CHECK_LT(option, NODE_OPTION_STRING_COUNT);
  return strings_[option];
}

NodeOptions::NodeOptions(int flags) : flags_(flags) {
  std::string text;
  if (SafeGetenv("NODE_PENDING_DEPRECATION", &text) && text[0] == '1')
    Set(NODE_OPTION_PENDING_DEPRECATION);

  if (SafeGetenv("NODE_PRESERVE_SYMLINKS", &text) && text[0] == '1')
    Set(NODE_OPTION_PRESERVE_SYMLINKS);

  SafeGetenv("NODE_REDIRECT_WARNINGS",
             &strings_[NODE_OPTION_STRING_WARNING_FILE]);
  SafeGetenv("NODE_ICU_DATA",
             &strings_[NODE_OPTION_STRING_ICU_DATA_DIR]);

#if HAVE_OPENSSL
  strings_[NODE_OPTION_STRING_OPENSSL_DEFAULT_CIPHER_LIST] =
      DEFAULT_CIPHER_LIST_CORE;
  SafeGetenv("OPENSSL_CONF",
             &strings_[NODE_OPTION_STRING_OPENSSL_CONFIG]);
  SafeGetenv("NODE_EXTRA_CA_CERTS",
             &strings_[NODE_OPTION_STRING_OPENSSL_EXTRA_CA_CERTS]);
#if defined(NODE_OPENSSL_CERT_STORE)
  Set(NODE_OPTION_OPENSSL_CERT_STORE);
#endif  // defined(NODE_OPENSSL_CERT_STORE)
#endif  // HAVE_OPENSSL
}

static void PrintHelp() {
  // XXX: If you add an option here, please also add it to doc/node.1 and
  // doc/api/cli.md
  printf("Usage: node [options] [ -e script | script.js | - ] [arguments]\n"
         "       node inspect script.js [arguments]\n"
         "\n"
         "Options:\n"
         "  -v, --version              print Node.js version\n"
         "  -e, --eval script          evaluate script\n"
         "  -p, --print                evaluate script and print result\n"
         "  -c, --check                syntax check script without executing\n"
         "  -i, --interactive          always enter the REPL even if stdin\n"
         "                             does not appear to be a terminal\n"
         "  -r, --require              module to preload (option can be "
         "repeated)\n"
         "  -                          script read from stdin (default; "
         "interactive mode if a tty)\n"
#if HAVE_INSPECTOR
         "  --inspect[=[host:]port]    activate inspector on host:port\n"
         "                             (default: 127.0.0.1:9229)\n"
         "  --inspect-brk[=[host:]port]\n"
         "                             activate inspector on host:port\n"
         "                             and break at start of user script\n"
         "  --inspect-port=[host:]port\n"
         "                             set host:port for inspector\n"
#endif
         "  --no-deprecation           silence deprecation warnings\n"
         "  --trace-deprecation        show stack traces on deprecations\n"
         "  --throw-deprecation        throw an exception on deprecations\n"
         "  --pending-deprecation      emit pending deprecation warnings\n"
         "  --no-warnings              silence all process warnings\n"
         "  --napi-modules             load N-API modules (no-op - option\n"
         "                             kept for compatibility)\n"
         "  --abort-on-uncaught-exception\n"
         "                             aborting instead of exiting causes a\n"
         "                             core file to be generated for analysis\n"
         "  --trace-warnings           show stack traces on process warnings\n"
         "  --redirect-warnings=file\n"
         "                             write warnings to file instead of\n"
         "                             stderr\n"
         "  --trace-sync-io            show stack trace when use of sync IO\n"
         "                             is detected after the first tick\n"
         "  --no-force-async-hooks-checks\n"
         "                             disable checks for async_hooks\n"
         "  --trace-events-enabled     track trace events\n"
         "  --trace-event-categories   comma separated list of trace event\n"
         "                             categories to record\n"
         "  --track-heap-objects       track heap object allocations for heap "
         "snapshots\n"
         "  --prof-process             process v8 profiler output generated\n"
         "                             using --prof\n"
         "  --zero-fill-buffers        automatically zero-fill all newly "
         "allocated\n"
         "                             Buffer and SlowBuffer instances\n"
         "  --v8-options               print v8 command line options\n"
         "  --v8-pool-size=num         set v8's thread pool size\n"
#if HAVE_OPENSSL
         "  --tls-cipher-list=val      use an alternative default TLS cipher "
         "list\n"
         "  --use-bundled-ca           use bundled CA store"
#if !defined(NODE_OPENSSL_CERT_STORE)
         " (default)"
#endif
         "\n"
         "  --use-openssl-ca           use OpenSSL's default CA store"
#if defined(NODE_OPENSSL_CERT_STORE)
         " (default)"
#endif
         "\n"
#if NODE_FIPS_MODE
         "  --enable-fips              enable FIPS crypto at startup\n"
         "  --force-fips               force FIPS crypto (cannot be disabled)\n"
#endif  /* NODE_FIPS_MODE */
         "  --openssl-config=file      load OpenSSL configuration from the\n"
         "                             specified file (overrides\n"
         "                             OPENSSL_CONF)\n"
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
         "  --icu-data-dir=dir         set ICU data load path to dir\n"
         "                             (overrides NODE_ICU_DATA)\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                             note: linked-in ICU data is present\n"
#endif
         "  --preserve-symlinks        preserve symbolic links when resolving\n"
         "  --experimental-modules     experimental ES Module support\n"
         "                             and caching modules\n"
         "  --experimental-vm-modules  experimental ES Module support\n"
         "                             in vm module\n"
#endif
         "\n"
         "Environment variables:\n"
         "NODE_DEBUG                   ','-separated list of core modules\n"
         "                             that should print debug information\n"
         "NODE_DISABLE_COLORS          set to 1 to disable colors in the REPL\n"
         "NODE_EXTRA_CA_CERTS          path to additional CA certificates\n"
         "                             file\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         "NODE_ICU_DATA                data path for ICU (Intl object) data\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "                             (will extend linked-in data)\n"
#endif
#endif
         "NODE_NO_WARNINGS             set to 1 to silence process warnings\n"
#if !defined(NODE_WITHOUT_NODE_OPTIONS)
         "NODE_OPTIONS                 set CLI options in the environment\n"
         "                             via a space-separated list\n"
#endif
#ifdef _WIN32
         "NODE_PATH                    ';'-separated list of directories\n"
#else
         "NODE_PATH                    ':'-separated list of directories\n"
#endif
         "                             prefixed to the module search path\n"
         "NODE_PENDING_DEPRECATION     set to 1 to emit pending deprecation\n"
         "                             warnings\n"
         "NODE_REPL_HISTORY            path to the persistent REPL history\n"
         "                             file\n"
         "NODE_REDIRECT_WARNINGS       write warnings to path instead of\n"
         "                             stderr\n"
         "OPENSSL_CONF                 load OpenSSL configuration from file\n"
         "\n"
         "Documentation can be found at https://nodejs.org/\n");
}

static bool ArgIsAllowed(const char* arg, const char* allowed) {
  for (; *arg && *allowed; arg++, allowed++) {
    // Like normal strcmp(), except that a '_' in `allowed` matches either a '-'
    // or '_' in `arg`.
    if (*allowed == '_') {
      if (!(*arg == '_' || *arg == '-'))
        return false;
    } else {
      if (*arg != *allowed)
        return false;
    }
  }

  // "--some-arg=val" is allowed for "--some-arg"
  if (*arg == '=')
    return true;

  // Both must be null, or one string is just a prefix of the other, not a
  // match.
  return !*arg && !*allowed;
}


static void CheckIfAllowedInEnv(const char* exe, bool is_env,
                                const char* arg) {
  if (!is_env)
    return;

  static const char* whitelist[] = {
    // Node options, sorted in `node --help` order for ease of comparison.
    "--require", "-r",
    "--inspect",
    "--inspect-brk",
    "--inspect-port",
    "--no-deprecation",
    "--trace-deprecation",
    "--throw-deprecation",
    "--pending-deprecation",
    "--no-warnings",
    "--napi-modules",
    "--expose-http2",   // keep as a non-op through v9.x
    "--experimental-modules",
    "--experimental-vm-modules",
    "--loader",
    "--trace-warnings",
    "--redirect-warnings",
    "--trace-sync-io",
    "--no-force-async-hooks-checks",
    "--trace-events-enabled",
    "--trace-event-categories",
    "--track-heap-objects",
    "--zero-fill-buffers",
    "--v8-pool-size",
    "--tls-cipher-list",
    "--use-bundled-ca",
    "--use-openssl-ca",
    "--enable-fips",
    "--force-fips",
    "--openssl-config",
    "--icu-data-dir",

    // V8 options (define with '_', which allows '-' or '_')
    "--perf_prof",
    "--perf_basic_prof",
    "--abort_on_uncaught_exception",
    "--max_old_space_size",
    "--stack_trace_limit",
  };

  for (unsigned i = 0; i < arraysize(whitelist); i++) {
    const char* allowed = whitelist[i];
    if (ArgIsAllowed(arg, allowed))
      return;
  }

  fprintf(stderr, "%s: %s is not allowed in NODE_OPTIONS\n", exe, arg);
  exit(9);
}

// Parse command line arguments.
//
// argv is modified in place. exec_argv and v8_argv are out arguments that
// ParseArgs() allocates memory for and stores a pointer to the output
// vector in.  The caller should free them with delete[].
//
// On exit:
//
//  * argv contains the arguments with node and V8 options filtered out.
//  * exec_argv contains both node and V8 options and nothing else.
//  * v8_argv contains argv[0] plus any V8 options
void NodeOptions::ParseArgs(int* argc,
                            const char** argv,
                            int* exec_argc,
                            const char*** exec_argv,
                            int* v8_argc,
                            const char*** v8_argv,
                            bool is_env,
                            DebugOptions* debug_options,
                            NodeOptions* state) {
  const unsigned int nargs = static_cast<unsigned int>(*argc);
  const char** new_exec_argv = new const char*[nargs];
  const char** new_v8_argv = new const char*[nargs];
  const char** new_argv = new const char*[nargs];
  const char* eval_string = nullptr;
#if HAVE_OPENSSL
  bool use_bundled_ca = false;
  bool use_openssl_ca = false;
#endif  // HAVE_OPENSSL

  for (unsigned int i = 0; i < nargs; ++i) {
    new_exec_argv[i] = nullptr;
    new_v8_argv[i] = nullptr;
    new_argv[i] = nullptr;
  }

  // exec_argv starts with the first option, the other two start with argv[0].
  unsigned int new_exec_argc = 0;
  unsigned int new_v8_argc = 1;
  unsigned int new_argc = 1;
  new_v8_argv[0] = argv[0];
  new_argv[0] = argv[0];

  unsigned int index = 1;
  bool short_circuit = false;
  while (index < nargs && argv[index][0] == '-' && !short_circuit) {
    const char* const arg = argv[index];
    unsigned int args_consumed = 1;

    CheckIfAllowedInEnv(argv[0], is_env, arg);
    if (debug_options->ParseOption(argv[0], arg)) {
      // Done, consumed by DebugOptions::ParseOption().
    } else if (strcmp(arg, "--version") == 0 || strcmp(arg, "-v") == 0) {
      printf("%s\n", NODE_VERSION);
      exit(0);
    } else if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
      PrintHelp();
      exit(0);
    } else if (strcmp(arg, "--eval") == 0 ||
               strcmp(arg, "-e") == 0 ||
               strcmp(arg, "--print") == 0 ||
               strcmp(arg, "-pe") == 0 ||
               strcmp(arg, "-p") == 0) {
      bool is_eval = strchr(arg, 'e') != nullptr;
      bool is_print = strchr(arg, 'p') != nullptr;
      if (is_print)
        state->Set(NODE_OPTION_PRINT_EVAL);
      // --eval, -e and -pe always require an argument.
      if (is_eval == true) {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (eval_string == nullptr) {
          fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
          exit(9);
        }
      } else if ((index + 1 < nargs) &&
                 argv[index + 1] != nullptr &&
                 argv[index + 1][0] != '-') {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (strncmp(eval_string, "\\-", 2) == 0) {
          // Starts with "\\-": escaped expression, drop the backslash.
          eval_string += 1;
        }
      }
      if (eval_string != nullptr) {
        state->Set(NODE_OPTION_EVAL_STRING);
        state->SetString(NODE_OPTION_STRING_EVAL_STRING, eval_string);
      }
    } else if (strcmp(arg, "--require") == 0 ||
               strcmp(arg, "-r") == 0) {
      const char* module = argv[index + 1];
      if (module == nullptr) {
        fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
        exit(9);
      }
      args_consumed += 1;
      state->add_preload_module(module);
    } else if (strcmp(arg, "--check") == 0 || strcmp(arg, "-c") == 0) {
      state->Set(NODE_OPTION_SYNTAX_CHECK);
    } else if (strcmp(arg, "--interactive") == 0 || strcmp(arg, "-i") == 0) {
      state->Set(NODE_OPTION_INTERACTIVE);
    } else if (strcmp(arg, "--no-deprecation") == 0) {
      state->Set(NODE_OPTION_NO_DEPRECATION);
    } else if (strcmp(arg, "--napi-modules") == 0) {
      // no-op
    } else if (strcmp(arg, "--no-warnings") == 0) {
      state->Set(NODE_OPTION_NO_WARNINGS);
    } else if (strcmp(arg, "--trace-warnings") == 0) {
      state->Set(NODE_OPTION_TRACE_WARNINGS);
    } else if (strncmp(arg, "--redirect-warnings=", 20) == 0) {
      state->SetString(NODE_OPTION_STRING_WARNING_FILE, arg + 20);
    } else if (strcmp(arg, "--trace-deprecation") == 0) {
      state->Set(NODE_OPTION_TRACE_DEPRECATION);
    } else if (strcmp(arg, "--trace-sync-io") == 0) {
      state->Set(NODE_OPTION_TRACE_SYNC_IO);
    } else if (strcmp(arg, "--no-force-async-hooks-checks") == 0) {
      state->Set(NODE_OPTION_NO_FORCE_ASYNC_HOOKS_CHECKS);
    } else if (strcmp(arg, "--trace-events-enabled") == 0) {
      state->Set(NODE_OPTION_TRACE_ENABLED);
    } else if (strcmp(arg, "--trace-event-categories") == 0) {
      const char* categories = argv[index + 1];
      if (categories == nullptr) {
        fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
        exit(9);
      }
      args_consumed += 1;
      state->SetString(NODE_OPTION_STRING_TRACE_CATEGORIES, categories);
    } else if (strcmp(arg, "--track-heap-objects") == 0) {
      state->Set(NODE_OPTION_TRACK_HEAP_OBJECTS);
    } else if (strcmp(arg, "--throw-deprecation") == 0) {
      state->Set(NODE_OPTION_THROW_DEPRECATION);
    } else if (strncmp(arg, "--security-revert=", 18) == 0) {
      const char* cve = arg + 18;
      Revert(state, cve);
    } else if (strcmp(arg, "--preserve-symlinks") == 0) {
      state->Set(NODE_OPTION_PRESERVE_SYMLINKS);
    } else if (strcmp(arg, "--experimental-modules") == 0) {
      state->Set(NODE_OPTION_EXPERIMENTAL_MODULES);
      new_v8_argv[new_v8_argc] = "--harmony-dynamic-import";
      new_v8_argc += 1;
      new_v8_argv[new_v8_argc] = "--harmony-import-meta";
      new_v8_argc += 1;
    } else if (strcmp(arg, "--experimental-vm-modules") == 0) {
      state->Set(NODE_OPTION_EXPERIMENTAL_VM_MODULES);
    }  else if (strcmp(arg, "--loader") == 0) {
      const char* module = argv[index + 1];
      if (!state->IsSet(NODE_OPTION_EXPERIMENTAL_MODULES)) {
        fprintf(stderr, "%s: %s requires --experimental-modules be enabled\n",
            argv[0], arg);
        exit(9);
      }
      if (module == nullptr) {
        fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
        exit(9);
      }
      args_consumed += 1;
      state->SetString(NODE_OPTION_STRING_USERLAND_LOADER, module);
    } else if (strcmp(arg, "--prof-process") == 0) {
      state->Set(NODE_OPTION_PROF_PROCESS);
      short_circuit = true;
    } else if (strcmp(arg, "--zero-fill-buffers") == 0) {
      state->Set(NODE_OPTION_ZERO_FILL_BUFFERS);
    } else if (strcmp(arg, "--pending-deprecation") == 0) {
      state->Set(NODE_OPTION_PENDING_DEPRECATION);
    } else if (strcmp(arg, "--v8-options") == 0) {
      new_v8_argv[new_v8_argc] = "--help";
      new_v8_argc += 1;
    } else if (strncmp(arg, "--v8-pool-size=", 15) == 0) {
      state->set_v8_thread_pool_size(atoi(arg + 15));
#if HAVE_OPENSSL
    } else if (strncmp(arg, "--tls-cipher-list=", 18) == 0) {
      state->SetString(NODE_OPTION_STRING_OPENSSL_DEFAULT_CIPHER_LIST,
                       arg + 18);
    } else if (strncmp(arg, "--use-openssl-ca", 16) == 0) {
      state->Set(NODE_OPTION_OPENSSL_CERT_STORE);
      use_openssl_ca = true;
    } else if (strncmp(arg, "--use-bundled-ca", 16) == 0) {
      state->Unset(NODE_OPTION_OPENSSL_CERT_STORE);
      use_bundled_ca = true;
#if NODE_FIPS_MODE
    } else if (strcmp(arg, "--enable-fips") == 0) {
      state->Set(NODE_OPTION_OPENSSL_ENABLE_FIPS);
    } else if (strcmp(arg, "--force-fips") == 0) {
      state->Set(NODE_OPTION_OPENSSL_FORCE_FIPS);
#endif /* NODE_FIPS_MODE */
    } else if (strncmp(arg, "--openssl-config=", 17) == 0) {
      state->SetString(NODE_OPTION_STRING_OPENSSL_CONFIG, arg + 17);
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
    } else if (strncmp(arg, "--icu-data-dir=", 15) == 0) {
      state->SetString(NODE_OPTION_STRING_ICU_DATA_DIR, arg + 15);
#endif
    } else if (strcmp(arg, "--expose-internals") == 0 ||
               strcmp(arg, "--expose_internals") == 0) {
      state->Set(NODE_OPTION_EXPOSE_INTERNALS);
    } else if (strcmp(arg, "--expose-http2") == 0 ||
               strcmp(arg, "--expose_http2") == 0) {
      // Keep as a non-op through v9.x
    } else if (strcmp(arg, "-") == 0) {
      break;
    } else if (strcmp(arg, "--") == 0) {
      index += 1;
      break;
    } else if (strcmp(arg, "--abort-on-uncaught-exception") == 0 ||
               strcmp(arg, "--abort_on_uncaught_exception") == 0) {
      state->Set(NODE_OPTION_ABORT_ON_UNCAUGHT_EXCEPTION);
      // Also a V8 option.  Pass through as-is.
      new_v8_argv[new_v8_argc] = arg;
      new_v8_argc += 1;
    } else {
      if (strncmp(arg, "--prof", sizeof("--prof") - 1) == 0)
        state->Set(NODE_OPTION_PROFILING);
      // V8 option.  Pass through as-is.
      new_v8_argv[new_v8_argc] = arg;
      new_v8_argc += 1;
    }

    memcpy(new_exec_argv + new_exec_argc,
           argv + index,
           args_consumed * sizeof(*argv));

    new_exec_argc += args_consumed;
    index += args_consumed;
  }

#if HAVE_OPENSSL
  if (use_openssl_ca && use_bundled_ca) {
    fprintf(stderr,
            "%s: either --use-openssl-ca or --use-bundled-ca can be used, "
            "not both\n",
            argv[0]);
    exit(9);
  }
#endif

  if (eval_string != nullptr && state->IsSet(NODE_OPTION_SYNTAX_CHECK)) {
    fprintf(stderr,
            "%s: either --check or --eval can be used, not both\n", argv[0]);
    exit(9);
  }

  // Copy remaining arguments.
  const unsigned int args_left = nargs - index;

  if (is_env && args_left) {
    fprintf(stderr, "%s: %s is not supported in NODE_OPTIONS\n",
            argv[0], argv[index]);
    exit(9);
  }

  memcpy(new_argv + new_argc, argv + index, args_left * sizeof(*argv));
  new_argc += args_left;

  *exec_argc = new_exec_argc;
  *exec_argv = new_exec_argv;
  *v8_argc = new_v8_argc;
  *v8_argv = new_v8_argv;

  // Copy new_argv over argv and update argc.
  memcpy(argv, new_argv, new_argc * sizeof(*argv));
  delete[] new_argv;
  *argc = static_cast<int>(new_argc);
}

}  // namespace node
