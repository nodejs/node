// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_ENV_H_
#define SRC_ENV_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_buffer.h"
#include "ares.h"
#if HAVE_INSPECTOR
#include "inspector_agent.h"
#endif
#include "handle_wrap.h"
#include "req_wrap.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "node.h"
#include "node_http2_state.h"

#include <list>
#include <map>
#include <stdint.h>
#include <vector>
#include <unordered_map>

struct nghttp2_rcbuf;

namespace node {

namespace performance {
class performance_state;
}

namespace loader {
class ModuleWrap;
}

// Pick an index that's hopefully out of the way when we're embedded inside
// another application. Performance-wise or memory-wise it doesn't matter:
// Context::SetAlignedPointerInEmbedderData() is backed by a FixedArray,
// worst case we pay a one-time penalty for resizing the array.
#ifndef NODE_CONTEXT_EMBEDDER_DATA_INDEX
#define NODE_CONTEXT_EMBEDDER_DATA_INDEX 32
#endif

// The number of items passed to push_values_to_array_function has diminishing
// returns around 8. This should be used at all call sites using said function.
#ifndef NODE_PUSH_VAL_TO_ARRAY_MAX
#define NODE_PUSH_VAL_TO_ARRAY_MAX 8
#endif

// PER_ISOLATE_* macros: We have a lot of per-isolate properties
// and adding and maintaining their getters and setters by hand would be
// difficult so let's make the preprocessor generate them for us.
//
// In each macro, `V` is expected to be the name of a macro or function which
// accepts the number of arguments provided in each tuple in the macro body,
// typically two. The named function will be invoked against each tuple.
//
// Make sure that any macro V defined for use with the PER_ISOLATE_* macros is
// undefined again after use.

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
  V(selected_npn_buffer_private_symbol, "node:selectedNpnBuffer")             \

// Strings are per-isolate primitives but Environment proxies them
// for the sake of convenience.  Strings should be ASCII-only.
#define PER_ISOLATE_STRING_PROPERTIES(V)                                      \
  V(address_string, "address")                                                \
  V(args_string, "args")                                                      \
  V(async, "async")                                                           \
  V(async_ids_stack_string, "async_ids_stack")                                \
  V(buffer_string, "buffer")                                                  \
  V(bytes_string, "bytes")                                                    \
  V(bytes_parsed_string, "bytesParsed")                                       \
  V(bytes_read_string, "bytesRead")                                           \
  V(cached_data_string, "cachedData")                                         \
  V(cached_data_produced_string, "cachedDataProduced")                        \
  V(cached_data_rejected_string, "cachedDataRejected")                        \
  V(change_string, "change")                                                  \
  V(channel_string, "channel")                                                \
  V(chunks_sent_since_last_write_string, "chunksSentSinceLastWrite")          \
  V(constants_string, "constants")                                            \
  V(oncertcb_string, "oncertcb")                                              \
  V(onclose_string, "_onclose")                                               \
  V(code_string, "code")                                                      \
  V(cwd_string, "cwd")                                                        \
  V(dest_string, "dest")                                                      \
  V(destroyed_string, "destroyed")                                            \
  V(detached_string, "detached")                                              \
  V(dns_a_string, "A")                                                        \
  V(dns_aaaa_string, "AAAA")                                                  \
  V(dns_cname_string, "CNAME")                                                \
  V(dns_mx_string, "MX")                                                      \
  V(dns_naptr_string, "NAPTR")                                                \
  V(dns_ns_string, "NS")                                                      \
  V(dns_ptr_string, "PTR")                                                    \
  V(dns_soa_string, "SOA")                                                    \
  V(dns_srv_string, "SRV")                                                    \
  V(dns_txt_string, "TXT")                                                    \
  V(emit_warning_string, "emitWarning")                                       \
  V(exchange_string, "exchange")                                              \
  V(encoding_string, "encoding")                                              \
  V(enter_string, "enter")                                                    \
  V(entries_string, "entries")                                                \
  V(env_pairs_string, "envPairs")                                             \
  V(errno_string, "errno")                                                    \
  V(error_string, "error")                                                    \
  V(exiting_string, "_exiting")                                               \
  V(exit_code_string, "exitCode")                                             \
  V(exit_string, "exit")                                                      \
  V(expire_string, "expire")                                                  \
  V(exponent_string, "exponent")                                              \
  V(exports_string, "exports")                                                \
  V(ext_key_usage_string, "ext_key_usage")                                    \
  V(external_stream_string, "_externalStream")                                \
  V(family_string, "family")                                                  \
  V(fatal_exception_string, "_fatalException")                                \
  V(fd_string, "fd")                                                          \
  V(file_string, "file")                                                      \
  V(fingerprint_string, "fingerprint")                                        \
  V(flags_string, "flags")                                                    \
  V(get_data_clone_error_string, "_getDataCloneError")                        \
  V(get_shared_array_buffer_id_string, "_getSharedArrayBufferId")             \
  V(gid_string, "gid")                                                        \
  V(handle_string, "handle")                                                  \
  V(homedir_string, "homedir")                                                \
  V(hostmaster_string, "hostmaster")                                          \
  V(ignore_string, "ignore")                                                  \
  V(infoaccess_string, "infoAccess")                                          \
  V(inherit_string, "inherit")                                                \
  V(input_string, "input")                                                    \
  V(internal_string, "internal")                                              \
  V(ipv4_string, "IPv4")                                                      \
  V(ipv6_string, "IPv6")                                                      \
  V(isclosing_string, "isClosing")                                            \
  V(issuer_string, "issuer")                                                  \
  V(issuercert_string, "issuerCertificate")                                   \
  V(kill_signal_string, "killSignal")                                         \
  V(mac_string, "mac")                                                        \
  V(main_string, "main")                                                      \
  V(max_buffer_string, "maxBuffer")                                           \
  V(message_string, "message")                                                \
  V(minttl_string, "minttl")                                                  \
  V(modulus_string, "modulus")                                                \
  V(name_string, "name")                                                      \
  V(netmask_string, "netmask")                                                \
  V(nsname_string, "nsname")                                                  \
  V(ocsp_request_string, "OCSPRequest")                                       \
  V(onaltsvc_string, "onaltsvc")                                              \
  V(onchange_string, "onchange")                                              \
  V(onclienthello_string, "onclienthello")                                    \
  V(oncomplete_string, "oncomplete")                                          \
  V(onconnection_string, "onconnection")                                      \
  V(ondone_string, "ondone")                                                  \
  V(onerror_string, "onerror")                                                \
  V(onexit_string, "onexit")                                                  \
  V(onframeerror_string, "onframeerror")                                      \
  V(ongetpadding_string, "ongetpadding")                                      \
  V(onhandshakedone_string, "onhandshakedone")                                \
  V(onhandshakestart_string, "onhandshakestart")                              \
  V(onheaders_string, "onheaders")                                            \
  V(onmessage_string, "onmessage")                                            \
  V(onnewsession_string, "onnewsession")                                      \
  V(onocspresponse_string, "onocspresponse")                                  \
  V(ongoawaydata_string, "ongoawaydata")                                      \
  V(onpriority_string, "onpriority")                                          \
  V(onread_string, "onread")                                                  \
  V(onreadstart_string, "onreadstart")                                        \
  V(onreadstop_string, "onreadstop")                                          \
  V(onsettings_string, "onsettings")                                          \
  V(onshutdown_string, "onshutdown")                                          \
  V(onsignal_string, "onsignal")                                              \
  V(onstop_string, "onstop")                                                  \
  V(onstreamclose_string, "onstreamclose")                                    \
  V(ontrailers_string, "ontrailers")                                          \
  V(onwrite_string, "onwrite")                                                \
  V(openssl_error_stack, "opensslErrorStack")                                 \
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
  V(read_host_object_string, "_readHostObject")                               \
  V(readable_string, "readable")                                              \
  V(refresh_string, "refresh")                                                \
  V(regexp_string, "regexp")                                                  \
  V(rename_string, "rename")                                                  \
  V(replacement_string, "replacement")                                        \
  V(retry_string, "retry")                                                    \
  V(serial_string, "serial")                                                  \
  V(scopeid_string, "scopeid")                                                \
  V(serial_number_string, "serialNumber")                                     \
  V(service_string, "service")                                                \
  V(servername_string, "servername")                                          \
  V(session_id_string, "sessionId")                                           \
  V(shell_string, "shell")                                                    \
  V(signal_string, "signal")                                                  \
  V(size_string, "size")                                                      \
  V(sni_context_err_string, "Invalid SNI context")                            \
  V(sni_context_string, "sni_context")                                        \
  V(stack_string, "stack")                                                    \
  V(status_string, "status")                                                  \
  V(stdio_string, "stdio")                                                    \
  V(subject_string, "subject")                                                \
  V(subjectaltname_string, "subjectaltname")                                  \
  V(syscall_string, "syscall")                                                \
  V(ticketkeycallback_string, "onticketkeycallback")                          \
  V(timeout_string, "timeout")                                                \
  V(tls_ticket_string, "tlsTicket")                                           \
  V(ttl_string, "ttl")                                                        \
  V(type_string, "type")                                                      \
  V(uid_string, "uid")                                                        \
  V(unknown_string, "<unknown>")                                              \
  V(user_string, "user")                                                      \
  V(username_string, "username")                                              \
  V(valid_from_string, "valid_from")                                          \
  V(valid_to_string, "valid_to")                                              \
  V(value_string, "value")                                                    \
  V(verify_error_string, "verifyError")                                       \
  V(version_string, "version")                                                \
  V(weight_string, "weight")                                                  \
  V(windows_hide_string, "windowsHide")                                       \
  V(windows_verbatim_arguments_string, "windowsVerbatimArguments")            \
  V(wrap_string, "wrap")                                                      \
  V(writable_string, "writable")                                              \
  V(write_host_object_string, "_writeHostObject")                             \
  V(write_queue_size_string, "writeQueueSize")                                \
  V(x_forwarded_string, "x-forwarded-for")                                    \
  V(zero_return_string, "ZERO_RETURN")

#define ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)                           \
  V(as_external, v8::External)                                                \
  V(async_hooks_destroy_function, v8::Function)                               \
  V(async_hooks_init_function, v8::Function)                                  \
  V(async_hooks_before_function, v8::Function)                                \
  V(async_hooks_after_function, v8::Function)                                 \
  V(async_hooks_promise_resolve_function, v8::Function)                       \
  V(async_hooks_binding, v8::Object)                                          \
  V(buffer_prototype_object, v8::Object)                                      \
  V(context, v8::Context)                                                     \
  V(domain_callback, v8::Function)                                            \
  V(host_import_module_dynamically_callback, v8::Function)                    \
  V(http2ping_constructor_template, v8::ObjectTemplate)                       \
  V(http2stream_constructor_template, v8::ObjectTemplate)                     \
  V(http2settings_constructor_template, v8::ObjectTemplate)                   \
  V(immediate_callback_function, v8::Function)                                \
  V(inspector_console_api_object, v8::Object)                                 \
  V(pbkdf2_constructor_template, v8::ObjectTemplate)                          \
  V(pipe_constructor_template, v8::FunctionTemplate)                          \
  V(performance_entry_callback, v8::Function)                                 \
  V(performance_entry_template, v8::Function)                                 \
  V(process_object, v8::Object)                                               \
  V(promise_reject_handled_function, v8::Function)                            \
  V(promise_reject_unhandled_function, v8::Function)                          \
  V(promise_wrap_template, v8::ObjectTemplate)                                \
  V(push_values_to_array_function, v8::Function)                              \
  V(randombytes_constructor_template, v8::ObjectTemplate)                     \
  V(script_context_constructor_template, v8::FunctionTemplate)                \
  V(script_data_constructor_function, v8::Function)                           \
  V(secure_context_constructor_template, v8::FunctionTemplate)                \
  V(tcp_constructor_template, v8::FunctionTemplate)                           \
  V(tick_callback_function, v8::Function)                                     \
  V(tls_wrap_constructor_function, v8::Function)                              \
  V(tty_constructor_template, v8::FunctionTemplate)                           \
  V(udp_constructor_function, v8::Function)                                   \
  V(vm_parsing_context_symbol, v8::Symbol)                                    \
  V(url_constructor_function, v8::Function)                                   \
  V(write_wrap_constructor_function, v8::Function)                            \

class Environment;

class IsolateData {
 public:
  IsolateData(v8::Isolate* isolate, uv_loop_t* event_loop,
              MultiIsolatePlatform* platform = nullptr,
              uint32_t* zero_fill_field = nullptr);
  ~IsolateData();
  inline uv_loop_t* event_loop() const;
  inline uint32_t* zero_fill_field() const;
  inline MultiIsolatePlatform* platform() const;

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName(v8::Isolate* isolate) const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VP

