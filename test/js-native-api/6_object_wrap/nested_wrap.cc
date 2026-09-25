#include "nested_wrap.h"
#include "../common.h"
#include "../entry_point.h"

napi_ref NestedWrap::constructor{};
static int finalization_count = 0;

NestedWrap::NestedWrap() {}

NestedWrap::~NestedWrap() {
  napi_delete_reference(env_, wrapper_);

  // Delete the nested reference as well.
  napi_delete_reference(env_, nested_);
}

void NestedWrap::Destructor(node_api_basic_env env,
                            void* nativeObject,
                            void* /*finalize_hint*/) {
  // Once this destructor is called, it cancels all pending
  // finalizers for the object by deleting the references.
  NestedWrap* obj = static_cast<NestedWrap*>(nativeObject);
  delete obj;

  finalization_count++;
}

void NestedWrap::Init(napi_env env, napi_value exports) {
  napi_value cons;
  NODE_API_CALL_RETURN_VOID(
      env,
      napi_define_class(
          env, "NestedWrap", -1, New, nullptr, 0, nullptr, &cons));

  NODE_API_CALL_RETURN_VOID(env,
                            napi_create_reference(env, cons, 1, &constructor));

  NODE_API_CALL_RETURN_VOID(
      env, napi_set_named_property(env, exports, "NestedWrap", cons));
}

napi_value NestedWrap::New(napi_env env, napi_callback_info info) {
  napi_value new_target;
  NODE_API_CALL(env, napi_get_new_target(env, info, &new_target));
  bool is_constructor = (new_target != nullptr);
  NODE_API_BASIC_ASSERT_BASE(
      is_constructor, "Constructor called without new", nullptr);

  napi_value this_val;
  NODE_API_CALL(env,
                napi_get_cb_info(env, info, 0, nullptr, &this_val, nullptr));

  NestedWrap* obj = new NestedWrap();

  obj->env_ = env;
  NODE_API_CALL(env,
                napi_wrap(env,
                          this_val,
                          obj,
                          NestedWrap::Destructor,
                          nullptr /* finalize_hint */,
                          &obj->wrapper_));

  // Create a second napi_ref to be deleted in the destructor.
  NODE_API_CALL(env,
                napi_add_finalizer(env,
                                   this_val,
                                   obj,
                                   NestedWrap::Destructor,
                                   nullptr /* finalize_hint */,
                                   &obj->nested_));

  return this_val;
}

static napi_value GetFinalizerCallCount(napi_env env, napi_callback_info info) {
  napi_value result;
  NODE_API_CALL(env, napi_create_int32(env, finalization_count, &result));
  return result;
}

EXTERN_C_START
napi_value Init(napi_env env, napi_value exports) {
  NestedWrap::Init(env, exports);

  napi_property_descriptor descriptors[] = {
      DECLARE_NODE_API_PROPERTY("getFinalizerCallCount", GetFinalizerCallCount),
  };

  NODE_API_CALL(
      env,
      napi_define_properties(env,
                             exports,
                             sizeof(descriptors) / sizeof(*descriptors),
                             descriptors));

  return exports;
}
EXTERN_C_END
