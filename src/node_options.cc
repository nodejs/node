#include "node_options.h"
#include "node_version.h"
#include "node_internals.h"
#include "getopt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace node {

enum {
  OPT_NO_DEPRECATION = 1000,
  OPT_THROW_DEPRECATION,
  OPT_TRACE_DEPRECATION,
  OPT_TRACE_SYNC_IO,
  OPT_V8_OPTIONS,
  OPT_ABORT_UNCAUGHT,
  OPT_EXPOSE_INTERNALS,
  OPT_DEBUG,
  OPT_DEBUG_BRK,
  OPT_DEBUG_PORT,
#if defined(NODE_HAVE_I18N_SUPPORT)
  OPT_ICU_DATA_DIR
#endif
};

static struct option longopts[] = {
  { "help", no_argument, nullptr, 'h', "show help and usage" },
  { "version", no_argument, nullptr, 'v', "print io.js version" },
  { "eval", optional_argument, nullptr, 'e', "evaluate script" },
  { "print", optional_argument, nullptr, 'p',
    "evaluate script and print result" },
  { "interactive", no_argument, nullptr, 'i',
    "always enter the REPL even if stdin "
    "does not appear to be a terminal" },
  { "require", required_argument, nullptr, 'r', "module to preload" },
  { "no-deprecation", no_argument, nullptr, OPT_NO_DEPRECATION,
    "silence deprecation warnings" },
  { "throw-deprecation", no_argument, nullptr, OPT_THROW_DEPRECATION,
    "throw an exception anytime a deprecated function is used" },
  { "trace-deprecation", no_argument, nullptr, OPT_TRACE_DEPRECATION,
    "show stack traces on deprecations" },
  { "trace-sync-io", no_argument, nullptr, OPT_TRACE_SYNC_IO,
    "show stake trace when use of sync IO "
    "is detected after the first tick" },
  { "v8-options", no_argument, nullptr, OPT_V8_OPTIONS,
    "print v8 command line options" },
  { "v8_options", no_argument, nullptr, OPT_V8_OPTIONS,
    "print v8 command line options" },
  { "abort-on-uncaught-exception", no_argument, nullptr, OPT_ABORT_UNCAUGHT,
    "abort on uncaught exception" },
  { "abort_on_uncaught_exception", no_argument, nullptr, OPT_ABORT_UNCAUGHT,
    "abort on uncaught exception" },
  { "expose-internals", no_argument, nullptr, OPT_EXPOSE_INTERNALS,
    "expose internal modules" },
  { "expose_internals", no_argument, nullptr, OPT_EXPOSE_INTERNALS,
    "expose internal modules" },
  { "debug", optional_argument, nullptr, OPT_DEBUG, "enable debug mode" },
  { "debug-brk", optional_argument, nullptr, OPT_DEBUG_BRK,
    "break before starting" },
  { "debug-port", required_argument, nullptr, OPT_DEBUG_PORT,
    "specify debug port (defaults to 5858)" },
#if defined(NODE_HAVE_I18N_SUPPORT)
  { "icu-data-dir", required_argument, nullptr, OPT_ICU_DATA_DIR },
#endif
  { nullptr, 0, nullptr, 0, "" }
};

void NodeOptions::PrintHelp() {
  printf("Usage: iojs [options] [ -e script | script.js ] [arguments] \n"
         "       iojs debug script.js [arguments] \n"
         "\n"
         "Options:\n");
  for (size_t i = 0; i < ARRAY_SIZE(longopts); i++) {
    if (longopts[i].name == nullptr)
      continue;
    if (longopts[i].val < 1000) {
      printf("\t-%c, --%-30s %-50s\n",
             longopts[i].val,
             longopts[i].name,
             longopts[i].desc);
    } else {
      printf("\t    --%-30s %-50s\n", longopts[i].name, longopts[i].desc);
    }
  }

  printf("\n");

  printf("Environment variables:\n"
#ifdef _WIN32
         "\t  NODE_PATH              ';'-separated list of directories\n"
#else
         "\t  NODE_PATH              ':'-separated list of directories\n"
#endif
         "\t                          prefixed to the module search path.\n"
         "\t  NODE_DISABLE_COLORS     Set to 1 to disable colors in the REPL\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         "\t  NODE_ICU_DATA           Data path for ICU (Intl object) data\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         "\t                          (will extend linked-in data)\n"
#endif
#endif
         "\n"
         "Documentation can be found at https://iojs.org/\n");
}

