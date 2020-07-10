#ifndef SRC_NODE_FILE_H_
#define SRC_NODE_FILE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "aliased_buffer.h"
#include "node_messaging.h"
#include "stream_base.h"
#include <iostream>

namespace node {
namespace fs {

class FileHandleReadWrap;

class BindingData : public BaseObject {
 public:
  explicit BindingData(Environment* env, v8::Local<v8::Object> wrap)
      : BaseObject(env, wrap),
        stats_field_array(env->isolate(), kFsStatsBufferLength),
        stats_field_bigint_array(env->isolate(), kFsStatsBufferLength) {}

  AliasedFloat64Array stats_field_array;
  AliasedBigUint64Array stats_field_bigint_array;

  std::vector<BaseObjectPtr<FileHandleReadWrap>>
      file_handle_read_wrap_freelist;

  static constexpr FastStringKey binding_data_name { "fs" };

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_SELF_SIZE(BindingData)
  SET_MEMORY_INFO_NAME(BindingData)
};

// structure used to store state during a complex operation, e.g., mkdirp.
class FSContinuationData : public MemoryRetainer {
 public:
  inline FSContinuationData(uv_fs_t* req, int mode, uv_fs_cb done_cb);

  inline void PushPath(std::string&& path);
  inline void PushPath(const std::string& path);
  inline std::string PopPath();
  // Used by mkdirp to track the first path created:
  inline void MaybeSetFirstPath(const std::string& path);
  inline void Done(int result);

  int mode() const { return mode_; }
  const std::vector<std::string>& paths() const { return paths_; }
  const std::string& first_path() const { return first_path_; }

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(FSContinuationData)
  SET_SELF_SIZE(FSContinuationData)

 private:
  uv_fs_cb done_cb_;
  uv_fs_t* req_;
  int mode_;
  std::vector<std::string> paths_;
  std::string first_path_;
};

class FSReqBase : public ReqWrap<uv_fs_t> {
 public:
  typedef MaybeStackBuffer<char, 64> FSReqBuffer;

  inline FSReqBase(BindingData* binding_data,
                   v8::Local<v8::Object> req,
                   AsyncWrap::ProviderType type,
                   bool use_bigint);
  ~FSReqBase() override;

  inline void Init(const char* syscall,
                   const char* data,
                   size_t len,
                   enum encoding encoding);
  inline FSReqBuffer& Init(const char* syscall, size_t len,
                           enum encoding encoding);

  virtual void Reject(v8::Local<v8::Value> reject) = 0;
  virtual void Resolve(v8::Local<v8::Value> value) = 0;
  virtual void ResolveStat(const uv_stat_t* stat) = 0;
  virtual void SetReturnValue(
      const v8::FunctionCallbackInfo<v8::Value>& args) = 0;

  const char* syscall() const { return syscall_; }
  const char* data() const { return has_data_ ? *buffer_ : nullptr; }
  enum encoding encoding() const { return encoding_; }
  bool use_bigint() const { return use_bigint_; }
  bool is_plain_open() const { return is_plain_open_; }

  void set_is_plain_open(bool value) { is_plain_open_ = value; }

  FSContinuationData* continuation_data() const {
    return continuation_data_.get();
  }
  void set_continuation_data(std::unique_ptr<FSContinuationData> data) {
    continuation_data_ = std::move(data);
  }

  static FSReqBase* from_req(uv_fs_t* req) {
    return static_cast<FSReqBase*>(ReqWrap::from_req(req));
  }

  FSReqBase(const FSReqBase&) = delete;
  FSReqBase& operator=(const FSReqBase&) = delete;

  void MemoryInfo(MemoryTracker* tracker) const override;

  BindingData* binding_data() { return binding_data_.get(); }

 private:
  std::unique_ptr<FSContinuationData> continuation_data_;
  enum encoding encoding_ = UTF8;
  bool has_data_ = false;
  bool use_bigint_ = false;
  bool is_plain_open_ = false;
  const char* syscall_ = nullptr;

  BaseObjectPtr<BindingData> binding_data_;

  // Typically, the content of buffer_ is something like a file name, so
  // something around 64 bytes should be enough.
  FSReqBuffer buffer_;
};

class FSReqCallback final : public FSReqBase {
 public:
  inline FSReqCallback(BindingData* binding_data,
                       v8::Local<v8::Object> req,
                       bool use_bigint);

