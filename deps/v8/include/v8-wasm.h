// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_WASM_H_
#define INCLUDE_V8_WASM_H_

#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "v8-internal.h"      // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-memory-span.h"   // NOLINT(build/include_directory)
#include "v8-object.h"        // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class ArrayBuffer;
class Promise;

namespace internal::wasm {
class NativeModule;
}  // namespace internal::wasm

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

/**
 * Wrapper around a compiled WebAssembly module, which is potentially shared by
 * different WasmModuleObjects.
 */
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
  friend class WasmModuleCompilation;
  friend class WasmModuleObject;
  friend class WasmStreaming;

  explicit CompiledWasmModule(std::shared_ptr<internal::wasm::NativeModule>,
                              std::string source_url);

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

  /**
   * Compile a Wasm module from the provided uncompiled bytes.
   */
  static MaybeLocal<WasmModuleObject> Compile(
      Isolate* isolate, MemorySpan<const uint8_t> wire_bytes);

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
  static constexpr internal::ExternalPointerTag kManagedTag =
      internal::kWasmWasmStreamingTag;
  class WasmStreamingImpl;

  class ModuleCachingInterface {
   public:
    // Get the full wire bytes, to check against the cached version.
    virtual MemorySpan<const uint8_t> GetWireBytes() const = 0;
    // Pass serialized (cached) compiled module bytes, to be deserialized and
    // used as the result of this streaming compilation.
    // The passed bytes will only be accessed inside this callback, i.e.
    // lifetime can end after the call.
    // The return value indicates whether V8 could use the passed bytes; {false}
    // would be returned on e.g. version mismatch.
    // This method can only be called once.
    virtual bool SetCachedCompiledModuleBytes(MemorySpan<const uint8_t>) = 0;
  };

  using ModuleCachingCallback = std::function<void(ModuleCachingInterface&)>;

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
   * must not be called after {Abort} has been called already.
   * If {can_use_compiled_module} is true and {SetCompiledModuleBytes} was
   * previously called, the compiled module bytes can be used.
   * If {can_use_compiled_module} is false, the compiled module bytes previously
   * set by {SetCompiledModuleBytes} should not be used.
   */
  V8_DEPRECATED(
      "Use the new variant of Finish which takes the caching callback argument")
  void Finish(bool can_use_compiled_module = true) {
    ModuleCachingCallback callback;
    if (can_use_compiled_module && !cached_compiled_module_bytes_.empty()) {
      callback = [bytes = cached_compiled_module_bytes_](
                     ModuleCachingInterface& caching_interface) {
        caching_interface.SetCachedCompiledModuleBytes(bytes);
      };
    }
    Finish(callback);
  }

  /**
   * {Finish} should be called after all received bytes where passed to
   * {OnBytesReceived} to tell V8 that there will be no more bytes. {Finish}
   * must not be called after {Abort} has been called already.
   * If {SetHasCompiledModuleBytes()} was called before, a {caching_callback}
   * can be passed which can inspect the full received wire bytes and set cached
   * module bytes which will be deserialized then. This callback will happen
   * synchronously within this call; the callback is not stored.
   */
  void Finish(const ModuleCachingCallback& caching_callback);

  /**
   * Abort streaming compilation. If {exception} has a value, then the promise
   * associated with streaming compilation is rejected with that value. If
   * {exception} does not have value, the promise does not get rejected.
   * {Abort} must not be called repeatedly, or after {Finish}.
   */
  void Abort(MaybeLocal<Value> exception);

  /**
   * Passes previously compiled module bytes. This must be called before
   * {OnBytesReceived}, {Finish}, or {Abort}. Returns true if the module bytes
   * can be used, false otherwise. The buffer passed via {bytes} and {size}
   * is owned by the caller. If {SetCompiledModuleBytes} returns true, the
   * buffer must remain valid until either {Finish} or {Abort} completes.
   * The compiled module bytes should not be used until {Finish(true)} is
   * called, because they can be invalidated later by {Finish(false)}.
   */
  V8_DEPRECATED(
      "Use SetHasCompiledModule in combination with the new variant of Finish")
  bool SetCompiledModuleBytes(const uint8_t* bytes, size_t size) {
    SetHasCompiledModuleBytes();
    cached_compiled_module_bytes_ = {bytes, size};
    // Optimistically return true here, even though we might later find out that
    // we cannot use the bytes.
    return true;
  }

  /**
   * Mark that the embedder has (potentially) cached compiled module bytes (i.e.
   * a serialized {CompiledWasmModule}) that could match this streaming request.
   * This will cause V8 to skip streaming compilation.
   * The embedder should then pass a callback to the {Finish} method to pass the
   * serialized bytes, after potentially checking their validity against the
   * full received wire bytes.
   */
  void SetHasCompiledModuleBytes();

  /**
   * Sets a callback which is called whenever a significant number of new
   * functions are ready for serialization.
   */
  void SetMoreFunctionsCanBeSerializedCallback(
      std::function<void(CompiledWasmModule)>);

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
  // Temporarily store the compiled module bytes here until the deprecation (see
  // methods above) has gone through.
  MemorySpan<const uint8_t> cached_compiled_module_bytes_;
};

