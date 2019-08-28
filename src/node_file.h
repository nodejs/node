#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "aliased_buffer.h"
#include "stream_base.h"
#include "memory_tracker-inl.h"
#include "req_wrap-inl.h"
#include <iostream>

namespace node {

using v8::Context;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::Promise;
using v8::Undefined;
using v8::Value;

namespace fs {

// structure used to store state during a complex operation, e.g., mkdirp.
class FSContinuationData : public MemoryRetainer {
 public:
  FSContinuationData(uv_fs_t* req, int mode, uv_fs_cb done_cb)
      : req(req), mode(mode), done_cb(done_cb) {
  }

  uv_fs_t* req;
  int mode;
  std::vector<std::string> paths{};

  void PushPath(std::string&& path) {
    paths.emplace_back(std::move(path));
  }

  void PushPath(const std::string& path) {
    paths.push_back(path);
  }

  std::string PopPath() {
    CHECK_GT(paths.size(), 0);
    std::string path = std::move(paths.back());
    paths.pop_back();
    return path;
  }

  void Done(int result) {
    req->result = result;
    done_cb(req);
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("paths", paths);
  }

  SET_MEMORY_INFO_NAME(FSContinuationData)
  SET_SELF_SIZE(FSContinuationData)

 private:
  uv_fs_cb done_cb;
};

class FSReqBase : public ReqWrap<uv_fs_t> {
 public:
  typedef MaybeStackBuffer<char, 64> FSReqBuffer;
  std::unique_ptr<FSContinuationData> continuation_data = nullptr;

  FSReqBase(Environment* env, Local<Object> req, AsyncWrap::ProviderType type,
            bool use_bigint)
      : ReqWrap(env, req, type), use_bigint_(use_bigint) {
  }

  void Init(const char* syscall,
            const char* data,
            size_t len,
            enum encoding encoding) {
    syscall_ = syscall;
    encoding_ = encoding;

    if (data != nullptr) {
      CHECK(!has_data_);
      buffer_.AllocateSufficientStorage(len + 1);
      buffer_.SetLengthAndZeroTerminate(len);
      memcpy(*buffer_, data, len);
      has_data_ = true;
    }
  }

  FSReqBuffer& Init(const char* syscall, size_t len,
                    enum encoding encoding) {
    syscall_ = syscall;
    encoding_ = encoding;

    buffer_.AllocateSufficientStorage(len + 1);
    has_data_ = false;  // so that the data does not show up in error messages
    return buffer_;
  }

  virtual void Reject(Local<Value> reject) = 0;
  virtual void Resolve(Local<Value> value) = 0;
  virtual void ResolveStat(const uv_stat_t* stat) = 0;
  virtual void SetReturnValue(const FunctionCallbackInfo<Value>& args) = 0;

  const char* syscall() const { return syscall_; }
  const char* data() const { return has_data_ ? *buffer_ : nullptr; }
  enum encoding encoding() const { return encoding_; }

  bool use_bigint() const { return use_bigint_; }

  static FSReqBase* from_req(uv_fs_t* req) {
    return static_cast<FSReqBase*>(ReqWrap::from_req(req));
  }

  FSReqBase(const FSReqBase&) = delete;
  FSReqBase& operator=(const FSReqBase&) = delete;

 private:
  enum encoding encoding_ = UTF8;
  bool has_data_ = false;
  const char* syscall_ = nullptr;
  bool use_bigint_ = false;

  // Typically, the content of buffer_ is something like a file name, so
  // something around 64 bytes should be enough.
  FSReqBuffer buffer_;
};

class FSReqCallback : public FSReqBase {
 public:
  FSReqCallback(Environment* env, Local<Object> req, bool use_bigint)
      : FSReqBase(env, req, AsyncWrap::PROVIDER_FSREQCALLBACK, use_bigint) { }

  void Reject(Local<Value> reject) override;
  void Resolve(Local<Value> value) override;
  void ResolveStat(const uv_stat_t* stat) override;
  void SetReturnValue(const FunctionCallbackInfo<Value>& args) override;

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("continuation_data", continuation_data);
  }

  SET_MEMORY_INFO_NAME(FSReqCallback)
  SET_SELF_SIZE(FSReqCallback)