  void Reject(v8::Local<v8::Value> reject) override;
  void Resolve(v8::Local<v8::Value> value) override;
  void ResolveStat(const uv_stat_t* stat) override;
  void SetReturnValue(const v8::FunctionCallbackInfo<v8::Value>& args) override;

  SET_MEMORY_INFO_NAME(FSReqCallback)
  SET_SELF_SIZE(FSReqCallback)

  FSReqCallback(const FSReqCallback&) = delete;
  FSReqCallback& operator=(const FSReqCallback&) = delete;
};

template <typename NativeT, typename V8T>
void FillStatsArray(AliasedBufferBase<NativeT, V8T>* fields,
                    const uv_stat_t* s,
                    const size_t offset = 0);

inline v8::Local<v8::Value> FillGlobalStatsArray(BindingData* binding_data,
                                                 const bool use_bigint,
                                                 const uv_stat_t* s,
                                                 const bool second = false);

template <typename AliasedBufferT>
class FSReqPromise final : public FSReqBase {
 public:
  static inline FSReqPromise* New(BindingData* binding_data,
                                  bool use_bigint);
  inline ~FSReqPromise() override;

  inline void Reject(v8::Local<v8::Value> reject) override;
  inline void Resolve(v8::Local<v8::Value> value) override;
  inline void ResolveStat(const uv_stat_t* stat) override;
  inline void SetReturnValue(
      const v8::FunctionCallbackInfo<v8::Value>& args) override;
  inline void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(FSReqPromise)
  SET_SELF_SIZE(FSReqPromise)

  FSReqPromise(const FSReqPromise&) = delete;
  FSReqPromise& operator=(const FSReqPromise&) = delete;
  FSReqPromise(const FSReqPromise&&) = delete;
  FSReqPromise& operator=(const FSReqPromise&&) = delete;

 private:
  inline FSReqPromise(BindingData* binding_data,
                      v8::Local<v8::Object> obj,
                      bool use_bigint);

  bool finished_ = false;
  AliasedBufferT stats_field_array_;
};

class FSReqAfterScope final {
 public:
  FSReqAfterScope(FSReqBase* wrap, uv_fs_t* req);
  ~FSReqAfterScope();
  void Clear();

  bool Proceed();

  void Reject(uv_fs_t* req);

  FSReqAfterScope(const FSReqAfterScope&) = delete;
  FSReqAfterScope& operator=(const FSReqAfterScope&) = delete;
  FSReqAfterScope(const FSReqAfterScope&&) = delete;
  FSReqAfterScope& operator=(const FSReqAfterScope&&) = delete;

 private:
  BaseObjectPtr<FSReqBase> wrap_;
  uv_fs_t* req_ = nullptr;
  v8::HandleScope handle_scope_;
  v8::Context::Scope context_scope_;
};

class FileHandle;

// A request wrap specifically for uv_fs_read()s scheduled for reading
// from a FileHandle.
class FileHandleReadWrap final : public ReqWrap<uv_fs_t> {
 public:
  FileHandleReadWrap(FileHandle* handle, v8::Local<v8::Object> obj);
  ~FileHandleReadWrap() override;

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
class FileHandle final : public AsyncWrap, public StreamBase {
 public:
  static FileHandle* New(BindingData* binding_data,
                         int fd,
                         v8::Local<v8::Object> obj = v8::Local<v8::Object>());
  ~FileHandle() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  int GetFD() override { return fd_; }

  // Will asynchronously close the FD and return a Promise that will
  // be resolved once closing is complete.
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Releases ownership of the FD.
  static void ReleaseFD(const v8::FunctionCallbackInfo<v8::Value>& args);

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
              uv_stream_t* send_handle) override;

  void MemoryInfo(MemoryTracker* tracker) const override;

  SET_MEMORY_INFO_NAME(FileHandle)
  SET_SELF_SIZE(FileHandle)

  FileHandle(const FileHandle&) = delete;
  FileHandle& operator=(const FileHandle&) = delete;
  FileHandle(const FileHandle&&) = delete;
  FileHandle& operator=(const FileHandle&&) = delete;

