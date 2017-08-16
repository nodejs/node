// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_VALUE_SERIALIZER_H_
#define V8_VALUE_SERIALIZER_H_

#include <cstdint>
#include <vector>

#include "include/v8.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"
#include "src/identity-map.h"
#include "src/messages.h"
#include "src/vector.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class HeapNumber;
class Isolate;
class JSArrayBuffer;
class JSArrayBufferView;
class JSDate;
class JSMap;
class JSRegExp;
class JSSet;
class JSValue;
class Object;
class Oddball;
class Smi;
class WasmModuleObject;

enum class SerializationTag : uint8_t;

/**
 * Writes V8 objects in a binary format that allows the objects to be cloned
 * according to the HTML structured clone algorithm.
 *
 * Format is based on Blink's previous serialization logic.
 */
class ValueSerializer {
 public:
  ValueSerializer(Isolate* isolate, v8::ValueSerializer::Delegate* delegate);
  ~ValueSerializer();

  /*
   * Writes out a header, which includes the format version.
   */
  void WriteHeader();

  /*
   * Serializes a V8 object into the buffer.
   */
  Maybe<bool> WriteObject(Handle<Object> object) WARN_UNUSED_RESULT;

  /*
   * Returns the stored data. This serializer should not be used once the buffer
   * is released. The contents are undefined if a previous write has failed.
   */
  std::vector<uint8_t> ReleaseBuffer();

  /*
   * Returns the buffer, allocated via the delegate, and its size.
   * Caller assumes ownership of the buffer.
   */
  std::pair<uint8_t*, size_t> Release();

  /*
   * Marks an ArrayBuffer as havings its contents transferred out of band.
   * Pass the corresponding JSArrayBuffer in the deserializing context to
   * ValueDeserializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Handle<JSArrayBuffer> array_buffer);

  /*
   * Publicly exposed wire format writing methods.
   * These are intended for use within the delegate's WriteHostObject method.
   */
  void WriteUint32(uint32_t value);
  void WriteUint64(uint64_t value);
  void WriteRawBytes(const void* source, size_t length);
  void WriteDouble(double value);

  /*
   * Indicate whether to treat ArrayBufferView objects as host objects,
   * i.e. pass them to Delegate::WriteHostObject. This should not be
   * called when no Delegate was passed.
   *
   * The default is not to treat ArrayBufferViews as host objects.
   */
  void SetTreatArrayBufferViewsAsHostObjects(bool mode);

 private:
  // Managing allocations of the internal buffer.
  Maybe<bool> ExpandBuffer(size_t required_capacity);

  // Writing the wire format.
  void WriteTag(SerializationTag tag);
  template <typename T>
  void WriteVarint(T value);
  template <typename T>
  void WriteZigZag(T value);
  void WriteOneByteString(Vector<const uint8_t> chars);
  void WriteTwoByteString(Vector<const uc16> chars);
  Maybe<uint8_t*> ReserveRawBytes(size_t bytes);

  // Writing V8 objects of various kinds.
  void WriteOddball(Oddball* oddball);
  void WriteSmi(Smi* smi);
  void WriteHeapNumber(HeapNumber* number);
  void WriteString(Handle<String> string);
  Maybe<bool> WriteJSReceiver(Handle<JSReceiver> receiver) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSObject(Handle<JSObject> object) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSObjectSlow(Handle<JSObject> object) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSArray(Handle<JSArray> array) WARN_UNUSED_RESULT;
  void WriteJSDate(JSDate* date);
  Maybe<bool> WriteJSValue(Handle<JSValue> value) WARN_UNUSED_RESULT;
  void WriteJSRegExp(JSRegExp* regexp);
  Maybe<bool> WriteJSMap(Handle<JSMap> map) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSSet(Handle<JSSet> map) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSArrayBuffer(Handle<JSArrayBuffer> array_buffer)
      WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSArrayBufferView(JSArrayBufferView* array_buffer);
  Maybe<bool> WriteWasmModule(Handle<JSObject> object) WARN_UNUSED_RESULT;
  Maybe<bool> WriteHostObject(Handle<JSObject> object) WARN_UNUSED_RESULT;

