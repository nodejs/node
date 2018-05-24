#include <stdio.h>

#include "node_api.h"
#include "../common.h"

#define NAPI_CALL_BASIC(env, call)                                              \
  do {                                                                          \
    napi_status status = call;                                                  \
    if (status != napi_ok) {                                                    \
      return status;                                                            \
    }                                                                           \
  } while(0)

#define RETURN_UNDEFINED(env)                                                   \
  do {                                                                          \
    napi_value undefined;                                                       \
    NAPI_CALL((env), napi_get_undefined((env), &undefined));                    \
    return undefined;                                                           \
  } while(0)

static napi_status
napi_inherit(napi_env env, napi_value subclass, napi_value superclass) {
  napi_value global, object_class, set_proto;
  napi_value argv[2];

  NAPI_CALL_BASIC(env, napi_get_global(env, &global));
  NAPI_CALL_BASIC(env,
      napi_get_named_property(env, global, "Object", &object_class));
  NAPI_CALL_BASIC(env,
      napi_get_named_property(env, object_class, "setPrototypeOf", &set_proto));

  NAPI_CALL_BASIC(env,
      napi_get_named_property(env, subclass, "prototype", &argv[0]));
  NAPI_CALL_BASIC(env,
      napi_get_named_property(env, superclass, "prototype", &argv[1]));
  NAPI_CALL_BASIC(env,
      napi_call_function(env, object_class, set_proto, 2, argv, NULL));

  argv[0] = subclass;
  argv[1] = superclass;
  NAPI_CALL_BASIC(env,
      napi_call_function(env, object_class, set_proto, 2, argv, NULL));

  return napi_ok;
}

static napi_value SuperclassConstructor(napi_env env, napi_callback_info info) {
  fprintf(stderr, "SuperclassConstructor\n");
  RETURN_UNDEFINED(env);
}

static napi_value SuperclassMethod(napi_env env, napi_callback_info info) {
  fprintf(stderr, "SuperclassMethod\n");
  RETURN_UNDEFINED(env);
}

static napi_value SubclassConstructor(napi_env env, napi_callback_info info) {
  SuperclassConstructor(env, info);
  fprintf(stderr, "SubclassConstructor\n");
  RETURN_UNDEFINED(env);
}

static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor superclass_descriptors[] = {
    DECLARE_NAPI_PROPERTY("superclassMethod", SuperclassMethod),
  };
  napi_value superclass, subclass;

  NAPI_CALL(env, napi_define_class(env, "Superclass", NAPI_AUTO_LENGTH,
      SuperclassConstructor, NULL,
      sizeof(superclass_descriptors) / sizeof(*superclass_descriptors),
      superclass_descriptors, &superclass));

  NAPI_CALL(env, napi_define_class(env, "Subclass", NAPI_AUTO_LENGTH,
    SubclassConstructor, NULL, 0, NULL, &subclass));

  NAPI_CALL(env, napi_inherit(env, subclass, superclass));

  NAPI_CALL(env, napi_set_named_property(env, exports, "Subclass", subclass));

  return exports;
}

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
