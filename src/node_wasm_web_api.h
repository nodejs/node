#ifndef SRC_NODE_WASM_WEB_API_H_
#define SRC_NODE_WASM_WEB_API_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "base_object-inl.h"
#include "v8.h"

namespace node {
namespace wasm_web_api {

// Wrapper for interacting with a v8::WasmStreaming instance from JavaScript.
class WasmStreamingObject final : public BaseObject {
 public:
  static v8::Local<v8::Function> Initialize(Environment* env);

  static void RegisterExternalReferences(ExternalReferenceRegistry* registry);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(WasmStreamingObject)
  SET_SELF_SIZE(WasmStreamingObject)

  static v8::MaybeLocal<v8::Object> Create(
      Environment* env, std::shared_ptr<v8::WasmStreaming> streaming);

 private:
  WasmStreamingObject(Environment* env, v8::Local<v8::Object> object)
      : BaseObject(env, object) {
    MakeWeak();
  }

  ~WasmStreamingObject() override {}

 private:
  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetURL(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Push(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Finish(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Abort(const v8::FunctionCallbackInfo<v8::Value>& args);

  std::shared_ptr<v8::WasmStreaming> streaming_;
  size_t wasm_size_ = 0;
};

// This is a v8::WasmStreamingCallback implementation that must be passed to
// v8::Isolate::SetWasmStreamingCallback when setting up the isolate in order to
// enable the WebAssembly.(compile|instantiate)Streaming APIs.
void StartStreamingCompilation(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace wasm_web_api
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_WASM_WEB_API_H_