  FSReqCallback(const FSReqCallback&) = delete;
  FSReqCallback& operator=(const FSReqCallback&) = delete;
};

template <typename NativeT, typename V8T>
constexpr void FillStatsArray(AliasedBufferBase<NativeT, V8T>* fields,
                              const uv_stat_t* s,
                              const size_t offset = 0) {
#define SET_FIELD_WITH_STAT(stat_offset, stat)                               \
  fields->SetValue(offset + static_cast<size_t>(FsStatsOffset::stat_offset), \
                   static_cast<NativeT>(stat))

#define SET_FIELD_WITH_TIME_STAT(stat_offset, stat)                          \
  /* NOLINTNEXTLINE(runtime/int) */                                          \
  SET_FIELD_WITH_STAT(stat_offset, static_cast<unsigned long>(stat))

  SET_FIELD_WITH_STAT(kDev, s->st_dev);
  SET_FIELD_WITH_STAT(kMode, s->st_mode);
  SET_FIELD_WITH_STAT(kNlink, s->st_nlink);
  SET_FIELD_WITH_STAT(kUid, s->st_uid);
  SET_FIELD_WITH_STAT(kGid, s->st_gid);
  SET_FIELD_WITH_STAT(kRdev, s->st_rdev);
  SET_FIELD_WITH_STAT(kBlkSize, s->st_blksize);
  SET_FIELD_WITH_STAT(kIno, s->st_ino);
  SET_FIELD_WITH_STAT(kSize, s->st_size);
  SET_FIELD_WITH_STAT(kBlocks, s->st_blocks);

  SET_FIELD_WITH_TIME_STAT(kATimeSec, s->st_atim.tv_sec);
  SET_FIELD_WITH_TIME_STAT(kATimeNsec, s->st_atim.tv_nsec);
  SET_FIELD_WITH_TIME_STAT(kMTimeSec, s->st_mtim.tv_sec);
  SET_FIELD_WITH_TIME_STAT(kMTimeNsec, s->st_mtim.tv_nsec);
  SET_FIELD_WITH_TIME_STAT(kCTimeSec, s->st_ctim.tv_sec);
  SET_FIELD_WITH_TIME_STAT(kCTimeNsec, s->st_ctim.tv_nsec);
  SET_FIELD_WITH_TIME_STAT(kBirthTimeSec, s->st_birthtim.tv_sec);
  SET_FIELD_WITH_TIME_STAT(kBirthTimeNsec, s->st_birthtim.tv_nsec);

#undef SET_FIELD_WITH_TIME_STAT
#undef SET_FIELD_WITH_STAT
}

inline Local<Value> FillGlobalStatsArray(Environment* env,
                                         const bool use_bigint,
                                         const uv_stat_t* s,
                                         const bool second = false) {
  const ptrdiff_t offset =
      second ? static_cast<ptrdiff_t>(FsStatsOffset::kFsStatsFieldsNumber) : 0;
  if (use_bigint) {
    auto* const arr = env->fs_stats_field_bigint_array();
    FillStatsArray(arr, s, offset);
    return arr->GetJSArray();
  } else {
    auto* const arr = env->fs_stats_field_array();
    FillStatsArray(arr, s, offset);
    return arr->GetJSArray();
  }
}

template <typename AliasedBufferT>
class FSReqPromise : public FSReqBase {
 public:
  static FSReqPromise* New(Environment* env, bool use_bigint) {
    v8::Local<Object> obj;
    if (!env->fsreqpromise_constructor_template()
             ->NewInstance(env->context())
             .ToLocal(&obj)) {
      return nullptr;
    }
    v8::Local<v8::Promise::Resolver> resolver;
    if (!v8::Promise::Resolver::New(env->context()).ToLocal(&resolver) ||
        obj->Set(env->context(), env->promise_string(), resolver).IsNothing()) {
      return nullptr;
    }
    return new FSReqPromise(env, obj, use_bigint);
  }

  ~FSReqPromise() override {
    // Validate that the promise was explicitly resolved or rejected.
    CHECK(finished_);
  }

  void Reject(Local<Value> reject) override {
    finished_ = true;
    HandleScope scope(env()->isolate());
    InternalCallbackScope callback_scope(this);
    Local<Value> value =
        object()->Get(env()->context(),
                      env()->promise_string()).ToLocalChecked();
    Local<Promise::Resolver> resolver = value.As<Promise::Resolver>();
    USE(resolver->Reject(env()->context(), reject).FromJust());
  }

  void Resolve(Local<Value> value) override {
    finished_ = true;
    HandleScope scope(env()->isolate());
    InternalCallbackScope callback_scope(this);
    Local<Value> val =
        object()->Get(env()->context(),
                      env()->promise_string()).ToLocalChecked();
    Local<Promise::Resolver> resolver = val.As<Promise::Resolver>();
    USE(resolver->Resolve(env()->context(), value).FromJust());
  }

  void ResolveStat(const uv_stat_t* stat) override {
    FillStatsArray(&stats_field_array_, stat);
    Resolve(stats_field_array_.GetJSArray());
  }

