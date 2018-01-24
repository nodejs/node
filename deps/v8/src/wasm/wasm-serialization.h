// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_SERIALIZATION_H_
#define V8_WASM_SERIALIZATION_H_

#include "src/wasm/wasm-heap.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

class WasmSerializedFormatVersion {
 public:
  static size_t GetVersionSize();
  static bool WriteVersion(Isolate* isolate, Vector<byte>);
  static bool IsSupportedVersion(Isolate* isolate, const Vector<const byte>);

 private:
  static constexpr size_t kVersionSize = 4 * sizeof(uint32_t);
};

enum SerializationSection { Init, Metadata, Stubs, CodeSection, Done };

class V8_EXPORT_PRIVATE NativeModuleSerializer {
 public:
  explicit NativeModuleSerializer(Isolate*, const NativeModule*);
  size_t Measure() const;
  size_t Write(Vector<byte>);
  bool IsDone() const { return state_ == Done; }
  static std::pair<std::unique_ptr<byte[]>, size_t> SerializeWholeModule(
      Isolate*, Handle<WasmCompiledModule>);

 private:
  size_t MeasureHeader() const;
  static size_t GetCodeHeaderSize();
  size_t MeasureCode(const WasmCode*) const;
  size_t MeasureCopiedStubs() const;
  FixedArray* GetHandlerTable(const WasmCode*) const;
  ByteArray* GetSourcePositions(const WasmCode*) const;

  void BufferHeader();
  // we buffer all the stubs because they are small
  void BufferCopiedStubs();
  void BufferCodeInAllocatedScratch(const WasmCode*);
  void BufferCurrentWasmCode();
  size_t DrainBuffer(Vector<byte> dest);
  uint32_t EncodeBuiltinOrStub(Address);

  Isolate* const isolate_ = nullptr;
  const NativeModule* const native_module_ = nullptr;
  SerializationSection state_ = Init;
  uint32_t index_ = 0;
  std::vector<byte> scratch_;
  Vector<byte> remaining_;
  // wasm and copied stubs reverse lookup
  std::map<Address, uint32_t> wasm_targets_lookup_;
  // immovable builtins and runtime entries lookup
  std::map<Address, uint32_t> reference_table_lookup_;
  std::map<Address, uint32_t> stub_lookup_;
  std::map<Address, uint32_t> builtin_lookup_;
};

class V8_EXPORT_PRIVATE NativeModuleDeserializer {
 public:
  explicit NativeModuleDeserializer(Isolate*, NativeModule*);
  // Currently, we don't support streamed reading, yet albeit the
  // API suggests that.
  bool Read(Vector<const byte>);
  static MaybeHandle<WasmCompiledModule> DeserializeFullBuffer(
      Isolate*, Vector<const byte> data, Vector<const byte> wire_bytes);

 private:
  void ExpectHeader();
  void Expect(size_t size);
  bool ReadHeader();
  bool ReadCode();
  bool ReadStubs();
  Address GetTrampolineOrStubFromTag(uint32_t);

  Isolate* const isolate_ = nullptr;
  NativeModule* const native_module_ = nullptr;
  std::vector<byte> scratch_;
  std::vector<Address> stubs_;
  Vector<const byte> unread_;
  size_t current_expectation_ = 0;
  uint32_t index_ = 0;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif
