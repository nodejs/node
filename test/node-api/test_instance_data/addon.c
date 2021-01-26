#include <stdio.h>
#include <stdlib.h>
#include <node_api.h>

static void addon_free(node_api_env env, void* data, void* hint) {
  node_api_ref* ref = data;
  node_api_delete_reference(env, *ref);
  free(ref);
  fprintf(stderr, "addon_free");
}

node_api_value addon_new(node_api_env env, node_api_value exports, bool ref_first) {
  node_api_ref* ref = malloc(sizeof(*ref));
  if (ref_first) {
    node_api_create_reference(env, exports, 1, ref);
    node_api_set_instance_data(env, ref, addon_free, NULL);
  } else {
    node_api_set_instance_data(env, ref, addon_free, NULL);
    node_api_create_reference(env, exports, 1, ref);
  }
  return exports;
}
