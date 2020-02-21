#ifndef SRC_NODE_WASI_H_
#define SRC_NODE_WASI_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object.h"
#include "node_mem.h"
#include "uvwasi.h"

namespace node {
namespace wasi {


class WASI : public BaseObject,
             public mem::NgLibMemoryManager<WASI, uvwasi_mem_t> {
 public:
  WASI(Environment* env,
       v8::Local<v8::Object> object,
       uvwasi_options_t* options);
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WASI)
  SET_SELF_SIZE(WASI)

  static void ArgsGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ArgsSizesGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClockResGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ClockTimeGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnvironGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnvironSizesGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdAdvise(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdAllocate(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdClose(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdDatasync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFdstatGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFdstatSetFlags(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFdstatSetRights(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFilestatGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFilestatSetSize(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdFilestatSetTimes(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdPread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdPrestatGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdPrestatDirName(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdPwrite(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdRead(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdReaddir(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdRenumber(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdSeek(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdSync(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdTell(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void FdWrite(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathCreateDirectory(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathFilestatGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathFilestatSetTimes(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathLink(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathOpen(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathReadlink(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathRemoveDirectory(
    const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathRename(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathSymlink(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PathUnlinkFile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void PollOneoff(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ProcExit(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ProcRaise(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void RandomGet(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SchedYield(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SockRecv(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SockSend(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SockShutdown(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void _SetMemory(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Implementation for mem::NgLibMemoryManager
  void CheckAllocatedSize(size_t previous_size) const;
  void IncreaseAllocatedSize(size_t size);
  void DecreaseAllocatedSize(size_t size);

 private:
  ~WASI() override;
  inline void readUInt8(char* memory, uint8_t* value, uint32_t offset);
  inline void readUInt16(char* memory, uint16_t* value, uint32_t offset);
  inline void readUInt32(char* memory, uint32_t* value, uint32_t offset);
  inline void readUInt64(char* memory, uint64_t* value, uint32_t offset);
  inline void writeUInt8(char* memory, uint8_t value, uint32_t offset);
  inline void writeUInt16(char* memory, uint16_t value, uint32_t offset);
  inline void writeUInt32(char* memory, uint32_t value, uint32_t offset);
  inline void writeUInt64(char* memory, uint64_t value, uint32_t offset);
  uvwasi_errno_t backingStore(char** store, size_t* byte_length);
  uvwasi_t uvw_;
  v8::Global<v8::Object> memory_;
  uvwasi_mem_t alloc_info_;
  size_t current_uvwasi_memory_ = 0;
};


}  // namespace wasi
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WASI_H_
