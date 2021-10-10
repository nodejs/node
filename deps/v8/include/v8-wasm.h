// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_WASM_H_
#define INCLUDE_V8_WASM_H_

#include <memory>
#include <string>

#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-memory-span.h"   // NOLINT(build/include_directory)
#include "v8-object.h"        // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class ArrayBuffer;
class Promise;

namespace internal {
namespace wasm {
class NativeModule;
class StreamingDecoder;
}  // namespace wasm
}  // namespace internal

/**
 * An owned byte buffer with associated size.
 */
struct OwnedBuffer {
  std::unique_ptr<const uint8_t[]> buffer;
  size_t size = 0;
  OwnedBuffer(std::unique_ptr<const uint8_t[]> buffer, size_t size)
      : buffer(std::move(buffer)), size(size) {}
  OwnedBuffer() = default;
};

// Wrapper around a compiled WebAssembly module, which is potentially shared by
// different WasmModuleObjects.
class V8_EXPORT CompiledWasmModule {
 public:
  /**
   * Serialize the compiled module. The serialized data does not include the
   * wire bytes.
   */
  OwnedBuffer Serialize();

  /**
   * Get the (wasm-encoded) wire bytes that were used to compile this module.
   */
  MemorySpan<const uint8_t> GetWireBytesRef();

  const std::string& source_url() const { return source_url_; }

 private:
  friend class WasmModuleObject;
  friend class WasmStreaming;

  explicit CompiledWasmModule(std::shared_ptr<internal::wasm::NativeModule>,
                              const char* source_url, size_t url_length);

  const std::shared_ptr<internal::wasm::NativeModule> native_module_;
  const std::string source_url_;
};

// An instance of WebAssembly.Memory.
class V8_EXPORT WasmMemoryObject : public Object {
 public:
  WasmMemoryObject() = delete;

  /**
   * Returns underlying ArrayBuffer.
   */
  Local<ArrayBuffer> Buffer();

  V8_INLINE static WasmMemoryObject* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<WasmMemoryObject*>(value);
  }

 private:
  static void CheckCast(Value* object);
};

// An instance of WebAssembly.Module.
class V8_EXPORT WasmModuleObject : public Object {
 public:
  WasmModuleObject() = delete;

  /**
   * Efficiently re-create a WasmModuleObject, without recompiling, from
   * a CompiledWasmModule.
   */
  static MaybeLocal<WasmModuleObject> FromCompiledModule(
      Isolate* isolate, const CompiledWasmModule&);

  /**
   * Get the compiled module for this module object. The compiled module can be
   * shared by several module objects.
   */
  CompiledWasmModule GetCompiledModule();

  V8_INLINE static WasmModuleObject* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<WasmModuleObject*>(value);
  }

 private:
  static void CheckCast(Value* obj);
};

/**
 * The V8 interface for WebAssembly streaming compilation. When streaming
 * compilation is initiated, V8 passes a {WasmStreaming} object to the embedder
 * such that the embedder can pass the input bytes for streaming compilation to
 * V8.
 */
class V8_EXPORT WasmStreaming final {
 public:
  class WasmStreamingImpl;

  /**
   * Client to receive streaming event notifications.
   */
  class Client {
   public:
    virtual ~Client() = default;
    /**
     * Passes the fully compiled module to the client. This can be used to
     * implement code caching.
     */
    virtual void OnModuleCompiled(CompiledWasmModule compiled_module) = 0;
  };

  explicit WasmStreaming(std::unique_ptr<WasmStreamingImpl> impl);

  ~WasmStreaming();

  /**
   * Pass a new chunk of bytes to WebAssembly streaming compilation.
   * The buffer passed into {OnBytesReceived} is owned by the caller.
   */
  void OnBytesReceived(const uint8_t* bytes, size_t size);

  /**
   * {Finish} should be called after all received bytes where passed to
   * {OnBytesReceived} to tell V8 that there will be no more bytes. {Finish}
   * does not have to be called after {Abort} has been called already.
   */
  void Finish();

  /**
   * Abort streaming compilation. If {exception} has a value, then the promise
   * associated with streaming compilation is rejected with that value. If
   * {exception} does not have value, the promise does not get rejected.
   */
  void Abort(MaybeLocal<Value> exception);

  /**
   * Passes previously compiled module bytes. This must be called before
   * {OnBytesReceived}, {Finish}, or {Abort}. Returns true if the module bytes
   * can be used, false otherwise. The buffer passed via {bytes} and {size}
   * is owned by the caller. If {SetCompiledModuleBytes} returns true, the
   * buffer must remain valid until either {Finish} or {Abort} completes.
   */
  bool SetCompiledModuleBytes(const uint8_t* bytes, size_t size);

  /**
   * Sets the client object that will receive streaming event notifications.
   * This must be called before {OnBytesReceived}, {Finish}, or {Abort}.
   */
  void SetClient(std::shared_ptr<Client> client);

  /*
   * Sets the UTF-8 encoded source URL for the {Script} object. This must be
   * called before {Finish}.
   */
  void SetUrl(const char* url, size_t length);

  /**
   * Unpacks a {WasmStreaming} object wrapped in a  {Managed} for the embedder.
   * Since the embedder is on the other side of the API, it cannot unpack the
   * {Managed} itself.
   */
  static std::shared_ptr<WasmStreaming> Unpack(Isolate* isolate,
                                               Local<Value> value);

 private:
  std::unique_ptr<WasmStreamingImpl> impl_;
};

// TODO(mtrofin): when streaming compilation is done, we can rename this
// to simply WasmModuleObjectBuilder
class V8_EXPORT WasmModuleObjectBuilderStreaming final {
 public:
  explicit WasmModuleObjectBuilderStreaming(Isolate* isolate);
  /**
   * The buffer passed into OnBytesReceived is owned by the caller.
   */
  void OnBytesReceived(const uint8_t*, size_t size);
  void Finish();
  /**
   * Abort streaming compilation. If {exception} has a value, then the promise
   * associated with streaming compilation is rejected with that value. If
   * {exception} does not have value, the promise does not get rejected.
   */
  void Abort(MaybeLocal<Value> exception);
  Local<Promise> GetPromise();

  ~WasmModuleObjectBuilderStreaming() = default;

 private:
  WasmModuleObjectBuilderStreaming(const WasmModuleObjectBuilderStreaming&) =
      delete;
  WasmModuleObjectBuilderStreaming(WasmModuleObjectBuilderStreaming&&) =
      default;
  WasmModuleObjectBuilderStreaming& operator=(
      const WasmModuleObjectBuilderStreaming&) = delete;
  WasmModuleObjectBuilderStreaming& operator=(
      WasmModuleObjectBuilderStreaming&&) = default;
  Isolate* isolate_ = nullptr;

#if V8_CC_MSVC
  /**
   * We don't need the static Copy API, so the default
   * NonCopyablePersistentTraits would be sufficient, however,
   * MSVC eagerly instantiates the Copy.
   * We ensure we don't use Copy, however, by compiling with the
   * defaults everywhere else.
   */
  Persistent<Promise, CopyablePersistentTraits<Promise>> promise_;
#else
  Persistent<Promise> promise_;
#endif
  std::shared_ptr<internal::wasm::StreamingDecoder> streaming_decoder_;
};

}  // namespace v8

#endif  // INCLUDE_V8_WASM_H_