  std::unordered_map<nghttp2_rcbuf*, v8::Eternal<v8::String>> http2_static_strs;
  inline v8::Isolate* isolate() const;

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

  v8::Isolate* const isolate_;
  uv_loop_t* const event_loop_;
  uint32_t* const zero_fill_field_;
  MultiIsolatePlatform* platform_;

  DISALLOW_COPY_AND_ASSIGN(IsolateData);
};

struct ContextInfo {
  explicit ContextInfo(const std::string& name) : name(name) {}
  const std::string name;
  std::string origin;
  bool is_default = false;
};

class Environment {
 public:
  class AsyncHooks {
   public:
    // Reason for both UidFields and Fields are that one is stored as a double*
    // and the other as a uint32_t*.
    enum Fields {
      kInit,
      kBefore,
      kAfter,
      kDestroy,
      kPromiseResolve,
      kTotals,
      kCheck,
      kStackLength,
      kFieldsCount,
    };

    enum UidFields {
      kExecutionAsyncId,
      kTriggerAsyncId,
      kAsyncIdCounter,
      kDefaultTriggerAsyncId,
      kUidFieldsCount,
    };

    inline AliasedBuffer<uint32_t, v8::Uint32Array>& fields();
    inline AliasedBuffer<double, v8::Float64Array>& async_id_fields();
    inline AliasedBuffer<double, v8::Float64Array>& async_ids_stack();

