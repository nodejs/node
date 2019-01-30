#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "stream_base.h"
#include "req_wrap-inl.h"

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

 private:
  enum encoding encoding_ = UTF8;
  bool has_data_ = false;
  const char* syscall_ = nullptr;
  bool use_bigint_ = false;

  // Typically, the content of buffer_ is something like a file name, so
  // something around 64 bytes should be enough.
  FSReqBuffer buffer_;

  DISALLOW_COPY_AND_ASSIGN(FSReqBase);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(FSReqCallback);
};

// Wordaround a GCC4.9 bug that C++14 N3652 was not implemented
// Refs: https://www.gnu.org/software/gcc/projects/cxx-status.html#cxx14
// Refs: https://isocpp.org/files/papers/N3652.html
#if __cpp_constexpr < 201304
#  define constexpr inline
#endif

template <typename NativeT,
          // SFINAE limit NativeT to arithmetic types
          typename = std::enable_if<std::is_arithmetic<NativeT>::value>>
constexpr NativeT ToNative(uv_timespec_t ts) {
  // This template has exactly two specializations below.
  static_assert(std::is_arithmetic<NativeT>::value == false, "Not implemented");
  UNREACHABLE();
}

template <>
constexpr double ToNative(uv_timespec_t ts) {
  // We need to do a static_cast since the original FS values are ulong.
  /* NOLINTNEXTLINE(runtime/int) */
  const auto u_sec = static_cast<unsigned long>(ts.tv_sec);
  const double full_sec = u_sec * 1000.0;
  /* NOLINTNEXTLINE(runtime/int) */
  const auto u_nsec = static_cast<unsigned long>(ts.tv_nsec);
  const double full_nsec = u_nsec / 1000'000.0;
  return full_sec + full_nsec;
}

template <>
constexpr uint64_t ToNative(uv_timespec_t ts) {
  // We need to do a static_cast since the original FS values are ulong.
  /* NOLINTNEXTLINE(runtime/int) */
  const auto u_sec = static_cast<unsigned long>(ts.tv_sec);
  const auto full_sec = static_cast<uint64_t>(u_sec) * 1000UL;
  /* NOLINTNEXTLINE(runtime/int) */
  const auto u_nsec = static_cast<unsigned long>(ts.tv_nsec);
  const auto full_nsec = static_cast<uint64_t>(u_nsec) / 1000'000UL;
  return full_sec + full_nsec;
}

#undef constexpr  // end N3652 bug workaround

template <typename NativeT, typename V8T>
constexpr void FillStatsArray(AliasedBuffer<NativeT, V8T>* fields,
                              const uv_stat_t* s, const size_t offset = 0) {
  fields->SetValue(offset + 0, s->st_dev);
  fields->SetValue(offset + 1, s->st_mode);
  fields->SetValue(offset + 2, s->st_nlink);
  fields->SetValue(offset + 3, s->st_uid);
  fields->SetValue(offset + 4, s->st_gid);
  fields->SetValue(offset + 5, s->st_rdev);
#if defined(__POSIX__)
  fields->SetValue(offset + 6, s->st_blksize);
#else
  fields->SetValue(offset + 6, 0);
#endif
  fields->SetValue(offset + 7, s->st_ino);
  fields->SetValue(offset + 8, s->st_size);
#if defined(__POSIX__)
  fields->SetValue(offset + 9, s->st_blocks);
#else
  fields->SetValue(offset + 9, 0);
#endif
// Dates.
  fields->SetValue(offset + 10, ToNative<NativeT>(s->st_atim));
  fields->SetValue(offset + 11, ToNative<NativeT>(s->st_mtim));
  fields->SetValue(offset + 12, ToNative<NativeT>(s->st_ctim));
  fields->SetValue(offset + 13, ToNative<NativeT>(s->st_birthtim));
}

inline Local<Value> FillGlobalStatsArray(Environment* env,
                                         const bool use_bigint,
                                         const uv_stat_t* s,
                                         const bool second = false) {
  const ptrdiff_t offset = second ? kFsStatsFieldsNumber : 0;
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

template <typename NativeT = double, typename V8T = v8::Float64Array>
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
        stats_field_array_(env->isolate(), kFsStatsFieldsNumber) {}

  bool finished_ = false;
  AliasedBuffer<NativeT, V8T> stats_field_array_;
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
  virtual ~FileHandle();

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
    Persistent<Promise> promise_{};
    Persistent<Value> ref_{};
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

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