/**
 * An interface for asynchronous WebAssembly module compilation, to be used e.g.
 * for implementing source phase imports.
 * Note: This interface is experimental and can change or be removed without
 * notice.
 */
class V8_EXPORT WasmModuleCompilation final {
 public:
  using ModuleCachingCallback = WasmStreaming::ModuleCachingCallback;

  /**
   * Start an asynchronous module compilation. This can be called on any thread.
   * TODO(clemensb): Add some way to pass enabled features.
   * TODO(clemensb): Add some way to pass compile time imports.
   */
  WasmModuleCompilation();

  ~WasmModuleCompilation();

  WasmModuleCompilation(const WasmModuleCompilation&) = delete;
  WasmModuleCompilation& operator=(const WasmModuleCompilation&) = delete;

  /**
   * Pass a new chunk of bytes to WebAssembly compilation.
   * The buffer passed into {OnBytesReceived} is owned by the caller and will
   * not be accessed any more after this call returns.
   */
  void OnBytesReceived(const uint8_t* bytes, size_t size);

  /**
   * {Finish} must be called on the main thread after all bytes were passed to
   * {OnBytesReceived}.
   * It eventually calls the provided callback to deliver the compiled module or
   * an error. This callback will also be called in foreground, but not
   * necessarily within this call.
   * {Finish} must not be called after {Abort} has been called already.
   * If {SetHasCompiledModuleBytes()} was called before, a {caching_callback}
   * can be passed which can inspect the full received wire bytes and set cached
   * module bytes which will be deserialized then. This callback will happen
   * synchronously within this call; the callback is not stored.
   */
  void Finish(
      Isolate*, const ModuleCachingCallback& caching_callback,
      const std::function<void(
          std::variant<Local<WasmModuleObject>, Local<Value>> module_or_error)>&
          resolution_callback);

  /**
   * Abort compilation. This can be called from any thread.
   * {Abort} must not be called repeatedly, or after {Finish}.
   */
  void Abort();

  /**
   * Mark that the embedder has (potentially) cached compiled module bytes (i.e.
   * a serialized {CompiledWasmModule}) that could match this streaming request.
   * This will cause V8 to skip streaming compilation.
   * The embedder should then pass a callback to the {Finish} method to pass the
   * serialized bytes, after potentially checking their validity against the
   * full received wire bytes.
   */
  void SetHasCompiledModuleBytes();

  /**
   * Sets a callback which is called whenever a significant number of new
   * functions are ready for serialization.
   */
  void SetMoreFunctionsCanBeSerializedCallback(
      std::function<void(CompiledWasmModule)>);

  /*
   * Sets the UTF-8 encoded source URL for the {Script} object. This must be
   * called before {Finish}.
   */
  void SetUrl(const char* url, size_t length);

 private:
  class Impl;
  const std::unique_ptr<Impl> impl_;
};

/**
 * The V8 interface for a WebAssembly memory map descriptor. This is an
 * experimental feature that may change and be removed without further
 * communication.
 */
class V8_EXPORT WasmMemoryMapDescriptor : public Object {
 public:
  WasmMemoryMapDescriptor() = delete;

  V8_INLINE static WasmMemoryMapDescriptor* Cast(Value* value) {
#ifdef V8_ENABLE_CHECKS
    CheckCast(value);
#endif
    return static_cast<WasmMemoryMapDescriptor*>(value);
  }

  using WasmFileDescriptor = int32_t;

  static Local<WasmMemoryMapDescriptor> New(Isolate* isolate,
                                            WasmFileDescriptor fd);

 private:
  static void CheckCast(Value* object);
};
}  // namespace v8

#endif  // INCLUDE_V8_WASM_H_
