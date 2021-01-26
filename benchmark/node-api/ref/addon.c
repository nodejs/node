#include <stdlib.h>
#define NODE_API_EXPERIMENTAL
#include <node_api.h>

#define NODE_API_CALL(env, call)                          \
  do {                                                \
    node_api_status status = (call);                      \
    if (status != node_api_ok) {                          \
      node_api_throw_error((env), NULL, #call " failed"); \
      return NULL;                                    \
    }                                                 \
  } while (0)

static node_api_value
GetCount(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  size_t* count;

  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&count));
  NODE_API_CALL(env, node_api_create_uint32(env, *count, &result));

  return result;
}

static node_api_value
SetCount(node_api_env env, node_api_callback_info info) {
  size_t* count;

  NODE_API_CALL(env, node_api_get_instance_data(env, (void**)&count));

  // Set the count to zero irrespective of what is passed into the setter.
  *count = 0;

  return NULL;
}

static void
IncrementCounter(node_api_env env, void* data, void* hint) {
  size_t* count = data;
  (*count) = (*count) + 1;
}

static node_api_value
NewWeak(node_api_env env, node_api_callback_info info) {
  node_api_value result;
  void* instance_data;

  NODE_API_CALL(env, node_api_create_object(env, &result));
  NODE_API_CALL(env, node_api_get_instance_data(env, &instance_data));
  NODE_API_CALL(env, node_api_add_finalizer(env,
                                            result,
                                            instance_data,
                                            IncrementCounter,
                                            NULL,
                                            NULL));

  return result;
}

static void
FreeCount(node_api_env env, void* data, void* hint) {
  free(data);
}

/* node_api_value */
NODE_API_MODULE_INIT(/* node_api_env env, node_api_value exports */) {
  node_api_property_descriptor props[] = {
    { "count", NULL, NULL, GetCount, SetCount, NULL, node_api_enumerable,
        NULL },
    { "newWeak", NULL, NewWeak, NULL, NULL, NULL, node_api_enumerable, NULL }
  };

  size_t* count = malloc(sizeof(*count));
  *count = 0;

  NODE_API_CALL(env, node_api_define_properties(env,
                                                exports,
                                                sizeof(props) / sizeof(*props),
                                                props));
  NODE_API_CALL(env, node_api_set_instance_data(env, count, FreeCount, NULL));

  return exports;
}
