#ifndef SRC_NODE_API_TYPES_H_
#define SRC_NODE_API_TYPES_H_

#include "js_native_api_types.h"

typedef napi_value(NAPI_CDECL* napi_addon_register_func)(napi_env env,
                                                         napi_value exports);
// False positive: https://github.com/cpplint/cpplint/issues/409
// NOLINTNEXTLINE (readability/casting)
typedef int32_t(NAPI_CDECL* node_api_addon_get_api_version_func)(void);

typedef struct napi_callback_scope__* napi_callback_scope;
typedef struct napi_async_context__* napi_async_context;
typedef struct napi_async_work__* napi_async_work;

#if NAPI_VERSION >= 3
typedef void(NAPI_CDECL* napi_cleanup_hook)(void* arg);
#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4
typedef struct napi_threadsafe_function__* napi_threadsafe_function;
#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 4
typedef enum {
  napi_tsfn_release,
  napi_tsfn_abort
} napi_threadsafe_function_release_mode;

typedef enum {
  napi_tsfn_nonblocking,
  napi_tsfn_blocking
} napi_threadsafe_function_call_mode;
#endif  // NAPI_VERSION >= 4

typedef void(NAPI_CDECL* napi_async_execute_callback)(napi_env env, void* data);
typedef void(NAPI_CDECL* napi_async_complete_callback)(napi_env env,
                                                       napi_status status,
                                                       void* data);
#if NAPI_VERSION >= 4
typedef void(NAPI_CDECL* napi_threadsafe_function_call_js)(
    napi_env env, napi_value js_callback, void* context, void* data);
#endif  // NAPI_VERSION >= 4

typedef struct {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  const char* release;
} napi_node_version;

#if NAPI_VERSION >= 8
typedef struct napi_async_cleanup_hook_handle__* napi_async_cleanup_hook_handle;
typedef void(NAPI_CDECL* napi_async_cleanup_hook)(
    napi_async_cleanup_hook_handle handle, void* data);
#endif  // NAPI_VERSION >= 8

// Used by deprecated registration method napi_module_register.
typedef struct napi_module {
  int nm_version;
  unsigned int nm_flags;
  const char* nm_filename;
  napi_addon_register_func nm_register_func;
  const char* nm_modname;
  void* nm_priv;
  void* reserved[4];
} napi_module;