  /*
   * Reads the specified keys from the object and writes key-value pairs to the
   * buffer. Returns the number of keys actually written, which may be smaller
   * if some keys are not own properties when accessed.
   */
  Maybe<uint32_t> WriteJSObjectPropertiesSlow(
      Handle<JSObject> object, Handle<FixedArray> keys) WARN_UNUSED_RESULT;

  /*
   * Asks the delegate to handle an error that occurred during data cloning, by
   * throwing an exception appropriate for the host.
   */
  void ThrowDataCloneError(MessageTemplate::Template template_index);
  V8_NOINLINE void ThrowDataCloneError(MessageTemplate::Template template_index,
                                       Handle<Object> arg0);

  Maybe<bool> ThrowIfOutOfMemory();

  Isolate* const isolate_;
  v8::ValueSerializer::Delegate* const delegate_;
  bool treat_array_buffer_views_as_host_objects_ = false;
  uint8_t* buffer_ = nullptr;
  size_t buffer_size_ = 0;
  size_t buffer_capacity_ = 0;
  bool out_of_memory_ = false;
  Zone zone_;

  // To avoid extra lookups in the identity map, ID+1 is actually stored in the
  // map (checking if the used identity is zero is the fast way of checking if
  // the entry is new).
  IdentityMap<uint32_t, ZoneAllocationPolicy> id_map_;
  uint32_t next_id_ = 0;

  // A similar map, for transferred array buffers.
  IdentityMap<uint32_t, ZoneAllocationPolicy> array_buffer_transfer_map_;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializer);
};

/*
 * Deserializes values from data written with ValueSerializer, or a compatible
 * implementation.
 */
class ValueDeserializer {
 public:
  ValueDeserializer(Isolate* isolate, Vector<const uint8_t> data,
                    v8::ValueDeserializer::Delegate* delegate);
  ~ValueDeserializer();

  /*
   * Runs version detection logic, which may fail if the format is invalid.
   */
  Maybe<bool> ReadHeader() WARN_UNUSED_RESULT;

  /*
   * Reads the underlying wire format version. Likely mostly to be useful to
   * legacy code reading old wire format versions. Must be called after
   * ReadHeader.
   */
  uint32_t GetWireFormatVersion() const { return version_; }

  /*
   * Deserializes a V8 object from the buffer.
   */
  MaybeHandle<Object> ReadObject() WARN_UNUSED_RESULT;

  /*
   * Reads an object, consuming the entire buffer.
   *
   * This is required for the legacy "version 0" format, which did not allow
   * reference deduplication, and instead relied on a "stack" model for
   * deserializing, with the contents of objects and arrays provided first.
   */
  MaybeHandle<Object> ReadObjectUsingEntireBufferForLegacyFormat()
      WARN_UNUSED_RESULT;

  /*
   * Accepts the array buffer corresponding to the one passed previously to
   * ValueSerializer::TransferArrayBuffer.
   */
  void TransferArrayBuffer(uint32_t transfer_id,
                           Handle<JSArrayBuffer> array_buffer);

  /*
   * Publicly exposed wire format writing methods.
   * These are intended for use within the delegate's WriteHostObject method.
   */
  bool ReadUint32(uint32_t* value) WARN_UNUSED_RESULT;
  bool ReadUint64(uint64_t* value) WARN_UNUSED_RESULT;
  bool ReadDouble(double* value) WARN_UNUSED_RESULT;
  bool ReadRawBytes(size_t length, const void** data) WARN_UNUSED_RESULT;
  void set_expect_inline_wasm(bool expect_inline_wasm) {
    expect_inline_wasm_ = expect_inline_wasm;
  }

