#include <js_native_api.h>

// Empty value so that macros here are able to return NULL or void
#define NODE_API_RETVAL_NOTHING  // Intentionally blank #define

#define GET_AND_THROW_LAST_ERROR(env)                                    \
  do {                                                                   \
    const node_api_extended_error_info *error_info;                      \
    node_api_get_last_error_info((env), &error_info);                    \
    bool is_pending;                                                     \
    node_api_is_exception_pending((env), &is_pending);                   \
    /* If an exception is already pending, don't rethrow it */           \
    if (!is_pending) {                                                   \
      const char* error_message = error_info->error_message != NULL ?    \
        error_info->error_message :                                      \
        "empty error message";                                           \
      node_api_throw_error((env), NULL, error_message);                  \
    }                                                                    \
  } while (0)

#define NODE_API_ASSERT_BASE(env, assertion, message, ret_val)           \
  do {                                                                   \
    if (!(assertion)) {                                                  \
      node_api_throw_error(                                              \
          (env),                                                         \
        NULL,                                                            \
          "assertion (" #assertion ") failed: " message);                \
      return ret_val;                                                    \
    }                                                                    \
  } while (0)

// Returns NULL on failed assertion.
// This is meant to be used inside node_api_callback methods.
#define NODE_API_ASSERT(env, assertion, message)                         \
  NODE_API_ASSERT_BASE(env, assertion, message, NULL)

// Returns empty on failed assertion.
// This is meant to be used inside functions with void return type.
#define NODE_API_ASSERT_RETURN_VOID(env, assertion, message)             \
  NODE_API_ASSERT_BASE(env, assertion, message, NODE_API_RETVAL_NOTHING)

#define NODE_API_CALL_BASE(env, the_call, ret_val)                       \
  do {                                                                   \
    if ((the_call) != node_api_ok) {                                     \
      GET_AND_THROW_LAST_ERROR((env));                                   \
      return ret_val;                                                    \
    }                                                                    \
  } while (0)

// Returns NULL if the_call doesn't return node_api_ok.
#define NODE_API_CALL(env, the_call)                                     \
  NODE_API_CALL_BASE(env, the_call, NULL)

// Returns empty if the_call doesn't return node_api_ok.
#define NODE_API_CALL_RETURN_VOID(env, the_call)                         \
  NODE_API_CALL_BASE(env, the_call, NODE_API_RETVAL_NOTHING)

#define DECLARE_NODE_API_PROPERTY(name, func)                            \
  { (name), NULL, (func), NULL, NULL, NULL, node_api_default, NULL }

#define DECLARE_NODE_API_GETTER(name, func)                              \
  { (name), NULL, NULL, (func), NULL, NULL, node_api_default, NULL }

void add_returned_status(node_api_env env,
                         const char* key,
                         node_api_value object,
                         char* expected_message,
                         node_api_status expected_status,
                         node_api_status actual_status);

void add_last_status(node_api_env env,
                     const char* key,
                     node_api_value return_value);