typedef struct {
  napi_status(NAPI_CDECL* get_last_error_info)(
      node_api_basic_env env, const napi_extended_error_info** result);

  napi_status(NAPI_CDECL* get_undefined)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_null)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_global)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* get_boolean)(napi_env env,
                                       bool value,
                                       napi_value* result);

  napi_status(NAPI_CDECL* create_object)(napi_env env, napi_value* result);

  napi_status(NAPI_CDECL* create_array)(napi_env env, napi_value* result);
  napi_status(NAPI_CDECL* create_array_with_length)(napi_env env,
                                                    size_t length,
                                                    napi_value* result);
  napi_status(NAPI_CDECL* create_double)(napi_env env,
                                         double value,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_int32)(napi_env env,
                                        int32_t value,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_uint32)(napi_env env,
                                         uint32_t value,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_int64)(napi_env env,
                                        int64_t value,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_string_latin1)(napi_env env,
                                                const char* str,
                                                size_t length,
                                                napi_value* result);
  napi_status(NAPI_CDECL* create_string_utf8)(napi_env env,
                                              const char* str,
                                              size_t length,
                                              napi_value* result);
  napi_status(NAPI_CDECL* create_string_utf16)(napi_env env,
                                               const char16_t* str,
                                               size_t length,
                                               napi_value* result);

  napi_status(NAPI_CDECL* create_symbol)(napi_env env,
                                         napi_value description,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_function)(napi_env env,
                                           const char* utf8name,
                                           size_t length,
                                           napi_callback cb,
                                           void* data,
                                           napi_value* result);
  napi_status(NAPI_CDECL* create_error)(napi_env env,
                                        napi_value code,
                                        napi_value msg,
                                        napi_value* result);
  napi_status(NAPI_CDECL* create_type_error)(napi_env env,
                                             napi_value code,
                                             napi_value msg,
                                             napi_value* result);
  napi_status(NAPI_CDECL* create_range_error)(napi_env env,
                                              napi_value code,
                                              napi_value msg,
                                              napi_value* result);

  napi_status(NAPI_CDECL* typeof)(napi_env env,
                                  napi_value value,
                                  napi_valuetype* result);
  napi_status(NAPI_CDECL* get_value_double)(napi_env env,
                                            napi_value value,
                                            double* result);
  napi_status(NAPI_CDECL* get_value_int32)(napi_env env,
                                           napi_value value,
                                           int32_t* result);
  napi_status(NAPI_CDECL* get_value_uint32)(napi_env env,
                                            napi_value value,
                                            uint32_t* result);
  napi_status(NAPI_CDECL* get_value_int64)(napi_env env,
                                           napi_value value,
                                           int64_t* result);
  napi_status(NAPI_CDECL* get_value_bool)(napi_env env,
                                          napi_value value,
                                          bool* result);

  napi_status(NAPI_CDECL* get_value_string_latin1)(napi_env env,
                                                   napi_value value,
                                                   char* buf,
                                                   size_t bufsize,
                                                   size_t* result);

  napi_status(NAPI_CDECL* get_value_string_utf8)(napi_env env,
                                                 napi_value value,
                                                 char* buf,
                                                 size_t bufsize,
                                                 size_t* result);

  napi_status(NAPI_CDECL* get_value_string_utf16)(napi_env env,
                                                  napi_value value,
                                                  char16_t* buf,
                                                  size_t bufsize,
                                                  size_t* result);

  napi_status(NAPI_CDECL* coerce_to_bool)(napi_env env,
                                          napi_value value,
                                          napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_number)(napi_env env,
                                            napi_value value,
                                            napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_object)(napi_env env,
                                            napi_value value,
                                            napi_value* result);
  napi_status(NAPI_CDECL* coerce_to_string)(napi_env env,
                                            napi_value value,
                                            napi_value* result);

  napi_status(NAPI_CDECL* get_prototype)(napi_env env,
                                         napi_value object,
                                         napi_value* result);
  napi_status(NAPI_CDECL* get_property_names)(napi_env env,
                                              napi_value object,
                                              napi_value* result);
  napi_status(NAPI_CDECL* set_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        napi_value value);
  napi_status(NAPI_CDECL* has_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        bool* result);
  napi_status(NAPI_CDECL* get_property)(napi_env env,
                                        napi_value object,
                                        napi_value key,
                                        napi_value* result);
  napi_status(NAPI_CDECL* delete_property)(napi_env env,
                                           napi_value object,
                                           napi_value key,
                                           bool* result);
  napi_status(NAPI_CDECL* has_own_property)(napi_env env,
                                            napi_value object,
                                            napi_value key,
                                            bool* result);
  napi_status(NAPI_CDECL* set_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              napi_value value);
  napi_status(NAPI_CDECL* has_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              bool* result);
  napi_status(NAPI_CDECL* get_named_property)(napi_env env,
                                              napi_value object,
                                              const char* utf8name,
                                              napi_value* result);
  napi_status(NAPI_CDECL* set_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       napi_value value);
  napi_status(NAPI_CDECL* has_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       bool* result);
  napi_status(NAPI_CDECL* get_element)(napi_env env,
                                       napi_value object,
                                       uint32_t index,
                                       napi_value* result);
  napi_status(NAPI_CDECL* delete_element)(napi_env env,
                                          napi_value object,
                                          uint32_t index,
                                          bool* result);
  napi_status(NAPI_CDECL* define_properties)(
      napi_env env,
      napi_value object,
      size_t property_count,
      const napi_property_descriptor* properties);

  napi_status(NAPI_CDECL* is_array)(napi_env env,
                                    napi_value value,
                                    bool* result);
  napi_status(NAPI_CDECL* get_array_length)(napi_env env,
                                            napi_value value,
                                            uint32_t* result);

  napi_status(NAPI_CDECL* strict_equals)(napi_env env,
                                         napi_value lhs,
                                         napi_value rhs,
                                         bool* result);

  napi_status(NAPI_CDECL* call_function)(napi_env env,
                                         napi_value recv,
                                         napi_value func,
                                         size_t argc,
                                         const napi_value* argv,
                                         napi_value* result);
  napi_status(NAPI_CDECL* new_instance)(napi_env env,
                                        napi_value constructor,
                                        size_t argc,
                                        const napi_value* argv,
                                        napi_value* result);
  napi_status(NAPI_CDECL* instanceof)(napi_env env,
                                      napi_value object,
                                      napi_value constructor,
                                      bool* result);

  napi_status(NAPI_CDECL* get_cb_info)(napi_env env,
                                       napi_callback_info cbinfo,
                                       size_t* argc,
                                       napi_value* argv,
                                       napi_value* this_arg,
                                       void** data);

  napi_status(NAPI_CDECL* get_new_target)(napi_env env,
                                          napi_callback_info cbinfo,
                                          napi_value* result);
  napi_status(NAPI_CDECL* define_class)(
      napi_env env,
      const char* utf8name,
      size_t length,
      napi_callback constructor,
      void* data,
      size_t property_count,
      const napi_property_descriptor* properties,
      napi_value* result);

  napi_status(NAPI_CDECL* wrap)(napi_env env,
                                napi_value js_object,
                                void* native_object,
                                node_api_basic_finalize finalize_cb,
                                void* finalize_hint,
                                napi_ref* result);
  napi_status(NAPI_CDECL* unwrap)(napi_env env,
                                  napi_value js_object,
                                  void** result);
  napi_status(NAPI_CDECL* remove_wrap)(napi_env env,
                                       napi_value js_object,
                                       void** result);
  napi_status(NAPI_CDECL* create_external)(napi_env env,
                                           void* data,
                                           node_api_basic_finalize finalize_cb,
                                           void* finalize_hint,
                                           napi_value* result);
  napi_status(NAPI_CDECL* get_value_external)(napi_env env,
                                              napi_value value,
                                              void** result);

  napi_status(NAPI_CDECL* create_reference)(napi_env env,
                                            napi_value value,
                                            uint32_t initial_refcount,
                                            napi_ref* result);

  napi_status(NAPI_CDECL* delete_reference)(node_api_basic_env env,
                                            napi_ref ref);

  napi_status(NAPI_CDECL* reference_ref)(napi_env env,
                                         napi_ref ref,
                                         uint32_t* result);

  napi_status(NAPI_CDECL* reference_unref)(napi_env env,
                                           napi_ref ref,
                                           uint32_t* result);

  napi_status(NAPI_CDECL* get_reference_value)(napi_env env,
                                               napi_ref ref,
                                               napi_value* result);

  napi_status(NAPI_CDECL* open_handle_scope)(napi_env env,
                                             napi_handle_scope* result);
  napi_status(NAPI_CDECL* close_handle_scope)(napi_env env,
                                              napi_handle_scope scope);
  napi_status(NAPI_CDECL* open_escapable_handle_scope)(
      napi_env env, napi_escapable_handle_scope* result);
  napi_status(NAPI_CDECL* close_escapable_handle_scope)(
      napi_env env, napi_escapable_handle_scope scope);

  napi_status(NAPI_CDECL* escape_handle)(napi_env env,
                                         napi_escapable_handle_scope scope,
                                         napi_value escapee,
                                         napi_value* result);

  napi_status(NAPI_CDECL* throw_error)(napi_env env, napi_value error);
  napi_status(NAPI_CDECL* throw_js_error)(napi_env env,
                                          const char* code,
                                          const char* msg);
  napi_status(NAPI_CDECL* throw_type_error)(napi_env env,
                                            const char* code,
                                            const char* msg);
  napi_status(NAPI_CDECL* throw_range_error)(napi_env env,
                                             const char* code,
                                             const char* msg);
  napi_status(NAPI_CDECL* is_error)(napi_env env,
                                    napi_value value,
                                    bool* result);

  napi_status(NAPI_CDECL* is_exception_pending)(napi_env env, bool* result);
  napi_status(NAPI_CDECL* get_and_clear_last_exception)(napi_env env,
                                                        napi_value* result);

  napi_status(NAPI_CDECL* is_arraybuffer)(napi_env env,
                                          napi_value value,
                                          bool* result);
  napi_status(NAPI_CDECL* create_arraybuffer)(napi_env env,
                                              size_t byte_length,
                                              void** data,
                                              napi_value* result);
  napi_status(NAPI_CDECL* create_external_arraybuffer)(
      napi_env env,
      void* external_data,
      size_t byte_length,
      node_api_basic_finalize finalize_cb,
      void* finalize_hint,
      napi_value* result);
  napi_status(NAPI_CDECL* get_arraybuffer_info)(napi_env env,
                                                napi_value arraybuffer,
                                                void** data,
                                                size_t* byte_length);
  napi_status(NAPI_CDECL* is_typedarray)(napi_env env,
                                         napi_value value,
                                         bool* result);
  napi_status(NAPI_CDECL* create_typedarray)(napi_env env,
                                             napi_typedarray_type type,
                                             size_t length,
                                             napi_value arraybuffer,
                                             size_t byte_offset,
                                             napi_value* result);
  napi_status(NAPI_CDECL* get_typedarray_info)(napi_env env,
                                               napi_value typedarray,
                                               napi_typedarray_type* type,
                                               size_t* length,
                                               void** data,
                                               napi_value* arraybuffer,
                                               size_t* byte_offset);

  napi_status(NAPI_CDECL* create_dataview)(napi_env env,
                                           size_t length,
                                           napi_value arraybuffer,
                                           size_t byte_offset,
                                           napi_value* result);
  napi_status(NAPI_CDECL* is_dataview)(napi_env env,
                                       napi_value value,
                                       bool* result);
  napi_status(NAPI_CDECL* get_dataview_info)(napi_env env,
                                             napi_value dataview,
                                             size_t* bytelength,
                                             void** data,
                                             napi_value* arraybuffer,
                                             size_t* byte_offset);

  napi_status(NAPI_CDECL* get_version)(node_api_basic_env env,
                                       uint32_t* result);

  napi_status(NAPI_CDECL* create_promise)(napi_env env,
                                          napi_deferred* deferred,
                                          napi_value* promise);
  napi_status(NAPI_CDECL* resolve_deferred)(napi_env env,
                                            napi_deferred deferred,
                                            napi_value resolution);
  napi_status(NAPI_CDECL* reject_deferred)(napi_env env,
                                           napi_deferred deferred,
                                           napi_value rejection);
  napi_status(NAPI_CDECL* is_promise)(napi_env env,
                                      napi_value value,
                                      bool* is_promise);

  napi_status(NAPI_CDECL* run_script)(napi_env env,
                                      napi_value script,
                                      napi_value* result);

  napi_status(NAPI_CDECL* adjust_external_memory)(node_api_basic_env env,
                                                  int64_t change_in_bytes,
                                                  int64_t* adjusted_value);

  void(NAPI_CDECL* module_register)(napi_module* mod);

  void(NAPI_CDECL* fatal_error)(const char* location,
                                size_t location_len,
                                const char* message,
                                size_t message_len);

  napi_status(NAPI_CDECL* async_init)(napi_env env,
                                      napi_value async_resource,
                                      napi_value async_resource_name,
                                      napi_async_context* result);

  napi_status(NAPI_CDECL* async_destroy)(napi_env env,
                                         napi_async_context async_context);

  napi_status(NAPI_CDECL* make_callback)(napi_env env,
                                         napi_async_context async_context,
                                         napi_value recv,
                                         napi_value func,
                                         size_t argc,
                                         const napi_value* argv,
                                         napi_value* result);

  napi_status(NAPI_CDECL* create_buffer)(napi_env env,
                                         size_t length,
                                         void** data,
                                         napi_value* result);
  napi_status(NAPI_CDECL* create_external_buffer)(
      napi_env env,
      size_t length,
      void* data,
      node_api_basic_finalize finalize_cb,
      void* finalize_hint,
      napi_value* result);

  napi_status(NAPI_CDECL* create_buffer_copy)(napi_env env,
                                              size_t length,
                                              const void* data,
                                              void** result_data,
                                              napi_value* result);
  napi_status(NAPI_CDECL* is_buffer)(napi_env env,
                                     napi_value value,
                                     bool* result);
  napi_status(NAPI_CDECL* get_buffer_info)(napi_env env,
                                           napi_value value,
                                           void** data,
                                           size_t* length);

  napi_status(NAPI_CDECL* create_async_work)(
      napi_env env,
      napi_value async_resource,
      napi_value async_resource_name,
      napi_async_execute_callback execute,
      napi_async_complete_callback complete,
      void* data,
      napi_async_work* result);
  napi_status(NAPI_CDECL* delete_async_work)(napi_env env,
                                             napi_async_work work);
  napi_status(NAPI_CDECL* queue_async_work)(node_api_basic_env env,
                                            napi_async_work work);
  napi_status(NAPI_CDECL* cancel_async_work)(node_api_basic_env env,
                                             napi_async_work work);

  napi_status(NAPI_CDECL* get_node_version)(node_api_basic_env env,
                                            const napi_node_version** version);