    inline v8::Local<v8::String> provider_string(int idx);

    inline void no_force_checks();
    inline Environment* env();

    inline void push_async_ids(double async_id, double trigger_async_id);
    inline bool pop_async_id(double async_id);
    inline void clear_async_id_stack();  // Used in fatal exceptions.

    // Used to set the kDefaultTriggerAsyncId in a scope. This is instead of
    // passing the trigger_async_id along with other constructor arguments.
    class DefaultTriggerAsyncIdScope {
     public:
      DefaultTriggerAsyncIdScope() = delete;
      explicit DefaultTriggerAsyncIdScope(Environment* env,
                                          double init_trigger_async_id);
      ~DefaultTriggerAsyncIdScope();

     private:
      AliasedBuffer<double, v8::Float64Array> async_id_fields_ref_;
      double old_default_trigger_async_id_;

      DISALLOW_COPY_AND_ASSIGN(DefaultTriggerAsyncIdScope);
    };


   private:
    friend class Environment;  // So we can call the constructor.
    inline AsyncHooks();
    // Keep a list of all Persistent strings used for Provider types.
    v8::Eternal<v8::String> providers_[AsyncWrap::PROVIDERS_LENGTH];
    // Keep track of the environment copy itself.
    Environment* env_;
    // Stores the ids of the current execution context stack.
    AliasedBuffer<double, v8::Float64Array> async_ids_stack_;
    // Attached to a Uint32Array that tracks the number of active hooks for
    // each type.
    AliasedBuffer<uint32_t, v8::Uint32Array> fields_;
    // Attached to a Float64Array that tracks the state of async resources.
    AliasedBuffer<double, v8::Float64Array> async_id_fields_;

