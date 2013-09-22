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

#include "ares.h"
#include "tree.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

#include <stdint.h>

// Caveat emptor: we're going slightly crazy with macros here but the end
// hopefully justifies the means. We have a lot of per-context properties
// and adding and maintaining their getters and setters by hand would be
// a nightmare so let's make the preprocessor generate them for us.
//
// Make sure that any macros defined here are undefined again at the bottom
// of context-inl.h. The sole exception is NODE_CONTEXT_EMBEDDER_DATA_INDEX,
// it may have been defined externally.
namespace node {

// Pick an index that's hopefully out of the way when we're embedded inside
// another application. Performance-wise or memory-wise it doesn't matter:
// Context::SetAlignedPointerInEmbedderData() is backed by a FixedArray,
// worst case we pay a one-time penalty for resizing the array.
#ifndef NODE_CONTEXT_EMBEDDER_DATA_INDEX
#define NODE_CONTEXT_EMBEDDER_DATA_INDEX 32
#endif

// Strings are per-isolate primitives but Environment proxies them
// for the sake of convenience.
#define PER_ISOLATE_STRING_PROPERTIES(V)                                      \
  V(address_string, "address")                                                \
  V(atime_string, "atime")                                                    \
  V(async_queue_string, "_asyncQueue")                                        \
  V(birthtime_string, "birthtime")                                            \
  V(blksize_string, "blksize")                                                \
  V(blocks_string, "blocks")                                                  \
  V(buffer_string, "buffer")                                                  \
  V(bytes_string, "bytes")                                                    \
  V(callback_string, "callback")                                              \
  V(change_string, "change")                                                  \
  V(close_string, "close")                                                    \
  V(code_string, "code")                                                      \
  V(ctime_string, "ctime")                                                    \
  V(dev_string, "dev")                                                        \
  V(disposed_string, "_disposed")                                             \
  V(enter_string, "enter")                                                    \
  V(errno_string, "errno")                                                    \
  V(exit_string, "exit")                                                      \
  V(exponent_string, "exponent")                                              \
  V(exports_string, "exports")                                                \
  V(ext_key_usage_string, "ext_key_usage")                                    \
  V(family_string, "family")                                                  \
  V(fatal_exception_string, "_fatalException")                                \
  V(fingerprint_string, "fingerprint")                                        \
  V(gid_string, "gid")                                                        \
  V(handle_string, "handle")                                                  \
  V(headers_string, "headers")                                                \
  V(heap_total_string, "heapTotal")                                           \
  V(heap_used_string, "heapUsed")                                             \
  V(immediate_callback_string, "_immediateCallback")                          \
  V(ino_string, "ino")                                                        \
  V(ipv4_string, "IPv4")                                                      \
  V(ipv6_string, "IPv6")                                                      \
  V(issuer_string, "issuer")                                                  \
  V(method_string, "method")                                                  \
  V(mode_string, "mode")                                                      \
  V(modulus_string, "modulus")                                                \
  V(mtime_string, "mtime")                                                    \
  V(name_string, "name")                                                      \
  V(nlink_string, "nlink")                                                    \
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
  V(onread_string, "onread")                                                  \
  V(onselect_string, "onselect")                                              \
  V(onsignal_string, "onsignal")                                              \
  V(onstop_string, "onstop")                                                  \
  V(path_string, "path")                                                      \
  V(port_string, "port")                                                      \
  V(rdev_string, "rdev")                                                      \
  V(rename_string, "rename")                                                  \
  V(rss_string, "rss")                                                        \
  V(serial_number_string, "serialNumber")                                     \
  V(servername_string, "servername")                                          \
  V(session_id_string, "sessionId")                                           \
  V(should_keep_alive_string, "shouldKeepAlive")                              \
  V(size_string, "size")                                                      \
  V(smalloc_p_string, "_smalloc_p")                                           \
  V(sni_context_string, "sni_context")                                        \
  V(status_code_string, "statusCode")                                         \
  V(status_message_string, "statusMessage")                                   \
  V(subject_string, "subject")                                                \
  V(subjectaltname_string, "subjectaltname")                                  \
  V(syscall_string, "syscall")                                                \
  V(tls_ticket_string, "tlsTicket")                                           \
  V(uid_string, "uid")                                                        \
  V(upgrade_string, "upgrade")                                                \
  V(url_string, "url")                                                        \
  V(valid_from_string, "valid_from")                                          \
  V(valid_to_string, "valid_to")                                              \
  V(version_major_string, "versionMajor")                                     \
  V(version_minor_string, "versionMinor")                                     \
  V(version_string, "version")                                                \
  V(write_queue_size_string, "writeQueueSize")                                \

#define ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)                           \
  V(async_listener_load_function, v8::Function)                               \
  V(async_listener_push_function, v8::Function)                               \
  V(async_listener_run_function, v8::Function)                                \
  V(async_listener_strip_function, v8::Function)                              \
  V(async_listener_unload_function, v8::Function)                             \
  V(binding_cache_object, v8::Object)                                         \
  V(buffer_constructor_function, v8::Function)                                \
  V(context, v8::Context)                                                     \
  V(module_load_list_array, v8::Array)                                        \
  V(pipe_constructor_template, v8::FunctionTemplate)                          \
  V(process_object, v8::Object)                                               \
  V(script_context_constructor_template, v8::FunctionTemplate)                \
  V(script_data_constructor_function, v8::Function)                           \
  V(secure_context_constructor_template, v8::FunctionTemplate)                \
  V(stats_constructor_function, v8::Function)                                 \
  V(tcp_constructor_template, v8::FunctionTemplate)                           \
  V(tick_callback_function, v8::Function)                                     \
  V(tls_wrap_constructor_function, v8::Function)                              \
  V(tty_constructor_template, v8::FunctionTemplate)                           \
  V(udp_constructor_function, v8::Function)                                   \

class Environment;

// TODO(bnoordhuis) Rename struct, the ares_ prefix implies it's part
// of the c-ares API while the _t suffix implies it's a typedef.
struct ares_task_t {
  Environment* env;
  ares_socket_t sock;
  uv_poll_t poll_watcher;
  RB_ENTRY(ares_task_t) node;
};

RB_HEAD(ares_task_list, ares_task_t);

class Environment {
 public:
  class AsyncListener {
   public:
    inline uint32_t* fields();
    inline int fields_count() const;
    inline uint32_t count() const;