 private:
  // Reading the wire format.
  Maybe<SerializationTag> PeekTag() const WARN_UNUSED_RESULT;
  void ConsumeTag(SerializationTag peeked_tag);
  Maybe<SerializationTag> ReadTag() WARN_UNUSED_RESULT;
  template <typename T>
  Maybe<T> ReadVarint() WARN_UNUSED_RESULT;
  template <typename T>
  Maybe<T> ReadZigZag() WARN_UNUSED_RESULT;
  Maybe<double> ReadDouble() WARN_UNUSED_RESULT;
  Maybe<Vector<const uint8_t>> ReadRawBytes(int size) WARN_UNUSED_RESULT;
  bool expect_inline_wasm() const { return expect_inline_wasm_; }

  // Reads a string if it matches the one provided.
  // Returns true if this was the case. Otherwise, nothing is consumed.
  bool ReadExpectedString(Handle<String> expected) WARN_UNUSED_RESULT;

  // Like ReadObject, but skips logic for special cases in simulating the
  // "stack machine".
  MaybeHandle<Object> ReadObjectInternal() WARN_UNUSED_RESULT;

  // Reads a string intended to be part of a more complicated object.
  // Before v12, these are UTF-8 strings. After, they can be any encoding
  // permissible for a string (with the relevant tag).
  MaybeHandle<String> ReadString() WARN_UNUSED_RESULT;

  // Reading V8 objects of specific kinds.
  // The tag is assumed to have already been read.
  MaybeHandle<String> ReadUtf8String() WARN_UNUSED_RESULT;
  MaybeHandle<String> ReadOneByteString() WARN_UNUSED_RESULT;
  MaybeHandle<String> ReadTwoByteString() WARN_UNUSED_RESULT;
  MaybeHandle<JSObject> ReadJSObject() WARN_UNUSED_RESULT;
  MaybeHandle<JSArray> ReadSparseJSArray() WARN_UNUSED_RESULT;
  MaybeHandle<JSArray> ReadDenseJSArray() WARN_UNUSED_RESULT;
  MaybeHandle<JSDate> ReadJSDate() WARN_UNUSED_RESULT;
  MaybeHandle<JSValue> ReadJSValue(SerializationTag tag) WARN_UNUSED_RESULT;
  MaybeHandle<JSRegExp> ReadJSRegExp() WARN_UNUSED_RESULT;
  MaybeHandle<JSMap> ReadJSMap() WARN_UNUSED_RESULT;
  MaybeHandle<JSSet> ReadJSSet() WARN_UNUSED_RESULT;
  MaybeHandle<JSArrayBuffer> ReadJSArrayBuffer() WARN_UNUSED_RESULT;
  MaybeHandle<JSArrayBuffer> ReadTransferredJSArrayBuffer(bool is_shared)
      WARN_UNUSED_RESULT;
  MaybeHandle<JSArrayBufferView> ReadJSArrayBufferView(
      Handle<JSArrayBuffer> buffer) WARN_UNUSED_RESULT;
  MaybeHandle<JSObject> ReadWasmModule() WARN_UNUSED_RESULT;
  MaybeHandle<JSObject> ReadWasmModuleTransfer() WARN_UNUSED_RESULT;
  MaybeHandle<JSObject> ReadHostObject() WARN_UNUSED_RESULT;

  /*
   * Reads key-value pairs into the object until the specified end tag is
   * encountered. If successful, returns the number of properties read.
   */
  Maybe<uint32_t> ReadJSObjectProperties(Handle<JSObject> object,
                                         SerializationTag end_tag,
                                         bool can_use_transitions);

  // Manipulating the map from IDs to reified objects.
  bool HasObjectWithID(uint32_t id);
  MaybeHandle<JSReceiver> GetObjectWithID(uint32_t id);
  void AddObjectWithID(uint32_t id, Handle<JSReceiver> object);

  Isolate* const isolate_;
  v8::ValueDeserializer::Delegate* const delegate_;
  const uint8_t* position_;
  const uint8_t* const end_;
  PretenureFlag pretenure_;
  uint32_t version_ = 0;
  uint32_t next_id_ = 0;
  bool expect_inline_wasm_ = false;

  // Always global handles.
  Handle<FixedArray> id_map_;
  MaybeHandle<SeededNumberDictionary> array_buffer_transfer_map_;

  DISALLOW_COPY_AND_ASSIGN(ValueDeserializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VALUE_SERIALIZER_H_
