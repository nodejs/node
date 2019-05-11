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
#if HAVE_INSPECTOR
#include "inspector_agent.h"
#endif
#include "handle_wrap.h"
#include "req_wrap.h"
#include "util.h"
#include "uv.h"
#include "v8.h"
#include "node.h"
#include "node_options.h"
#include "node_http2_state.h"

#include <list>
#include <stdint.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>

struct nghttp2_rcbuf;

namespace node {

namespace contextify {
class ContextifyScript;
}

namespace fs {
class FileHandleReadWrap;
}

namespace performance {
class performance_state;
}

namespace tracing {
class AgentWriterHandle;
}

namespace worker {
class Worker;
}

namespace loader {
class ModuleWrap;

struct PackageConfig {
  enum class Exists { Yes, No };
  enum class IsValid { Yes, No };
  enum class HasMain { Yes, No };

  Exists exists;
  IsValid is_valid;
  HasMain has_main;
  std::string main;
};
}  // namespace loader

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
  V(napi_wrapper, "node:napi:wrapper")                                        \
  V(sab_lifetimepartner_symbol, "node:sharedArrayBufferLifetimePartner")      \

// Symbols are per-isolate primitives but Environment proxies them
// for the sake of convenience.
#define PER_ISOLATE_SYMBOL_PROPERTIES(V)                                      \
  V(handle_onclose_symbol, "handle_onclose")                                  \
  V(oninit_symbol, "oninit")                                                  \
  V(owner_symbol, "owner")                                                    \

// Strings are per-isolate primitives but Environment proxies them
// for the sake of convenience.  Strings should be ASCII-only.
#define PER_ISOLATE_STRING_PROPERTIES(V)                                      \
  V(address_string, "address")                                                \
  V(aliases_string, "aliases")                                                \
  V(args_string, "args")                                                      \
  V(async, "async")                                                           \
  V(async_ids_stack_string, "async_ids_stack")                                \
  V(buffer_string, "buffer")                                                  \
  V(bytes_parsed_string, "bytesParsed")                                       \
  V(bytes_read_string, "bytesRead")                                           \
  V(bytes_string, "bytes")                                                    \
  V(bytes_written_string, "bytesWritten")                                     \
  V(cached_data_produced_string, "cachedDataProduced")                        \
  V(cached_data_rejected_string, "cachedDataRejected")                        \
  V(cached_data_string, "cachedData")                                         \
  V(change_string, "change")                                                  \
  V(channel_string, "channel")                                                \
  V(chunks_sent_since_last_write_string, "chunksSentSinceLastWrite")          \
  V(code_string, "code")                                                      \
  V(constants_string, "constants")                                            \
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
  V(duration_string, "duration")                                              \
  V(emit_warning_string, "emitWarning")                                       \
  V(encoding_string, "encoding")                                              \
  V(entries_string, "entries")                                                \
  V(entry_type_string, "entryType")                                           \
  V(env_pairs_string, "envPairs")                                             \
  V(env_var_settings_string, "envVarSettings")                                \
  V(errno_string, "errno")                                                    \
  V(error_string, "error")                                                    \
  V(exchange_string, "exchange")                                              \
  V(exit_code_string, "exitCode")                                             \
  V(expire_string, "expire")                                                  \
  V(exponent_string, "exponent")                                              \
  V(exports_string, "exports")                                                \
  V(ext_key_usage_string, "ext_key_usage")                                    \
  V(external_stream_string, "_externalStream")                                \
  V(family_string, "family")                                                  \
  V(fatal_exception_string, "_fatalException")                                \
  V(fd_string, "fd")                                                          \
  V(fields_string, "fields")                                                  \
  V(file_string, "file")                                                      \
  V(fingerprint256_string, "fingerprint256")                                  \
  V(fingerprint_string, "fingerprint")                                        \
  V(flags_string, "flags")                                                    \
  V(fragment_string, "fragment")                                              \
  V(get_data_clone_error_string, "_getDataCloneError")                        \
  V(get_shared_array_buffer_id_string, "_getSharedArrayBufferId")             \
  V(gid_string, "gid")                                                        \
  V(handle_string, "handle")                                                  \
  V(help_text_string, "helpText")                                             \
  V(homedir_string, "homedir")                                                \
  V(host_string, "host")                                                      \
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
  V(kind_string, "kind")                                                      \
  V(mac_string, "mac")                                                        \
  V(main_string, "main")                                                      \
  V(max_buffer_string, "maxBuffer")                                           \
  V(message_port_constructor_string, "MessagePort")                           \
  V(message_port_string, "messagePort")                                       \
  V(message_string, "message")                                                \
  V(minttl_string, "minttl")                                                  \
  V(modulus_string, "modulus")                                                \
  V(name_string, "name")                                                      \
  V(netmask_string, "netmask")                                                \
  V(nsname_string, "nsname")                                                  \
  V(ocsp_request_string, "OCSPRequest")                                       \
  V(onaltsvc_string, "onaltsvc")                                              \
  V(oncertcb_string, "oncertcb")                                              \
  V(onchange_string, "onchange")                                              \
  V(onclienthello_string, "onclienthello")                                    \
  V(oncomplete_string, "oncomplete")                                          \
  V(onconnection_string, "onconnection")                                      \
  V(ondone_string, "ondone")                                                  \
  V(onerror_string, "onerror")                                                \
  V(onexit_string, "onexit")                                                  \
  V(onframeerror_string, "onframeerror")                                      \
  V(ongetpadding_string, "ongetpadding")                                      \
  V(ongoawaydata_string, "ongoawaydata")                                      \
  V(onhandshakedone_string, "onhandshakedone")                                \
  V(onhandshakestart_string, "onhandshakestart")                              \
  V(onheaders_string, "onheaders")                                            \
  V(onkeylog_string, "onkeylog")                                              \
  V(onmessage_string, "onmessage")                                            \
  V(onnewsession_string, "onnewsession")                                      \
  V(onocspresponse_string, "onocspresponse")                                  \
  V(onorigin_string, "onorigin")                                              \
  V(onping_string, "onping")                                                  \
  V(onpriority_string, "onpriority")                                          \
  V(onread_string, "onread")                                                  \
  V(onreadstart_string, "onreadstart")                                        \
  V(onreadstop_string, "onreadstop")                                          \
  V(onsettings_string, "onsettings")                                          \
  V(onshutdown_string, "onshutdown")                                          \
  V(onsignal_string, "onsignal")                                              \
  V(onstreamclose_string, "onstreamclose")                                    \
  V(ontrailers_string, "ontrailers")                                          \
  V(onunpipe_string, "onunpipe")                                              \
  V(onwrite_string, "onwrite")                                                \
  V(openssl_error_stack, "opensslErrorStack")                                 \
  V(options_string, "options")                                                \
  V(order_string, "order")                                                    \
  V(output_string, "output")                                                  \
  V(parse_error_string, "Parse Error")                                        \
  V(password_string, "password")                                              \
  V(path_string, "path")                                                      \
  V(pending_handle_string, "pendingHandle")                                   \
  V(pid_string, "pid")                                                        \
  V(pipe_source_string, "pipeSource")                                         \
  V(pipe_string, "pipe")                                                      \
  V(pipe_target_string, "pipeTarget")                                         \
  V(port1_string, "port1")                                                    \
  V(port2_string, "port2")                                                    \
  V(port_string, "port")                                                      \
  V(preference_string, "preference")                                          \
  V(priority_string, "priority")                                              \
  V(promise_string, "promise")                                                \
  V(pubkey_string, "pubkey")                                                  \
  V(query_string, "query")                                                    \
  V(raw_string, "raw")                                                        \
  V(read_host_object_string, "_readHostObject")                               \
  V(readable_string, "readable")                                              \
  V(refresh_string, "refresh")                                                \
  V(regexp_string, "regexp")                                                  \
  V(rename_string, "rename")                                                  \
  V(replacement_string, "replacement")                                        \
  V(retry_string, "retry")                                                    \
  V(scheme_string, "scheme")                                                  \
  V(scopeid_string, "scopeid")                                                \
  V(serial_number_string, "serialNumber")                                     \
  V(serial_string, "serial")                                                  \
  V(servername_string, "servername")                                          \
  V(service_string, "service")                                                \
  V(session_id_string, "sessionId")                                           \
  V(shell_string, "shell")                                                    \
  V(signal_string, "signal")                                                  \
  V(sink_string, "sink")                                                      \
  V(size_string, "size")                                                      \
  V(sni_context_err_string, "Invalid SNI context")                            \
  V(sni_context_string, "sni_context")                                        \
  V(source_string, "source")                                                  \
  V(stack_string, "stack")                                                    \
  V(start_time_string, "startTime")                                           \
  V(status_string, "status")                                                  \
  V(stdio_string, "stdio")                                                    \
  V(subject_string, "subject")                                                \
  V(subjectaltname_string, "subjectaltname")                                  \
  V(syscall_string, "syscall")                                                \
  V(thread_id_string, "threadId")                                             \
  V(ticketkeycallback_string, "onticketkeycallback")                          \
  V(timeout_string, "timeout")                                                \
  V(tls_ticket_string, "tlsTicket")                                           \
  V(ttl_string, "ttl")                                                        \
  V(type_string, "type")                                                      \
  V(uid_string, "uid")                                                        \
  V(unknown_string, "<unknown>")                                              \
  V(url_string, "url")                                                        \
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
  V(zero_return_string, "ZERO_RETURN")                                        \

#define ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)                           \
  V(as_external, v8::External)                                                \
  V(async_hooks_after_function, v8::Function)                                 \
  V(async_hooks_before_function, v8::Function)                                \
  V(async_hooks_binding, v8::Object)                                          \
  V(async_hooks_destroy_function, v8::Function)                               \
  V(async_hooks_init_function, v8::Function)                                  \
  V(async_hooks_promise_resolve_function, v8::Function)                       \
  V(async_wrap_ctor_template, v8::FunctionTemplate)                           \
  V(async_wrap_object_ctor_template, v8::FunctionTemplate)                    \
  V(buffer_prototype_object, v8::Object)                                      \
  V(context, v8::Context)                                                     \
  V(domain_callback, v8::Function)                                            \
  V(domexception_function, v8::Function)                                      \
  V(fd_constructor_template, v8::ObjectTemplate)                              \
  V(fdclose_constructor_template, v8::ObjectTemplate)                         \
  V(filehandlereadwrap_template, v8::ObjectTemplate)                          \
  V(fs_use_promises_symbol, v8::Symbol)                                       \
  V(fsreqpromise_constructor_template, v8::ObjectTemplate)                    \
  V(handle_wrap_ctor_template, v8::FunctionTemplate)                          \
  V(host_import_module_dynamically_callback, v8::Function)                    \
  V(host_initialize_import_meta_object_callback, v8::Function)                \
  V(http2ping_constructor_template, v8::ObjectTemplate)                       \
  V(http2settings_constructor_template, v8::ObjectTemplate)                   \
  V(http2stream_constructor_template, v8::ObjectTemplate)                     \
  V(immediate_callback_function, v8::Function)                                \
  V(inspector_console_api_object, v8::Object)                                 \
  V(libuv_stream_wrap_ctor_template, v8::FunctionTemplate)                    \
  V(message_port, v8::Object)                                                 \
  V(message_port_constructor_template, v8::FunctionTemplate)                  \
  V(performance_entry_callback, v8::Function)                                 \
  V(performance_entry_template, v8::Function)                                 \
  V(pipe_constructor_template, v8::FunctionTemplate)                          \
  V(process_object, v8::Object)                                               \
  V(promise_handler_function, v8::Function)                                   \
  V(promise_wrap_template, v8::ObjectTemplate)                                \
  V(push_values_to_array_function, v8::Function)                              \
  V(sab_lifetimepartner_constructor_template, v8::FunctionTemplate)           \
  V(script_context_constructor_template, v8::FunctionTemplate)                \
  V(script_data_constructor_function, v8::Function)                           \
  V(secure_context_constructor_template, v8::FunctionTemplate)                \
  V(shutdown_wrap_template, v8::ObjectTemplate)                               \
  V(tcp_constructor_template, v8::FunctionTemplate)                           \
  V(tick_callback_function, v8::Function)                                     \
  V(timers_callback_function, v8::Function)                                   \
  V(tls_wrap_constructor_function, v8::Function)                              \
  V(tty_constructor_template, v8::FunctionTemplate)                           \
  V(udp_constructor_function, v8::Function)                                   \
  V(url_constructor_function, v8::Function)                                   \
  V(write_wrap_template, v8::ObjectTemplate)                                  \

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
  inline std::shared_ptr<PerIsolateOptions> options();

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName(v8::Isolate* isolate) const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VY
#undef VS
#undef VP

