#ifndef NODE_VARS_H
#define NODE_VARS_H

// This file contains all Isolate-local variables. We allow people to
// compile Node either with Isolates or without. In the case that they
// compile without isolates, these will be static variables.

#include <v8.h>
#include <uv.h>
#include <http_parser.h>

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
  int uncaught_exception_counter;
  uv_async_t debug_watcher;
  v8::Persistent<v8::Object> binding_cache;
  v8::Persistent<v8::Array> module_load_list;
  v8::Isolate* node_isolate;
  volatile bool debugger_running;



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

  // node_http_parser.cc
  v8::Persistent<v8::String> on_headers_sym;
  v8::Persistent<v8::String> on_headers_complete_sym;
  v8::Persistent<v8::String> on_body_sym;
  v8::Persistent<v8::String> on_message_complete_sym;
  v8::Persistent<v8::String> delete_sym;
  v8::Persistent<v8::String> get_sym;
  v8::Persistent<v8::String> head_sym;
  v8::Persistent<v8::String> post_sym;
  v8::Persistent<v8::String> put_sym;
  v8::Persistent<v8::String> connect_sym;
  v8::Persistent<v8::String> options_sym;
  v8::Persistent<v8::String> trace_sym;
  v8::Persistent<v8::String> patch_sym;
  v8::Persistent<v8::String> copy_sym;
  v8::Persistent<v8::String> lock_sym;
  v8::Persistent<v8::String> mkcol_sym;
  v8::Persistent<v8::String> move_sym;
  v8::Persistent<v8::String> propfind_sym;
  v8::Persistent<v8::String> proppatch_sym;
  v8::Persistent<v8::String> unlock_sym;
  v8::Persistent<v8::String> report_sym;
  v8::Persistent<v8::String> mkactivity_sym;
  v8::Persistent<v8::String> checkout_sym;
  v8::Persistent<v8::String> merge_sym;
  v8::Persistent<v8::String> msearch_sym;
  v8::Persistent<v8::String> notify_sym;
  v8::Persistent<v8::String> subscribe_sym;
  v8::Persistent<v8::String> unsubscribe_sym;
  v8::Persistent<v8::String> unknown_method_sym;
  v8::Persistent<v8::String> method_sym;
  v8::Persistent<v8::String> status_code_sym;
  v8::Persistent<v8::String> http_version_sym;
  v8::Persistent<v8::String> version_major_sym;
  v8::Persistent<v8::String> version_minor_sym;
  v8::Persistent<v8::String> should_keep_alive_sym;
  v8::Persistent<v8::String> upgrade_sym;
  v8::Persistent<v8::String> headers_sym;
  v8::Persistent<v8::String> url_sym;
  struct http_parser_settings settings;
  // This is a hack to get the current_buffer to the callbacks with the least
  // amount of overhead. Nothing else will run while http_parser_execute()
  // runs, therefore this pointer can be set and used for the execution.
  v8::Local<v8::Value>* current_buffer;
  char* current_buffer_data;
  size_t current_buffer_len;

  // node_file.cc
  v8::Persistent<v8::String> encoding_symbol;
  v8::Persistent<v8::String> buf_symbol;
  v8::Persistent<v8::String> oncomplete_sym;
  v8::Persistent<v8::FunctionTemplate> stats_constructor_template;
  v8::Persistent<v8::String> dev_symbol;
  v8::Persistent<v8::String> ino_symbol;
  v8::Persistent<v8::String> mode_symbol;
  v8::Persistent<v8::String> nlink_symbol;
  v8::Persistent<v8::String> uid_symbol;
  v8::Persistent<v8::String> gid_symbol;
  v8::Persistent<v8::String> rdev_symbol;
  v8::Persistent<v8::String> size_symbol;
  v8::Persistent<v8::String> blksize_symbol;
  v8::Persistent<v8::String> blocks_symbol;
  v8::Persistent<v8::String> atime_symbol;
  v8::Persistent<v8::String> mtime_symbol;
  v8::Persistent<v8::String> ctime_symbol;

  // node_zlib.cc
  v8::Persistent<v8::String> callback_sym;

  // node_crypto.cc
  uv_rwlock_t* locks;
  v8::Persistent<v8::String> subject_symbol;
  v8::Persistent<v8::String> subjectaltname_symbol;
  v8::Persistent<v8::String> modulus_symbol;
  v8::Persistent<v8::String> exponent_symbol;
  v8::Persistent<v8::String> issuer_symbol;
  v8::Persistent<v8::String> valid_from_symbol;
  v8::Persistent<v8::String> valid_to_symbol;
  v8::Persistent<v8::String> fingerprint_symbol;
  v8::Persistent<v8::String> name_symbol;
  v8::Persistent<v8::String> version_symbol;
  v8::Persistent<v8::String> ext_key_usage_symbol;
  v8::Persistent<v8::FunctionTemplate> secure_context_constructor;

  // node_buffer.cc
  v8::Persistent<v8::String> length_symbol;
  v8::Persistent<v8::String> chars_written_sym;
  v8::Persistent<v8::String> write_sym;
  v8::Persistent<v8::FunctionTemplate> buffer_constructor_template;

  // platform*.cc
  char* process_title;
  struct {
    char *str;
    size_t len;
  } linux_process_title;

  // node_signal_watcher.cc
  v8::Persistent<v8::String> callback_symbol;
  v8::Persistent<v8::FunctionTemplate> signal_watcher_constructor_template;

  // cares_wrap.cc
  ::ares_channel ares_channel;
};

struct globals* globals_get();

}  // namespace node
#endif  // NODE_VARS_H