#if NAPI_VERSION >= 2

  napi_status(NAPI_CDECL* get_uv_event_loop)(node_api_basic_env env,
                                             struct uv_loop_s** loop);

#endif  // NAPI_VERSION >= 2

#if NAPI_VERSION >= 3

  napi_status(NAPI_CDECL* fatal_exception)(napi_env env, napi_value err);

  napi_status(NAPI_CDECL* add_env_cleanup_hook)(node_api_basic_env env,
                                                napi_cleanup_hook fun,
                                                void* arg);

  napi_status(NAPI_CDECL* remove_env_cleanup_hook)(node_api_basic_env env,
                                                   napi_cleanup_hook fun,
                                                   void* arg);

  napi_status(NAPI_CDECL* open_callback_scope)(napi_env env,
                                               napi_value resource_object,
                                               napi_async_context context,
                                               napi_callback_scope* result);

  napi_status(NAPI_CDECL* close_callback_scope)(napi_env env,
                                                napi_callback_scope scope);

#endif  // NAPI_VERSION >= 3

#if NAPI_VERSION >= 4

  napi_status(NAPI_CDECL* create_threadsafe_function)(
      napi_env env,
      napi_value func,
      napi_value async_resource,
      napi_value async_resource_name,
      size_t max_queue_size,
      size_t initial_thread_count,
      void* thread_finalize_data,
      napi_finalize thread_finalize_cb,
      void* context,
      napi_threadsafe_function_call_js call_js_cb,
      napi_threadsafe_function* result);

  napi_status(NAPI_CDECL* get_threadsafe_function_context)(
      napi_threadsafe_function func, void** result);

  napi_status(NAPI_CDECL* call_threadsafe_function)(
      napi_threadsafe_function func,
      void* data,
      napi_threadsafe_function_call_mode is_blocking);

  napi_status(NAPI_CDECL* acquire_threadsafe_function)(
      napi_threadsafe_function func);

  napi_status(NAPI_CDECL* release_threadsafe_function)(
      napi_threadsafe_function func,
      napi_threadsafe_function_release_mode mode);

  napi_status(NAPI_CDECL* unref_threadsafe_function)(
      node_api_basic_env env, napi_threadsafe_function func);

  napi_status(NAPI_CDECL* ref_threadsafe_function)(
      node_api_basic_env env, napi_threadsafe_function func);

