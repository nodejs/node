#include "gtest/gtest.h"
#include "node.h"
#include "node_internals.h"
#include "node_test_fixture.h"
#include "req_wrap-inl.h"
#include "tracing/agent.h"
#include "v8.h"

#define NODE_OFF_EXTSTR_DATA sizeof(void*)

extern "C" {
extern uintptr_t
    nodedbg_offset_HandleWrap__handle_wrap_queue___ListNode_HandleWrap;
extern uintptr_t
    nodedbg_offset_Environment__handle_wrap_queue___Environment_HandleWrapQueue;
extern int debug_symbols_generated;
extern int nodedbg_const_ContextEmbedderIndex__kEnvironment__int;
extern int nodedbg_const_BaseObject__kInternalFieldCount__int;
extern uintptr_t
    nodedbg_offset_Environment_HandleWrapQueue__head___ListNode_HandleWrap;
extern uintptr_t
    nodedbg_offset_Environment__req_wrap_queue___Environment_ReqWrapQueue;
extern uintptr_t nodedbg_offset_ExternalString__data__uintptr_t;
extern uintptr_t nodedbg_offset_ListNode_ReqWrap__prev___uintptr_t;
extern uintptr_t nodedbg_offset_ListNode_ReqWrap__next___uintptr_t;
extern uintptr_t nodedbg_offset_ReqWrap__req_wrap_queue___ListNode_ReqWrapQueue;
extern uintptr_t nodedbg_offset_ListNode_HandleWrap__prev___uintptr_t;
extern uintptr_t nodedbg_offset_ListNode_HandleWrap__next___uintptr_t;
extern uintptr_t
    nodedbg_offset_Environment_ReqWrapQueue__head___ListNode_ReqWrapQueue;
extern uintptr_t
    nodedbg_offset_BaseObject__persistent_handle___v8_Persistent_v8_Object;
}


class DebugSymbolsTest : public EnvironmentTestFixture {};


class TestHandleWrap : public node::HandleWrap {
 public:
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(TestHandleWrap)
  SET_SELF_SIZE(TestHandleWrap)

  TestHandleWrap(node::Environment* env,
                 v8::Local<v8::Object> object,
                 uv_tcp_t* handle)
      : node::HandleWrap(env,
                         object,
                         reinterpret_cast<uv_handle_t*>(handle),
                         node::AsyncWrap::PROVIDER_TCPWRAP) {}
};


class TestReqWrap : public node::ReqWrap<uv_req_t> {
 public:
  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(TestReqWrap)
  SET_SELF_SIZE(TestReqWrap)

  TestReqWrap(node::Environment* env, v8::Local<v8::Object> object)
      : node::ReqWrap<uv_req_t>(env,
                                object,
                                node::AsyncWrap::PROVIDER_FSREQCALLBACK) {}
};

TEST_F(DebugSymbolsTest, ContextEmbedderEnvironmentIndex) {
  int kEnvironmentIndex = node::ContextEmbedderIndex::kEnvironment;
  EXPECT_EQ(nodedbg_const_ContextEmbedderIndex__kEnvironment__int,
            kEnvironmentIndex);
}

TEST_F(DebugSymbolsTest, BaseObjectkInternalFieldCount) {
  int kInternalFieldCount = node::BaseObject::kInternalFieldCount;
  EXPECT_EQ(nodedbg_const_BaseObject__kInternalFieldCount__int,
            kInternalFieldCount);
}

TEST_F(DebugSymbolsTest, ExternalStringDataOffset) {
  EXPECT_EQ(nodedbg_offset_ExternalString__data__uintptr_t,
            NODE_OFF_EXTSTR_DATA);
}

class DummyBaseObject : public node::BaseObject {
 public:
  DummyBaseObject(node::Environment* env, v8::Local<v8::Object> obj) :
    BaseObject(env, obj) {}

  SET_NO_MEMORY_INFO()
  SET_MEMORY_INFO_NAME(DummyBaseObject)
  SET_SELF_SIZE(DummyBaseObject)
};

