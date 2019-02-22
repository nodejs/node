#include "stream_pipe.h"
#include "stream_base-inl.h"
#include "node_buffer.h"

using v8::Context;
using v8::External;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace node {

StreamPipe::StreamPipe(StreamBase* source,
                       StreamBase* sink,
                       Local<Object> obj)
    : AsyncWrap(source->stream_env(), obj, AsyncWrap::PROVIDER_STREAMPIPE) {
  MakeWeak();

  CHECK_NOT_NULL(sink);
  CHECK_NOT_NULL(source);

  source->PushStreamListener(&readable_listener_);
  sink->PushStreamListener(&writable_listener_);

  CHECK(sink->HasWantsWrite());

  // Set up links between this object and the source/sink objects.
  // In particular, this makes sure that they are garbage collected as a group,
  // if that applies to the given streams (for example, Http2Streams use
  // weak references).
  obj->Set(env()->context(), env()->source_string(), source->GetObject())
      .FromJust();
  source->GetObject()->Set(env()->context(), env()->pipe_target_string(), obj)
      .FromJust();
  obj->Set(env()->context(), env()->sink_string(), sink->GetObject())
      .FromJust();
  sink->GetObject()->Set(env()->context(), env()->pipe_source_string(), obj)
      .FromJust();
}

StreamPipe::~StreamPipe() {
  CHECK(is_closed_);
}

StreamBase* StreamPipe::source() {
  return static_cast<StreamBase*>(readable_listener_.stream());
}

StreamBase* StreamPipe::sink() {
  return static_cast<StreamBase*>(writable_listener_.stream());
}

void StreamPipe::Unpipe() {
  if (is_closed_)
    return;

  // Note that we possibly cannot use virtual methods on `source` and `sink`
  // here, because this function can be called from their destructors via
  // `OnStreamDestroy()`.
  if (!source_destroyed_)
    source()->ReadStop();

  is_closed_ = true;
  is_reading_ = false;
  source()->RemoveStreamListener(&readable_listener_);
  sink()->RemoveStreamListener(&writable_listener_);

  // Delay the JS-facing part with SetImmediate, because this might be from
  // inside the garbage collector, so we can’t run JS here.
  HandleScope handle_scope(env()->isolate());
  env()->SetImmediate([](Environment* env, void* data) {
    StreamPipe* pipe = static_cast<StreamPipe*>(data);

    HandleScope handle_scope(env->isolate());
    Context::Scope context_scope(env->context());
    Local<Object> object = pipe->object();

    Local<Value> onunpipe;
    if (!object->Get(env->context(), env->onunpipe_string()).ToLocal(&onunpipe))
      return;
    if (onunpipe->IsFunction() &&
        pipe->MakeCallback(onunpipe.As<Function>(), 0, nullptr).IsEmpty()) {
      return;
    }

    // Set all the links established in the constructor to `null`.
    Local<Value> null = Null(env->isolate());

    Local<Value> source_v;
    Local<Value> sink_v;
    if (!object->Get(env->context(), env->source_string()).ToLocal(&source_v) ||
        !object->Get(env->context(), env->sink_string()).ToLocal(&sink_v) ||
        !source_v->IsObject() || !sink_v->IsObject()) {
      return;
    }

    if (object->Set(env->context(), env->source_string(), null).IsNothing() ||
        object->Set(env->context(), env->sink_string(), null).IsNothing() ||
        source_v.As<Object>()
            ->Set(env->context(), env->pipe_target_string(), null)
            .IsNothing() ||
        sink_v.As<Object>()
            ->Set(env->context(), env->pipe_source_string(), null)
            .IsNothing()) {
      return;
    }
  }, static_cast<void*>(this), object());
}

uv_buf_t StreamPipe::ReadableListener::OnStreamAlloc(size_t suggested_size) {
  StreamPipe* pipe = ContainerOf(&StreamPipe::readable_listener_, this);
  size_t size = std::min(suggested_size, pipe->wanted_data_);
  CHECK_GT(size, 0);
  return uv_buf_init(Malloc(size), size);
}

void StreamPipe::ReadableListener::OnStreamRead(ssize_t nread,
                                                const uv_buf_t& buf) {
  StreamPipe* pipe = ContainerOf(&StreamPipe::readable_listener_, this);
  AsyncScope async_scope(pipe);
  if (nread < 0) {
    // EOF or error; stop reading and pass the error to the previous listener
    // (which might end up in JS).
    free(buf.base);
    pipe->is_eof_ = true;
    stream()->ReadStop();
    CHECK_NOT_NULL(previous_listener_);
    previous_listener_->OnStreamRead(nread, uv_buf_init(nullptr, 0));
    // If we’re not writing, close now. Otherwise, we’ll do that in
    // `OnStreamAfterWrite()`.
    if (!pipe->is_writing_) {
      pipe->ShutdownWritable();
      pipe->Unpipe();
    }
    return;
  }

  pipe->ProcessData(nread, buf);
}

void StreamPipe::ProcessData(size_t nread, const uv_buf_t& buf) {
  uv_buf_t buffer = uv_buf_init(buf.base, nread);
  StreamWriteResult res = sink()->Write(&buffer, 1);
  if (!res.async) {
    free(buf.base);
    writable_listener_.OnStreamAfterWrite(nullptr, res.err);
  } else {
    is_writing_ = true;
    is_reading_ = false;
    res.wrap->SetAllocatedStorage(buf.base, buf.len);
    if (source() != nullptr)
      source()->ReadStop();
  }
}

void StreamPipe::ShutdownWritable() {
  sink()->Shutdown();
}

void StreamPipe::WritableListener::OnStreamAfterWrite(WriteWrap* w,
                                                      int status) {
  StreamPipe* pipe = ContainerOf(&StreamPipe::writable_listener_, this);
  pipe->is_writing_ = false;
  if (pipe->is_eof_) {
    AsyncScope async_scope(pipe);
    pipe->ShutdownWritable();
    pipe->Unpipe();
    return;
  }

  if (status != 0) {
    CHECK_NOT_NULL(previous_listener_);
    StreamListener* prev = previous_listener_;
    pipe->Unpipe();
    prev->OnStreamAfterWrite(w, status);
    return;
  }
}

void StreamPipe::WritableListener::OnStreamAfterShutdown(ShutdownWrap* w,
                                                         int status) {
  StreamPipe* pipe = ContainerOf(&StreamPipe::writable_listener_, this);
  CHECK_NOT_NULL(previous_listener_);
  StreamListener* prev = previous_listener_;
  pipe->Unpipe();
  prev->OnStreamAfterShutdown(w, status);
}

void StreamPipe::ReadableListener::OnStreamDestroy() {
  StreamPipe* pipe = ContainerOf(&StreamPipe::readable_listener_, this);
  pipe->source_destroyed_ = true;
  if (!pipe->is_eof_) {
    OnStreamRead(UV_EPIPE, uv_buf_init(nullptr, 0));
  }
}

void StreamPipe::WritableListener::OnStreamDestroy() {
  StreamPipe* pipe = ContainerOf(&StreamPipe::writable_listener_, this);
  pipe->sink_destroyed_ = true;
  pipe->is_eof_ = true;
  pipe->Unpipe();
}

void StreamPipe::WritableListener::OnStreamWantsWrite(size_t suggested_size) {
  StreamPipe* pipe = ContainerOf(&StreamPipe::writable_listener_, this);
  pipe->wanted_data_ = suggested_size;
  if (pipe->is_reading_ || pipe->is_closed_)
    return;
  AsyncScope async_scope(pipe);
  pipe->is_reading_ = true;
  pipe->source()->ReadStart();
}

uv_buf_t StreamPipe::WritableListener::OnStreamAlloc(size_t suggested_size) {
  CHECK_NOT_NULL(previous_listener_);
  return previous_listener_->OnStreamAlloc(suggested_size);
}

void StreamPipe::WritableListener::OnStreamRead(ssize_t nread,
                                                const uv_buf_t& buf) {
  CHECK_NOT_NULL(previous_listener_);
  return previous_listener_->OnStreamRead(nread, buf);
}

void StreamPipe::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsExternal());
  CHECK(args[1]->IsExternal());
  auto source = static_cast<StreamBase*>(args[0].As<External>()->Value());
  auto sink = static_cast<StreamBase*>(args[1].As<External>()->Value());

  new StreamPipe(source, sink, args.This());
}

