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
#include "inspector_profiler.h"
#endif
#include "callback_queue.h"
#include "debug_utils.h"
#include "handle_wrap.h"
#include "node.h"
#include "node_binding.h"
#include "node_main_instance.h"
#include "node_options.h"
#include "req_wrap.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace node {

namespace contextify {
class ContextifyScript;
class CompiledFnEntry;
}

namespace performance {
class PerformanceState;
}

namespace tracing {
class AgentWriterHandle;
}

#if HAVE_INSPECTOR
namespace profiler {
class V8CoverageConnection;
class V8CpuProfilerConnection;
class V8HeapProfilerConnection;
}  // namespace profiler

namespace inspector {
class ParentInspectorHandle;
}
#endif  // HAVE_INSPECTOR

namespace worker {
class Worker;
}

namespace loader {
class ModuleWrap;

struct PackageConfig {
  enum class Exists { Yes, No };
  enum class IsValid { Yes, No };
  enum class HasMain { Yes, No };
  enum class HasName { Yes, No };
  enum PackageType : uint32_t { None = 0, CommonJS, Module };

  const Exists exists;
  const IsValid is_valid;
  const HasMain has_main;
  const std::string main;
  const HasName has_name;
  const std::string name;
  const PackageType type;

  v8::Global<v8::Value> exports;
};
}  // namespace loader

enum class FsStatsOffset {
  kDev = 0,
  kMode,
  kNlink,
  kUid,
  kGid,
  kRdev,
  kBlkSize,
  kIno,
  kSize,
  kBlocks,
  kATimeSec,
  kATimeNsec,
  kMTimeSec,
  kMTimeNsec,
  kCTimeSec,
  kCTimeNsec,
  kBirthTimeSec,
  kBirthTimeNsec,
  kFsStatsFieldsNumber
};

// Stat fields buffers contain twice the number of entries in an uv_stat_t
// because `fs.StatWatcher` needs room to store 2 `fs.Stats` instances.
constexpr size_t kFsStatsBufferLength =
    static_cast<size_t>(FsStatsOffset::kFsStatsFieldsNumber) * 2;

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
  V(untransferable_object_private_symbol, "node:untransferableObject")        \

// Symbols are per-isolate primitives but Environment proxies them
// for the sake of convenience.
#define PER_ISOLATE_SYMBOL_PROPERTIES(V)                                       \
  V(async_id_symbol, "async_id_symbol")                                        \
  V(handle_onclose_symbol, "handle_onclose")                                   \
  V(no_message_symbol, "no_message_symbol")                                    \
  V(messaging_deserialize_symbol, "messaging_deserialize_symbol")              \
  V(messaging_transfer_symbol, "messaging_transfer_symbol")                    \
  V(messaging_clone_symbol, "messaging_clone_symbol")                          \
  V(messaging_transfer_list_symbol, "messaging_transfer_list_symbol")          \
  V(oninit_symbol, "oninit")                                                   \
  V(owner_symbol, "owner_symbol")                                              \
  V(onpskexchange_symbol, "onpskexchange")                                     \
  V(resource_symbol, "resource_symbol")                                        \
  V(trigger_async_id_symbol, "trigger_async_id_symbol")                        \

