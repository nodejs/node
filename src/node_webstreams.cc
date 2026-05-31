#include "env-inl.h"
#include "node_binding.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "util-inl.h"
#include "v8.h"

namespace node {
namespace webstreams {

using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::Name;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::Uint32;
using v8::Uint8Array;
using v8::Value;

// Returns { buffer, byteOffset, byteLength } in a single binding crossing,
// equivalent to reading the three properties via
// Reflect.get(view.constructor.prototype, ..., view). Uses the V8 API
// directly so it is immune to prototype tampering and avoids the JS-side
// overhead of the defensive accessors in lib/internal/.
void GetArrayBufferView(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  CHECK(args[0]->IsArrayBufferView());
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  Local<Name> names[] = {
      FIXED_ONE_BYTE_STRING(isolate, "buffer"),
      FIXED_ONE_BYTE_STRING(isolate, "byteOffset"),
      FIXED_ONE_BYTE_STRING(isolate, "byteLength"),
  };
  Local<Value> values[] = {
      view->Buffer(),
      Number::New(isolate, static_cast<double>(view->ByteOffset())),
      Number::New(isolate, static_cast<double>(view->ByteLength())),
  };
  args.GetReturnValue().Set(
      Object::New(isolate, Null(isolate), names, values, arraysize(names)));
}

// Returns true iff bytes can be safely copied between the buffers given the
// requested offsets and count. Matches lib/internal/webstreams/util.js:
//   toBuffer !== fromBuffer &&
//   !toBuffer.detached &&
//   !fromBuffer.detached &&
//   toIndex + count <= toBuffer.byteLength &&
//   fromIndex + count <= fromBuffer.byteLength
void CanCopyArrayBuffer(const FunctionCallbackInfo<Value>& args) {
  CHECK(args[0]->IsArrayBuffer() || args[0]->IsSharedArrayBuffer());
  CHECK(args[1]->IsUint32());
  CHECK(args[2]->IsArrayBuffer() || args[2]->IsSharedArrayBuffer());
  CHECK(args[3]->IsUint32());
  CHECK(args[4]->IsUint32());

  // SharedArrayBuffer handles are interoperable with ArrayBuffer handles in
  // V8, so we can use the ArrayBuffer accessors uniformly. WasDetached()
  // always returns false on a SAB.
  Local<ArrayBuffer> to_buffer = args[0].As<ArrayBuffer>();
  Local<ArrayBuffer> from_buffer = args[2].As<ArrayBuffer>();

  if (to_buffer->StrictEquals(from_buffer)) {
    args.GetReturnValue().Set(false);
    return;
  }
  if (to_buffer->WasDetached() || from_buffer->WasDetached()) {
    args.GetReturnValue().Set(false);
    return;
  }

  uint32_t to_index = args[1].As<Uint32>()->Value();
  uint32_t from_index = args[3].As<Uint32>()->Value();
  uint32_t count = args[4].As<Uint32>()->Value();

  size_t to_byte_length = to_buffer->ByteLength();
  size_t from_byte_length = from_buffer->ByteLength();

  bool ok = static_cast<uint64_t>(to_index) + count <= to_byte_length &&
            static_cast<uint64_t>(from_index) + count <= from_byte_length;
  args.GetReturnValue().Set(ok);
}

// Equivalent to:
//   new Uint8Array(view.buffer.slice(view.byteOffset,
//                                    view.byteOffset + view.byteLength))
// Allocates a fresh ArrayBuffer with the view's bytes copied into it, then
// returns a Uint8Array over the full new buffer. Avoids the JS-side
// Reflect.get + slice round-trip.
void CloneAsUint8Array(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();
  CHECK(args[0]->IsArrayBufferView());
  Local<ArrayBufferView> view = args[0].As<ArrayBufferView>();
  size_t byte_length = view->ByteLength();
  Local<ArrayBuffer> new_buffer;
  if (!ArrayBuffer::MaybeNew(isolate, byte_length).ToLocal(&new_buffer)) {
    // MaybeNew does not schedule an exception on allocation failure.
    env->ThrowRangeError("Array buffer allocation failed");
    return;
  }
  if (byte_length > 0) {
    size_t copied = view->CopyContents(new_buffer->Data(), byte_length);
    CHECK_EQ(copied, byte_length);
  }
  args.GetReturnValue().Set(Uint8Array::New(new_buffer, 0, byte_length));
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  SetMethod(context, target, "getArrayBufferView", GetArrayBufferView);
  SetMethod(context, target, "canCopyArrayBuffer", CanCopyArrayBuffer);
  SetMethod(context, target, "cloneAsUint8Array", CloneAsUint8Array);
}

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(GetArrayBufferView);
  registry->Register(CanCopyArrayBuffer);
  registry->Register(CloneAsUint8Array);
}

}  // namespace webstreams
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(webstreams, node::webstreams::Initialize)
NODE_BINDING_EXTERNAL_REFERENCE(webstreams,
                                node::webstreams::RegisterExternalReferences)
