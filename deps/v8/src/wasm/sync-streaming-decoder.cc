// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/isolate.h"
#include "src/wasm/streaming-decoder.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-serialization.h"

namespace v8::internal::wasm {

class V8_EXPORT_PRIVATE SyncStreamingDecoder final : public StreamingDecoder {
 public:
  SyncStreamingDecoder(WasmEnabledFeatures enabled,
                       CompileTimeImports compile_imports,
                       const char* api_method_name_for_errors,
                       std::shared_ptr<CompilationResultResolver> resolver)
      : enabled_(enabled),
        compile_imports_(std::move(compile_imports)),
        api_method_name_for_errors_(api_method_name_for_errors),
        resolver_(resolver) {}

  void InitializeIsolateSpecificInfo(Isolate* isolate) override {
    isolate_ = isolate;
    context_ =
        indirect_handle(direct_handle(isolate->raw_native_context(), isolate));
  }

  // The buffer passed into OnBytesReceived is owned by the caller.
  void OnBytesReceived(base::Vector<const uint8_t> bytes) override {
    buffer_.emplace_back(bytes.size());
    CHECK_EQ(buffer_.back().size(), bytes.size());
    std::memcpy(buffer_.back().data(), bytes.data(), bytes.size());
    buffer_size_ += bytes.size();
  }

  void SetHasCompiledModuleBytes() override {
    if (buffer_size_ != 0) {
      FATAL(
          "SetHasCompiledModuleBytes has to be called before OnBytesReceived");
    }

    has_compiled_module_bytes_ = true;
  }

  void Finish(
      const WasmStreaming::ModuleCachingCallback& caching_callback) override {
    // We copy all received chunks into one byte buffer.
    base::OwnedVector<const uint8_t> bytes_copy;
    {
      auto bytes = base::OwnedVector<uint8_t>::NewForOverwrite(buffer_size_);
      uint8_t* destination = bytes.begin();
      for (auto& chunk : buffer_) {
        std::memcpy(destination, chunk.data(), chunk.size());
        destination += chunk.size();
      }
      CHECK_EQ(destination - bytes.begin(), buffer_size_);
      bytes_copy = std::move(bytes);
    }

    // Check if we can deserialize the module from cache.
    if (caching_callback) {
      // The embedder should have called `SetHasCompiledModuleBytes()` before.
      CHECK(has_compiled_module_bytes_);
      HandleScope scope(isolate_);
      SaveAndSwitchContext saved_context(isolate_, *context_);

      struct CachingInterface : public WasmStreaming::ModuleCachingInterface {
        SyncStreamingDecoder* const decoder;
        base::OwnedVector<const uint8_t>& wire_bytes;
        bool did_try_deserialization = false;
        MaybeDirectHandle<WasmModuleObject> deserialized_module_object;

        CachingInterface(SyncStreamingDecoder* decoder,
                         base::OwnedVector<const uint8_t>& wire_bytes)
            : decoder(decoder), wire_bytes(wire_bytes) {}

        // Public API:
        MemorySpan<const uint8_t> GetWireBytes() const override {
          return {wire_bytes.data(), wire_bytes.size()};
        }

        bool SetCachedCompiledModuleBytes(
            MemorySpan<const uint8_t> module_bytes) override {
          CHECK(!did_try_deserialization);
          did_try_deserialization = true;
          deserialized_module_object =
              decoder->Deserialize(wire_bytes, base::VectorOf(module_bytes));
          return !deserialized_module_object.IsEmpty();
        }
      } caching_interface{this, bytes_copy};

      // Call the embedder.
      caching_callback(caching_interface);

      if (DirectHandle<WasmModuleObject> module_object;
          caching_interface.deserialized_module_object.ToHandle(
              &module_object)) {
        resolver_->OnCompilationSucceeded(module_object);
        return;
      }
    }

    // If we did not deserialize then `bytes_copy` still holds our owned copy.
    DCHECK_EQ(buffer_size_, bytes_copy.size());

    // Compile the received bytes synchronously.
    ErrorThrower thrower(isolate_, api_method_name_for_errors_);
    MaybeDirectHandle<WasmModuleObject> module_object =
        GetWasmEngine()->SyncCompile(isolate_, enabled_,
                                     std::move(compile_imports_), &thrower,
                                     std::move(bytes_copy));
    if (thrower.error()) {
      resolver_->OnCompilationFailed(thrower.Reify());
      return;
    }
    DirectHandle<WasmModuleObject> module = module_object.ToHandleChecked();
    resolver_->OnCompilationSucceeded(module);
  }

  void Abort() override {
    // Abort is fully handled by the API, we only clear the buffer.
    buffer_.clear();
  }

  void NotifyCompilationDiscarded() override { buffer_.clear(); }

  void NotifyNativeModuleCreated(
      const std::shared_ptr<NativeModule>&) override {
    // This function is only called from the {AsyncCompileJob}.
    UNREACHABLE();
  }

 private:
  MaybeDirectHandle<WasmModuleObject> Deserialize(
      base::OwnedVector<const uint8_t>& wire_bytes,
      base::Vector<const uint8_t> cached_module_bytes) {
    return DeserializeNativeModule(isolate_, enabled_, cached_module_bytes,
                                   wire_bytes, compile_imports_,
                                   base::VectorOf(url()));
  }

  Isolate* isolate_;
  const WasmEnabledFeatures enabled_;
  CompileTimeImports compile_imports_;
  IndirectHandle<Context> context_;
  const char* api_method_name_for_errors_;
  std::shared_ptr<CompilationResultResolver> resolver_;
  bool has_compiled_module_bytes_ = false;

  std::vector<std::vector<uint8_t>> buffer_;
  size_t buffer_size_ = 0;
};

std::unique_ptr<StreamingDecoder> StreamingDecoder::CreateSyncStreamingDecoder(
    WasmEnabledFeatures enabled, CompileTimeImports compile_imports,
    const char* api_method_name_for_errors,
    std::shared_ptr<CompilationResultResolver> resolver) {
  return std::make_unique<SyncStreamingDecoder>(
      enabled, std::move(compile_imports), api_method_name_for_errors,
      std::move(resolver));
}

}  // namespace v8::internal::wasm