// Strings are per-isolate primitives but Environment proxies them
// for the sake of convenience.  Strings should be ASCII-only.
#define PER_ISOLATE_STRING_PROPERTIES(V)                                       \
  V(address_string, "address")                                                 \
  V(aliases_string, "aliases")                                                 \
  V(args_string, "args")                                                       \
  V(asn1curve_string, "asn1Curve")                                             \
  V(async_ids_stack_string, "async_ids_stack")                                 \
  V(bits_string, "bits")                                                       \
  V(buffer_string, "buffer")                                                   \
  V(bytes_parsed_string, "bytesParsed")                                        \
  V(bytes_read_string, "bytesRead")                                            \
  V(bytes_written_string, "bytesWritten")                                      \
  V(cached_data_produced_string, "cachedDataProduced")                         \
  V(cached_data_rejected_string, "cachedDataRejected")                         \
  V(cached_data_string, "cachedData")                                          \
  V(cache_key_string, "cacheKey")                                              \
  V(change_string, "change")                                                   \
  V(channel_string, "channel")                                                 \
  V(chunks_sent_since_last_write_string, "chunksSentSinceLastWrite")           \
  V(clone_unsupported_type_str, "Cannot transfer object of unsupported type.") \
  V(code_string, "code")                                                       \
  V(commonjs_string, "commonjs")                                               \
  V(config_string, "config")                                                   \
  V(constants_string, "constants")                                             \
  V(crypto_dh_string, "dh")                                                    \
  V(crypto_dsa_string, "dsa")                                                  \
  V(crypto_ec_string, "ec")                                                    \
  V(crypto_ed25519_string, "ed25519")                                          \
  V(crypto_ed448_string, "ed448")                                              \
  V(crypto_x25519_string, "x25519")                                            \
  V(crypto_x448_string, "x448")                                                \
  V(crypto_rsa_string, "rsa")                                                  \
  V(crypto_rsa_pss_string, "rsa-pss")                                          \
  V(cwd_string, "cwd")                                                         \
  V(data_string, "data")                                                       \
  V(deserialize_info_string, "deserializeInfo")                                \
  V(dest_string, "dest")                                                       \
  V(destroyed_string, "destroyed")                                             \
  V(detached_string, "detached")                                               \
  V(dh_string, "DH")                                                           \
  V(dns_a_string, "A")                                                         \
  V(dns_aaaa_string, "AAAA")                                                   \
  V(dns_cname_string, "CNAME")                                                 \
  V(dns_mx_string, "MX")                                                       \
  V(dns_naptr_string, "NAPTR")                                                 \
  V(dns_ns_string, "NS")                                                       \
  V(dns_ptr_string, "PTR")                                                     \
  V(dns_soa_string, "SOA")                                                     \
  V(dns_srv_string, "SRV")                                                     \
  V(dns_txt_string, "TXT")                                                     \
  V(done_string, "done")                                                       \
  V(duration_string, "duration")                                               \
  V(ecdh_string, "ECDH")                                                       \
  V(emit_string, "emit")                                                       \
  V(emit_warning_string, "emitWarning")                                        \
  V(empty_object_string, "{}")                                                 \
  V(encoding_string, "encoding")                                               \
  V(entries_string, "entries")                                                 \
  V(entry_type_string, "entryType")                                            \
  V(env_pairs_string, "envPairs")                                              \
  V(env_var_settings_string, "envVarSettings")                                 \
  V(errno_string, "errno")                                                     \
  V(error_string, "error")                                                     \
  V(exchange_string, "exchange")                                               \
  V(exit_code_string, "exitCode")                                              \
  V(expire_string, "expire")                                                   \
  V(exponent_string, "exponent")                                               \
  V(exports_string, "exports")                                                 \
  V(ext_key_usage_string, "ext_key_usage")                                     \
  V(external_stream_string, "_externalStream")                                 \
  V(family_string, "family")                                                   \
  V(fatal_exception_string, "_fatalException")                                 \
  V(fd_string, "fd")                                                           \
  V(fields_string, "fields")                                                   \
  V(file_string, "file")                                                       \
  V(fingerprint256_string, "fingerprint256")                                   \
  V(fingerprint_string, "fingerprint")                                         \
  V(flags_string, "flags")                                                     \
  V(fragment_string, "fragment")                                               \
  V(function_string, "function")                                               \
  V(get_data_clone_error_string, "_getDataCloneError")                         \
  V(get_shared_array_buffer_id_string, "_getSharedArrayBufferId")              \
  V(gid_string, "gid")                                                         \
  V(h2_string, "h2")                                                           \
  V(handle_string, "handle")                                                   \
  V(help_text_string, "helpText")                                              \
  V(homedir_string, "homedir")                                                 \
  V(host_string, "host")                                                       \
  V(hostmaster_string, "hostmaster")                                           \
  V(http_1_1_string, "http/1.1")                                               \
  V(identity_string, "identity")                                               \
  V(ignore_string, "ignore")                                                   \
  V(infoaccess_string, "infoAccess")                                           \
  V(inherit_string, "inherit")                                                 \
  V(input_string, "input")                                                     \
  V(internal_binding_string, "internalBinding")                                \
  V(internal_string, "internal")                                               \
  V(ipv4_string, "IPv4")                                                       \
  V(ipv6_string, "IPv6")                                                       \
  V(isclosing_string, "isClosing")                                             \
  V(issuer_string, "issuer")                                                   \
  V(issuercert_string, "issuerCertificate")                                    \
  V(kill_signal_string, "killSignal")                                          \
  V(kind_string, "kind")                                                       \
  V(length_string, "length")                                                   \
  V(library_string, "library")                                                 \
  V(mac_string, "mac")                                                         \
  V(max_buffer_string, "maxBuffer")                                            \
  V(message_port_constructor_string, "MessagePort")                            \
  V(message_port_string, "messagePort")                                        \
  V(message_string, "message")                                                 \
  V(messageerror_string, "messageerror")                                       \
  V(minttl_string, "minttl")                                                   \
  V(module_string, "module")                                                   \
  V(modulus_string, "modulus")                                                 \
  V(name_string, "name")                                                       \
  V(netmask_string, "netmask")                                                 \
  V(next_string, "next")                                                       \
  V(nistcurve_string, "nistCurve")                                             \
  V(node_string, "node")                                                       \
  V(nsname_string, "nsname")                                                   \
  V(ocsp_request_string, "OCSPRequest")                                        \
  V(oncertcb_string, "oncertcb")                                               \
  V(onchange_string, "onchange")                                               \
  V(onclienthello_string, "onclienthello")                                     \
  V(oncomplete_string, "oncomplete")                                           \
  V(onconnection_string, "onconnection")                                       \
  V(ondone_string, "ondone")                                                   \
  V(onerror_string, "onerror")                                                 \
  V(onexit_string, "onexit")                                                   \
  V(onhandshakedone_string, "onhandshakedone")                                 \
  V(onhandshakestart_string, "onhandshakestart")                               \
  V(onkeylog_string, "onkeylog")                                               \
  V(onmessage_string, "onmessage")                                             \
  V(onnewsession_string, "onnewsession")                                       \
  V(onocspresponse_string, "onocspresponse")                                   \
  V(onreadstart_string, "onreadstart")                                         \
  V(onreadstop_string, "onreadstop")                                           \
  V(onshutdown_string, "onshutdown")                                           \
  V(onsignal_string, "onsignal")                                               \
  V(onunpipe_string, "onunpipe")                                               \
  V(onwrite_string, "onwrite")                                                 \
  V(openssl_error_stack, "opensslErrorStack")                                  \
  V(options_string, "options")                                                 \
  V(order_string, "order")                                                     \
  V(output_string, "output")                                                   \
  V(parse_error_string, "Parse Error")                                         \
  V(password_string, "password")                                               \
  V(path_string, "path")                                                       \
  V(pending_handle_string, "pendingHandle")                                    \
  V(pid_string, "pid")                                                         \
  V(pipe_source_string, "pipeSource")                                          \
  V(pipe_string, "pipe")                                                       \
  V(pipe_target_string, "pipeTarget")                                          \
  V(port1_string, "port1")                                                     \
  V(port2_string, "port2")                                                     \
  V(port_string, "port")                                                       \
  V(preference_string, "preference")                                           \
  V(primordials_string, "primordials")                                         \
  V(priority_string, "priority")                                               \
  V(process_string, "process")                                                 \
  V(promise_string, "promise")                                                 \
  V(psk_string, "psk")                                                         \
  V(pubkey_string, "pubkey")                                                   \
  V(query_string, "query")                                                     \
  V(raw_string, "raw")                                                         \
  V(read_host_object_string, "_readHostObject")                                \
  V(readable_string, "readable")                                               \
  V(reason_string, "reason")                                                   \
  V(refresh_string, "refresh")                                                 \
  V(regexp_string, "regexp")                                                   \
  V(rename_string, "rename")                                                   \
  V(replacement_string, "replacement")                                         \
  V(require_string, "require")                                                 \
  V(retry_string, "retry")                                                     \
  V(scheme_string, "scheme")                                                   \
  V(scopeid_string, "scopeid")                                                 \
  V(serial_number_string, "serialNumber")                                      \
  V(serial_string, "serial")                                                   \
  V(servername_string, "servername")                                           \
  V(service_string, "service")                                                 \
  V(session_id_string, "sessionId")                                            \
  V(shell_string, "shell")                                                     \
  V(signal_string, "signal")                                                   \
  V(sink_string, "sink")                                                       \
  V(size_string, "size")                                                       \
  V(sni_context_err_string, "Invalid SNI context")                             \
  V(sni_context_string, "sni_context")                                         \
  V(source_string, "source")                                                   \
  V(stack_string, "stack")                                                     \
  V(standard_name_string, "standardName")                                      \
  V(start_time_string, "startTime")                                            \
  V(status_string, "status")                                                   \
  V(stdio_string, "stdio")                                                     \
  V(subject_string, "subject")                                                 \
  V(subjectaltname_string, "subjectaltname")                                   \
  V(syscall_string, "syscall")                                                 \
  V(target_string, "target")                                                   \
  V(thread_id_string, "threadId")                                              \
  V(ticketkeycallback_string, "onticketkeycallback")                           \
  V(timeout_string, "timeout")                                                 \
  V(tls_ticket_string, "tlsTicket")                                            \
  V(transfer_string, "transfer")                                               \
  V(ttl_string, "ttl")                                                         \
  V(type_string, "type")                                                       \
  V(uid_string, "uid")                                                         \
  V(unknown_string, "<unknown>")                                               \
  V(url_special_ftp_string, "ftp:")                                            \
  V(url_special_file_string, "file:")                                          \
  V(url_special_gopher_string, "gopher:")                                      \
  V(url_special_http_string, "http:")                                          \
  V(url_special_https_string, "https:")                                        \
  V(url_special_ws_string, "ws:")                                              \
  V(url_special_wss_string, "wss:")                                            \
  V(url_string, "url")                                                         \
  V(username_string, "username")                                               \
  V(valid_from_string, "valid_from")                                           \
  V(valid_to_string, "valid_to")                                               \
  V(value_string, "value")                                                     \
  V(verify_error_string, "verifyError")                                        \
  V(version_string, "version")                                                 \
  V(weight_string, "weight")                                                   \
  V(windows_hide_string, "windowsHide")                                        \
  V(windows_verbatim_arguments_string, "windowsVerbatimArguments")             \
  V(wrap_string, "wrap")                                                       \
  V(writable_string, "writable")                                               \
  V(write_host_object_string, "_writeHostObject")                              \
  V(write_queue_size_string, "writeQueueSize")                                 \
  V(x_forwarded_string, "x-forwarded-for")                                     \
  V(zero_return_string, "ZERO_RETURN")