  std::unordered_map<nghttp2_rcbuf*, v8::Eternal<v8::String>> http2_static_strs;
  inline v8::Isolate* isolate() const;

 private:
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  v8::Eternal<TypeName> PropertyName ## _;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VY
#undef VS
#undef VP

  v8::Isolate* const isolate_;
  uv_loop_t* const event_loop_;
  uint32_t* const zero_fill_field_;
  MultiIsolatePlatform* platform_;
  std::shared_ptr<PerIsolateOptions> options_;

  DISALLOW_COPY_AND_ASSIGN(IsolateData);
};

struct ContextInfo {
  explicit ContextInfo(const std::string& name) : name(name) {}
  const std::string name;
  std::string origin;
  bool is_default = false;
};

struct CompileFnEntry {
  Environment* env;
  uint32_t id;
  CompileFnEntry(Environment* env, uint32_t id);
};

// Listing the AsyncWrap provider types first enables us to cast directly
// from a provider type to a debug category.
#define DEBUG_CATEGORY_NAMES(V) \
    NODE_ASYNC_PROVIDER_TYPES(V) \
    V(INSPECTOR_SERVER)

enum class DebugCategory {
#define V(name) name,
  DEBUG_CATEGORY_NAMES(V)
#undef V
  CATEGORY_COUNT
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
      explicit DefaultTriggerAsyncIdScope(AsyncWrap* async_wrap);
      ~DefaultTriggerAsyncIdScope();

