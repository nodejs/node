#ifndef SRC_QUIC_QLOG_H_
#define SRC_QUIC_QLOG_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "quic/quic.h"
#include "async_wrap-inl.h"
#include "base_object.h"
#include "env-inl.h"
#include "stream_base-inl.h"
#include <v8.h>

#include <vector>

namespace node {
namespace quic {

struct Chunk {
  uv_buf_t buf;
  ssize_t avail;
};

class LogStream final : public AsyncWrap, public StreamBase {
 public:
  inline static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env) {
    BindingState* state = env->GetBindingData<BindingState>(env->context());
    v8::Local<v8::FunctionTemplate> tmpl =
        state->logstream_constructor_template();
    if (tmpl.IsEmpty()) {
      tmpl = v8::FunctionTemplate::New(env->isolate());
      tmpl->Inherit(AsyncWrap::GetConstructorTemplate(env));
      tmpl->InstanceTemplate()->SetInternalFieldCount(
          StreamBase::kInternalFieldCount);
      tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "LogStream"));
      StreamBase::AddMethods(env, tmpl);
      state->set_logstream_constructor_template(tmpl);
    }
    return tmpl;
  }

  inline static BaseObjectPtr<LogStream> Create(Environment* env) {
    v8::Local<v8::Object> obj;
    if (!GetConstructorTemplate(env)
            ->InstanceTemplate()
            ->NewInstance(env->context()).ToLocal(&obj)) {
      return BaseObjectPtr<LogStream>();
    }
    return MakeDetachedBaseObject<LogStream>(env, obj);
  }

  inline LogStream(Environment* env, v8::Local<v8::Object> obj)
      : AsyncWrap(env, obj, AsyncWrap::PROVIDER_LOGSTREAM),
        StreamBase(env) {
    MakeWeak();
    StreamBase::AttachToObject(GetObject());
  }

  inline void Emit(const uint8_t* data, size_t len, uint32_t flags) {
    size_t remaining = len;
    while (remaining != 0) {
      uv_buf_t buf = EmitAlloc(len);
      ssize_t avail = std::min<size_t>(remaining, buf.len);
      memcpy(buf.base, data, avail);
      remaining -= avail;
      data += avail;
      if (reading_)
        EmitRead(avail, buf);
      else
        buffer_.emplace_back(Chunk{ buf, avail });
    }

    if (ended_ && flags & NGTCP2_QLOG_WRITE_FLAG_FIN)
      EmitRead(UV_EOF);
  }

  inline void Emit(const std::string& line, uint32_t flags = 0) {
    Emit(reinterpret_cast<const uint8_t*>(line.c_str()), line.length(), flags);
  }

  inline void End() { ended_ = true; }

  inline int ReadStart() override {
    reading_ = true;
    for (const Chunk& chunk : buffer_)
      EmitRead(chunk.avail, chunk.buf);
    return 0;
  }

  inline int ReadStop() override {
    reading_ = false;
    return 0;
  }

  inline int DoShutdown(ShutdownWrap* req_wrap) override {
    UNREACHABLE();
  }

  inline int DoWrite(
      WriteWrap* w,
      uv_buf_t* bufs,
      size_t count,
      uv_stream_t* send_handle) override {
    UNREACHABLE();
  }

  inline bool IsAlive() override { return !ended_; }
  inline bool IsClosing() override { return ended_; }
  inline AsyncWrap* GetAsyncWrap() override { return this; }

  SET_NO_MEMORY_INFO();
  SET_MEMORY_INFO_NAME(LogStream);
  SET_SELF_SIZE(LogStream);

 private:
  bool ended_ = false;
  bool reading_ = false;
  std::vector<Chunk> buffer_;
};

}  // namespace quic
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS
#endif  // SRC_QUIC_QLOG_H_
