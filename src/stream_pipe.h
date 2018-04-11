#ifndef SRC_STREAM_PIPE_H_
#define SRC_STREAM_PIPE_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "stream_base.h"

namespace node {

class StreamPipe : public AsyncWrap {
 public:
  StreamPipe(StreamBase* source, StreamBase* sink, v8::Local<v8::Object> obj);
  ~StreamPipe();

  void Unpipe();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unpipe(const v8::FunctionCallbackInfo<v8::Value>& args);

  size_t self_size() const override { return sizeof(*this); }

 private:
  StreamBase* source();
  StreamBase* sink();

  void ShutdownWritable();
  void FlushToWritable();

  bool is_reading_ = false;
  bool is_writing_ = false;
  bool is_eof_ = false;
  bool is_closed_ = true;

  // Set a default value so that when we’re coming from Start(), we know
  // that we don’t want to read just yet.
  // This will likely need to be changed when supporting streams without
  // `OnStreamWantsWrite()` support.
  size_t wanted_data_ = 0;

  void ProcessData(size_t nread, const uv_buf_t& buf);

  class ReadableListener : public StreamListener {
   public:
    uv_buf_t OnStreamAlloc(size_t suggested_size) override;
    void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
    void OnStreamDestroy() override;
  };

  class WritableListener : public StreamListener {
   public:
    uv_buf_t OnStreamAlloc(size_t suggested_size) override;
    void OnStreamRead(ssize_t nread, const uv_buf_t& buf) override;
    void OnStreamAfterWrite(WriteWrap* w, int status) override;
    void OnStreamAfterShutdown(ShutdownWrap* w, int status) override;
    void OnStreamWantsWrite(size_t suggested_size) override;
    void OnStreamDestroy() override;
  };

  ReadableListener readable_listener_;
  WritableListener writable_listener_;
};

}  // namespace node

#endif

#endif  // SRC_STREAM_PIPE_H_
