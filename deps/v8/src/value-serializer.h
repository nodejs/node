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
#include "src/vector.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

class HeapNumber;
class Isolate;
class JSDate;
class JSMap;
class JSRegExp;
class JSSet;
class JSValue;
class Object;
class Oddball;
class Smi;

enum class SerializationTag : uint8_t;

/**
 * Writes V8 objects in a binary format that allows the objects to be cloned
 * according to the HTML structured clone algorithm.
 *
 * Format is based on Blink's previous serialization logic.
 */
class ValueSerializer {
 public:
  explicit ValueSerializer(Isolate* isolate);
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
  std::vector<uint8_t> ReleaseBuffer() { return std::move(buffer_); }

 private:
  // Writing the wire format.
  void WriteTag(SerializationTag tag);
  template <typename T>
  void WriteVarint(T value);
  template <typename T>
  void WriteZigZag(T value);
  void WriteDouble(double value);
  void WriteOneByteString(Vector<const uint8_t> chars);
  void WriteTwoByteString(Vector<const uc16> chars);
  uint8_t* ReserveRawBytes(size_t bytes);

  // Writing V8 objects of various kinds.
  void WriteOddball(Oddball* oddball);
  void WriteSmi(Smi* smi);
  void WriteHeapNumber(HeapNumber* number);
  void WriteString(Handle<String> string);
  Maybe<bool> WriteJSReceiver(Handle<JSReceiver> receiver) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSObject(Handle<JSObject> object) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSArray(Handle<JSArray> array) WARN_UNUSED_RESULT;
  void WriteJSDate(JSDate* date);
  Maybe<bool> WriteJSValue(Handle<JSValue> value) WARN_UNUSED_RESULT;
  void WriteJSRegExp(JSRegExp* regexp);
  Maybe<bool> WriteJSMap(Handle<JSMap> map) WARN_UNUSED_RESULT;
  Maybe<bool> WriteJSSet(Handle<JSSet> map) WARN_UNUSED_RESULT;

  /*
   * Reads the specified keys from the object and writes key-value pairs to the
   * buffer. Returns the number of keys actually written, which may be smaller
   * if some keys are not own properties when accessed.
   */
  Maybe<uint32_t> WriteJSObjectProperties(
      Handle<JSObject> object, Handle<FixedArray> keys) WARN_UNUSED_RESULT;

  Isolate* const isolate_;
  std::vector<uint8_t> buffer_;
  Zone zone_;

  // To avoid extra lookups in the identity map, ID+1 is actually stored in the
  // map (checking if the used identity is zero is the fast way of checking if
  // the entry is new).
  IdentityMap<uint32_t> id_map_;
  uint32_t next_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ValueSerializer);
};

/*
 * Deserializes values from data written with ValueSerializer, or a compatible
 * implementation.
 */
class ValueDeserializer {
 public:
  ValueDeserializer(Isolate* isolate, Vector<const uint8_t> data);
  ~ValueDeserializer();

  /*
   * Runs version detection logic, which may fail if the format is invalid.
   */
  Maybe<bool> ReadHeader() WARN_UNUSED_RESULT;

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

  // Reading V8 objects of specific kinds.
  // The tag is assumed to have already been read.
  MaybeHandle<String> ReadUtf8String() WARN_UNUSED_RESULT;
  MaybeHandle<String> ReadTwoByteString() WARN_UNUSED_RESULT;
  MaybeHandle<JSObject> ReadJSObject() WARN_UNUSED_RESULT;
  MaybeHandle<JSArray> ReadSparseJSArray() WARN_UNUSED_RESULT;
  MaybeHandle<JSArray> ReadDenseJSArray() WARN_UNUSED_RESULT;
  MaybeHandle<JSDate> ReadJSDate() WARN_UNUSED_RESULT;
  MaybeHandle<JSValue> ReadJSValue(SerializationTag tag) WARN_UNUSED_RESULT;
  MaybeHandle<JSRegExp> ReadJSRegExp() WARN_UNUSED_RESULT;
  MaybeHandle<JSMap> ReadJSMap() WARN_UNUSED_RESULT;
  MaybeHandle<JSSet> ReadJSSet() WARN_UNUSED_RESULT;

  /*
   * Reads key-value pairs into the object until the specified end tag is
   * encountered. If successful, returns the number of properties read.
   */
  Maybe<uint32_t> ReadJSObjectProperties(Handle<JSObject> object,
                                         SerializationTag end_tag);

  // Manipulating the map from IDs to reified objects.
  bool HasObjectWithID(uint32_t id);
  MaybeHandle<JSReceiver> GetObjectWithID(uint32_t id);
  void AddObjectWithID(uint32_t id, Handle<JSReceiver> object);

  Isolate* const isolate_;
  const uint8_t* position_;
  const uint8_t* const end_;
  uint32_t version_ = 0;
  Handle<SeededNumberDictionary> id_map_;  // Always a global handle.
  uint32_t next_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ValueDeserializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_VALUE_SERIALIZER_H_