#endif  // NAPI_VERSION >= 4

#if NAPI_VERSION >= 5

  napi_status(NAPI_CDECL* create_date)(napi_env env,
                                       double time,
                                       napi_value* result);

  napi_status(NAPI_CDECL* is_date)(napi_env env,
                                   napi_value value,
                                   bool* is_date);

  napi_status(NAPI_CDECL* get_date_value)(napi_env env,
                                          napi_value value,
                                          double* result);

  napi_status(NAPI_CDECL* add_finalizer)(napi_env env,
                                         napi_value js_object,
                                         void* finalize_data,
                                         node_api_basic_finalize finalize_cb,
                                         void* finalize_hint,
                                         napi_ref* result);

#endif  // NAPI_VERSION >= 5

#if NAPI_VERSION >= 6

  napi_status(NAPI_CDECL* create_bigint_int64)(napi_env env,
                                               int64_t value,
                                               napi_value* result);
  napi_status(NAPI_CDECL* create_bigint_uint64)(napi_env env,
                                                uint64_t value,
                                                napi_value* result);
  napi_status(NAPI_CDECL* create_bigint_words)(napi_env env,
                                               int sign_bit,
                                               size_t word_count,
                                               const uint64_t* words,
                                               napi_value* result);
  napi_status(NAPI_CDECL* get_value_bigint_int64)(napi_env env,
                                                  napi_value value,
                                                  int64_t* result,
                                                  bool* lossless);
  napi_status(NAPI_CDECL* get_value_bigint_uint64)(napi_env env,
                                                   napi_value value,
                                                   uint64_t* result,
                                                   bool* lossless);
  napi_status(NAPI_CDECL* get_value_bigint_words)(napi_env env,
                                                  napi_value value,
                                                  int* sign_bit,
                                                  size_t* word_count,
                                                  uint64_t* words);

  napi_status(NAPI_CDECL* get_all_property_names)(
      napi_env env,
      napi_value object,
      napi_key_collection_mode key_mode,
      napi_key_filter key_filter,
      napi_key_conversion key_conversion,
      napi_value* result);

  napi_status(NAPI_CDECL* set_instance_data)(node_api_basic_env env,
                                             void* data,
                                             napi_finalize finalize_cb,
                                             void* finalize_hint);

  napi_status(NAPI_CDECL* get_instance_data)(node_api_basic_env env,
                                             void** data);
