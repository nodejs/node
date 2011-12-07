#ifndef NODE_VARS_H
#define NODE_VARS_H

// This file contains all Isolate-local variables. We allow people to
// compile Node either with Isolates or without. In the case that they
// compile without isolates, these will be static variables.

#include <v8.h>
#include <uv.h>

#ifndef PATH_MAX 
# define PATH_MAX 4096
#endif

namespace node {


#define NODE_VAR(x) (globals_get()->x)

struct globals {
  // node.cc
  v8::Persistent<v8::Object> process;
  v8::Persistent<v8::String> errno_symbol;
  v8::Persistent<v8::String> syscall_symbol;
  v8::Persistent<v8::String> errpath_symbol;
  v8::Persistent<v8::String> code_symbol;
  v8::Persistent<v8::String> rss_symbol;
  v8::Persistent<v8::String> heap_total_symbol;
  v8::Persistent<v8::String> heap_used_symbol;
  v8::Persistent<v8::String> listeners_symbol;
  v8::Persistent<v8::String> uncaught_exception_symbol;
  v8::Persistent<v8::String> emit_symbol;

  // stream_wrap.cc
  size_t slab_used;
  uv_stream_t* handle_that_last_alloced;
  v8::Persistent<v8::String> slab_sym;
  v8::Persistent<v8::String> buffer_sym;
  v8::Persistent<v8::String> write_queue_size_sym;
  bool stream_wrap_initialized;

  // tcp_wrap.cc
  v8::Persistent<v8::Function> tcpConstructor;
  v8::Persistent<v8::String> family_symbol;
  v8::Persistent<v8::String> address_symbol;
  v8::Persistent<v8::String> port_symbol;

  bool print_eval;
  char *eval_string;
  int option_end_index;
  bool use_debug_agent;
  bool debug_wait_connect;
  int debug_port;
  int max_stack_size;

  uv_check_t check_tick_watcher;
  uv_prepare_t prepare_tick_watcher;
  uv_idle_t tick_spinner;
  bool need_tick_cb;
  v8::Persistent<v8::String> tick_callback_sym;

  bool use_npn;
  bool use_sni;

  // Buffer for getpwnam_r(), getgrpam_r() and other misc callers; keep this
  // scoped at file-level rather than method-level to avoid excess stack usage.
  char getbuf[PATH_MAX + 1];

  // We need to notify V8 when we're idle so that it can run the garbage
  // collector. The interface to this is V8::IdleNotification(). It returns
  // true if the heap hasn't be fully compacted, and needs to be run again.
  // Returning false means that it doesn't have anymore work to do.
  //
  // A rather convoluted algorithm has been devised to determine when Node is
  // idle. You'll have to figure it out for yourself.
  uv_check_t gc_check;
  uv_idle_t gc_idle;
  uv_timer_t gc_timer;
  bool need_gc;

# define FAST_TICK 700.
# define GC_WAIT_TIME 5000.
# define RPM_SAMPLES 100

  int64_t tick_times[RPM_SAMPLES];
  int tick_time_head;
};

struct globals* globals_get();

}  // namespace node
#endif  // NODE_VARS_H