  void SetReturnValue(const FunctionCallbackInfo<Value>& args) override {
    Local<Value> val =
        object()->Get(env()->context(),
                      env()->promise_string()).ToLocalChecked();
    Local<Promise::Resolver> resolver = val.As<Promise::Resolver>();
    args.GetReturnValue().Set(resolver->GetPromise());
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("stats_field_array", stats_field_array_);
    tracker->TrackField("continuation_data", continuation_data);
  }

  SET_MEMORY_INFO_NAME(FSReqPromise)
  SET_SELF_SIZE(FSReqPromise)

  FSReqPromise(const FSReqPromise&) = delete;
  FSReqPromise& operator=(const FSReqPromise&) = delete;
  FSReqPromise(const FSReqPromise&&) = delete;
  FSReqPromise& operator=(const FSReqPromise&&) = delete;

 private:
  FSReqPromise(Environment* env, v8::Local<v8::Object> obj, bool use_bigint)
      : FSReqBase(env, obj, AsyncWrap::PROVIDER_FSREQPROMISE, use_bigint),
        stats_field_array_(
            env->isolate(),
            static_cast<size_t>(FsStatsOffset::kFsStatsFieldsNumber)) {}

  bool finished_ = false;
  AliasedBufferT stats_field_array_;
};

class FSReqAfterScope {
 public:
  FSReqAfterScope(FSReqBase* wrap, uv_fs_t* req);
  ~FSReqAfterScope();

  bool Proceed();

  void Reject(uv_fs_t* req);

  FSReqAfterScope(const FSReqAfterScope&) = delete;
  FSReqAfterScope& operator=(const FSReqAfterScope&) = delete;
  FSReqAfterScope(const FSReqAfterScope&&) = delete;
  FSReqAfterScope& operator=(const FSReqAfterScope&&) = delete;

 private:
  FSReqBase* wrap_ = nullptr;
  uv_fs_t* req_ = nullptr;
  HandleScope handle_scope_;
  Context::Scope context_scope_;
};

class FileHandle;

// A request wrap specifically for uv_fs_read()s scheduled for reading
// from a FileHandle.
class FileHandleReadWrap : public ReqWrap<uv_fs_t> {
 public:
  FileHandleReadWrap(FileHandle* handle, v8::Local<v8::Object> obj);

  static inline FileHandleReadWrap* from_req(uv_fs_t* req) {
    return static_cast<FileHandleReadWrap*>(ReqWrap::from_req(req));
  }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(FileHandleReadWrap)
  SET_SELF_SIZE(FileHandleReadWrap)

 private:
  FileHandle* file_handle_;
  uv_buf_t buffer_;

  friend class FileHandle;
};

// A wrapper for a file descriptor that will automatically close the fd when
// the object is garbage collected
class FileHandle : public AsyncWrap, public StreamBase {
 public:
  static FileHandle* New(Environment* env,
                         int fd,
                         v8::Local<v8::Object> obj = v8::Local<v8::Object>());
  ~FileHandle() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int fd() const { return fd_; }

  // Will asynchronously close the FD and return a Promise that will
  // be resolved once closing is complete.
  static void Close(const FunctionCallbackInfo<Value>& args);

  // Releases ownership of the FD.
  static void ReleaseFD(const FunctionCallbackInfo<Value>& args);

  // StreamBase interface:
  int ReadStart() override;
  int ReadStop() override;

  bool IsAlive() override { return !closed_; }
  bool IsClosing() override { return closing_; }
  AsyncWrap* GetAsyncWrap() override { return this; }

  // In the case of file streams, shutting down corresponds to closing.
  ShutdownWrap* CreateShutdownWrap(v8::Local<v8::Object> object) override;
  int DoShutdown(ShutdownWrap* req_wrap) override;

  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override {
    return UV_ENOSYS;  // Not implemented (yet).
  }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackField("current_read", current_read_);
  }

  SET_MEMORY_INFO_NAME(FileHandle)
  SET_SELF_SIZE(FileHandle)

  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;
  FileHandle(const FileHandle&&) = delete;
  FileHandle& operator=(const FileHandle&&) = delete;

 private:
  FileHandle(Environment* env, v8::Local<v8::Object> obj, int fd);

  // Synchronous close that emits a warning
  void Close();
  void AfterClose();

  class CloseReq : public ReqWrap<uv_fs_t> {
   public:
    CloseReq(Environment* env,
             Local<Object> obj,
             Local<Promise> promise,
             Local<Value> ref)
        : ReqWrap(env, obj, AsyncWrap::PROVIDER_FILEHANDLECLOSEREQ) {
      promise_.Reset(env->isolate(), promise);
      ref_.Reset(env->isolate(), ref);
    }

    ~CloseReq() override {
      uv_fs_req_cleanup(req());
      promise_.Reset();
      ref_.Reset();
    }

    FileHandle* file_handle();