     private:
      AsyncHooks* async_hooks_;
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

   private:
    Environment* env_;

    DISALLOW_COPY_AND_ASSIGN(AsyncCallbackScope);
  };

  inline size_t makecallback_depth() const;

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
    inline bool has_thrown() const;

    inline void promise_rejections_toggle_on();
    inline void set_has_thrown(bool state);

   private:
    friend class Environment;  // So we can call the constructor.
    inline explicit TickInfo(v8::Isolate* isolate);

    enum Fields {
      kHasScheduled,
      kHasPromiseRejections,
      kHasThrown,
      kFieldsCount
    };

    AliasedBuffer<uint8_t, v8::Uint8Array> fields_;

    DISALLOW_COPY_AND_ASSIGN(TickInfo);
  };

  static inline Environment* GetCurrent(v8::Isolate* isolate);
  static inline Environment* GetCurrent(v8::Local<v8::Context> context);
  static inline Environment* GetCurrent(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  template <typename T>
  static inline Environment* GetCurrent(
      const v8::PropertyCallbackInfo<T>& info);

  static uv_key_t thread_local_env;
  static inline Environment* GetThreadLocalEnv();

  Environment(IsolateData* isolate_data,
              v8::Local<v8::Context> context,
              tracing::AgentWriterHandle* tracing_agent_writer);
  ~Environment();

  void Start(const std::vector<std::string>& args,
             const std::vector<std::string>& exec_args,
             bool start_profiler_idle_notifier);

  typedef void (*HandleCleanupCb)(Environment* env,
                                  uv_handle_t* handle,
                                  void* arg);
  struct HandleCleanup {
    uv_handle_t* handle_;
    HandleCleanupCb cb_;
    void* arg_;
  };

  void RegisterHandleCleanups();
  void CleanupHandles();
  void Exit(int code);

  // Register clean-up cb to be called on environment destruction.
  inline void RegisterHandleCleanup(uv_handle_t* handle,
                                    HandleCleanupCb cb,
                                    void* arg);

  template <typename T, typename OnCloseCallback>
  inline void CloseHandle(T* handle, OnCloseCallback callback);

  inline void AssignToContext(v8::Local<v8::Context> context,
                              const ContextInfo& info);

  void StartProfilerIdleNotifier();
  void StopProfilerIdleNotifier();
  inline bool profiler_idle_notifier_started() const;

  inline v8::Isolate* isolate() const;
  inline tracing::AgentWriterHandle* tracing_agent_writer() const;
  inline uv_loop_t* event_loop() const;
  inline uint32_t watched_providers() const;

  static inline Environment* from_immediate_check_handle(uv_check_t* handle);
  inline uv_check_t* immediate_check_handle();
  inline uv_idle_t* immediate_idle_handle();

  inline void IncreaseWaitingRequestCounter();
  inline void DecreaseWaitingRequestCounter();

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

  std::unordered_multimap<int, loader::ModuleWrap*> hash_to_module_map;
  std::unordered_map<uint32_t, loader::ModuleWrap*> id_to_module_map;
  std::unordered_map<uint32_t, contextify::ContextifyScript*>
      id_to_script_map;
  std::unordered_set<CompileFnEntry*> compile_fn_entries;
  std::unordered_map<uint32_t, Persistent<v8::Function>> id_to_function_map;

  inline uint32_t get_next_module_id();
  inline uint32_t get_next_script_id();
  inline uint32_t get_next_function_id();

  std::unordered_map<std::string, const loader::PackageConfig>
      package_json_cache;

  inline double* heap_statistics_buffer() const;
  inline void set_heap_statistics_buffer(double* pointer);

  inline double* heap_space_statistics_buffer() const;
  inline void set_heap_space_statistics_buffer(double* pointer);

  inline char* http_parser_buffer() const;
  inline void set_http_parser_buffer(char* buffer);
  inline bool http_parser_buffer_in_use() const;
  inline void set_http_parser_buffer_in_use(bool in_use);

  inline http2::Http2State* http2_state() const;
  inline void set_http2_state(std::unique_ptr<http2::Http2State> state);

  inline bool debug_enabled(DebugCategory category) const;
  inline void set_debug_enabled(DebugCategory category, bool enabled);
  void set_debug_categories(const std::string& cats, bool enabled);

  inline AliasedBuffer<double, v8::Float64Array>* fs_stats_field_array();
  inline AliasedBuffer<uint64_t, v8::BigUint64Array>*
      fs_stats_field_bigint_array();

  // stat fields contains twice the number of entries because `fs.StatWatcher`
  // needs room to store data for *two* `fs.Stats` instances.
  static const int kFsStatsFieldsLength = 14;

  inline std::vector<std::unique_ptr<fs::FileHandleReadWrap>>&
      file_handle_read_wrap_freelist();

  inline performance::performance_state* performance_state();
  inline std::unordered_map<std::string, uint64_t>* performance_marks();

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

  // If this flag is set, calls into JS (if they would be observable
  // from userland) must be avoided.  This flag does not indicate whether
  // calling into JS is allowed from a VM perspective at this point.
  inline bool can_call_into_js() const;
  inline void set_can_call_into_js(bool can_call_into_js);

  inline bool is_main_thread() const;
  inline uint64_t thread_id() const;
  inline void set_thread_id(uint64_t id);
  inline worker::Worker* worker_context() const;
  inline void set_worker_context(worker::Worker* context);
  inline void add_sub_worker_context(worker::Worker* context);
  inline void remove_sub_worker_context(worker::Worker* context);
  void stop_sub_worker_contexts();
  inline bool is_stopping_worker() const;

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
                              v8::Local<v8::Signature>(),
                          v8::ConstructorBehavior behavior =
                              v8::ConstructorBehavior::kAllow,
                          v8::SideEffectType side_effect =
                              v8::SideEffectType::kHasSideEffect);

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

  // Safe variants denote the function has no side effects.
  inline void SetMethodNoSideEffect(v8::Local<v8::Object> that,
                                    const char* name,
                                    v8::FunctionCallback callback);
  inline void SetProtoMethodNoSideEffect(v8::Local<v8::FunctionTemplate> that,
                                         const char* name,
                                         v8::FunctionCallback callback);
  inline void SetTemplateMethodNoSideEffect(
      v8::Local<v8::FunctionTemplate> that,
      const char* name,
      v8::FunctionCallback callback);

  void BeforeExit(void (*cb)(void* arg), void* arg);
  void RunBeforeExitCallbacks();
  void AtExit(void (*cb)(void* arg), void* arg);
  void RunAtExitCallbacks();

  // Strings and private symbols are shared across shared contexts
  // The getters simply proxy to the per-isolate primitive.
