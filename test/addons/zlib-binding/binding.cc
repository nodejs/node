#include <node.h>
#include <node_buffer.h>
#include <assert.h>
#include <zlib.h>

namespace {

inline void CompressBytes(const v8::FunctionCallbackInfo<v8::Value>& info) {
  assert(info[0]->IsArrayBufferView());
  auto view = info[0].As<v8::ArrayBufferView>();
  auto byte_offset = view->ByteOffset();
  auto byte_length = view->ByteLength();
  assert(view->HasBuffer());
  auto buffer = view->Buffer();
  auto contents = buffer->GetContents();
  auto data = static_cast<unsigned char*>(contents.Data()) + byte_offset;

  Bytef buf[1024];

  z_stream stream;
  stream.zalloc = nullptr;
  stream.zfree = nullptr;

  int err = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
                         -15, MAX_MEM_LEVEL, Z_DEFAULT_STRATEGY);
  assert(err == Z_OK);

  stream.avail_in = byte_length;
  stream.next_in = data;
  stream.avail_out = sizeof(buf);
  stream.next_out = buf;
  err = deflate(&stream, Z_FINISH);
  assert(err == Z_STREAM_END);

  auto result = node::Buffer::Copy(info.GetIsolate(),
                                   reinterpret_cast<const char*>(buf),
                                   sizeof(buf) - stream.avail_out);

  deflateEnd(&stream);

  info.GetReturnValue().Set(result.ToLocalChecked());
}

inline void Initialize(v8::Local<v8::Object> exports,
                       v8::Local<v8::Value> module,
                       v8::Local<v8::Context> context) {
  auto isolate = context->GetIsolate();
  auto key = v8::String::NewFromUtf8(
      isolate, "compressBytes", v8::NewStringType::kNormal).ToLocalChecked();
  auto value = v8::FunctionTemplate::New(isolate, CompressBytes)
                   ->GetFunction(context)
                   .ToLocalChecked();
  assert(exports->Set(context, key, value).IsJust());
}

}  // anonymous namespace

NODE_MODULE_CONTEXT_AWARE(NODE_GYP_MODULE_NAME, Initialize)