    void MemoryInfo(MemoryTracker* tracker) const override {
      tracker->TrackField("promise", promise_);
      tracker->TrackField("ref", ref_);
    }

    SET_MEMORY_INFO_NAME(CloseReq)
    SET_SELF_SIZE(CloseReq)

    void Resolve();

    void Reject(Local<Value> reason);

    static CloseReq* from_req(uv_fs_t* req) {
      return static_cast<CloseReq*>(ReqWrap::from_req(req));
    }

    CloseReq(const CloseReq&) = delete;
    CloseReq& operator=(const CloseReq&) = delete;
    CloseReq(const CloseReq&&) = delete;
    CloseReq& operator=(const CloseReq&&) = delete;

   private:
    v8::Global<Promise> promise_{};
    v8::Global<Value> ref_{};
  };

  // Asynchronous close
  inline MaybeLocal<Promise> ClosePromise();

  int fd_;
  bool closing_ = false;
  bool closed_ = false;
  int64_t read_offset_ = -1;
  int64_t read_length_ = -1;

  bool reading_ = false;
  std::unique_ptr<FileHandleReadWrap> current_read_ = nullptr;
};

int MKDirpSync(uv_loop_t* loop,
               uv_fs_t* req,
               const std::string& path,
               int mode,
               uv_fs_cb cb = nullptr);

class FSReqWrapSync {
 public:
  FSReqWrapSync() = default;
  ~FSReqWrapSync() { uv_fs_req_cleanup(&req); }
  uv_fs_t req;

  FSReqWrapSync(const FSReqWrapSync&) = delete;
  FSReqWrapSync& operator=(const FSReqWrapSync&) = delete;
};

// TODO(addaleax): Currently, callers check the return value and assume
// that nullptr indicates a synchronous call, rather than a failure.
// Failure conditions should be disambiguated and handled appropriately.
inline FSReqBase* GetReqWrap(Environment* env, v8::Local<v8::Value> value,
                             bool use_bigint = false) {
  if (value->IsObject()) {
    return Unwrap<FSReqBase>(value.As<Object>());
  } else if (value->StrictEquals(env->fs_use_promises_symbol())) {
    if (use_bigint) {
      return FSReqPromise<AliasedBigUint64Array>::New(env, use_bigint);
    } else {
      return FSReqPromise<AliasedFloat64Array>::New(env, use_bigint);
    }
  }
  return nullptr;
}

// Returns nullptr if the operation fails from the start.
template <typename Func, typename... Args>
inline FSReqBase* AsyncDestCall(Environment* env, FSReqBase* req_wrap,
                                const v8::FunctionCallbackInfo<Value>& args,
                                const char* syscall, const char* dest,
                                size_t len, enum encoding enc, uv_fs_cb after,
                                Func fn, Args... fn_args) {
  CHECK_NOT_NULL(req_wrap);
  req_wrap->Init(syscall, dest, len, enc);
  int err = req_wrap->Dispatch(fn, fn_args..., after);
  if (err < 0) {
    uv_fs_t* uv_req = req_wrap->req();
    uv_req->result = err;
    uv_req->path = nullptr;
    after(uv_req);  // after may delete req_wrap if there is an error
    req_wrap = nullptr;
  } else {
    req_wrap->SetReturnValue(args);
  }

  return req_wrap;
}

// Returns nullptr if the operation fails from the start.
template <typename Func, typename... Args>
inline FSReqBase* AsyncCall(Environment* env,
                            FSReqBase* req_wrap,
                            const v8::FunctionCallbackInfo<Value>& args,
                            const char* syscall, enum encoding enc,
                            uv_fs_cb after, Func fn, Args... fn_args) {
  return AsyncDestCall(env, req_wrap, args,
                       syscall, nullptr, 0, enc,
                       after, fn, fn_args...);
}

// Template counterpart of SYNC_CALL, except that it only puts
// the error number and the syscall in the context instead of
// creating an error in the C++ land.
// ctx must be checked using value->IsObject() before being passed.
template <typename Func, typename... Args>
inline int SyncCall(Environment* env, v8::Local<v8::Value> ctx,
                    FSReqWrapSync* req_wrap, const char* syscall,
                    Func fn, Args... args) {
  env->PrintSyncTrace();
  int err = fn(env->event_loop(), &(req_wrap->req), args..., nullptr);
  if (err < 0) {
    v8::Local<Context> context = env->context();
    v8::Local<Object> ctx_obj = ctx.As<v8::Object>();
    v8::Isolate* isolate = env->isolate();
    ctx_obj->Set(context,
                 env->errno_string(),
                 v8::Integer::New(isolate, err)).Check();
    ctx_obj->Set(context,
                 env->syscall_string(),
                 OneByteString(isolate, syscall)).Check();
  }
  return err;
}

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