#endif  // NAPI_VERSION >= 6

#if NAPI_VERSION >= 7
  napi_status(NAPI_CDECL* detach_arraybuffer)(napi_env env,
                                              napi_value arraybuffer);

  napi_status(NAPI_CDECL* is_detached_arraybuffer)(napi_env env,
                                                   napi_value value,
                                                   bool* result);
#endif  // NAPI_VERSION >= 7

#if NAPI_VERSION >= 8

  napi_status(NAPI_CDECL* type_tag_object)(napi_env env,
                                           napi_value value,
                                           const napi_type_tag* type_tag);

  napi_status(NAPI_CDECL* check_object_type_tag)(napi_env env,
                                                 napi_value value,
                                                 const napi_type_tag* type_tag,
                                                 bool* result);
  napi_status(NAPI_CDECL* object_freeze)(napi_env env, napi_value object);
  napi_status(NAPI_CDECL* object_seal)(napi_env env, napi_value object);

  napi_status(NAPI_CDECL* add_async_cleanup_hook)(
      node_api_basic_env env,
      napi_async_cleanup_hook hook,
      void* arg,
      napi_async_cleanup_hook_handle* remove_handle);

  napi_status(NAPI_CDECL* remove_async_cleanup_hook)(
      napi_async_cleanup_hook_handle remove_handle);

