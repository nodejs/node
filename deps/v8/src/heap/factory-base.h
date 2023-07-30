// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_BASE_H_
#define V8_HEAP_FACTORY_BASE_H_

#include "src/base/export-template.h"
#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/objects/code-kind.h"
#include "src/objects/fixed-array.h"
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

enum class NumberCacheMode { kIgnore, kSetOnly, kBoth };

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

struct NewCodeOptions {
  CodeKind kind;
  Builtin builtin;
  bool is_turbofanned;
  int stack_slots;
  int kind_specific_flags;
  AllocationType allocation;
  int instruction_size;
  int metadata_size;
  unsigned int inlined_bytecode_size;
  BytecodeOffset osr_offset;
  int handler_table_offset;
  int constant_pool_offset;
  int code_comments_offset;
  int32_t unwinding_info_offset;
  Handle<ByteArray> reloc_info;
  Handle<HeapObject> bytecode_or_deoptimization_data;
  Handle<ByteArray> bytecode_offsets_or_source_position_table;
};

template <typename Impl>
class FactoryBase : public TorqueGeneratedFactory<Impl> {
 public:
  // Converts the given boolean condition to JavaScript boolean value.
  inline Handle<Oddball> ToBoolean(bool value);

#define ROOT_ACCESSOR(Type, name, CamelName) inline Handle<Type> name();
  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

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

  // Creates a new Code for a InstructionStream object.
  Handle<Code> NewCode(const NewCodeOptions& options);

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

  // Allocate a new fixed array with Smi(0) entries.
  Handle<FixedArray> NewFixedArrayWithZeroes(
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

  Handle<Script> NewScript(
      Handle<PrimitiveHeapObject> source,
      ScriptEventType event_type = ScriptEventType::kCreate);
  Handle<Script> NewScriptWithId(
      Handle<PrimitiveHeapObject> source, int script_id,
      ScriptEventType event_type = ScriptEventType::kCreate);

  Handle<ArrayList> NewArrayList(
      int size, AllocationType allocation = AllocationType::kYoung);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForLiteral(
      FunctionLiteral* literal, Handle<Script> script, bool is_toplevel);

  // Create a copy of a given SharedFunctionInfo for use as a placeholder in
  // off-thread compilation
  Handle<SharedFunctionInfo> CloneSharedFunctionInfo(
      Handle<SharedFunctionInfo> other);

  Handle<PreparseData> NewPreparseData(int data_length, int children_length);

  Handle<UncompiledDataWithoutPreparseData>
  NewUncompiledDataWithoutPreparseData(Handle<String> inferred_name,
                                       int32_t start_position,
                                       int32_t end_position);

  Handle<UncompiledDataWithPreparseData> NewUncompiledDataWithPreparseData(
      Handle<String> inferred_name, int32_t start_position,
      int32_t end_position, Handle<PreparseData>);

  Handle<UncompiledDataWithoutPreparseDataWithJob>
  NewUncompiledDataWithoutPreparseDataWithJob(Handle<String> inferred_name,
                                              int32_t start_position,
                                              int32_t end_position);

  Handle<UncompiledDataWithPreparseDataAndJob>
  NewUncompiledDataWithPreparseDataAndJob(Handle<String> inferred_name,
                                          int32_t start_position,
                                          int32_t end_position,
                                          Handle<PreparseData>);

  // Allocates a FeedbackMetadata object and zeroes the data section.
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
  Handle<SeqOneByteString> NewOneByteInternalizedStringFromTwoByte(
      const base::Vector<const base::uc16>& str, uint32_t raw_hash_field);

  Handle<SeqOneByteString> AllocateRawOneByteInternalizedString(
      int length, uint32_t raw_hash_field);
  Handle<SeqTwoByteString> AllocateRawTwoByteInternalizedString(
      int length, uint32_t raw_hash_field);

  // Creates a single character string where the character has given code.
  // A cache is used for Latin1 codes.
  Handle<String> LookupSingleCharacterStringFromCode(uint16_t code);

  MaybeHandle<String> NewStringFromOneByte(
      const base::Vector<const uint8_t>& string,
      AllocationType allocation = AllocationType::kYoung);

  inline Handle<String> NewStringFromAsciiChecked(
      const char* str, AllocationType allocation = AllocationType::kYoung) {
    return NewStringFromOneByte(base::OneByteVector(str), allocation)
        .ToHandleChecked();
  }

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

  V8_WARN_UNUSED_RESULT Handle<String> NumberToString(
      Handle<Object> number, NumberCacheMode mode = NumberCacheMode::kBoth);
  V8_WARN_UNUSED_RESULT Handle<String> HeapNumberToString(
      Handle<HeapNumber> number, double value,
      NumberCacheMode mode = NumberCacheMode::kBoth);
  V8_WARN_UNUSED_RESULT Handle<String> SmiToString(
      Smi number, NumberCacheMode mode = NumberCacheMode::kBoth);

  V8_WARN_UNUSED_RESULT MaybeHandle<SeqOneByteString> NewRawSharedOneByteString(
      int length);
  V8_WARN_UNUSED_RESULT MaybeHandle<SeqTwoByteString> NewRawSharedTwoByteString(
      int length);

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

  MaybeHandle<Map> GetInPlaceInternalizedStringMap(Map from_string_map);

  AllocationType RefineAllocationTypeForInPlaceInternalizableString(
      AllocationType allocation, Map string_map);

 protected:
  // Must be large enough to fit any double, int, or size_t.
  static constexpr int kNumberToStringBufferSize = 32;

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
      AllocationAlignment alignment = kTaggedAligned);
  HeapObject NewWithImmortalMap(Map map, AllocationType allocation);

  Handle<FixedArray> NewFixedArrayWithFiller(Handle<Map> map, int length,
                                             Handle<Oddball> filler,
                                             AllocationType allocation);

  Handle<SharedFunctionInfo> NewSharedFunctionInfo();
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      MaybeHandle<String> maybe_name,
      MaybeHandle<HeapObject> maybe_function_data, Builtin builtin,
      FunctionKind kind = FunctionKind::kNormalFunction);

  Handle<String> MakeOrFindTwoCharacterString(uint16_t c1, uint16_t c2);

  template <typename SeqStringT>
  MaybeHandle<SeqStringT> NewRawStringWithMap(int length, Map map,
                                              AllocationType allocation);

 private:
  Impl* impl() { return static_cast<Impl*>(this); }
  auto isolate() { return impl()->isolate(); }
  ReadOnlyRoots read_only_roots() { return impl()->read_only_roots(); }

  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kTaggedAligned);

  friend TorqueGeneratedFactory<Impl>;
};

extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    FactoryBase<Factory>;
extern template class EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
    FactoryBase<LocalFactory>;

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_BASE_H_