    void grow_async_ids_stack();

    DISALLOW_COPY_AND_ASSIGN(AsyncHooks);
  };

  class AsyncCallbackScope {
   public:
    AsyncCallbackScope() = delete;
    explicit AsyncCallbackScope(Environment* env);
    ~AsyncCallbackScope();
    inline bool in_makecallback() const;

   private:
    Environment* env_;

    DISALLOW_COPY_AND_ASSIGN(AsyncCallbackScope);
  };

  class ImmediateInfo {
   public:
    inline AliasedBuffer<uint32_t, v8::Uint32Array>& fields();
    inline uint32_t count() const;
    inline uint32_t ref_count() const;
    inline bool has_outstanding() const;

    inline void count_inc(uint32_t increment);
    inline void count_dec(uint32_t decrement);

    inline void ref_count_inc(uint32_t increment);
    inline void ref_count_dec(uint32_t decrement);

   private:
    friend class Environment;  // So we can call the constructor.
    inline explicit ImmediateInfo(v8::Isolate* isolate);

    enum Fields {
      kCount,
      kRefCount,
      kHasOutstanding,
      kFieldsCount
    };

    AliasedBuffer<uint32_t, v8::Uint32Array> fields_;

    DISALLOW_COPY_AND_ASSIGN(ImmediateInfo);
  };

  class TickInfo {
   public:
    inline AliasedBuffer<uint8_t, v8::Uint8Array>& fields();
    inline bool has_scheduled() const;
    inline bool has_promise_rejections() const;

    inline void promise_rejections_toggle_on();

   private:
    friend class Environment;  // So we can call the constructor.
    inline explicit TickInfo(v8::Isolate* isolate);

    enum Fields {
      kHasScheduled,
      kHasPromiseRejections,
      kFieldsCount
    };

