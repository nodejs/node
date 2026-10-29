#include <stdio.h>
#include <stdlib.h>
#include <node_api.h>

static void addon_free(napi_env env, void* data, void* hint) {
  napi_ref* ref = data;
  napi_delete_reference(env, *ref);
  free(ref);
  fprintf(stderr, "addon_free");
}

napi_value addon_new(napi_env env, napi_value exports, bool ref_first) {
  napi_ref* ref = malloc(sizeof(*ref));
  if (ref_first) {
    napi_create_reference(env, exports, 1, ref);
    napi_set_instance_data(env, ref, addon_free, NULL);
  } else {
    napi_set_instance_data(env, ref, addon_free, NULL);
    napi_create_reference(env, exports, 1, ref);
  }
  return exports;
}
