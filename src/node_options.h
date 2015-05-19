#ifndef SRC_NODE_OPTIONS_H_
#define SRC_NODE_OPTIONS_H_

#include "node_version.h"
#include "util.h"
#include "getopt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace node {

class NodeOptions {
 public:
  bool print_eval = false;
  bool force_repl = false;
  bool trace_deprecation = false;
  bool throw_deprecation = false;
  bool abort_on_uncaught_exception = false;
  bool trace_sync_io = false;
  const char* eval_string = nullptr;
  unsigned int preload_module_count = 0;
  const char** preload_modules = nullptr;
  bool use_debug_agent = false;
  bool debug_wait_connect = false;
  int debug_port = 5858;
  bool no_deprecation = false;
#if defined(NODE_HAVE_I18N_SUPPORT)
  // Path to ICU data (for i18n / Intl)
  const char* icu_data_dir = nullptr;
#endif

  NodeOptions() = default;

  // Print help to stdout
  void PrintHelp();

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
  void ParseArgs(int* argc,
                 const char** argv,
                 int* exec_argc,
                 const char*** exec_argv,
                 int* v8_argc,
                 const char*** v8_argv);

 private:
  DISALLOW_COPY_AND_ASSIGN(NodeOptions);
};

}  // namespace node

#endif  // SRC_NODE_OPTIONS_H_