#define ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V)                             \
  V(async_wrap_ctor_template, v8::FunctionTemplate)                            \
  V(async_wrap_object_ctor_template, v8::FunctionTemplate)                     \
  V(base_object_ctor_template, v8::FunctionTemplate)                           \
  V(binding_data_ctor_template, v8::FunctionTemplate)                          \
  V(compiled_fn_entry_template, v8::ObjectTemplate)                            \
  V(dir_instance_template, v8::ObjectTemplate)                                 \
  V(fd_constructor_template, v8::ObjectTemplate)                               \
  V(fdclose_constructor_template, v8::ObjectTemplate)                          \
  V(filehandlereadwrap_template, v8::ObjectTemplate)                           \
  V(fsreqpromise_constructor_template, v8::ObjectTemplate)                     \
  V(handle_wrap_ctor_template, v8::FunctionTemplate)                           \
  V(histogram_instance_template, v8::ObjectTemplate)                           \
  V(http2settings_constructor_template, v8::ObjectTemplate)                    \
  V(http2stream_constructor_template, v8::ObjectTemplate)                      \
  V(http2ping_constructor_template, v8::ObjectTemplate)                        \
  V(i18n_converter_template, v8::ObjectTemplate)                               \
  V(libuv_stream_wrap_ctor_template, v8::FunctionTemplate)                     \
  V(message_port_constructor_template, v8::FunctionTemplate)                   \
  V(microtask_queue_ctor_template, v8::FunctionTemplate)                       \
  V(pipe_constructor_template, v8::FunctionTemplate)                           \
  V(promise_wrap_template, v8::ObjectTemplate)                                 \
  V(sab_lifetimepartner_constructor_template, v8::FunctionTemplate)            \
  V(script_context_constructor_template, v8::FunctionTemplate)                 \
  V(secure_context_constructor_template, v8::FunctionTemplate)                 \
  V(shutdown_wrap_template, v8::ObjectTemplate)                                \
  V(streambaseoutputstream_constructor_template, v8::ObjectTemplate)           \
  V(tcp_constructor_template, v8::FunctionTemplate)                            \
  V(tty_constructor_template, v8::FunctionTemplate)                            \
  V(write_wrap_template, v8::ObjectTemplate)                                   \
  V(worker_heap_snapshot_taker_template, v8::ObjectTemplate)

