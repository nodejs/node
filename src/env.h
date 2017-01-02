#ifndef SRC_ENV_H_
#define SRC_ENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "ares.h"
#include "debug-agent.h"
#if HAVE_INSPECTOR
#include "inspector_agent.h"
#endif
#include "handle_wrap.h"
#include "req-wrap.h"
#include "tree.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <stdint.h>
#include <vector>

// Caveat emptor: we're going slightly crazy with macros here but the end
// hopefully justifies the means. We have a lot of per-context properties
// and adding and maintaining their getters and setters by hand would be
// a nightmare so let's make the preprocessor generate them for us.
//
// Make sure that any macros defined here are undefined again at the bottom
// of context-inl.h. The exceptions are NODE_CONTEXT_EMBEDDER_DATA_INDEX
// and NODE_ISOLATE_SLOT, they may have been defined externally.
namespace node {

// Pick an index that's hopefully out of the way when we're embedded inside
// another application. Performance-wise or memory-wise it doesn't matter:
// Context::SetAlignedPointerInEmbedderData() is backed by a FixedArray,
// worst case we pay a one-time penalty for resizing the array.
#ifndef NODE_CONTEXT_EMBEDDER_DATA_INDEX
#define NODE_CONTEXT_EMBEDDER_DATA_INDEX 32
#endif

// The slot 0 and 1 had already been taken by "gin" and "blink" in Chrome,
// and the size of isolate's slots is 4 by default, so using 3 should
// hopefully make node work independently when embedded into other
// application.
#ifndef NODE_ISOLATE_SLOT
#define NODE_ISOLATE_SLOT 3
#endif

// The number of items passed to push_values_to_array_function has diminishing
// returns around 8. This should be used at all call sites using said function.
#ifndef NODE_PUSH_VAL_TO_ARRAY_MAX
#define NODE_PUSH_VAL_TO_ARRAY_MAX 8
#endif

// Private symbols are per-isolate primitives but Environment proxies them
// for the sake of convenience.  Strings should be ASCII-only and have a
// "node:" prefix to avoid name clashes with third-party code.
#define PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(V)                              \
  V(alpn_buffer_private_symbol, "node:alpnBuffer")                            \
  V(arrow_message_private_symbol, "node:arrowMessage")                        \
  V(contextify_context_private_symbol, "node:contextify:context")             \
  V(contextify_global_private_symbol, "node:contextify:global")               \
  V(decorated_private_symbol, "node:decorated")                               \
  V(npn_buffer_private_symbol, "node:npnBuffer")                              \
  V(processed_private_symbol, "node:processed")                               \
  V(selected_npn_buffer_private_symbol, "node:selectedNpnBuffer")             \

// Strings are per-isolate primitives but Environment proxies them
// for the sake of convenience.  Strings should be ASCII-only.
#define PER_ISOLATE_STRING_PROPERTIES(V)                                      \
  V(address_string, "address")                                                \
  V(args_string, "args")                                                      \
  V(async, "async")                                                           \
  V(async_queue_string, "_asyncQueue")                                        \
  V(buffer_string, "buffer")                                                  \
  V(bytes_string, "bytes")                                                    \
  V(bytes_parsed_string, "bytesParsed")                                       \
  V(bytes_read_string, "bytesRead")                                           \
  V(cached_data_string, "cachedData")                                         \
  V(cached_data_produced_string, "cachedDataProduced")                        \
  V(cached_data_rejected_string, "cachedDataRejected")                        \
  V(callback_string, "callback")                                              \
  V(change_string, "change")                                                  \
  V(oncertcb_string, "oncertcb")                                              \
  V(onclose_string, "_onclose")                                               \
  V(code_string, "code")                                                      \
  V(cwd_string, "cwd")                                                        \
  V(dest_string, "dest")                                                      \
  V(detached_string, "detached")                                              \
  V(disposed_string, "_disposed")                                             \
  V(domain_string, "domain")                                                  \
  V(emitting_top_level_domain_error_string, "_emittingTopLevelDomainError")   \
  V(exchange_string, "exchange")                                              \
  V(idle_string, "idle")                                                      \
  V(irq_string, "irq")                                                        \
  V(encoding_string, "encoding")                                              \
  V(enter_string, "enter")                                                    \
  V(env_pairs_string, "envPairs")                                             \
  V(errno_string, "errno")                                                    \
  V(error_string, "error")                                                    \
  V(events_string, "_events")                                                 \
  V(exiting_string, "_exiting")                                               \
  V(exit_code_string, "exitCode")                                             \
  V(exit_string, "exit")                                                      \
  V(expire_string, "expire")                                                  \
  V(exponent_string, "exponent")                                              \
  V(exports_string, "exports")                                                \
  V(ext_key_usage_string, "ext_key_usage")                                    \
  V(external_string, "external")                                              \
  V(external_stream_string, "_externalStream")                                \
  V(family_string, "family")                                                  \
  V(fatal_exception_string, "_fatalException")                                \
  V(fd_string, "fd")                                                          \
  V(file_string, "file")                                                      \
  V(fingerprint_string, "fingerprint")                                        \
  V(flags_string, "flags")                                                    \
  V(gid_string, "gid")                                                        \
  V(handle_string, "handle")                                                  \
  V(heap_total_string, "heapTotal")                                           \
  V(heap_used_string, "heapUsed")                                             \
  V(homedir_string, "homedir")                                                \
  V(hostmaster_string, "hostmaster")                                          \
  V(ignore_string, "ignore")                                                  \
  V(immediate_callback_string, "_immediateCallback")                          \
  V(infoaccess_string, "infoAccess")                                          \
  V(inherit_string, "inherit")                                                \
  V(input_string, "input")                                                    \
  V(internal_string, "internal")                                              \
  V(ipv4_string, "IPv4")                                                      \
  V(ipv6_string, "IPv6")                                                      \
  V(isalive_string, "isAlive")                                                \
  V(isclosing_string, "isClosing")                                            \
  V(issuer_string, "issuer")                                                  \
  V(issuercert_string, "issuerCertificate")                                   \
  V(kill_signal_string, "killSignal")                                         \
  V(mac_string, "mac")                                                        \
  V(max_buffer_string, "maxBuffer")                                           \
  V(message_string, "message")                                                \
  V(minttl_string, "minttl")                                                  \
  V(model_string, "model")                                                    \
  V(modulus_string, "modulus")                                                \
  V(name_string, "name")                                                      \
  V(netmask_string, "netmask")                                                \
  V(nice_string, "nice")                                                      \
  V(nsname_string, "nsname")                                                  \
  V(ocsp_request_string, "OCSPRequest")                                       \
  V(onchange_string, "onchange")                                              \
  V(onclienthello_string, "onclienthello")                                    \
  V(oncomplete_string, "oncomplete")                                          \
  V(onconnection_string, "onconnection")                                      \
  V(ondone_string, "ondone")                                                  \
  V(onerror_string, "onerror")                                                \
  V(onexit_string, "onexit")                                                  \
  V(onhandshakedone_string, "onhandshakedone")                                \
  V(onhandshakestart_string, "onhandshakestart")                              \
  V(onmessage_string, "onmessage")                                            \
  V(onnewsession_string, "onnewsession")                                      \
  V(onnewsessiondone_string, "onnewsessiondone")                              \
  V(onocspresponse_string, "onocspresponse")                                  \
  V(onread_string, "onread")                                                  \
  V(onreadstart_string, "onreadstart")                                        \
  V(onreadstop_string, "onreadstop")                                          \
  V(onselect_string, "onselect")                                              \
  V(onshutdown_string, "onshutdown")                                          \
  V(onsignal_string, "onsignal")                                              \
  V(onstop_string, "onstop")                                                  \
  V(onwrite_string, "onwrite")                                                \
  V(output_string, "output")                                                  \
  V(order_string, "order")                                                    \
  V(owner_string, "owner")                                                    \
  V(parse_error_string, "Parse Error")                                        \
  V(path_string, "path")                                                      \
  V(pbkdf2_error_string, "PBKDF2 Error")                                      \
  V(pid_string, "pid")                                                        \
  V(pipe_string, "pipe")                                                      \
  V(port_string, "port")                                                      \
  V(preference_string, "preference")                                          \
  V(priority_string, "priority")                                              \
  V(produce_cached_data_string, "produceCachedData")                          \
  V(raw_string, "raw")                                                        \
  V(readable_string, "readable")                                              \
  V(received_shutdown_string, "receivedShutdown")                             \
  V(refresh_string, "refresh")                                                \
  V(regexp_string, "regexp")                                                  \
  V(rename_string, "rename")                                                  \
  V(replacement_string, "replacement")                                        \
  V(retry_string, "retry")                                                    \
  V(rss_string, "rss")                                                        \
  V(serial_string, "serial")                                                  \
  V(scopeid_string, "scopeid")                                                \
  V(sent_shutdown_string, "sentShutdown")                                     \
  V(serial_number_string, "serialNumber")                                     \
  V(service_string, "service")                                                \
  V(servername_string, "servername")                                          \
  V(session_id_string, "sessionId")                                           \
  V(shell_string, "shell")                                                    \
  V(signal_string, "signal")                                                  \
  V(size_string, "size")                                                      \
  V(sni_context_err_string, "Invalid SNI context")                            \
  V(sni_context_string, "sni_context")                                        \
  V(speed_string, "speed")                                                    \
  V(stack_string, "stack")                                                    \
  V(status_string, "status")                                                  \
  V(stdio_string, "stdio")                                                    \
  V(subject_string, "subject")                                                \
  V(subjectaltname_string, "subjectaltname")                                  \
  V(sys_string, "sys")                                                        \
  V(syscall_string, "syscall")                                                \
  V(tick_callback_string, "_tickCallback")                                    \
  V(tick_domain_cb_string, "_tickDomainCallback")                             \
  V(ticketkeycallback_string, "onticketkeycallback")                          \
  V(timeout_string, "timeout")                                                \
  V(times_string, "times")                                                    \
  V(tls_ticket_string, "tlsTicket")                                           \
  V(type_string, "type")                                                      \
  V(uid_string, "uid")                                                        \
  V(unknown_string, "<unknown>")                                              \
  V(user_string, "user")                                                      \
  V(username_string, "username")                                              \
  V(valid_from_string, "valid_from")                                          \
  V(valid_to_string, "valid_to")                                              \
  V(verify_error_string, "verifyError")                                       \
  V(version_string, "version")                                                \
  V(weight_string, "weight")                                                  \
  V(windows_verbatim_arguments_string, "windowsVerbatimArguments")            \
  V(wrap_string, "wrap")                                                      \
  V(writable_string, "writable")                                              \
  V(write_queue_size_string, "writeQueueSize")                                \
  V(x_forwarded_string, "x-forwarded-for")                                    \
  V(zero_return_string, "ZERO_RETURN")                                        \

#define ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)                           \
  V(as_external, v8::External)                                                \
  V(async_hooks_destroy_function, v8::Function)                               \
  V(async_hooks_init_function, v8::Function)                                  \
  V(async_hooks_post_function, v8::Function)                                  \
  V(async_hooks_pre_function, v8::Function)                                   \
  V(binding_cache_object, v8::Object)                                         \
  V(buffer_constructor_function, v8::Function)                                \
  V(buffer_prototype_object, v8::Object)                                      \
  V(context, v8::Context)                                                     \
  V(domain_array, v8::Array)                                                  \
  V(domains_stack_array, v8::Array)                                           \
  V(fs_stats_constructor_function, v8::Function)                              \
  V(generic_internal_field_template, v8::ObjectTemplate)                      \
  V(jsstream_constructor_template, v8::FunctionTemplate)                      \
  V(module_load_list_array, v8::Array)                                        \
  V(pipe_constructor_template, v8::FunctionTemplate)                          \
  V(process_object, v8::Object)                                               \
  V(promise_reject_function, v8::Function)                                    \
  V(push_values_to_array_function, v8::Function)                              \
  V(script_context_constructor_template, v8::FunctionTemplate)                \
  V(script_data_constructor_function, v8::Function)                           \
  V(secure_context_constructor_template, v8::FunctionTemplate)                \
  V(tcp_constructor_template, v8::FunctionTemplate)                           \
  V(tick_callback_function, v8::Function)                                     \
  V(tls_wrap_constructor_function, v8::Function)                              \
  V(tls_wrap_constructor_template, v8::FunctionTemplate)                      \
  V(tty_constructor_template, v8::FunctionTemplate)                           \
  V(udp_constructor_function, v8::Function)                                   \
  V(write_wrap_constructor_function, v8::Function)                            \

class Environment;

struct node_ares_task {
  Environment* env;
  ares_socket_t sock;
  uv_poll_t poll_watcher;
  RB_ENTRY(node_ares_task) node;
};

RB_HEAD(node_ares_task_list, node_ares_task);

class IsolateData {
 public:
  inline IsolateData(v8::Isolate* isolate, uv_loop_t* event_loop,
                     uint32_t* zero_fill_field = nullptr);
  inline uv_loop_t* event_loop() const;
  inline uint32_t* zero_fill_field() const;

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName(v8::Isolate* isolate) const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