    AliasedBuffer<uint8_t, v8::Uint8Array> fields_;

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
  void CleanupHandles();

  inline void AssignToContext(v8::Local<v8::Context> context,
                              const ContextInfo& info);

  void StartProfilerIdleNotifier();
  void StopProfilerIdleNotifier();

  inline v8::Isolate* isolate() const;
  inline uv_loop_t* event_loop() const;
  inline uint32_t watched_providers() const;

  static inline Environment* from_immediate_check_handle(uv_check_t* handle);
  inline uv_check_t* immediate_check_handle();
  inline uv_idle_t* immediate_idle_handle();

  // Register clean-up cb to be called on environment destruction.
  inline void RegisterHandleCleanup(uv_handle_t* handle,
                                    HandleCleanupCb cb,
                                    void *arg);
  inline void FinishHandleCleanup(uv_handle_t* handle);

  inline AsyncHooks* async_hooks();
  inline ImmediateInfo* immediate_info();
  inline TickInfo* tick_info();
  inline uint64_t timer_base() const;

  inline IsolateData* isolate_data() const;

  inline bool printed_error() const;
  inline void set_printed_error(bool value);

  void PrintSyncTrace() const;
  inline void set_trace_sync_io(bool value);

  // This stores whether the --abort-on-uncaught-exception flag was passed
  // to Node.
  inline bool abort_on_uncaught_exception() const;
  inline void set_abort_on_uncaught_exception(bool value);
  // This is a pseudo-boolean that keeps track of whether an uncaught exception
  // should abort the process or not if --abort-on-uncaught-exception was
  // passed to Node. If the flag was not passed, it is ignored.
  inline AliasedBuffer<uint32_t, v8::Uint32Array>&
  should_abort_on_uncaught_toggle();

  // The necessary API for async_hooks.
  inline double new_async_id();
  inline double execution_async_id();
  inline double trigger_async_id();
  inline double get_default_trigger_async_id();

  // List of id's that have been destroyed and need the destroy() cb called.
  inline std::vector<double>* destroy_async_id_list();

  std::unordered_multimap<int, loader::ModuleWrap*> module_map;

  inline double* heap_statistics_buffer() const;
  inline void set_heap_statistics_buffer(double* pointer);

  inline double* heap_space_statistics_buffer() const;
  inline void set_heap_space_statistics_buffer(double* pointer);

  inline char* http_parser_buffer() const;
  inline void set_http_parser_buffer(char* buffer);

  inline http2::http2_state* http2_state() const;
  inline void set_http2_state(std::unique_ptr<http2::http2_state> state);

  inline AliasedBuffer<double, v8::Float64Array>* fs_stats_field_array();

  inline performance::performance_state* performance_state();
  inline std::map<std::string, uint64_t>* performance_marks();

  void CollectExceptionInfo(v8::Local<v8::Value> context,
                            int errorno,
                            const char* syscall = nullptr,
                            const char* message = nullptr,
                            const char* path = nullptr);

  void CollectUVExceptionInfo(v8::Local<v8::Value> context,
                              int errorno,
                              const char* syscall = nullptr,
                              const char* message = nullptr,
                              const char* path = nullptr,
                              const char* dest = nullptr);

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

  void BeforeExit(void (*cb)(void* arg), void* arg);
  void RunBeforeExitCallbacks();
  void AtExit(void (*cb)(void* arg), void* arg);
  void RunAtExitCallbacks();

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

#if HAVE_INSPECTOR
  inline inspector::Agent* inspector_agent() const {
    return inspector_agent_.get();
  }
#endif

  typedef ListHead<HandleWrap, &HandleWrap::handle_wrap_queue_> HandleWrapQueue;
  typedef ListHead<ReqWrap<uv_req_t>, &ReqWrap<uv_req_t>::req_wrap_queue_>
          ReqWrapQueue;

  inline HandleWrapQueue* handle_wrap_queue() { return &handle_wrap_queue_; }
  inline ReqWrapQueue* req_wrap_queue() { return &req_wrap_queue_; }

  static const int kContextEmbedderDataIndex = NODE_CONTEXT_EMBEDDER_DATA_INDEX;

  void AddPromiseHook(promise_hook_func fn, void* arg);
  bool RemovePromiseHook(promise_hook_func fn, void* arg);
  bool EmitNapiWarning();