#endif  // NAPI_VERSION >= 8

#if NAPI_VERSION >= 9

  napi_status(NAPI_CDECL* symbol_for)(napi_env env,
                                      const char* utf8description,
                                      size_t length,
                                      napi_value* result);

  napi_status(NAPI_CDECL* create_syntax_error)(napi_env env,
                                               napi_value code,
                                               napi_value msg,
                                               napi_value* result);

  napi_status(NAPI_CDECL* throw_syntax_error)(napi_env env,
                                              const char* code,
                                              const char* msg);

  napi_status(NAPI_CDECL* get_module_file_name)(node_api_basic_env env,
                                                const char** result);

#endif  // NAPI_VERSION >= 9

#if NAPI_VERSION >= 10
  napi_status(NAPI_CDECL* create_external_string_latin1)(
      napi_env env,
      char* str,
      size_t length,
      node_api_basic_finalize finalize_callback,
      void* finalize_hint,
      napi_value* result,
      bool* copied);
  napi_status(NAPI_CDECL* create_external_string_utf16)(
      napi_env env,
      char16_t* str,
      size_t length,
      node_api_basic_finalize finalize_callback,
      void* finalize_hint,
      napi_value* result,
      bool* copied);

  napi_status(NAPI_CDECL* create_property_key_latin1)(napi_env env,
                                                      const char* str,
                                                      size_t length,
                                                      napi_value* result);
  napi_status(NAPI_CDECL* create_property_key_utf8)(napi_env env,
                                                    const char* str,
                                                    size_t length,
                                                    napi_value* result);
  napi_status(NAPI_CDECL* create_property_key_utf16)(napi_env env,
                                                     const char16_t* str,
                                                     size_t length,
                                                     napi_value* result);

  napi_status(NAPI_CDECL* create_buffer_from_arraybuffer)(
      napi_env env,
      napi_value arraybuffer,
      size_t byte_offset,
      size_t byte_length,
      napi_value* result);

#endif  // NAPI_VERSION >= 10

#ifdef NAPI_EXPERIMENTAL
  napi_status(NAPI_CDECL* post_finalizer)(node_api_basic_env env,
                                          napi_finalize finalize_cb,
                                          void* finalize_data,
                                          void* finalize_hint);

  napi_status(NAPI_CDECL* create_object_with_properties)(
      napi_env env,
      napi_value prototype_or_null,
      napi_value* property_names,
      napi_value* property_values,
      size_t property_count,
      napi_value* result);

  napi_status(NAPI_CDECL* is_sharedarraybuffer)(napi_env env,
                                                napi_value value,
                                                bool* result);
  napi_status(NAPI_CDECL* create_sharedarraybuffer)(napi_env env,
                                                    size_t byte_length,
                                                    void** data,
                                                    napi_value* result);

#endif  // NAPI_EXPERIMENTAL
} node_api_vtable;

typedef void(NAPI_CDECL* node_api_addon_set_vtable_func)(
    node_api_vtable* vtable);

#endif  // SRC_NODE_API_TYPES_H_
