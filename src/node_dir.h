#ifndef SRC_NODE_DIR_H_
#define SRC_NODE_DIR_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_file.h"
#include "node.h"
#include "req_wrap-inl.h"

namespace node {

namespace fs_dir {

// Needed to propagate `uv_dir_t`.
class DirHandle : public AsyncWrap {
 public:
  static constexpr int kDirHandleFieldCount = 1;

  static DirHandle* New(Environment* env, uv_dir_t* dir);
  ~DirHandle() override;

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Open(const v8::FunctionCallbackInfo<Value>& args);
  static void Read(const v8::FunctionCallbackInfo<Value>& args);
  static void Close(const v8::FunctionCallbackInfo<Value>& args);

  inline uv_dir_t* dir() { return dir_; }

  void MemoryInfo(MemoryTracker* tracker) const override {
    tracker->TrackFieldWithSize("dir", sizeof(*dir_));
  }

  SET_MEMORY_INFO_NAME(DirHandle)
  SET_SELF_SIZE(DirHandle)

  DirHandle(const DirHandle&) = delete;
  DirHandle& operator=(const DirHandle&) = delete;
  DirHandle(const DirHandle&&) = delete;
  DirHandle& operator=(const DirHandle&&) = delete;

 private:
  DirHandle(Environment* env, v8::Local<v8::Object> obj, uv_dir_t* dir);

  // Synchronous close that emits a warning
  void GCClose();

  uv_dir_t* dir_;
  // Up to 32 directory entries are read through a single libuv call.
  uv_dirent_t dirents_[32];
  bool closing_ = false;
  bool closed_ = false;
};

}  // namespace fs_dir

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_DIR_H_