   private:
    friend class Environment;  // So we can call the constructor.
    inline AsyncListener();

    enum Fields {
      kCount,
      kFieldsCount
    };

    uint32_t fields_[kFieldsCount];

    DISALLOW_COPY_AND_ASSIGN(AsyncListener);
  };

  class TickInfo {
   public:
    inline uint32_t* fields();
    inline int fields_count() const;
    inline bool in_tick() const;
    inline bool last_threw() const;
    inline uint32_t index() const;
    inline uint32_t length() const;
    inline void set_in_tick(bool value);
    inline void set_index(uint32_t value);
    inline void set_last_threw(bool value);

   private:
    friend class Environment;  // So we can call the constructor.
    inline TickInfo();

    enum Fields {
      kIndex,
      kLength,
      kFieldsCount
    };

    uint32_t fields_[kFieldsCount];
    bool in_tick_;
    bool last_threw_;

    DISALLOW_COPY_AND_ASSIGN(TickInfo);
  };

  static inline Environment* GetCurrent(v8::Isolate* isolate);
  static inline Environment* GetCurrent(v8::Local<v8::Context> context);
  static inline Environment* GetCurrentChecked(v8::Isolate* isolate);
  static inline Environment* GetCurrentChecked(v8::Local<v8::Context> context);

  // See CreateEnvironment() in src/node.cc.
  static inline Environment* New(v8::Local<v8::Context> context);
  inline void Dispose();

  inline v8::Isolate* isolate() const;
  inline uv_loop_t* event_loop() const;
  inline bool has_async_listeners() const;

  static inline Environment* from_immediate_check_handle(uv_check_t* handle);
  inline uv_check_t* immediate_check_handle();
  inline uv_idle_t* immediate_idle_handle();

  static inline Environment* from_idle_prepare_handle(uv_prepare_t* handle);
  inline uv_prepare_t* idle_prepare_handle();

  static inline Environment* from_idle_check_handle(uv_check_t* handle);
  inline uv_check_t* idle_check_handle();

  inline AsyncListener* async_listener();
  inline TickInfo* tick_info();

  static inline Environment* from_cares_timer_handle(uv_timer_t* handle);
  inline uv_timer_t* cares_timer_handle();
  inline ares_channel cares_channel();
  inline ares_channel* cares_channel_ptr();
  inline ares_task_list* cares_task_list();

  inline bool using_smalloc_alloc_cb() const;
  inline void set_using_smalloc_alloc_cb(bool value);

  // Strings are shared across shared contexts. The getters simply proxy to
  // the per-isolate primitive.
#define V(PropertyName, StringValue)                                          \
  inline v8::Local<v8::String> PropertyName() const;
  PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

#define V(PropertyName, TypeName)                                             \
  inline v8::Local<TypeName> PropertyName() const;                            \
  inline void set_ ## PropertyName(v8::Local<TypeName> value);
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

 private:
  class IsolateData;
  inline explicit Environment(v8::Local<v8::Context> context);
  inline ~Environment();
  inline IsolateData* isolate_data() const;

  enum ContextEmbedderDataIndex {
    kContextEmbedderDataIndex = NODE_CONTEXT_EMBEDDER_DATA_INDEX
  };

  v8::Isolate* const isolate_;
  IsolateData* const isolate_data_;
  uv_check_t immediate_check_handle_;
  uv_idle_t immediate_idle_handle_;
  uv_prepare_t idle_prepare_handle_;
  uv_check_t idle_check_handle_;
  AsyncListener async_listener_count_;
  TickInfo tick_info_;
  uv_timer_t cares_timer_handle_;
  ares_channel cares_channel_;
  ares_task_list cares_task_list_;
  bool using_smalloc_alloc_cb_;

#define V(PropertyName, TypeName)                                             \
  v8::Persistent<TypeName> PropertyName ## _;
  ENVIRONMENT_STRONG_PERSISTENT_PROPERTIES(V)
#undef V

  // Per-thread, reference-counted singleton.
  class IsolateData {
   public:
    static inline IsolateData* GetOrCreate(v8::Isolate* isolate);
    inline void Put();
    inline uv_loop_t* event_loop() const;

#define V(PropertyName, StringValue)                                          \
    inline v8::Local<v8::String> PropertyName() const;
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

   private:
    inline explicit IsolateData(v8::Isolate* isolate);
    inline v8::Isolate* isolate() const;

    uv_loop_t* const event_loop_;
    v8::Isolate* const isolate_;

#define V(PropertyName, StringValue)                                          \
    v8::Eternal<v8::String> PropertyName ## _;
    PER_ISOLATE_STRING_PROPERTIES(V)
#undef V

    unsigned int ref_count_;

    DISALLOW_COPY_AND_ASSIGN(IsolateData);
  };

  DISALLOW_COPY_AND_ASSIGN(Environment);
};

}  // namespace node

#endif  // SRC_ENV_H_
