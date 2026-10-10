#include "base_object-inl.h"
#include "env-inl.h"
#include "gtest/gtest.h"
#include "node_file-inl.h"
#include "node_test_fixture.h"

#include <string>

// The branches taken when the path returned by mkstemp() cannot be encoded
// cannot be reached from JavaScript, so they are tested here.

namespace {

using node::fs::BindingData;
using node::fs::FSReqBase;
using node::fs::MkstempFd;
using node::fs::MkstempFileHandle;
using node::fs::MkstempResult;
using node::fs::ResolveMkstemp;
using v8::Array;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Global;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::TryCatch;
using v8::Value;

class NodeFileMkstempTest : public EnvironmentTestFixture {};

// Records how the request was settled. With `throw_on_settle`, settling
// throws, like a JS callback that throws would.
class MkstempTestReq final : public FSReqBase {
 public:
  MkstempTestReq(BindingData* binding_data, Local<Object> object)
      : FSReqBase(binding_data,
                  object,
                  node::AsyncWrap::PROVIDER_FSREQCALLBACK,
                  false) {}

  void Resolve(Local<Value> value) override {
    resolved_.Reset(env()->isolate(), value);
    MaybeThrow();
  }

  void Reject(Local<Value> error) override {
    rejected_.Reset(env()->isolate(), error);
    MaybeThrow();
  }

  void ResolveStat(const uv_stat_t*) override {}
  void ResolveStatFs(const uv_statfs_t*) override {}
  void SetReturnValue(const FunctionCallbackInfo<Value>&) override {}

  SET_MEMORY_INFO_NAME(MkstempTestReq)
  SET_SELF_SIZE(MkstempTestReq)

  Local<Value> resolved() const { return resolved_.Get(env()->isolate()); }
  Local<Value> rejected() const { return rejected_.Get(env()->isolate()); }

  bool throw_on_settle = false;

 private:
  void MaybeThrow() {
    if (!throw_on_settle) return;
    Isolate* isolate = env()->isolate();
    isolate->ThrowException(
        Exception::Error(String::NewFromUtf8Literal(isolate, "settle")));
  }

  Global<Value> resolved_;
  Global<Value> rejected_;
};

struct TempFile {
  TempFile() {
    char dir[4096];
    size_t size = sizeof(dir);
    CHECK_EQ(0, uv_os_tmpdir(dir, &size));
    const std::string tmpl = std::string(dir, size) + "/node-cctest-XXXXXX";
    uv_fs_t req;
    fd = uv_fs_mkstemp(nullptr, &req, tmpl.c_str(), nullptr);
    CHECK_GE(fd, 0);
    path = req.path;
    uv_fs_req_cleanup(&req);
  }

  ~TempFile() {
    uv_fs_t req;
    if (!IsClosed()) {
      uv_fs_close(nullptr, &req, fd, nullptr);
      uv_fs_req_cleanup(&req);
    }
    uv_fs_unlink(nullptr, &req, path.c_str(), nullptr);
    uv_fs_req_cleanup(&req);
  }

  bool IsClosed() const {
    uv_fs_t req;
    const int err = uv_fs_fstat(nullptr, &req, fd, nullptr);
    uv_fs_req_cleanup(&req);
    return err == UV_EBADF;
  }

  int fd;
  std::string path;
};

BindingData* GetBindingData(node::Environment* env) {
  BindingData* binding_data =
      env->principal_realm()->GetBindingData<BindingData>();
  CHECK_NOT_NULL(binding_data);
  return binding_data;
}

node::BaseObjectPtr<MkstempTestReq> NewReq(node::Environment* env) {
  Local<ObjectTemplate> object_template = ObjectTemplate::New(env->isolate());
  object_template->SetInternalFieldCount(FSReqBase::kInternalFieldCount);
  Local<Object> object =
      object_template->NewInstance(env->context()).ToLocalChecked();
  return node::MakeDetachedBaseObject<MkstempTestReq>(GetBindingData(env),
                                                      object);
}

MaybeLocal<Value> Path(Isolate* isolate) {
  return String::NewFromUtf8Literal(isolate, "path");
}

MaybeLocal<Value> Throw(Isolate* isolate, Local<Value> error) {
  isolate->ThrowException(error);
  return {};
}

Local<Value> NewError(Isolate* isolate) {
  return Exception::Error(String::NewFromUtf8Literal(isolate, "expected"));
}

TEST_F(NodeFileMkstempTest, ResultHoldsPathAndFile) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  TempFile file;