#define ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)                                \
  V(async_hooks_after_function, v8::Function)                                  \
  V(async_hooks_before_function, v8::Function)                                 \
  V(async_hooks_callback_trampoline, v8::Function)                             \
  V(async_hooks_binding, v8::Object)                                           \
  V(async_hooks_destroy_function, v8::Function)                                \
  V(async_hooks_init_function, v8::Function)                                   \
  V(async_hooks_promise_resolve_function, v8::Function)                        \
  V(buffer_prototype_object, v8::Object)                                       \
  V(crypto_key_object_constructor, v8::Function)                               \
  V(crypto_key_object_handle_constructor, v8::Function)                        \
  V(crypto_key_object_private_constructor, v8::Function)                       \
  V(crypto_key_object_public_constructor, v8::Function)                        \
  V(crypto_key_object_secret_constructor, v8::Function)                        \
  V(domexception_function, v8::Function)                                       \
  V(enhance_fatal_stack_after_inspector, v8::Function)                         \
  V(enhance_fatal_stack_before_inspector, v8::Function)                        \
  V(fs_use_promises_symbol, v8::Symbol)                                        \
  V(host_import_module_dynamically_callback, v8::Function)                     \
  V(host_initialize_import_meta_object_callback, v8::Function)                 \
  V(http2session_on_altsvc_function, v8::Function)                             \
  V(http2session_on_error_function, v8::Function)                              \
  V(http2session_on_frame_error_function, v8::Function)                        \
  V(http2session_on_goaway_data_function, v8::Function)                        \
  V(http2session_on_headers_function, v8::Function)                            \
  V(http2session_on_origin_function, v8::Function)                             \
  V(http2session_on_ping_function, v8::Function)                               \
  V(http2session_on_priority_function, v8::Function)                           \
  V(http2session_on_settings_function, v8::Function)                           \
  V(http2session_on_stream_close_function, v8::Function)                       \
  V(http2session_on_stream_trailers_function, v8::Function)                    \
  V(internal_binding_loader, v8::Function)                                     \
  V(immediate_callback_function, v8::Function)                                 \
  V(inspector_console_extension_installer, v8::Function)                       \
  V(messaging_deserialize_create_object, v8::Function)                         \
  V(message_port, v8::Object)                                                  \
  V(native_module_require, v8::Function)                                       \
  V(performance_entry_callback, v8::Function)                                  \
  V(performance_entry_template, v8::Function)                                  \
  V(prepare_stack_trace_callback, v8::Function)                                \
  V(process_object, v8::Object)                                                \
  V(primordials, v8::Object)                                                   \
  V(promise_hook_handler, v8::Function)                                        \
  V(promise_reject_callback, v8::Function)                                     \
  V(script_data_constructor_function, v8::Function)                            \
  V(source_map_cache_getter, v8::Function)                                     \
  V(tick_callback_function, v8::Function)                                      \
  V(timers_callback_function, v8::Function)                                    \
  V(tls_wrap_constructor_function, v8::Function)                               \
  V(trace_category_state_function, v8::Function)                               \
  V(udp_constructor_function, v8::Function)                                    \
  V(url_constructor_function, v8::Function)

class Environment;
struct AllocatedBuffer;

class IsolateData : public MemoryRetainer {
 public:
  IsolateData(v8::Isolate* isolate,
              uv_loop_t* event_loop,
              MultiIsolatePlatform* platform = nullptr,
              ArrayBufferAllocator* node_allocator = nullptr,
              const std::vector<size_t>* indexes = nullptr);
  SET_MEMORY_INFO_NAME(IsolateData)
  SET_SELF_SIZE(IsolateData)
  void MemoryInfo(MemoryTracker* tracker) const override;
  std::vector<size_t> Serialize(v8::SnapshotCreator* creator);

  inline uv_loop_t* event_loop() const;
  inline MultiIsolatePlatform* platform() const;
  inline std::shared_ptr<PerIsolateOptions> options();
  inline void set_options(std::shared_ptr<PerIsolateOptions> options);

  inline NodeArrayBufferAllocator* node_allocator() const;

  inline worker::Worker* worker_context() const;
  inline void set_worker_context(worker::Worker* context);

#define VP(PropertyName, StringValue) V(v8::Private, PropertyName)
#define VY(PropertyName, StringValue) V(v8::Symbol, PropertyName)
#define VS(PropertyName, StringValue) V(v8::String, PropertyName)
#define V(TypeName, PropertyName)                                             \
  inline v8::Local<TypeName> PropertyName() const;
  PER_ISOLATE_PRIVATE_SYMBOL_PROPERTIES(VP)
  PER_ISOLATE_SYMBOL_PROPERTIES(VY)
  PER_ISOLATE_STRING_PROPERTIES(VS)
#undef V
#undef VY
#undef VS
#undef VP
  inline v8::Local<v8::String> async_wrap_provider(int index) const;

  std::unordered_map<const char*, v8::Eternal<v8::String>> static_str_map;

  inline v8::Isolate* isolate() const;
  IsolateData(const IsolateData&) = delete;
  IsolateData& operator=(const IsolateData&) = delete;
  IsolateData(IsolateData&&) = delete;
  IsolateData& operator=(IsolateData&&) = delete;

 private:
  void DeserializeProperties(const std::vector<size_t>* indexes);
  void CreateProperties();

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
  // Keep a list of all Persistent strings used for AsyncWrap Provider types.
  std::array<v8::Eternal<v8::String>, AsyncWrap::PROVIDERS_LENGTH>
      async_wrap_providers_;

  v8::Isolate* const isolate_;
  uv_loop_t* const event_loop_;
  NodeArrayBufferAllocator* const node_allocator_;
  MultiIsolatePlatform* platform_;
  std::shared_ptr<PerIsolateOptions> options_;
  worker::Worker* worker_context_ = nullptr;
};

struct ContextInfo {
  explicit ContextInfo(const std::string& name) : name(name) {}
  const std::string name;
  std::string origin;
  bool is_default = false;
};

class EnabledDebugList;

class KVStore {
 public:
  KVStore() = default;
  virtual ~KVStore() = default;
  KVStore(const KVStore&) = delete;
  KVStore& operator=(const KVStore&) = delete;
  KVStore(KVStore&&) = delete;
  KVStore& operator=(KVStore&&) = delete;

  virtual v8::MaybeLocal<v8::String> Get(v8::Isolate* isolate,
                                         v8::Local<v8::String> key) const = 0;
  virtual v8::Maybe<std::string> Get(const char* key) const = 0;
  virtual void Set(v8::Isolate* isolate,
                   v8::Local<v8::String> key,
                   v8::Local<v8::String> value) = 0;
  virtual int32_t Query(v8::Isolate* isolate,
                        v8::Local<v8::String> key) const = 0;
  virtual int32_t Query(const char* key) const = 0;
  virtual void Delete(v8::Isolate* isolate, v8::Local<v8::String> key) = 0;
  virtual v8::Local<v8::Array> Enumerate(v8::Isolate* isolate) const = 0;

  virtual std::shared_ptr<KVStore> Clone(v8::Isolate* isolate) const;
  virtual v8::Maybe<bool> AssignFromObject(v8::Local<v8::Context> context,
                                           v8::Local<v8::Object> entries);

  static std::shared_ptr<KVStore> CreateMapKVStore();
};

namespace per_process {
extern std::shared_ptr<KVStore> system_environment;
}

