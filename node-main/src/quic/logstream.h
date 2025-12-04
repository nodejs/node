#pragma once

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <async_wrap.h>
#include <base_object.h>
#include <env.h>
#include <stream_base.h>
#include <list>
#include "defs.h"

namespace node::quic {

// The LogStream is a utility that the QUIC impl uses to publish both QLog
// and Keylog diagnostic data (one instance for each).
class LogStream final : public AsyncWrap, public StreamBase {
 public:
  JS_CONSTRUCTOR(LogStream);

  static BaseObjectPtr<LogStream> Create(Environment* env);

  LogStream(Environment* env, v8::Local<v8::Object> obj);

  enum class EmitOption : uint8_t {
    NONE,
    FIN,
  };

  void Emit(const uint8_t* data,
            size_t len,
            EmitOption option = EmitOption::NONE);

  void Emit(const std::string_view line, EmitOption option = EmitOption::NONE);

  void End();

  int ReadStart() override;

  int ReadStop() override;

  // We do not use either of these.
  int DoShutdown(ShutdownWrap* req_wrap) override;
  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override;

  bool IsAlive() override;
  bool IsClosing() override;
  AsyncWrap* GetAsyncWrap() override;

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(LogStream)
  SET_SELF_SIZE(LogStream)

 private:
  struct Chunk {
    // len will be <= buf.len
    size_t len;
    uv_buf_t buf;
  };
  size_t total_ = 0;
  std::list<Chunk> buffer_;
  bool fin_seen_ = false;
  bool ended_ = false;
  bool reading_ = false;

  // The value here is fairly arbitrary. Once we get everything
  // fully implemented and start working with this, we might
  // tune this number further.
  static constexpr size_t kMaxLogStreamBuffer = 1024 * 10;

  // The LogStream buffer enforces a maximum size of kMaxLogStreamBuffer.
  void ensure_space(size_t amt);
};

}  // namespace node::quic

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