  Local<Value> result;
  ASSERT_TRUE(MkstempResult(
                  isolate_,
                  file.fd,
                  [&]() { return Path(isolate_); },
                  [&]() { return MkstempFd(*env, file.fd); })
                  .ToLocal(&result));
  ASSERT_TRUE(result->IsArray());
  Local<Array> array = result.As<Array>();
  EXPECT_EQ(array->Length(), 2U);
  Local<Value> fd = array->Get(env.context(), 1).ToLocalChecked();
  EXPECT_EQ(fd->Int32Value(env.context()).FromJust(), file.fd);
  EXPECT_FALSE(file.IsClosed());
}

TEST_F(NodeFileMkstempTest, ResultClosesDescriptorIfEncodingThrows) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  TempFile file;
  bool made_file = false;

  TryCatch try_catch(isolate_);
  EXPECT_TRUE(MkstempResult(
                  isolate_,
                  file.fd,
                  [&]() { return Throw(isolate_, NewError(isolate_)); },
                  [&]() {
                    made_file = true;
                    return MkstempFd(*env, file.fd);
                  })
                  .IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(made_file);
  EXPECT_TRUE(file.IsClosed());
}

TEST_F(NodeFileMkstempTest, ResultClosesDescriptorIfMakingFileThrows) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  TempFile file;

  TryCatch try_catch(isolate_);
  EXPECT_TRUE(MkstempResult(
                  isolate_,
                  file.fd,
                  [&]() { return Path(isolate_); },
                  [&]() { return Throw(isolate_, NewError(isolate_)); })
                  .IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_TRUE(file.IsClosed());
}

TEST_F(NodeFileMkstempTest, FileHandleFailureIsReported) {
  const HandleScope handle_scope(isolate_);
  EXPECT_TRUE(MkstempFileHandle([]() -> node::fs::FileHandle* {
                return nullptr;
              }).IsEmpty());
}

TEST_F(NodeFileMkstempTest, ResolveDeliversResult) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  node::LoadEnvironment(*env, "require('fs');");
  TempFile file;
  auto req = NewReq(*env);

  ResolveMkstemp(
      req.get(),
      file.fd,
      [&]() { return Path(isolate_); },
      [&]() { return MkstempFd(*env, file.fd); });

  EXPECT_TRUE(req->rejected().IsEmpty());
  ASSERT_FALSE(req->resolved().IsEmpty());
  EXPECT_TRUE(req->resolved()->IsArray());
  EXPECT_FALSE(file.IsClosed());
}

TEST_F(NodeFileMkstempTest, ResolveRejectsAndClosesDescriptorOnException) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  node::LoadEnvironment(*env, "require('fs');");
  TempFile file;
  auto req = NewReq(*env);
  Local<Value> expected = NewError(isolate_);

  TryCatch try_catch(isolate_);
  ResolveMkstemp(
      req.get(),
      file.fd,
      [&]() { return Throw(isolate_, expected); },
      [&]() { return MkstempFd(*env, file.fd); });

  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_TRUE(req->resolved().IsEmpty());
  ASSERT_FALSE(req->rejected().IsEmpty());
  EXPECT_TRUE(req->rejected()->StrictEquals(expected));
  EXPECT_TRUE(file.IsClosed());
}

// An exception thrown while the request is settled, as a JS callback could,
// must not be caught by the helper.
TEST_F(NodeFileMkstempTest, ExceptionThrownByResolveEscapes) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  node::LoadEnvironment(*env, "require('fs');");
  TempFile file;
  auto req = NewReq(*env);
  req->throw_on_settle = true;

  TryCatch try_catch(isolate_);
  ResolveMkstemp(
      req.get(),
      file.fd,
      [&]() { return Path(isolate_); },
      [&]() { return MkstempFd(*env, file.fd); });

  EXPECT_FALSE(req->resolved().IsEmpty());
  EXPECT_TRUE(try_catch.HasCaught());
}

TEST_F(NodeFileMkstempTest, ExceptionThrownByRejectEscapes) {
  const HandleScope handle_scope(isolate_);
  const Argv argv;
  Env env{handle_scope, argv};
  node::LoadEnvironment(*env, "require('fs');");
  TempFile file;
  auto req = NewReq(*env);
  req->throw_on_settle = true;
  Local<Value> expected = NewError(isolate_);

  TryCatch try_catch(isolate_);
  ResolveMkstemp(
      req.get(),
      file.fd,
      [&]() { return Throw(isolate_, expected); },
      [&]() { return MkstempFd(*env, file.fd); });

  ASSERT_FALSE(req->rejected().IsEmpty());
  EXPECT_TRUE(req->rejected()->StrictEquals(expected));
  ASSERT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(try_catch.Exception()->StrictEquals(expected));
}

}  // namespace