  typedef void (*native_immediate_callback)(Environment* env, void* data);
  // cb will be called as cb(env, data) on the next event loop iteration.
  // obj will be kept alive between now and after the callback has run.
  inline void SetImmediate(native_immediate_callback cb,
                           void* data,
                           v8::Local<v8::Object> obj = v8::Local<v8::Object>());
  inline void SetUnrefImmediate(native_immediate_callback cb,
                                void* data,
                                v8::Local<v8::Object> obj =
                                    v8::Local<v8::Object>());
  // This needs to be available for the JS-land setImmediate().
  void ToggleImmediateRef(bool ref);

  class ShouldNotAbortOnUncaughtScope {
   public:
    explicit inline ShouldNotAbortOnUncaughtScope(Environment* env);
    inline void Close();
    inline ~ShouldNotAbortOnUncaughtScope();

   private:
    Environment* env_;
  };

  inline bool inside_should_not_abort_on_uncaught_scope() const;

  static inline Environment* ForAsyncHooks(AsyncHooks* hooks);

 private:
  inline void CreateImmediate(native_immediate_callback cb,
                              void* data,
                              v8::Local<v8::Object> obj,
                              bool ref);

  inline void ThrowError(v8::Local<v8::Value> (*fun)(v8::Local<v8::String>),
                         const char* errmsg);

  v8::Isolate* const isolate_;
  IsolateData* const isolate_data_;
  uv_check_t immediate_check_handle_;
  uv_idle_t immediate_idle_handle_;
  uv_prepare_t idle_prepare_handle_;
  uv_check_t idle_check_handle_;

  AsyncHooks async_hooks_;
  ImmediateInfo immediate_info_;
  TickInfo tick_info_;
  const uint64_t timer_base_;
  bool printed_error_;
  bool trace_sync_io_;
  bool abort_on_uncaught_exception_;
  bool emit_napi_warning_;
  size_t makecallback_cntr_;
  std::vector<double> destroy_async_id_list_;

  AliasedBuffer<uint32_t, v8::Uint32Array> should_abort_on_uncaught_toggle_;

  int should_not_abort_scope_counter_ = 0;

  std::unique_ptr<performance::performance_state> performance_state_;
  std::map<std::string, uint64_t> performance_marks_;

#if HAVE_INSPECTOR
  std::unique_ptr<inspector::Agent> inspector_agent_;
#endif

  // handle_wrap_queue_ and req_wrap_queue_ needs to be at a fixed offset from
  // the start of the class because it is used by
  // src/node_postmortem_metadata.cc to calculate offsets and generate debug
  // symbols for Environment, which assumes that the position of members in
  // memory are predictable. For more information please refer to
  // `doc/guides/node-postmortem-support.md`
  HandleWrapQueue handle_wrap_queue_;
  ReqWrapQueue req_wrap_queue_;
  ListHead<HandleCleanup,
           &HandleCleanup::handle_cleanup_queue_> handle_cleanup_queue_;
  int handle_cleanup_waiting_;

  double* heap_statistics_buffer_ = nullptr;
  double* heap_space_statistics_buffer_ = nullptr;

  char* http_parser_buffer_;
  std::unique_ptr<http2::http2_state> http2_state_;

  // stat fields contains twice the number of entries because `fs.StatWatcher`
  // needs room to store data for *two* `fs.Stats` instances.
  static const int kFsStatsFieldsLength = 2 * 14;
  AliasedBuffer<double, v8::Float64Array> fs_stats_field_array_;

  struct BeforeExitCallback {
    void (*cb_)(void* arg);
    void* arg_;
  };
  std::list<BeforeExitCallback> before_exit_functions_;

  struct AtExitCallback {
    void (*cb_)(void* arg);
    void* arg_;
  };
  std::list<AtExitCallback> at_exit_functions_;

  struct PromiseHookCallback {
    promise_hook_func cb_;
    void* arg_;
    size_t enable_count_;
  };
  std::vector<PromiseHookCallback> promise_hooks_;

  struct NativeImmediateCallback {
    native_immediate_callback cb_;
    void* data_;
    std::unique_ptr<v8::Persistent<v8::Object>> keep_alive_;
    bool refed_;
  };
  std::vector<NativeImmediateCallback> native_immediate_callbacks_;
  void RunAndClearNativeImmediates();
  static void CheckImmediate(uv_check_t* handle);

  static void EnvPromiseHook(v8::PromiseHookType type,
                             v8::Local<v8::Promise> promise,
                             v8::Local<v8::Value> parent);

#define V(PropertyName, TypeName)                                             \
  v8::Persistent<TypeName> PropertyName ## _;
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_H_