 private:
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  v8::Eternal<TypeName> PropertyName ## _;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

  uv_loop_t* const event_loop_;
  uint32_t* const zero_fill_field_;

  DISALLOW_COPY_AND_ASSIGN(IsolateData);
};

class Environment {
 public:
  class AsyncHooks {
   public:
    inline uint32_t* fields();
    inline int fields_count() const;
    inline bool callbacks_enabled();
    inline void set_enable_callbacks(uint32_t flag);

   private:
    friend class Environment;  // So we can call the constructor.
    inline AsyncHooks();

    enum Fields {
      // Set this to not zero if the init hook should be called.
      kEnableCallbacks,
      kFieldsCount
    };

    uint32_t fields_[kFieldsCount];

    DISALLOW_COPY_AND_ASSIGN(AsyncHooks);
  };

  class AsyncCallbackScope {
   public:
    explicit AsyncCallbackScope(Environment* env);
    ~AsyncCallbackScope();

    inline bool in_makecallback();

   private:
    Environment* env_;

    DISALLOW_COPY_AND_ASSIGN(AsyncCallbackScope);
  };

  class DomainFlag {
   public:
    inline uint32_t* fields();
    inline int fields_count() const;
    inline uint32_t count() const;

   private:
    friend class Environment;  // So we can call the constructor.
    inline DomainFlag();