class AsyncHooks : public MemoryRetainer {
 public:
  SET_MEMORY_INFO_NAME(AsyncHooks)
  SET_SELF_SIZE(AsyncHooks)
  void MemoryInfo(MemoryTracker* tracker) const override;

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
    kUsesExecutionAsyncResource,
    kFieldsCount,
  };

  enum UidFields {
    kExecutionAsyncId,
    kTriggerAsyncId,
    kAsyncIdCounter,
    kDefaultTriggerAsyncId,
    kUidFieldsCount,
  };

  inline AliasedUint32Array& fields();
  inline AliasedFloat64Array& async_id_fields();
  inline AliasedFloat64Array& async_ids_stack();
  inline v8::Local<v8::Array> js_execution_async_resources();
  // Returns the native executionAsyncResource value at stack index `index`.
  // Resources provided on the JS side are not stored on the native stack,
  // in which case an empty `Local<>` is returned.
  // The `js_execution_async_resources` array contains the value in that case.
  inline v8::Local<v8::Object> native_execution_async_resource(size_t index);

  inline v8::Local<v8::String> provider_string(int idx);

  inline void no_force_checks();
  inline Environment* env();

  inline void push_async_context(double async_id, double trigger_async_id,
      v8::Local<v8::Object> execution_async_resource_);
  inline bool pop_async_context(double async_id);
  inline void clear_async_id_stack();  // Used in fatal exceptions.

  AsyncHooks(const AsyncHooks&) = delete;
  AsyncHooks& operator=(const AsyncHooks&) = delete;
  AsyncHooks(AsyncHooks&&) = delete;
  AsyncHooks& operator=(AsyncHooks&&) = delete;
  ~AsyncHooks() = default;

  // Used to set the kDefaultTriggerAsyncId in a scope. This is instead of
  // passing the trigger_async_id along with other constructor arguments.
  class DefaultTriggerAsyncIdScope {
   public:
    DefaultTriggerAsyncIdScope() = delete;
    explicit DefaultTriggerAsyncIdScope(Environment* env,
                                        double init_trigger_async_id);
    explicit DefaultTriggerAsyncIdScope(AsyncWrap* async_wrap);
    ~DefaultTriggerAsyncIdScope();

    DefaultTriggerAsyncIdScope(const DefaultTriggerAsyncIdScope&) = delete;
    DefaultTriggerAsyncIdScope& operator=(const DefaultTriggerAsyncIdScope&) =
        delete;
    DefaultTriggerAsyncIdScope(DefaultTriggerAsyncIdScope&&) = delete;
    DefaultTriggerAsyncIdScope& operator=(DefaultTriggerAsyncIdScope&&) =
        delete;

   private:
    AsyncHooks* async_hooks_;
    double old_default_trigger_async_id_;
  };

 private:
  friend class Environment;  // So we can call the constructor.
  inline AsyncHooks();
  // Stores the ids of the current execution context stack.
  AliasedFloat64Array async_ids_stack_;
  // Attached to a Uint32Array that tracks the number of active hooks for
  // each type.
  AliasedUint32Array fields_;
  // Attached to a Float64Array that tracks the state of async resources.
  AliasedFloat64Array async_id_fields_;

  void grow_async_ids_stack();

  v8::Global<v8::Array> js_execution_async_resources_;
  std::vector<v8::Global<v8::Object>> native_execution_async_resources_;
};

class ImmediateInfo : public MemoryRetainer {
 public:
  inline AliasedUint32Array& fields();
  inline uint32_t count() const;
  inline uint32_t ref_count() const;
  inline bool has_outstanding() const;
  inline void ref_count_inc(uint32_t increment);
  inline void ref_count_dec(uint32_t decrement);

  ImmediateInfo(const ImmediateInfo&) = delete;
  ImmediateInfo& operator=(const ImmediateInfo&) = delete;
  ImmediateInfo(ImmediateInfo&&) = delete;
  ImmediateInfo& operator=(ImmediateInfo&&) = delete;
  ~ImmediateInfo() = default;

  SET_MEMORY_INFO_NAME(ImmediateInfo)
  SET_SELF_SIZE(ImmediateInfo)
  void MemoryInfo(MemoryTracker* tracker) const override;

 private:
  friend class Environment;  // So we can call the constructor.
  inline explicit ImmediateInfo(v8::Isolate* isolate);

  enum Fields { kCount, kRefCount, kHasOutstanding, kFieldsCount };

  AliasedUint32Array fields_;
};

class TickInfo : public MemoryRetainer {
 public:
  inline AliasedUint8Array& fields();
  inline bool has_tick_scheduled() const;
  inline bool has_rejection_to_warn() const;

  SET_MEMORY_INFO_NAME(TickInfo)
  SET_SELF_SIZE(TickInfo)
  void MemoryInfo(MemoryTracker* tracker) const override;

  TickInfo(const TickInfo&) = delete;
  TickInfo& operator=(const TickInfo&) = delete;
  TickInfo(TickInfo&&) = delete;
  TickInfo& operator=(TickInfo&&) = delete;
  ~TickInfo() = default;

 private:
  friend class Environment;  // So we can call the constructor.
  inline explicit TickInfo(v8::Isolate* isolate);

  enum Fields { kHasTickScheduled = 0, kHasRejectionToWarn, kFieldsCount };

  AliasedUint8Array fields_;
};

class TrackingTraceStateObserver :
    public v8::TracingController::TraceStateObserver {
 public:
  explicit TrackingTraceStateObserver(Environment* env) : env_(env) {}

  void OnTraceEnabled() override {
    UpdateTraceCategoryState();
  }

  void OnTraceDisabled() override {
    UpdateTraceCategoryState();
  }

 private:
  void UpdateTraceCategoryState();

  Environment* env_;
};

class ShouldNotAbortOnUncaughtScope {
 public:
  explicit inline ShouldNotAbortOnUncaughtScope(Environment* env);
  inline void Close();
  inline ~ShouldNotAbortOnUncaughtScope();
  ShouldNotAbortOnUncaughtScope(const ShouldNotAbortOnUncaughtScope&) = delete;
  ShouldNotAbortOnUncaughtScope& operator=(
      const ShouldNotAbortOnUncaughtScope&) = delete;
  ShouldNotAbortOnUncaughtScope(ShouldNotAbortOnUncaughtScope&&) = delete;
  ShouldNotAbortOnUncaughtScope& operator=(ShouldNotAbortOnUncaughtScope&&) =
      delete;

 private:
  Environment* env_;
};

class CleanupHookCallback {
 public:
  CleanupHookCallback(void (*fn)(void*),
                      void* arg,
                      uint64_t insertion_order_counter)
      : fn_(fn), arg_(arg), insertion_order_counter_(insertion_order_counter) {}

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