TEST_F(DebugSymbolsTest, BaseObjectPersistentHandle) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  v8::Local<v8::ObjectTemplate> obj_templ = v8::ObjectTemplate::New(isolate_);
  obj_templ->SetInternalFieldCount(
      nodedbg_const_BaseObject__kInternalFieldCount__int);

  v8::Local<v8::Object> object =
      obj_templ->NewInstance(env.context()).ToLocalChecked();
  node::BaseObjectPtr<DummyBaseObject> obj =
      node::MakeDetachedBaseObject<DummyBaseObject>(*env, object);

  auto expected = reinterpret_cast<uintptr_t>(&obj->persistent());
  auto calculated = reinterpret_cast<uintptr_t>(obj.get()) +
      nodedbg_offset_BaseObject__persistent_handle___v8_Persistent_v8_Object;
  EXPECT_EQ(expected, calculated);
}


TEST_F(DebugSymbolsTest, EnvironmentHandleWrapQueue) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  auto expected = reinterpret_cast<uintptr_t>((*env)->handle_wrap_queue());
  auto calculated = reinterpret_cast<uintptr_t>(*env) +
      nodedbg_offset_Environment__handle_wrap_queue___Environment_HandleWrapQueue;  // NOLINT(whitespace/line_length)
  EXPECT_EQ(expected, calculated);
}

TEST_F(DebugSymbolsTest, EnvironmentReqWrapQueue) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  auto expected = reinterpret_cast<uintptr_t>((*env)->req_wrap_queue());
  auto calculated = reinterpret_cast<uintptr_t>(*env) +
      nodedbg_offset_Environment__req_wrap_queue___Environment_ReqWrapQueue;
  EXPECT_EQ(expected, calculated);
}

TEST_F(DebugSymbolsTest, HandleWrapList) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  auto queue = reinterpret_cast<uintptr_t>((*env)->handle_wrap_queue());
  auto head = queue +
      nodedbg_offset_Environment_HandleWrapQueue__head___ListNode_HandleWrap;
  auto tail = head + nodedbg_offset_ListNode_HandleWrap__prev___uintptr_t;
  tail = *reinterpret_cast<uintptr_t*>(tail);

  uv_tcp_t handle;

  auto obj_template = v8::FunctionTemplate::New(isolate_);
  obj_template->InstanceTemplate()->SetInternalFieldCount(
      nodedbg_const_BaseObject__kInternalFieldCount__int);

  v8::Local<v8::Object> object = obj_template->GetFunction(env.context())
                                     .ToLocalChecked()
                                     ->NewInstance(env.context())
                                     .ToLocalChecked();
  TestHandleWrap obj(*env, object, &handle);

  auto last = tail + nodedbg_offset_ListNode_HandleWrap__next___uintptr_t;
  last = *reinterpret_cast<uintptr_t*>(last);

  auto expected = reinterpret_cast<uintptr_t>(&obj);
  auto calculated =
      last - nodedbg_offset_HandleWrap__handle_wrap_queue___ListNode_HandleWrap;
  EXPECT_EQ(expected, calculated);

  obj.persistent().Reset();  // ~HandleWrap() expects an empty handle.
}

TEST_F(DebugSymbolsTest, ReqWrapList) {
  const v8::HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};

  auto queue = reinterpret_cast<uintptr_t>((*env)->req_wrap_queue());
  auto head =
      queue +
      nodedbg_offset_Environment_ReqWrapQueue__head___ListNode_ReqWrapQueue;
  auto tail = head + nodedbg_offset_ListNode_ReqWrap__prev___uintptr_t;
  tail = *reinterpret_cast<uintptr_t*>(tail);

  auto obj_template = v8::FunctionTemplate::New(isolate_);
  obj_template->InstanceTemplate()->SetInternalFieldCount(
      nodedbg_const_BaseObject__kInternalFieldCount__int);

  v8::Local<v8::Object> object = obj_template->GetFunction(env.context())
                                     .ToLocalChecked()
                                     ->NewInstance(env.context())
                                     .ToLocalChecked();
  TestReqWrap obj(*env, object);

  // NOTE (mmarchini): Workaround to fix failing tests on ARM64 machines with
  // older GCC. Should be removed once we upgrade the GCC version used on our
  // ARM64 CI machinies.
  for (auto it : *(*env)->req_wrap_queue()) (void) &it;

  volatile uintptr_t last =
      tail + nodedbg_offset_ListNode_ReqWrap__next___uintptr_t;
  last = *reinterpret_cast<uintptr_t*>(last);

  auto expected = reinterpret_cast<uintptr_t>(&obj);
  auto calculated =
      last - nodedbg_offset_ReqWrap__req_wrap_queue___ListNode_ReqWrapQueue;
  EXPECT_EQ(expected, calculated);

  obj.Dispatched();
}