    enum Fields {
      kCount,
      kFieldsCount
    };

    uint32_t fields_[kFieldsCount];

    DISALLOW_COPY_AND_ASSIGN(DomainFlag);
  };

  class TickInfo {
   public:
    inline uint32_t* fields();
    inline int fields_count() const;
    inline uint32_t index() const;
    inline uint32_t length() const;
    inline void set_index(uint32_t value);

   private:
    friend class Environment;  // So we can call the constructor.
    inline TickInfo();

    enum Fields {
      kIndex,
      kLength,
      kFieldsCount
    };

    uint32_t fields_[kFieldsCount];

    DISALLOW_COPY_AND_ASSIGN(TickInfo);
  };

  typedef void (*HandleCleanupCb)(Environment* env,
                                  uv_handle_t* handle,
                                  void* arg);

  class HandleCleanup {
   private:
    friend class Environment;

    HandleCleanup(uv_handle_t* handle, HandleCleanupCb cb, void* arg)
        : handle_(handle),
          cb_(cb),
          arg_(arg) {
    }

    uv_handle_t* handle_;
    HandleCleanupCb cb_;
    void* arg_;
    ListNode<HandleCleanup> handle_cleanup_queue_;
  };

  static inline Environment* GetCurrent(v8::Isolate* isolate);
  static inline Environment* GetCurrent(v8::Local<v8::Context> context);
  static inline Environment* GetCurrent(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  template <typename T>
  static inline Environment* GetCurrent(
      const v8::PropertyCallbackInfo<T>& info);

  inline Environment(IsolateData* isolate_data, v8::Local<v8::Context> context);
  inline ~Environment();

  void Start(int argc,
             const char* const* argv,
             int exec_argc,
             const char* const* exec_argv,
             bool start_profiler_idle_notifier);
  void AssignToContext(v8::Local<v8::Context> context);

  void StartProfilerIdleNotifier();
  void StopProfilerIdleNotifier();

  inline v8::Isolate* isolate() const;
  inline uv_loop_t* event_loop() const;
  inline bool async_wrap_callbacks_enabled() const;
  inline bool in_domain() const;
  inline uint32_t watched_providers() const;

  static inline Environment* from_immediate_check_handle(uv_check_t* handle);
  static inline Environment* from_destroy_ids_idle_handle(uv_idle_t* handle);
  inline uv_check_t* immediate_check_handle();
  inline uv_idle_t* immediate_idle_handle();
  inline uv_idle_t* destroy_ids_idle_handle();

  // Register clean-up cb to be called on environment destruction.
  inline void RegisterHandleCleanup(uv_handle_t* handle,
                                    HandleCleanupCb cb,
                                    void *arg);
  inline void FinishHandleCleanup(uv_handle_t* handle);

  inline AsyncHooks* async_hooks();
  inline DomainFlag* domain_flag();
  inline TickInfo* tick_info();
  inline uint64_t timer_base() const;

  static inline Environment* from_cares_timer_handle(uv_timer_t* handle);
  inline uv_timer_t* cares_timer_handle();
  inline ares_channel cares_channel();
  inline ares_channel* cares_channel_ptr();
  inline node_ares_task_list* cares_task_list();
  inline IsolateData* isolate_data() const;

  inline bool using_domains() const;
  inline void set_using_domains(bool value);

  inline bool printed_error() const;
  inline void set_printed_error(bool value);

  void PrintSyncTrace() const;
  inline void set_trace_sync_io(bool value);

  inline int64_t get_async_wrap_uid();

  // List of id's that have been destroyed and need the destroy() cb called.
  inline std::vector<int64_t>* destroy_ids_list();

  inline double* heap_statistics_buffer() const;
  inline void set_heap_statistics_buffer(double* pointer);

  inline double* heap_space_statistics_buffer() const;
  inline void set_heap_space_statistics_buffer(double* pointer);

  inline char* http_parser_buffer() const;
  inline void set_http_parser_buffer(char* buffer);

  inline void ThrowError(const char* errmsg);
  inline void ThrowTypeError(const char* errmsg);
  inline void ThrowRangeError(const char* errmsg);
  inline void ThrowErrnoException(int errorno,
                                  const char* syscall = nullptr,
                                  const char* message = nullptr,
                                  const char* path = nullptr);
  inline void ThrowUVException(int errorno,
                               const char* syscall = nullptr,
                               const char* message = nullptr,
                               const char* path = nullptr,
                               const char* dest = nullptr);

  inline v8::Local<v8::FunctionTemplate>
      NewFunctionTemplate(v8::FunctionCallback callback,
                          v8::Local<v8::Signature> signature =
                              v8::Local<v8::Signature>());

  // Convenience methods for NewFunctionTemplate().
  inline void SetMethod(v8::Local<v8::Object> that,
                        const char* name,
                        v8::FunctionCallback callback);
  inline void SetProtoMethod(v8::Local<v8::FunctionTemplate> that,
                             const char* name,
                             v8::FunctionCallback callback);
  inline void SetTemplateMethod(v8::Local<v8::FunctionTemplate> that,
                                const char* name,
                                v8::FunctionCallback callback);

  inline v8::Local<v8::Object> NewInternalFieldObject();

  // Strings and private symbols are shared across shared contexts
  // The getters simply proxy to the per-isolate primitive.
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> PropertyName() const;                            \
  inline void set_ ## PropertyName(v8::Local<TypeName> value);
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  inline debugger::Agent* debugger_agent() {
    return &debugger_agent_;
  }

#if HAVE_INSPECTOR
  inline inspector::Agent* inspector_agent() {
    return &inspector_agent_;
  }
#endif

  typedef ListHead<HandleWrap, &HandleWrap::handle_wrap_queue_> HandleWrapQueue;
  typedef ListHead<ReqWrap<uv_req_t>, &ReqWrap<uv_req_t>::req_wrap_queue_>
          ReqWrapQueue;

  inline HandleWrapQueue* handle_wrap_queue() { return &handle_wrap_queue_; }
  inline ReqWrapQueue* req_wrap_queue() { return &req_wrap_queue_; }

  static const int kContextEmbedderDataIndex = NODE_CONTEXT_EMBEDDER_DATA_INDEX;

 private:
  inline void ThrowError(v8::Local<v8::Value> (*fun)(v8::Local<v8::String>),
                         const char* errmsg);

  v8::Isolate* const isolate_;
  IsolateData* const isolate_data_;
  uv_check_t immediate_check_handle_;
  uv_idle_t immediate_idle_handle_;
  uv_idle_t destroy_ids_idle_handle_;
  uv_prepare_t idle_prepare_handle_;
  uv_check_t idle_check_handle_;
  AsyncHooks async_hooks_;
  DomainFlag domain_flag_;
  TickInfo tick_info_;
  const uint64_t timer_base_;
  uv_timer_t cares_timer_handle_;
  ares_channel cares_channel_;
  node_ares_task_list cares_task_list_;
  bool using_domains_;
  bool printed_error_;
  bool trace_sync_io_;
  size_t makecallback_cntr_;
  int64_t async_wrap_uid_;
  std::vector<int64_t> destroy_ids_list_;
  debugger::Agent debugger_agent_;
#if HAVE_INSPECTOR
  inspector::Agent inspector_agent_;
#endif

  HandleWrapQueue handle_wrap_queue_;
  ReqWrapQueue req_wrap_queue_;
  ListHead<HandleCleanup,
           &HandleCleanup::handle_cleanup_queue_> handle_cleanup_queue_;
  int handle_cleanup_waiting_;

  double* heap_statistics_buffer_ = nullptr;
  double* heap_space_statistics_buffer_ = nullptr;

  char* http_parser_buffer_;

#define V(PropertyName, TypeName)                                             \
  v8::Persistent<TypeName> PropertyName ## _;
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_H_