 private:
  friend class Environment;
  void (*fn_)(void*);
  void* arg_;

  // We keep track of the insertion order for these objects, so that we can
  // call the callbacks in reverse order when we are cleaning up.
  uint64_t insertion_order_counter_;
};

class Environment : public MemoryRetainer {
 public:
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;
  Environment(Environment&&) = delete;
  Environment& operator=(Environment&&) = delete;

  SET_MEMORY_INFO_NAME(Environment)

  inline size_t SelfSize() const override;
  bool IsRootNode() const override { return true; }
  void MemoryInfo(MemoryTracker* tracker) const override;

  void CreateProperties();
  // Should be called before InitializeInspector()
  void InitializeDiagnostics();
#if HAVE_INSPECTOR
  // If the environment is created for a worker, pass parent_handle and
  // the ownership if transferred into the Environment.
  int InitializeInspector(
      std::unique_ptr<inspector::ParentInspectorHandle> parent_handle);
#endif

  v8::MaybeLocal<v8::Value> BootstrapInternalLoaders();
  v8::MaybeLocal<v8::Value> BootstrapNode();
  v8::MaybeLocal<v8::Value> RunBootstrapping();

  inline size_t async_callback_scope_depth() const;
  inline void PushAsyncCallbackScope();
  inline void PopAsyncCallbackScope();

  static inline Environment* GetCurrent(v8::Isolate* isolate);
  static inline Environment* GetCurrent(v8::Local<v8::Context> context);
  static inline Environment* GetCurrent(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  template <typename T>
  static inline Environment* GetCurrent(
      const v8::PropertyCallbackInfo<T>& info);

  // Methods created using SetMethod(), SetPrototypeMethod(), etc. inside
  // this scope can access the created T* object using
  // GetBindingData<T>(args) later.
  template <typename T>
  T* AddBindingData(v8::Local<v8::Context> context,
                    v8::Local<v8::Object> target);
  template <typename T, typename U>
  static inline T* GetBindingData(const v8::PropertyCallbackInfo<U>& info);
  template <typename T>
  static inline T* GetBindingData(
      const v8::FunctionCallbackInfo<v8::Value>& info);
  template <typename T>
  static inline T* GetBindingData(v8::Local<v8::Context> context);

  typedef std::unordered_map<
      FastStringKey,
      BaseObjectPtr<BaseObject>,
      FastStringKey::Hash> BindingDataStore;

  static uv_key_t thread_local_env;
  static inline Environment* GetThreadLocalEnv();

  Environment(IsolateData* isolate_data,
              v8::Local<v8::Context> context,
              const std::vector<std::string>& args,
              const std::vector<std::string>& exec_args,
              EnvironmentFlags::Flags flags,
              ThreadId thread_id);
  ~Environment() override;

  void InitializeLibuv();
  inline const std::vector<std::string>& exec_argv();
  inline const std::vector<std::string>& argv();
  const std::string& exec_path() const;

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
  void ExitEnv();

  // Register clean-up cb to be called on environment destruction.
  inline void RegisterHandleCleanup(uv_handle_t* handle,
                                    HandleCleanupCb cb,
                                    void* arg);

  template <typename T, typename OnCloseCallback>
  inline void CloseHandle(T* handle, OnCloseCallback callback);

  inline void AssignToContext(v8::Local<v8::Context> context,
                              const ContextInfo& info);

  inline v8::Isolate* isolate() const;
  inline uv_loop_t* event_loop() const;
  inline void TryLoadAddon(
      const char* filename,
      int flags,
      const std::function<bool(binding::DLib*)>& was_loaded);

  static inline Environment* from_timer_handle(uv_timer_t* handle);
  inline uv_timer_t* timer_handle();

  static inline Environment* from_immediate_check_handle(uv_check_t* handle);
  inline uv_check_t* immediate_check_handle();
  inline uv_idle_t* immediate_idle_handle();

  inline void IncreaseWaitingRequestCounter();
  inline void DecreaseWaitingRequestCounter();

  inline AsyncHooks* async_hooks();
  inline ImmediateInfo* immediate_info();
  inline TickInfo* tick_info();
  inline uint64_t timer_base() const;
  inline std::shared_ptr<KVStore> env_vars();
  inline void set_env_vars(std::shared_ptr<KVStore> env_vars);

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
  inline AliasedUint32Array& should_abort_on_uncaught_toggle();

  inline AliasedInt32Array& stream_base_state();

  // The necessary API for async_hooks.
  inline double new_async_id();
  inline double execution_async_id();
  inline double trigger_async_id();
  inline double get_default_trigger_async_id();

  // List of id's that have been destroyed and need the destroy() cb called.
  inline std::vector<double>* destroy_async_id_list();

  std::set<std::string> native_modules_with_cache;
  std::set<std::string> native_modules_without_cache;

  std::unordered_multimap<int, loader::ModuleWrap*> hash_to_module_map;
  std::unordered_map<uint32_t, loader::ModuleWrap*> id_to_module_map;
  std::unordered_map<uint32_t, contextify::ContextifyScript*>
      id_to_script_map;
  std::unordered_map<uint32_t, contextify::CompiledFnEntry*> id_to_function_map;

  inline uint32_t get_next_module_id();
  inline uint32_t get_next_script_id();
  inline uint32_t get_next_function_id();

  EnabledDebugList* enabled_debug_list() { return &enabled_debug_list_; }

  inline performance::PerformanceState* performance_state();
  inline std::unordered_map<std::string, uint64_t>* performance_marks();

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

  // Increase or decrease a counter that manages whether this Environment
  // keeps the event loop alive on its own or not. The counter starts out at 0,
  // meaning it does not, and any positive value will make it keep the event
  // loop alive.
  // This is used by Workers to manage their own .ref()/.unref() implementation,
  // as Workers aren't directly associated with their own libuv handles.
  inline void add_refs(int64_t diff);

  inline bool has_run_bootstrapping_code() const;
  inline void set_has_run_bootstrapping_code(bool has_run_bootstrapping_code);

  inline bool has_serialized_options() const;
  inline void set_has_serialized_options(bool has_serialized_options);

  inline bool is_main_thread() const;
  inline bool should_not_register_esm_loader() const;
  inline bool owns_process_state() const;
  inline bool owns_inspector() const;
  inline bool tracks_unmanaged_fds() const;
  inline uint64_t thread_id() const;
  inline worker::Worker* worker_context() const;
  Environment* worker_parent_env() const;
  inline void add_sub_worker_context(worker::Worker* context);
  inline void remove_sub_worker_context(worker::Worker* context);
  void stop_sub_worker_contexts();
  template <typename Fn>
  inline void ForEachWorker(Fn&& iterator);
  inline bool is_stopping() const;
  inline void set_stopping(bool value);
  inline std::list<node_module>* extra_linked_bindings();
  inline node_module* extra_linked_bindings_head();
  inline const Mutex& extra_linked_bindings_mutex() const;

  inline bool filehandle_close_warning() const;
  inline void set_filehandle_close_warning(bool on);

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

  inline void SetInstanceMethod(v8::Local<v8::FunctionTemplate> that,
                                const char* name,
                                v8::FunctionCallback callback);


  // Safe variants denote the function has no side effects.
  inline void SetMethodNoSideEffect(v8::Local<v8::Object> that,
                                    const char* name,
                                    v8::FunctionCallback callback);
  inline void SetProtoMethodNoSideEffect(v8::Local<v8::FunctionTemplate> that,
                                         const char* name,
                                         v8::FunctionCallback callback);

  void AtExit(void (*cb)(void* arg), void* arg);
  void RunAtExitCallbacks();

  void RunWeakRefCleanup();

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
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
  ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V)
#undef V