void NodeOptions::ParseArgs(int* argc,
                            const char** argv,
                            int* exec_argc,
                            const char*** exec_argv,
                            int* v8_argc,
                            const char*** v8_argv) {
  const unsigned int nargs = static_cast<unsigned int>(*argc);
  const char** new_exec_argv = new const char*[nargs];
  const char** new_v8_argv = new const char*[nargs];
  const char** new_argv = new const char*[nargs];
  const char** local_preload_modules = new const char*[nargs];

  // we are mutating the strings vector but not the strings themselves
  char** largv = const_cast<char**>(argv);
  for (unsigned int i = 0; i < nargs; ++i) {
    new_exec_argv[i] = nullptr;
    new_v8_argv[i] = nullptr;
    new_argv[i] = nullptr;
    local_preload_modules[i] = nullptr;
  }

  const char* port = nullptr;

  // exec_argv starts with the first option, the other two start with argv[0].
  unsigned int new_exec_argc = 0;
  unsigned int new_v8_argc = 1;
  unsigned int new_argc = 1;
  new_v8_argv[0] = argv[0];
  new_argv[0] = argv[0];
  int rc = 0;
  unsigned int index = 1;
  bool is_eval = false;
  bool is_print = false;
  const char optstring[] = ":hve:p::ir:d::b::x:";
  while ((rc = getopt_long(*argc, largv, optstring, longopts, NULL)) != -1 &&
          argv[index][0] == '-') {
    unsigned int args_consumed = 1;
    const char* const arg = argv[index];
    switch (rc) {
      case 'h':
        PrintHelp();
        exit(0);
        break;
      case 'v':
        printf("%s\n", NODE_VERSION);
        exit(0);
        break;
      case 'e':
      case 'p':
      {
        if (!is_eval)
          is_eval = (rc == 'e');

        if (!is_print)
          is_print = (rc == 'p');
        const char* name = is_eval ? "eval" : "print";
        print_eval = print_eval || is_print;
        if (is_eval == true) {
          eval_string = argv[index + 1];
          args_consumed += 1;
          if (eval_string == nullptr) {
            fprintf(stderr, "%s: %s requires an argument\n", argv[0], name);
            exit(9);
          }
        } else if ((index + 1 < nargs) &&
                    argv[index + 1] != nullptr &&
                    argv[index + 1][0] != '-') {
            eval_string = argv[index + 1];
            args_consumed += 1;
            if (strncmp(eval_string, "\\-", 2) == 0) {
              // Starts with "\\-": escaped expression, drop the backslash.
              eval_string += 1;
            }
        }
        break;
      }
      case 'i':
        force_repl = true;
        break;
      case 'r':
      {
        const char* module = argv[index + 1];
        args_consumed += 1;
        if (module == nullptr) {
          fprintf(stderr, "%s: %s requires an argument\n", argv[0], arg);
          exit(9);
        }
        local_preload_modules[preload_module_count++] = module;
        break;
      }
      case OPT_NO_DEPRECATION:
        no_deprecation = true;
        break;
      case OPT_THROW_DEPRECATION:
        throw_deprecation = true;
        break;
      case OPT_TRACE_DEPRECATION:
        trace_deprecation = true;
        break;
      case OPT_TRACE_SYNC_IO:
        trace_sync_io = true;
        break;
      case OPT_V8_OPTIONS:
        new_v8_argv[new_v8_argc] = "--help";
        new_v8_argc += 1;
        break;
      case OPT_ABORT_UNCAUGHT:
        abort_on_uncaught_exception = true;
        break;
      case OPT_EXPOSE_INTERNALS:
        // pass through
        break;
      case OPT_DEBUG:
      {
        use_debug_agent = true;
        if (optarg != nullptr) {
          port = const_cast<const char*>(optarg);
        }
        break;
      }
      case OPT_DEBUG_BRK:
      {
        use_debug_agent = true;
        debug_wait_connect = true;
        if (optarg != nullptr) {
          port = const_cast<const char*>(optarg);
        }
        break;
      }
      case OPT_DEBUG_PORT:
      {
        port = optarg;
        break;
      }
#if defined(NODE_HAVE_I18N_SUPPORT)
      case OPT_ICU_DATA_DIR:
      {
        if (optarg != nullptr) {
          icu_data_dir = const_cast<const char*>(optarg);
        }
        break;
      }
#endif
      case '?':
      {
        if (arg[0] == '-') {
          // V8 option.  Pass through as-is.
          new_v8_argv[new_v8_argc] = arg;
          new_v8_argc += 1;
        }
        break;
      }
    }

    memcpy(new_exec_argv + new_exec_argc,
           largv + index,
           args_consumed * sizeof(*largv));

    new_exec_argc += args_consumed;
    index += args_consumed;
  }

  if (port != nullptr) {
    debug_port = atoi(port);
    if (debug_port < 1024 || debug_port > 65535) {
      fprintf(stderr, "Debug port must be in range 1024 to 65535.\n");
      PrintHelp();
      exit(12);
    }
  }

  // Copy remaining arguments.
  const unsigned int args_left = nargs - index;
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

  // Copy the preload_modules from the local array to an appropriately sized
  // global array.
  if (preload_module_count > 0) {
    CHECK(!preload_modules);
    preload_modules = new const char*[preload_module_count];
    memcpy(preload_modules,
           local_preload_modules,
           preload_module_count * sizeof(*preload_modules));
  }
  delete[] local_preload_modules;
}

}  // namespace node