#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VS
#undef VY
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

  void AddPromiseHook(promise_hook_func fn, void* arg);
  bool RemovePromiseHook(promise_hook_func fn, void* arg);
  inline bool EmitProcessEnvWarning() {
    bool current_value = emit_env_nonstring_warning_;
    emit_env_nonstring_warning_ = false;
    return current_value;
  }

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

  v8::Local<v8::Value> GetNow();

  inline void AddCleanupHook(void (*fn)(void*), void* arg);
  inline void RemoveCleanupHook(void (*fn)(void*), void* arg);
  void RunCleanup();

  static void BuildEmbedderGraph(v8::Isolate* isolate,
                                 v8::EmbedderGraph* graph,
                                 void* data);

  inline std::shared_ptr<EnvironmentOptions> options();

 private:
  inline void CreateImmediate(native_immediate_callback cb,
                              void* data,
                              v8::Local<v8::Object> obj,
                              bool ref);

  inline void ThrowError(v8::Local<v8::Value> (*fun)(v8::Local<v8::String>),
                         const char* errmsg);

  v8::Isolate* const isolate_;
  IsolateData* const isolate_data_;
  tracing::AgentWriterHandle* const tracing_agent_writer_;
  uv_check_t immediate_check_handle_;
  uv_idle_t immediate_idle_handle_;
  uv_prepare_t idle_prepare_handle_;
  uv_check_t idle_check_handle_;
  bool profiler_idle_notifier_started_ = false;

  AsyncHooks async_hooks_;
  ImmediateInfo immediate_info_;
  TickInfo tick_info_;
  const uint64_t timer_base_;
  bool printed_error_;
  bool abort_on_uncaught_exception_;
  bool emit_env_nonstring_warning_;
  size_t makecallback_cntr_;
  std::vector<double> destroy_async_id_list_;

  std::shared_ptr<EnvironmentOptions> options_;

  uint32_t module_id_counter_ = 0;
  uint32_t script_id_counter_ = 0;
  uint32_t function_id_counter_ = 0;

  AliasedBuffer<uint32_t, v8::Uint32Array> should_abort_on_uncaught_toggle_;
  int should_not_abort_scope_counter_ = 0;

  std::unique_ptr<performance::performance_state> performance_state_;
  std::unordered_map<std::string, uint64_t> performance_marks_;

  bool can_call_into_js_ = true;
  uint64_t thread_id_ = 0;
  std::unordered_set<worker::Worker*> sub_worker_contexts_;

  static void* kNodeContextTagPtr;
  static int const kNodeContextTag;

