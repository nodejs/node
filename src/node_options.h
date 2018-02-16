#ifndef SRC_NODE_OPTIONS_H_
#define SRC_NODE_OPTIONS_H_

#include "node.h"
#include "node_debug_options.h"
#include "uv.h"

#include <string>
#include <vector>

namespace node {

#if HAVE_OPENSSL
#define DEFAULT_CIPHER_LIST_CORE "ECDHE-RSA-AES128-GCM-SHA256:"     \
                                 "ECDHE-ECDSA-AES128-GCM-SHA256:"   \
                                 "ECDHE-RSA-AES256-GCM-SHA384:"     \
                                 "ECDHE-ECDSA-AES256-GCM-SHA384:"   \
                                 "DHE-RSA-AES128-GCM-SHA256:"       \
                                 "ECDHE-RSA-AES128-SHA256:"         \
                                 "DHE-RSA-AES128-SHA256:"           \
                                 "ECDHE-RSA-AES256-SHA384:"         \
                                 "DHE-RSA-AES256-SHA384:"           \
                                 "ECDHE-RSA-AES256-SHA256:"         \
                                 "DHE-RSA-AES256-SHA256:"           \
                                 "HIGH:"                            \
                                 "!aNULL:"                          \
                                 "!eNULL:"                          \
                                 "!EXPORT:"                         \
                                 "!DES:"                            \
                                 "!RC4:"                            \
                                 "!MD5:"                            \
                                 "!PSK:"                            \
                                 "!SRP:"                            \
                                 "!CAMELLIA"
#endif

#define V8_DEFAULT_THREAD_POOL_SIZE 4;

enum NodeOptionsFlags {
  NODE_OPTION_ABORT_ON_UNCAUGHT_EXCEPTION,
  NODE_OPTION_AT_SECURE,
  NODE_OPTION_INTERACTIVE,
  NODE_OPTION_NO_DEPRECATION,
  NODE_OPTION_EXPERIMENTAL_MODULES,
  NODE_OPTION_EXPERIMENTAL_VM_MODULES,
  NODE_OPTION_EXPOSE_INTERNALS,
  NODE_OPTION_EVAL_STRING,
  NODE_OPTION_NO_FORCE_ASYNC_HOOKS_CHECKS,
  NODE_OPTION_NO_WARNINGS,
#if HAVE_OPENSSL
  NODE_OPTION_OPENSSL_CERT_STORE,
# if NODE_FIPS_MODE
  NODE_OPTION_OPENSSL_ENABLE_FIPS,
  NODE_OPTION_OPENSSL_FORCE_FIPS,
# endif
#endif
  NODE_OPTION_PENDING_DEPRECATION,
  NODE_OPTION_PRESERVE_SYMLINKS,
  NODE_OPTION_PRINT_EVAL,
  NODE_OPTION_PROFILING,
  NODE_OPTION_PROF_PROCESS,
  NODE_OPTION_SYNTAX_CHECK,
  NODE_OPTION_THROW_DEPRECATION,
  NODE_OPTION_TRACE_DEPRECATION,
  NODE_OPTION_TRACE_ENABLED,
  NODE_OPTION_TRACE_SYNC_IO,
  NODE_OPTION_TRACE_WARNINGS,
  NODE_OPTION_TRACK_HEAP_OBJECTS,
  NODE_OPTION_ZERO_FILL_BUFFERS
};

enum NodeOptionsStrings {
  NODE_OPTION_STRING_EVAL_STRING,
#if defined(NODE_HAVE_I18N_SUPPORT)
  NODE_OPTION_STRING_ICU_DATA_DIR,
#endif
#if HAVE_OPENSSL
  NODE_OPTION_STRING_OPENSSL_CONFIG,
  NODE_OPTION_STRING_OPENSSL_DEFAULT_CIPHER_LIST,
  NODE_OPTION_STRING_OPENSSL_EXTRA_CA_CERTS,
#endif
  NODE_OPTION_STRING_TRACE_CATEGORIES,
  NODE_OPTION_STRING_USERLAND_LOADER,
  NODE_OPTION_STRING_WARNING_FILE,
  NODE_OPTION_STRING_COUNT
};

// The node::NodeOptions class encapsulates startup and options details
// about the Node.js process. The NodeOptions object is created within
// node_main.cc and passed down to collect current state detail. While
// the NodeOptions is accessed via the Environment, it is *not* owned by
// the Environment and is not per-Environment.
class NodeOptions {
 public:
  static void ParseArgs(int* argc,
                        const char** argv,
                        int* exec_argc,
                        const char*** exec_argv,
                        int* v8_argc,
                        const char*** v8_argv,
                        bool is_env,
                        DebugOptions* debug_options,
                        NodeOptions* state);

  explicit NodeOptions(int flags = 0);
  ~NodeOptions() {}

  bool SafeGetenv(const char* key, std::string* text);

  bool IsSet(NodeOptionsFlags flag) const;
  int v8_thread_pool_size() const { return v8_thread_pool_size_; }
  uint64_t prog_start_time() const { return prog_start_time_; }
  const std::vector<std::string>& preload_modules() const {
    return preload_modules_;
  }
  const std::string& GetString(NodeOptionsStrings option) const;

  void SetProgStartTime() {
    prog_start_time_ = uv_now(uv_default_loop());
  }

  void SetRevert(int cve) {
    reverted_ |= (1 << cve);
  }

  bool IsReverted(int cve) const {
    return reverted_ & (1 << cve);
  }

 private:
  void SetString(NodeOptionsStrings option, const char* str);
  void Set(NodeOptionsFlags flag);
  void Unset(NodeOptionsFlags flag);

  void set_v8_thread_pool_size(int size) {
    v8_thread_pool_size_ = size;
  }

  void add_preload_module(const char* module) {
    preload_modules_.push_back(module);
  }

  // flags_ is a bitfield. If we ever end up with more than 64 distinct
  // boolean command line options this will need to be refactored a bit.
  // For now, however, this gives us room to grow.
  uint64_t flags_ = 0;
  uint64_t prog_start_time_ = 0;

  int v8_thread_pool_size_ = V8_DEFAULT_THREAD_POOL_SIZE;
  unsigned int reverted_ = 0;

  std::string strings_[NODE_OPTION_STRING_COUNT];
  std::vector<std::string> preload_modules_;
};

}  // namespace node

#endif  // SRC_NODE_OPTIONS_H_