  inline v8::Local<v8::Context> context() const;

#if HAVE_INSPECTOR
  inline inspector::Agent* inspector_agent() const {
    return inspector_agent_.get();
  }

  inline bool is_in_inspector_console_call() const;
  inline void set_is_in_inspector_console_call(bool value);
#endif

  typedef ListHead<HandleWrap, &HandleWrap::handle_wrap_queue_> HandleWrapQueue;
  typedef ListHead<ReqWrapBase, &ReqWrapBase::req_wrap_queue_> ReqWrapQueue;

  inline HandleWrapQueue* handle_wrap_queue() { return &handle_wrap_queue_; }
  inline ReqWrapQueue* req_wrap_queue() { return &req_wrap_queue_; }

  inline bool EmitProcessEnvWarning() {
    bool current_value = emit_env_nonstring_warning_;
    emit_env_nonstring_warning_ = false;
    return current_value;
  }

  inline bool EmitErrNameWarning() {
    bool current_value = emit_err_name_warning_;
    emit_err_name_warning_ = false;
    return current_value;
  }

  // cb will be called as cb(env) on the next event loop iteration.
  // Unlike the JS setImmediate() function, nested SetImmediate() calls will
  // be run without returning control to the event loop, similar to nextTick().
  template <typename Fn>
  inline void SetImmediate(
      Fn&& cb, CallbackFlags::Flags flags = CallbackFlags::kRefed);
  template <typename Fn>
  // This behaves like SetImmediate() but can be called from any thread.
  inline void SetImmediateThreadsafe(
      Fn&& cb, CallbackFlags::Flags flags = CallbackFlags::kRefed);
  // This behaves like V8's Isolate::RequestInterrupt(), but also accounts for
  // the event loop (i.e. combines the V8 function with SetImmediate()).
  // The passed callback may not throw exceptions.
  // This function can be called from any thread.
  template <typename Fn>
  inline void RequestInterrupt(Fn&& cb);
  // This needs to be available for the JS-land setImmediate().
  void ToggleImmediateRef(bool ref);

  inline void PushShouldNotAbortOnUncaughtScope();
  inline void PopShouldNotAbortOnUncaughtScope();
  inline bool inside_should_not_abort_on_uncaught_scope() const;

  static inline Environment* ForAsyncHooks(AsyncHooks* hooks);

  v8::Local<v8::Value> GetNow();
  void ScheduleTimer(int64_t duration);
  void ToggleTimerRef(bool ref);

  inline void AddCleanupHook(void (*fn)(void*), void* arg);
  inline void RemoveCleanupHook(void (*fn)(void*), void* arg);
  void RunCleanup();

  static void BuildEmbedderGraph(v8::Isolate* isolate,
                                 v8::EmbedderGraph* graph,
                                 void* data);

  inline std::shared_ptr<EnvironmentOptions> options();
  inline std::shared_ptr<ExclusiveAccess<HostPort>> inspector_host_port();

  // The BaseObject count is a debugging helper that makes sure that there are
  // no memory leaks caused by BaseObjects staying alive longer than expected
  // (in particular, no circular BaseObjectPtr references).
  inline void modify_base_object_count(int64_t delta);
  inline int64_t base_object_count() const;

  inline int32_t stack_trace_limit() const { return 10; }

#if HAVE_INSPECTOR
  void set_coverage_connection(
      std::unique_ptr<profiler::V8CoverageConnection> connection);
  profiler::V8CoverageConnection* coverage_connection();

  inline void set_coverage_directory(const char* directory);
  inline const std::string& coverage_directory() const;

  void set_cpu_profiler_connection(
      std::unique_ptr<profiler::V8CpuProfilerConnection> connection);
  profiler::V8CpuProfilerConnection* cpu_profiler_connection();

  inline void set_cpu_prof_name(const std::string& name);
  inline const std::string& cpu_prof_name() const;

  inline void set_cpu_prof_interval(uint64_t interval);
  inline uint64_t cpu_prof_interval() const;

  inline void set_cpu_prof_dir(const std::string& dir);
  inline const std::string& cpu_prof_dir() const;

  void set_heap_profiler_connection(
      std::unique_ptr<profiler::V8HeapProfilerConnection> connection);
  profiler::V8HeapProfilerConnection* heap_profiler_connection();

  inline void set_heap_prof_name(const std::string& name);
  inline const std::string& heap_prof_name() const;

  inline void set_heap_prof_dir(const std::string& dir);
  inline const std::string& heap_prof_dir() const;

  inline void set_heap_prof_interval(uint64_t interval);
  inline uint64_t heap_prof_interval() const;

#endif  // HAVE_INSPECTOR

  inline void set_main_utf16(std::unique_ptr<v8::String::Value>);
  inline void set_process_exit_handler(
      std::function<void(Environment*, int)>&& handler);