#if HAVE_INSPECTOR
  std::unique_ptr<inspector::Agent> inspector_agent_;
#endif

  // handle_wrap_queue_ and req_wrap_queue_ needs to be at a fixed offset from
  // the start of the class because it is used by
  // src/node_postmortem_metadata.cc to calculate offsets and generate debug
  // symbols for Environment, which assumes that the position of members in
  // memory are predictable. For more information please refer to
  // `doc/guides/node-postmortem-support.md`
  friend int GenDebugSymbols();
  HandleWrapQueue handle_wrap_queue_;
  ReqWrapQueue req_wrap_queue_;
  std::list<HandleCleanup> handle_cleanup_queue_;
  int handle_cleanup_waiting_ = 0;
  int request_waiting_ = 0;

  double* heap_statistics_buffer_ = nullptr;
  double* heap_space_statistics_buffer_ = nullptr;

  char* http_parser_buffer_;
  bool http_parser_buffer_in_use_ = false;
  std::unique_ptr<http2::Http2State> http2_state_;

  bool debug_enabled_[static_cast<int>(DebugCategory::CATEGORY_COUNT)] = {0};

  AliasedBuffer<double, v8::Float64Array> fs_stats_field_array_;
  AliasedBuffer<uint64_t, v8::BigUint64Array> fs_stats_field_bigint_array_;

  std::vector<std::unique_ptr<fs::FileHandleReadWrap>>
      file_handle_read_wrap_freelist_;

  worker::Worker* worker_context_ = nullptr;

  struct ExitCallback {
    void (*cb_)(void* arg);
    void* arg_;
  };
  std::list<ExitCallback> before_exit_functions_;

  std::list<ExitCallback> at_exit_functions_;

  struct PromiseHookCallback {
    promise_hook_func cb_;
    void* arg_;
    size_t enable_count_;
  };
  std::vector<PromiseHookCallback> promise_hooks_;

  struct NativeImmediateCallback {
    native_immediate_callback cb_;
    void* data_;
    v8::Global<v8::Object> keep_alive_;
    bool refed_;
  };
  std::vector<NativeImmediateCallback> native_immediate_callbacks_;
  void RunAndClearNativeImmediates();
  static void CheckImmediate(uv_check_t* handle);

  struct CleanupHookCallback {
    void (*fn_)(void*);
    void* arg_;

    // We keep track of the insertion order for these objects, so that we can
    // call the callbacks in reverse order when we are cleaning up.
    uint64_t insertion_order_counter_;

    // Only hashes `arg_`, since that is usually enough to identify the hook.
    struct Hash {
      inline size_t operator()(const CleanupHookCallback& cb) const;
    };

    // Compares by `fn_` and `arg_` being equal.
    struct Equal {
      inline bool operator()(const CleanupHookCallback& a,
                             const CleanupHookCallback& b) const;
    };

    inline BaseObject* GetBaseObject() const;
  };

  // Use an unordered_set, so that we have efficient insertion and removal.
  std::unordered_set<CleanupHookCallback,
                     CleanupHookCallback::Hash,
                     CleanupHookCallback::Equal> cleanup_hooks_;
  uint64_t cleanup_hook_counter_ = 0;

  static void EnvPromiseHook(v8::PromiseHookType type,
                             v8::Local<v8::Promise> promise,
                             v8::Local<v8::Value> parent);

  template <typename T>
  void ForEachBaseObject(T&& iterator);

#define V(PropertyName, TypeName) Persistent<TypeName> PropertyName ## _;
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_H_