void StreamPipe::Start(const FunctionCallbackInfo<Value>& args) {
  StreamPipe* pipe;
  ASSIGN_OR_RETURN_UNWRAP(&pipe, args.Holder());
  pipe->is_closed_ = false;
  if (pipe->wanted_data_ > 0)
    pipe->writable_listener_.OnStreamWantsWrite(pipe->wanted_data_);
}

void StreamPipe::Unpipe(const FunctionCallbackInfo<Value>& args) {
  StreamPipe* pipe;
  ASSIGN_OR_RETURN_UNWRAP(&pipe, args.Holder());
  pipe->Unpipe();
}

namespace {

void InitializeStreamPipe(Local<Object> target,
                          Local<Value> unused,
                          Local<Context> context,
                          void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Create FunctionTemplate for FileHandle::CloseReq
  Local<FunctionTemplate> pipe = env->NewFunctionTemplate(StreamPipe::New);
  Local<String> stream_pipe_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "StreamPipe");
  env->SetProtoMethod(pipe, "unpipe", StreamPipe::Unpipe);
  env->SetProtoMethod(pipe, "start", StreamPipe::Start);
  pipe->Inherit(AsyncWrap::GetConstructorTemplate(env));
  pipe->SetClassName(stream_pipe_string);
  pipe->InstanceTemplate()->SetInternalFieldCount(1);
  target
      ->Set(context, stream_pipe_string,
            pipe->GetFunction(context).ToLocalChecked())
      .FromJust();
}

}  // anonymous namespace

}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(stream_pipe,
                                   node::InitializeStreamPipe)