  TransferMode GetTransferMode() const override;
  std::unique_ptr<worker::TransferData> TransferForMessaging() override;

 private:
  class TransferData : public worker::TransferData {
   public:
    explicit TransferData(int fd);
    ~TransferData();

    BaseObjectPtr<BaseObject> Deserialize(
        Environment* env,
        v8::Local<v8::Context> context,
        std::unique_ptr<worker::TransferData> self) override;

    SET_NO_MEMORY_INFO()
    SET_MEMORY_INFO_NAME(FileHandleTransferData)
    SET_SELF_SIZE(TransferData)

   private:
    int fd_;
  };

  FileHandle(BindingData* binding_data, v8::Local<v8::Object> obj, int fd);

  // Synchronous close that emits a warning
  void Close();
  void AfterClose();

  class CloseReq final : public ReqWrap<uv_fs_t> {
   public:
    CloseReq(Environment* env,
             v8::Local<v8::Object> obj,
             v8::Local<v8::Promise> promise,
             v8::Local<v8::Value> ref);
    ~CloseReq() override;

    FileHandle* file_handle();

    void MemoryInfo(MemoryTracker* tracker) const override;

    SET_MEMORY_INFO_NAME(CloseReq)
    SET_SELF_SIZE(CloseReq)

    void Resolve();

    void Reject(v8::Local<v8::Value> reason);

    static CloseReq* from_req(uv_fs_t* req) {
      return static_cast<CloseReq*>(ReqWrap::from_req(req));
    }

    CloseReq(const CloseReq&) = delete;
    CloseReq& operator=(const CloseReq&) = delete;
    CloseReq(const CloseReq&&) = delete;
    CloseReq& operator=(const CloseReq&&) = delete;

   private:
    v8::Global<v8::Promise> promise_{};
    v8::Global<v8::Value> ref_{};
  };

  // Asynchronous close
  v8::MaybeLocal<v8::Promise> ClosePromise();

  int fd_;
  bool closing_ = false;
  bool closed_ = false;
  bool reading_ = false;
  int64_t read_offset_ = -1;
  int64_t read_length_ = -1;

  BaseObjectPtr<FileHandleReadWrap> current_read_;

  BaseObjectPtr<BindingData> binding_data_;
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

  FSContinuationData* continuation_data() const {
    return continuation_data_.get();
  }
  void set_continuation_data(std::unique_ptr<FSContinuationData> data) {
    continuation_data_ = std::move(data);
  }

  FSReqWrapSync(const FSReqWrapSync&) = delete;
  FSReqWrapSync& operator=(const FSReqWrapSync&) = delete;

 private:
  std::unique_ptr<FSContinuationData> continuation_data_;
};

// TODO(addaleax): Currently, callers check the return value and assume
// that nullptr indicates a synchronous call, rather than a failure.
// Failure conditions should be disambiguated and handled appropriately.
inline FSReqBase* GetReqWrap(const v8::FunctionCallbackInfo<v8::Value>& args,
                             int index,
                             bool use_bigint = false);

// Returns nullptr if the operation fails from the start.
template <typename Func, typename... Args>
inline FSReqBase* AsyncDestCall(Environment* env, FSReqBase* req_wrap,
                                const v8::FunctionCallbackInfo<v8::Value>& args,
                                const char* syscall, const char* dest,
                                size_t len, enum encoding enc, uv_fs_cb after,
                                Func fn, Args... fn_args);

// Returns nullptr if the operation fails from the start.
template <typename Func, typename... Args>
inline FSReqBase* AsyncCall(Environment* env,
                            FSReqBase* req_wrap,
                            const v8::FunctionCallbackInfo<v8::Value>& args,
                            const char* syscall, enum encoding enc,
                            uv_fs_cb after, Func fn, Args... fn_args);

// Template counterpart of SYNC_CALL, except that it only puts
// the error number and the syscall in the context instead of
// creating an error in the C++ land.
// ctx must be checked using value->IsObject() before being passed.
template <typename Func, typename... Args>
inline int SyncCall(Environment* env, v8::Local<v8::Value> ctx,
                    FSReqWrapSync* req_wrap, const char* syscall,
                    Func fn, Args... args);

}  // namespace fs

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_FILE_H_