  void RunAndClearNativeImmediates(bool only_refed = false);
  void RunAndClearInterrupts();

  inline std::unordered_map<char*, std::unique_ptr<v8::BackingStore>>*
      released_allocated_buffers();

  void AddUnmanagedFd(int fd);
  void RemoveUnmanagedFd(int fd);

 private:
  inline void ThrowError(v8::Local<v8::Value> (*fun)(v8::Local<v8::String>),
                         const char* errmsg);

  std::list<binding::DLib> loaded_addons_;
  v8::Isolate* const isolate_;
  IsolateData* const isolate_data_;
  uv_timer_t timer_handle_;
  uv_check_t immediate_check_handle_;
  uv_idle_t immediate_idle_handle_;
  uv_async_t task_queues_async_;
  int64_t task_queues_async_refs_ = 0;

  AsyncHooks async_hooks_;
  ImmediateInfo immediate_info_;
  TickInfo tick_info_;
  const uint64_t timer_base_;
  std::shared_ptr<KVStore> env_vars_;
  bool printed_error_ = false;
  bool trace_sync_io_ = false;
  bool emit_env_nonstring_warning_ = true;
  bool emit_err_name_warning_ = true;
  bool emit_filehandle_warning_ = true;
  size_t async_callback_scope_depth_ = 0;
  std::vector<double> destroy_async_id_list_;

#if HAVE_INSPECTOR
  std::unique_ptr<profiler::V8CoverageConnection> coverage_connection_;
  std::unique_ptr<profiler::V8CpuProfilerConnection> cpu_profiler_connection_;
  std::string coverage_directory_;
  std::string cpu_prof_dir_;
  std::string cpu_prof_name_;
  uint64_t cpu_prof_interval_;
  std::unique_ptr<profiler::V8HeapProfilerConnection> heap_profiler_connection_;
  std::string heap_prof_dir_;
  std::string heap_prof_name_;
  uint64_t heap_prof_interval_;
#endif  // HAVE_INSPECTOR

  std::shared_ptr<EnvironmentOptions> options_;
  // options_ contains debug options parsed from CLI arguments,
  // while inspector_host_port_ stores the actual inspector host
  // and port being used. For example the port is -1 by default
  // and can be specified as 0 (meaning any port allocated when the
  // server starts listening), but when the inspector server starts
  // the inspector_host_port_->port() will be the actual port being
  // used.
  std::shared_ptr<ExclusiveAccess<HostPort>> inspector_host_port_;
  std::vector<std::string> exec_argv_;
  std::vector<std::string> argv_;
  std::string exec_path_;

  uint32_t module_id_counter_ = 0;
  uint32_t script_id_counter_ = 0;
  uint32_t function_id_counter_ = 0;

  AliasedUint32Array should_abort_on_uncaught_toggle_;
  int should_not_abort_scope_counter_ = 0;

  std::unique_ptr<TrackingTraceStateObserver> trace_state_observer_;

  AliasedInt32Array stream_base_state_;

  std::unique_ptr<performance::PerformanceState> performance_state_;
  std::unordered_map<std::string, uint64_t> performance_marks_;

  bool has_run_bootstrapping_code_ = false;
  bool has_serialized_options_ = false;

  std::atomic_bool can_call_into_js_ { true };
  uint64_t flags_;
  uint64_t thread_id_;
  std::unordered_set<worker::Worker*> sub_worker_contexts_;

  static void* const kNodeContextTagPtr;
  static int const kNodeContextTag;

#if HAVE_INSPECTOR
  std::unique_ptr<inspector::Agent> inspector_agent_;
  bool is_in_inspector_console_call_ = false;
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

  EnabledDebugList enabled_debug_list_;

  std::list<node_module> extra_linked_bindings_;
  Mutex extra_linked_bindings_mutex_;

  static void RunTimers(uv_timer_t* handle);

  struct ExitCallback {
    void (*cb_)(void* arg);
    void* arg_;
  };

  std::list<ExitCallback> at_exit_functions_;

  typedef CallbackQueue<void, Environment*> NativeImmediateQueue;
  NativeImmediateQueue native_immediates_;
  Mutex native_immediates_threadsafe_mutex_;
  NativeImmediateQueue native_immediates_threadsafe_;
  NativeImmediateQueue native_immediates_interrupts_;
  // Also guarded by native_immediates_threadsafe_mutex_. This can be used when
  // trying to post tasks from other threads to an Environment, as the libuv
  // handle for the immediate queues (task_queues_async_) may not be initialized
  // yet or already have been destroyed.
  bool task_queues_async_initialized_ = false;

  std::atomic<Environment**> interrupt_data_ {nullptr};
  void RequestInterruptFromV8();
  static void CheckImmediate(uv_check_t* handle);

  BindingDataStore bindings_;

  // Use an unordered_set, so that we have efficient insertion and removal.
  std::unordered_set<CleanupHookCallback,
                     CleanupHookCallback::Hash,
                     CleanupHookCallback::Equal> cleanup_hooks_;
  uint64_t cleanup_hook_counter_ = 0;
  bool started_cleanup_ = false;

  int64_t base_object_count_ = 0;
  int64_t initial_base_object_count_ = 0;
  std::atomic_bool is_stopping_ { false };

  std::unordered_set<int> unmanaged_fds_;

  std::function<void(Environment*, int)> process_exit_handler_ {
      DefaultProcessExitHandler };

  template <typename T>
  void ForEachBaseObject(T&& iterator);

#define V(PropertyName, TypeName) v8::Global<TypeName> PropertyName ## _;
  ENVIRONMENT_STRONG_PERSISTENT_VALUES(V)
  ENVIRONMENT_STRONG_PERSISTENT_TEMPLATES(V)
#undef V

  v8::Global<v8::Context> context_;

  // Keeps the main script source alive is one was passed to LoadEnvironment().
  // We should probably find a way to just use plain `v8::String`s created from
  // the source passed to LoadEnvironment() directly instead.
  std::unique_ptr<v8::String::Value> main_utf16_;

  // Used by AllocatedBuffer::release() to keep track of the BackingStore for
  // a given pointer.
  std::unordered_map<char*, std::unique_ptr<v8::BackingStore>>
      released_allocated_buffers_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ENV_H_
