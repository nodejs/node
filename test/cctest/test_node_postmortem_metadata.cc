#include "node_postmortem_metadata.cc"

#include "gtest/gtest.h"
#include "node.h"
#include "node_internals.h"
#include "node_test_fixture.h"
#include "req_wrap-inl.h"
#include "tracing/agent.h"
#include "v8.h"


class DebugSymbolsTest : public EnvironmentTestFixture {};


class TestHandleWrap : public node::HandleWrap {
 public:
  size_t self_size() const override { return sizeof(*this); }

  TestHandleWrap(node::Environment* env,
                 v8::Local<v8::Object> object,
                 uv_tcp_t* handle)
      : node::HandleWrap(env,
                         object,
                         reinterpret_cast<uv_handle_t*>(handle),
                         node::AsyncWrap::PROVIDER_TIMERWRAP) {}
};


class TestReqWrap : public node::ReqWrap<uv_req_t> {
 public:
  size_t self_size() const override { return sizeof(*this); }

  TestReqWrap(node::Environment* env, v8::Local<v8::Object> object)
      : node::ReqWrap<uv_req_t>(env,
                                object,
                                node::AsyncWrap::PROVIDER_TIMERWRAP) {}
};

TEST_F(DebugSymbolsTest, ContextEmbedderDataIndex) {
  int kContextEmbedderDataIndex = node::Environment::kContextEmbedderDataIndex;
  EXPECT_EQ(nodedbg_const_Environment__kContextEmbedderDataIndex__int,
            kContextEmbedderDataIndex);
}

TEST_F(DebugSymbolsTest, ExternalStringDataOffset) {
  EXPECT_EQ(nodedbg_offset_ExternalString__data__uintptr_t,
            NODE_OFF_EXTSTR_DATA);
}

TEST_F(DebugSymbolsTest, BaseObjectPersistentHandle) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv, this};

  v8::Local<v8::Object> object = v8::Object::New(isolate_);
  node::BaseObject obj(*env, object);

  auto expected = reinterpret_cast<uintptr_t>(&obj.persistent());
  auto calculated = reinterpret_cast<uintptr_t>(&obj) +
      nodedbg_offset_BaseObject__persistent_handle___v8_Persistent_v8_Object;
  EXPECT_EQ(expected, calculated);

  obj.persistent().Reset();  // ~BaseObject() expects an empty handle.
}


TEST_F(DebugSymbolsTest, EnvironmentHandleWrapQueue) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv, this};

  auto expected = reinterpret_cast<uintptr_t>((*env)->handle_wrap_queue());
  auto calculated = reinterpret_cast<uintptr_t>(*env) +
      nodedbg_offset_Environment__handle_wrap_queue___Environment_HandleWrapQueue;  // NOLINT(whitespace/line_length)
  EXPECT_EQ(expected, calculated);
}

TEST_F(DebugSymbolsTest, EnvironmentReqWrapQueue) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv, this};

  auto expected = reinterpret_cast<uintptr_t>((*env)->req_wrap_queue());
  auto calculated = reinterpret_cast<uintptr_t>(*env) +
      nodedbg_offset_Environment__req_wrap_queue___Environment_ReqWrapQueue;
  EXPECT_EQ(expected, calculated);
}

TEST_F(DebugSymbolsTest, HandleWrapList) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv, this};

  uv_tcp_t handle;

  auto obj_template = v8::FunctionTemplate::New(isolate_);
  obj_template->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::Object> object =
      obj_template->GetFunction()->NewInstance(env.context()).ToLocalChecked();
  TestHandleWrap obj(*env, object, &handle);

  auto queue = reinterpret_cast<uintptr_t>((*env)->handle_wrap_queue());
  auto head = queue +
      nodedbg_offset_Environment_HandleWrapQueue__head___ListNode_HandleWrap;
  auto next =
      head + nodedbg_offset_ListNode_HandleWrap__next___uintptr_t;
  next = *reinterpret_cast<uintptr_t*>(next);

  auto expected = reinterpret_cast<uintptr_t>(&obj);
  auto calculated = next -
      nodedbg_offset_HandleWrap__handle_wrap_queue___ListNode_HandleWrap;
  EXPECT_EQ(expected, calculated);

  obj.persistent().Reset();  // ~HandleWrap() expects an empty handle.
}

TEST_F(DebugSymbolsTest, ReqWrapList) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv, this};

  auto obj_template = v8::FunctionTemplate::New(isolate_);
  obj_template->InstanceTemplate()->SetInternalFieldCount(1);

  v8::Local<v8::Object> object =
      obj_template->GetFunction()->NewInstance(env.context()).ToLocalChecked();
  TestReqWrap obj(*env, object);

  // NOTE (mmarchini): Workaround to fix failing tests on ARM64 machines with
  // older GCC. Should be removed once we upgrade the GCC version used on our
  // ARM64 CI machinies.
  for (auto it : *(*env)->req_wrap_queue()) {}

  auto queue = reinterpret_cast<uintptr_t>((*env)->req_wrap_queue());
  auto head = queue +
      nodedbg_offset_Environment_ReqWrapQueue__head___ListNode_ReqWrapQueue;
  auto next =
      head + nodedbg_offset_ListNode_ReqWrap__next___uintptr_t;
  next = *reinterpret_cast<uintptr_t*>(next);

  auto expected = reinterpret_cast<uintptr_t>(&obj);
  auto calculated =
      next - nodedbg_offset_ReqWrap__req_wrap_queue___ListNode_ReqWrapQueue;
  EXPECT_EQ(expected, calculated);

  obj.Dispatched();
}
