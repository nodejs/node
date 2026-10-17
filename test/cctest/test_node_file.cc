#include "base_object-inl.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_file-inl.h"
#include "node_test_fixture.h"

using node::fs::BindingData;
using node::fs::FSReqBase;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::TryCatch;
using v8::Value;

class NodeFileTest : public EnvironmentTestFixture {};

class TestFSReq final : public FSReqBase {
 public:
  TestFSReq(BindingData* binding_data, Local<Object> object)
      : FSReqBase(binding_data,
                  object,
                  node::AsyncWrap::PROVIDER_FSREQCALLBACK,
                  false) {}

  void Resolve(Local<Value> value) override {
    resolved_.Reset(env()->isolate(), value);
  }

  void Reject(Local<Value> error) override {
    rejected_.Reset(env()->isolate(), error);
  }

  void ResolveStat(const uv_stat_t*) override {}
  void ResolveStatFs(const uv_statfs_t*) override {}
  void SetReturnValue(const FunctionCallbackInfo<Value>&) override {}

  SET_MEMORY_INFO_NAME(TestFSReq)
  SET_SELF_SIZE(TestFSReq)

  Local<Value> resolved() const { return resolved_.Get(env()->isolate()); }

  Local<Value> rejected() const { return rejected_.Get(env()->isolate()); }

 private:
  Global<Value> resolved_;
  Global<Value> rejected_;
};

// The reject branch cannot be reached from JavaScript, so test it in cctest.
TEST_F(NodeFileTest, ResolveOrRejectRejectsException) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  node::LoadEnvironment(*env, "require('fs');");

  BindingData* binding_data =
      (*env)->principal_realm()->GetBindingData<BindingData>();
  ASSERT_NE(binding_data, nullptr);

  Local<ObjectTemplate> object_template = ObjectTemplate::New(isolate_);
  object_template->SetInternalFieldCount(FSReqBase::kInternalFieldCount);
  Local<Object> object =
      object_template->NewInstance(env.context()).ToLocalChecked();
  auto req = node::MakeDetachedBaseObject<TestFSReq>(binding_data, object);
  Local<Value> expected = Exception::Error(
      String::NewFromUtf8Literal(isolate_, "expected exception"));
  TryCatch try_catch(isolate_);

  node::fs::ResolveOrReject(req.get(), [&]() -> MaybeLocal<Value> {
    isolate_->ThrowException(expected);
    return {};
  });

  EXPECT_TRUE(req->resolved().IsEmpty());
  ASSERT_FALSE(req->rejected().IsEmpty());
  EXPECT_TRUE(req->rejected()->StrictEquals(expected));
  EXPECT_FALSE(try_catch.HasCaught());
}
