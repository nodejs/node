// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_BASE_H_
#define V8_HEAP_FACTORY_BASE_H_

#include "src/base/export-template.h"
#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/objects/function-kind.h"
#include "src/objects/instance-type.h"
#include "src/roots/roots.h"
#include "torque-generated/class-forward-declarations.h"

namespace v8 {
namespace internal {

class HeapObject;
class SharedFunctionInfo;
class FunctionLiteral;
class SeqOneByteString;
class SeqTwoByteString;
class FreshlyAllocatedBigInt;
class ObjectBoilerplateDescription;
class ArrayBoilerplateDescription;
class RegExpBoilerplateDescription;
class TemplateObjectDescription;
class SourceTextModuleInfo;
class PreparseData;
template <class T>
class PodArray;
class UncompiledDataWithoutPreparseData;
class UncompiledDataWithPreparseData;
class BytecodeArray;
class CoverageInfo;
class ClassPositions;
struct SourceRange;
enum class Builtin : int32_t;
template <typename T>
class ZoneVector;

namespace wasm {
class ValueType;
}  // namespace wasm

template <typename Impl>
class FactoryBase;

// Putting Torque-generated definitions in a superclass allows to shadow them
// easily when they shouldn't be used and to reference them when they happen to
// have the same signature.
template <typename Impl>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) TorqueGeneratedFactory {
 private:
  FactoryBase<Impl>* factory() { return static_cast<FactoryBase<Impl>*>(this); }

 public:
#include "torque-generated/factory.inc"
};

template <typename Impl>
class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE) FactoryBase
    : public TorqueGeneratedFactory<Impl> {
 public:
  // Converts the given boolean condition to JavaScript boolean value.
  inline Handle<Oddball> ToBoolean(bool value);

  // Numbers (e.g. literals) are pretenured by the parser.
  // The return value may be a smi or a heap number.
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<Object> NewNumber(double value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<Object> NewNumberFromInt(int32_t value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<Object> NewNumberFromUint(uint32_t value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<Object> NewNumberFromSize(size_t value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<Object> NewNumberFromInt64(int64_t value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<HeapNumber> NewHeapNumber(double value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<HeapNumber> NewHeapNumberFromBits(uint64_t bits);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<HeapNumber> NewHeapNumberWithHoleNaN();

  template <AllocationType allocation>
  Handle<HeapNumber> NewHeapNumber();

  Handle<Struct> NewStruct(InstanceType type,
                           AllocationType allocation = AllocationType::kYoung);

  // Create a pre-tenured empty AccessorPair.
  Handle<AccessorPair> NewAccessorPair();

  // Allocates a fixed array initialized with undefined values.
  Handle<FixedArray> NewFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a fixed array-like object with given map and initialized with
  // undefined values.
  Handle<FixedArray> NewFixedArrayWithMap(
      Handle<Map> map, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Allocate a new fixed array with non-existing entries (the hole).
  Handle<FixedArray> NewFixedArrayWithHoles(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocate a new uninitialized fixed double array.
  // The function returns a pre-allocated empty fixed array for length = 0,
  // so the return type must be the general fixed array class.
  Handle<FixedArrayBase> NewFixedDoubleArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a weak fixed array-like object with given map and initialized
  // with undefined values. Length must be > 0.
  Handle<WeakFixedArray> NewWeakFixedArrayWithMap(
      Map map, int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a fixed array which may contain in-place weak references. The
  // array is initialized with undefined values
  // The function returns a pre-allocated empty weak fixed array for length = 0.
  Handle<WeakFixedArray> NewWeakFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // The function returns a pre-allocated empty byte array for length = 0.
  Handle<ByteArray> NewByteArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  Handle<BytecodeArray> NewBytecodeArray(int length, const byte* raw_bytecodes,
                                         int frame_size, int parameter_count,
                                         Handle<FixedArray> constant_pool);

  // Allocates a fixed array for name-value pairs of boilerplate properties and
  // calculates the number of properties we need to store in the backing store.
  Handle<ObjectBoilerplateDescription> NewObjectBoilerplateDescription(
      int boilerplate, int all_properties, int index_keys, bool has_seen_proto);

  // Create a new ArrayBoilerplateDescription struct.
  Handle<ArrayBoilerplateDescription> NewArrayBoilerplateDescription(
      ElementsKind elements_kind, Handle<FixedArrayBase> constant_values);

  Handle<RegExpBoilerplateDescription> NewRegExpBoilerplateDescription(
      Handle<FixedArray> data, Handle<String> source, Smi flags);

  // Create a new TemplateObjectDescription struct.
  Handle<TemplateObjectDescription> NewTemplateObjectDescription(
      Handle<FixedArray> raw_strings, Handle<FixedArray> cooked_strings);

  Handle<Script> NewScript(Handle<PrimitiveHeapObject> source);
  Handle<Script> NewScriptWithId(Handle<PrimitiveHeapObject> source,
                                 int script_id);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForLiteral(
      FunctionLiteral* literal, Handle<Script> script, bool is_toplevel);

  Handle<PreparseData> NewPreparseData(int data_length, int children_length);

  Handle<UncompiledDataWithoutPreparseData>
  NewUncompiledDataWithoutPreparseData(Handle<String> inferred_name,
                                       int32_t start_position,
                                       int32_t end_position);

  Handle<UncompiledDataWithPreparseData> NewUncompiledDataWithPreparseData(
      Handle<String> inferred_name, int32_t start_position,
      int32_t end_position, Handle<PreparseData>);

  // Allocates a FeedbackMedata object and zeroes the data section.
  Handle<FeedbackMetadata> NewFeedbackMetadata(
      int slot_count, int create_closure_slot_count,
      AllocationType allocation = AllocationType::kOld);

  Handle<CoverageInfo> NewCoverageInfo(const ZoneVector<SourceRange>& slots);

  Handle<String> InternalizeString(const base::Vector<const uint8_t>& string,
                                   bool convert_encoding = false);
  Handle<String> InternalizeString(const base::Vector<const uint16_t>& string,
                                   bool convert_encoding = false);

  template <class StringTableKey>
  Handle<String> InternalizeStringWithKey(StringTableKey* key);

  Handle<SeqOneByteString> NewOneByteInternalizedString(
      const base::Vector<const uint8_t>& str, uint32_t raw_hash_field);
  Handle<SeqTwoByteString> NewTwoByteInternalizedString(
      const base::Vector<const base::uc16>& str, uint32_t raw_hash_field);

  Handle<SeqOneByteString> AllocateRawOneByteInternalizedString(
      int length, uint32_t raw_hash_field);
  Handle<SeqTwoByteString> AllocateRawTwoByteInternalizedString(
      int length, uint32_t raw_hash_field);

  // Allocates and partially initializes an one-byte or two-byte String. The
  // characters of the string are uninitialized. Currently used in regexp code
  // only, where they are pretenured.
  V8_WARN_UNUSED_RESULT MaybeHandle<SeqOneByteString> NewRawOneByteString(
      int length, AllocationType allocation = AllocationType::kYoung);
  V8_WARN_UNUSED_RESULT MaybeHandle<SeqTwoByteString> NewRawTwoByteString(
      int length, AllocationType allocation = AllocationType::kYoung);
  // Create a new cons string object which consists of a pair of strings.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewConsString(
      Handle<String> left, Handle<String> right,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT Handle<String> NewConsString(
      Handle<String> left, Handle<String> right, int length, bool one_byte,
      AllocationType allocation = AllocationType::kYoung);

  // Allocates a new BigInt with {length} digits. Only to be used by
  // MutableBigInt::New*.
  Handle<FreshlyAllocatedBigInt> NewBigInt(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Create a serialized scope info.
  Handle<ScopeInfo> NewScopeInfo(int length,
                                 AllocationType type = AllocationType::kOld);

  Handle<SourceTextModuleInfo> NewSourceTextModuleInfo();

  Handle<DescriptorArray> NewDescriptorArray(
      int number_of_entries, int slack = 0,
      AllocationType allocation = AllocationType::kYoung);

  Handle<ClassPositions> NewClassPositions(int start, int end);

  Handle<SwissNameDictionary> NewSwissNameDictionary(
      int at_least_space_for = kSwissNameDictionaryInitialCapacity,
      AllocationType allocation = AllocationType::kYoung);

  Handle<SwissNameDictionary> NewSwissNameDictionaryWithCapacity(
      int capacity, AllocationType allocation);

  Handle<FunctionTemplateRareData> NewFunctionTemplateRareData();

 protected:
  // Allocate memory for an uninitialized array (e.g., a FixedArray or similar).
  HeapObject AllocateRawArray(int size, AllocationType allocation);
  HeapObject AllocateRawFixedArray(int length, AllocationType allocation);
  HeapObject AllocateRawWeakArrayList(int length, AllocationType allocation);

  template <typename StructType>
  inline StructType NewStructInternal(InstanceType type,
                                      AllocationType allocation);
  Struct NewStructInternal(ReadOnlyRoots roots, Map map, int size,
                           AllocationType allocation);

  HeapObject AllocateRawWithImmortalMap(
      int size, AllocationType allocation, Map map,
      AllocationAlignment alignment = kWordAligned);
  HeapObject NewWithImmortalMap(Map map, AllocationType allocation);

  Handle<FixedArray> NewFixedArrayWithFiller(Handle<Map> map, int length,
                                             Handle<Oddball> filler,
                                             AllocationType allocation);

  Handle<SharedFunctionInfo> NewSharedFunctionInfo();
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      MaybeHandle<String> maybe_name,
      MaybeHandle<HeapObject> maybe_function_data, Builtin builtin,
      FunctionKind kind = kNormalFunction);

  Handle<String> MakeOrFindTwoCharacterString(uint16_t c1, uint16_t c2);

 private:
  friend class WebSnapshotDeserializer;
  Impl* impl() { return static_cast<Impl*>(this); }
  auto isolate() { return impl()->isolate(); }
  ReadOnlyRoots read_only_roots() { return impl()->read_only_roots(); }

  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kWordAligned);

  friend TorqueGeneratedFactory<Impl>;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_BASE_H_
