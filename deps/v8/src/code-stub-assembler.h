// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUB_ASSEMBLER_H_
#define V8_CODE_STUB_ASSEMBLER_H_

#include <functional>

#include "src/compiler/code-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class StatsCounter;
class StubCache;

enum class PrimitiveType { kBoolean, kNumber, kString, kSymbol };

#define HEAP_CONSTANT_LIST(V)                 \
  V(BooleanMap, BooleanMap)                   \
  V(empty_string, EmptyString)                \
  V(EmptyFixedArray, EmptyFixedArray)         \
  V(FixedArrayMap, FixedArrayMap)             \
  V(FixedCOWArrayMap, FixedCOWArrayMap)       \
  V(FixedDoubleArrayMap, FixedDoubleArrayMap) \
  V(HeapNumberMap, HeapNumberMap)             \
  V(MinusZeroValue, MinusZero)                \
  V(NanValue, Nan)                            \
  V(NullValue, Null)                          \
  V(TheHoleValue, TheHole)                    \
  V(UndefinedValue, Undefined)

// Provides JavaScript-specific "macro-assembler" functionality on top of the
// CodeAssembler. By factoring the JavaScript-isms out of the CodeAssembler,
// it's possible to add JavaScript-specific useful CodeAssembler "macros"
// without modifying files in the compiler directory (and requiring a review
// from a compiler directory OWNER).
class CodeStubAssembler : public compiler::CodeAssembler {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeStubAssembler(Isolate* isolate, Zone* zone,
                    const CallInterfaceDescriptor& descriptor,
                    Code::Flags flags, const char* name,
                    size_t result_size = 1);

  // Create with JSCall linkage.
  CodeStubAssembler(Isolate* isolate, Zone* zone, int parameter_count,
                    Code::Flags flags, const char* name);

  enum AllocationFlag : uint8_t {
    kNone = 0,
    kDoubleAlignment = 1,
    kPretenured = 1 << 1
  };

  typedef base::Flags<AllocationFlag> AllocationFlags;

  // TODO(ishell): Fix all loads/stores from arrays by int32 offsets/indices
  // and eventually remove INTEGER_PARAMETERS in favour of INTPTR_PARAMETERS.
  enum ParameterMode { INTEGER_PARAMETERS, SMI_PARAMETERS, INTPTR_PARAMETERS };

  // On 32-bit platforms, there is a slight performance advantage to doing all
  // of the array offset/index arithmetic with SMIs, since it's possible
  // to save a few tag/untag operations without paying an extra expense when
  // calculating array offset (the smi math can be folded away) and there are
  // fewer live ranges. Thus only convert indices to untagged value on 64-bit
  // platforms.
  ParameterMode OptimalParameterMode() const {
    return Is64() ? INTPTR_PARAMETERS : SMI_PARAMETERS;
  }

  compiler::Node* UntagParameter(compiler::Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) value = SmiUntag(value);
    return value;
  }

  compiler::Node* TagParameter(compiler::Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) value = SmiTag(value);
    return value;
  }

  compiler::Node* NoContextConstant();
#define HEAP_CONSTANT_ACCESSOR(rootName, name) compiler::Node* name##Constant();
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootName, name) \
  compiler::Node* Is##name(compiler::Node* value);
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_TEST)
#undef HEAP_CONSTANT_TEST

  compiler::Node* HashSeed();
  compiler::Node* StaleRegisterConstant();

  compiler::Node* IntPtrOrSmiConstant(int value, ParameterMode mode);

  // Float64 operations.
  compiler::Node* Float64Ceil(compiler::Node* x);
  compiler::Node* Float64Floor(compiler::Node* x);
  compiler::Node* Float64Round(compiler::Node* x);
  compiler::Node* Float64RoundToEven(compiler::Node* x);
  compiler::Node* Float64Trunc(compiler::Node* x);

  // Tag a Word as a Smi value.
  compiler::Node* SmiTag(compiler::Node* value);
  // Untag a Smi value as a Word.
  compiler::Node* SmiUntag(compiler::Node* value);

  // Smi conversions.
  compiler::Node* SmiToFloat64(compiler::Node* value);
  compiler::Node* SmiFromWord(compiler::Node* value) { return SmiTag(value); }
  compiler::Node* SmiFromWord32(compiler::Node* value);
  compiler::Node* SmiToWord(compiler::Node* value) { return SmiUntag(value); }
  compiler::Node* SmiToWord32(compiler::Node* value);

  // Smi operations.
  compiler::Node* SmiAdd(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiAddWithOverflow(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiSub(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiSubWithOverflow(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiAbove(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiAboveOrEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiBelow(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiLessThan(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiLessThanOrEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiMax(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiMin(compiler::Node* a, compiler::Node* b);
  // Computes a % b for Smi inputs a and b; result is not necessarily a Smi.
  compiler::Node* SmiMod(compiler::Node* a, compiler::Node* b);
  // Computes a * b for Smi inputs a and b; result is not necessarily a Smi.
  compiler::Node* SmiMul(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiOr(compiler::Node* a, compiler::Node* b) {
    return WordOr(a, b);
  }

  // Allocate an object of the given size.
  compiler::Node* Allocate(compiler::Node* size, AllocationFlags flags = kNone);
  compiler::Node* Allocate(int size, AllocationFlags flags = kNone);
  compiler::Node* InnerAllocate(compiler::Node* previous, int offset);
  compiler::Node* InnerAllocate(compiler::Node* previous,
                                compiler::Node* offset);

  void Assert(compiler::Node* condition);

  // Check a value for smi-ness
  compiler::Node* WordIsSmi(compiler::Node* a);
  // Check that the value is a non-negative smi.
  compiler::Node* WordIsPositiveSmi(compiler::Node* a);

  void BranchIfSmiEqual(compiler::Node* a, compiler::Node* b, Label* if_true,
                        Label* if_false) {
    BranchIf(SmiEqual(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThan(compiler::Node* a, compiler::Node* b, Label* if_true,
                           Label* if_false) {
    BranchIf(SmiLessThan(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThanOrEqual(compiler::Node* a, compiler::Node* b,
                                  Label* if_true, Label* if_false) {
    BranchIf(SmiLessThanOrEqual(a, b), if_true, if_false);
  }

  void BranchIfFloat64IsNaN(compiler::Node* value, Label* if_true,
                            Label* if_false) {
    BranchIfFloat64Equal(value, value, if_false, if_true);
  }

  // Branches to {if_true} if ToBoolean applied to {value} yields true,
  // otherwise goes to {if_false}.
  void BranchIfToBooleanIsTrue(compiler::Node* value, Label* if_true,
                               Label* if_false);

  void BranchIfSimd128Equal(compiler::Node* lhs, compiler::Node* lhs_map,
                            compiler::Node* rhs, compiler::Node* rhs_map,
                            Label* if_equal, Label* if_notequal);
  void BranchIfSimd128Equal(compiler::Node* lhs, compiler::Node* rhs,
                            Label* if_equal, Label* if_notequal) {
    BranchIfSimd128Equal(lhs, LoadMap(lhs), rhs, LoadMap(rhs), if_equal,
                         if_notequal);
  }

  void BranchIfFastJSArray(compiler::Node* object, compiler::Node* context,
                           Label* if_true, Label* if_false);

  // Load value from current frame by given offset in bytes.
  compiler::Node* LoadFromFrame(int offset,
                                MachineType rep = MachineType::AnyTagged());
  // Load value from current parent frame by given offset in bytes.
  compiler::Node* LoadFromParentFrame(
      int offset, MachineType rep = MachineType::AnyTagged());

  // Load an object pointer from a buffer that isn't in the heap.
  compiler::Node* LoadBufferObject(compiler::Node* buffer, int offset,
                                   MachineType rep = MachineType::AnyTagged());
  // Load a field from an object on the heap.
  compiler::Node* LoadObjectField(compiler::Node* object, int offset,
                                  MachineType rep = MachineType::AnyTagged());
  compiler::Node* LoadObjectField(compiler::Node* object,
                                  compiler::Node* offset,
                                  MachineType rep = MachineType::AnyTagged());
  // Load a SMI field and untag it.
  compiler::Node* LoadAndUntagObjectField(compiler::Node* object, int offset);
  // Load a SMI field, untag it, and convert to Word32.
  compiler::Node* LoadAndUntagToWord32ObjectField(compiler::Node* object,
                                                  int offset);
  // Load a SMI and untag it.
  compiler::Node* LoadAndUntagSmi(compiler::Node* base, int index);
  // Load a SMI root, untag it, and convert to Word32.
  compiler::Node* LoadAndUntagToWord32Root(Heap::RootListIndex root_index);

  // Load the floating point value of a HeapNumber.
  compiler::Node* LoadHeapNumberValue(compiler::Node* object);
  // Load the Map of an HeapObject.
  compiler::Node* LoadMap(compiler::Node* object);
  // Load the instance type of an HeapObject.
  compiler::Node* LoadInstanceType(compiler::Node* object);
  // Checks that given heap object has given instance type.
  void AssertInstanceType(compiler::Node* object, InstanceType instance_type);
  // Load the properties backing store of a JSObject.
  compiler::Node* LoadProperties(compiler::Node* object);
  // Load the elements backing store of a JSObject.
  compiler::Node* LoadElements(compiler::Node* object);
  // Load the length of a JSArray instance.
  compiler::Node* LoadJSArrayLength(compiler::Node* array);
  // Load the length of a fixed array base instance.
  compiler::Node* LoadFixedArrayBaseLength(compiler::Node* array);
  // Load the length of a fixed array base instance.
  compiler::Node* LoadAndUntagFixedArrayBaseLength(compiler::Node* array);
  // Load the bit field of a Map.
  compiler::Node* LoadMapBitField(compiler::Node* map);
  // Load bit field 2 of a map.
  compiler::Node* LoadMapBitField2(compiler::Node* map);
  // Load bit field 3 of a map.
  compiler::Node* LoadMapBitField3(compiler::Node* map);
  // Load the instance type of a map.
  compiler::Node* LoadMapInstanceType(compiler::Node* map);
  // Load the ElementsKind of a map.
  compiler::Node* LoadMapElementsKind(compiler::Node* map);
  // Load the instance descriptors of a map.
  compiler::Node* LoadMapDescriptors(compiler::Node* map);
  // Load the prototype of a map.
  compiler::Node* LoadMapPrototype(compiler::Node* map);
  // Load the instance size of a Map.
  compiler::Node* LoadMapInstanceSize(compiler::Node* map);
  // Load the inobject properties count of a Map (valid only for JSObjects).
  compiler::Node* LoadMapInobjectProperties(compiler::Node* map);
  // Load the constructor function index of a Map (only for primitive maps).
  compiler::Node* LoadMapConstructorFunctionIndex(compiler::Node* map);
  // Load the constructor of a Map (equivalent to Map::GetConstructor()).
  compiler::Node* LoadMapConstructor(compiler::Node* map);

  // Load the hash field of a name as an uint32 value.
  compiler::Node* LoadNameHashField(compiler::Node* name);
  // Load the hash value of a name as an uint32 value.
  // If {if_hash_not_computed} label is specified then it also checks if
  // hash is actually computed.
  compiler::Node* LoadNameHash(compiler::Node* name,
                               Label* if_hash_not_computed = nullptr);

  // Load length field of a String object.
  compiler::Node* LoadStringLength(compiler::Node* object);
  // Load value field of a JSValue object.
  compiler::Node* LoadJSValueValue(compiler::Node* object);
  // Load value field of a WeakCell object.
  compiler::Node* LoadWeakCellValue(compiler::Node* weak_cell,
                                    Label* if_cleared = nullptr);

  // Load an array element from a FixedArray.
  compiler::Node* LoadFixedArrayElement(
      compiler::Node* object, compiler::Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);
  // Load an array element from a FixedArray, untag it and return it as Word32.
  compiler::Node* LoadAndUntagToWord32FixedArrayElement(
      compiler::Node* object, compiler::Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);
  // Load an array element from a FixedDoubleArray.
  compiler::Node* LoadFixedDoubleArrayElement(
      compiler::Node* object, compiler::Node* index, MachineType machine_type,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTEGER_PARAMETERS,
      Label* if_hole = nullptr);

  // Load Float64 value by |base| + |offset| address. If the value is a double
  // hole then jump to |if_hole|. If |machine_type| is None then only the hole
  // check is generated.
  compiler::Node* LoadDoubleWithHoleCheck(
      compiler::Node* base, compiler::Node* offset, Label* if_hole,
      MachineType machine_type = MachineType::Float64());

  // Context manipulation
  compiler::Node* LoadContextElement(compiler::Node* context, int slot_index);
  compiler::Node* LoadNativeContext(compiler::Node* context);

  compiler::Node* LoadJSArrayElementsMap(ElementsKind kind,
                                         compiler::Node* native_context);

  // Store the floating point value of a HeapNumber.
  compiler::Node* StoreHeapNumberValue(compiler::Node* object,
                                       compiler::Node* value);
  // Store a field to an object on the heap.
  compiler::Node* StoreObjectField(
      compiler::Node* object, int offset, compiler::Node* value);
  compiler::Node* StoreObjectField(compiler::Node* object,
                                   compiler::Node* offset,
                                   compiler::Node* value);
  compiler::Node* StoreObjectFieldNoWriteBarrier(
      compiler::Node* object, int offset, compiler::Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  compiler::Node* StoreObjectFieldNoWriteBarrier(
      compiler::Node* object, compiler::Node* offset, compiler::Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  // Store the Map of an HeapObject.
  compiler::Node* StoreMapNoWriteBarrier(compiler::Node* object,
                                         compiler::Node* map);
  compiler::Node* StoreObjectFieldRoot(compiler::Node* object, int offset,
                                       Heap::RootListIndex root);
  // Store an array element to a FixedArray.
  compiler::Node* StoreFixedArrayElement(
      compiler::Node* object, compiler::Node* index, compiler::Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);

  compiler::Node* StoreFixedDoubleArrayElement(
      compiler::Node* object, compiler::Node* index, compiler::Node* value,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);

  // Allocate a HeapNumber without initializing its value.
  compiler::Node* AllocateHeapNumber(MutableMode mode = IMMUTABLE);
  // Allocate a HeapNumber with a specific value.
  compiler::Node* AllocateHeapNumberWithValue(compiler::Node* value,
                                              MutableMode mode = IMMUTABLE);
  // Allocate a SeqOneByteString with the given length.
  compiler::Node* AllocateSeqOneByteString(int length);
  compiler::Node* AllocateSeqOneByteString(compiler::Node* context,
                                           compiler::Node* length);
  // Allocate a SeqTwoByteString with the given length.
  compiler::Node* AllocateSeqTwoByteString(int length);
  compiler::Node* AllocateSeqTwoByteString(compiler::Node* context,
                                           compiler::Node* length);

  // Allocate a SlicedOneByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  compiler::Node* AllocateSlicedOneByteString(compiler::Node* length,
                                              compiler::Node* parent,
                                              compiler::Node* offset);
  // Allocate a SlicedTwoByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  compiler::Node* AllocateSlicedTwoByteString(compiler::Node* length,
                                              compiler::Node* parent,
                                              compiler::Node* offset);

  // Allocate a RegExpResult with the given length (the number of captures,
  // including the match itself), index (the index where the match starts),
  // and input string. |length| and |index| are expected to be tagged, and
  // |input| must be a string.
  compiler::Node* AllocateRegExpResult(compiler::Node* context,
                                       compiler::Node* length,
                                       compiler::Node* index,
                                       compiler::Node* input);

  // Allocate a JSArray without elements and initialize the header fields.
  compiler::Node* AllocateUninitializedJSArrayWithoutElements(
      ElementsKind kind, compiler::Node* array_map, compiler::Node* length,
      compiler::Node* allocation_site);
  // Allocate and return a JSArray with initialized header fields and its
  // uninitialized elements.
  // The ParameterMode argument is only used for the capacity parameter.
  std::pair<compiler::Node*, compiler::Node*>
  AllocateUninitializedJSArrayWithElements(
      ElementsKind kind, compiler::Node* array_map, compiler::Node* length,
      compiler::Node* allocation_site, compiler::Node* capacity,
      ParameterMode capacity_mode = INTEGER_PARAMETERS);
  // Allocate a JSArray and fill elements with the hole.
  // The ParameterMode argument is only used for the capacity parameter.
  compiler::Node* AllocateJSArray(
      ElementsKind kind, compiler::Node* array_map, compiler::Node* capacity,
      compiler::Node* length, compiler::Node* allocation_site = nullptr,
      ParameterMode capacity_mode = INTEGER_PARAMETERS);

  compiler::Node* AllocateFixedArray(ElementsKind kind,
                                     compiler::Node* capacity,
                                     ParameterMode mode = INTEGER_PARAMETERS,
                                     AllocationFlags flags = kNone);

  void FillFixedArrayWithValue(ElementsKind kind, compiler::Node* array,
                               compiler::Node* from_index,
                               compiler::Node* to_index,
                               Heap::RootListIndex value_root_index,
                               ParameterMode mode = INTEGER_PARAMETERS);

  // Copies all elements from |from_array| of |length| size to
  // |to_array| of the same size respecting the elements kind.
  void CopyFixedArrayElements(
      ElementsKind kind, compiler::Node* from_array, compiler::Node* to_array,
      compiler::Node* length,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTEGER_PARAMETERS) {
    CopyFixedArrayElements(kind, from_array, kind, to_array, length, length,
                           barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| to |to_array| of
  // |capacity| size respecting both array's elements kinds.
  void CopyFixedArrayElements(
      ElementsKind from_kind, compiler::Node* from_array, ElementsKind to_kind,
      compiler::Node* to_array, compiler::Node* element_count,
      compiler::Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTEGER_PARAMETERS);

  // Copies |character_count| elements from |from_string| to |to_string|
  // starting at the |from_index|'th character. |from_index| and
  // |character_count| must be Smis s.t.
  // 0 <= |from_index| <= |from_index| + |character_count| < from_string.length.
  void CopyStringCharacters(compiler::Node* from_string,
                            compiler::Node* to_string,
                            compiler::Node* from_index,
                            compiler::Node* character_count,
                            String::Encoding encoding);

  // Loads an element from |array| of |from_kind| elements by given |offset|
  // (NOTE: not index!), does a hole check if |if_hole| is provided and
  // converts the value so that it becomes ready for storing to array of
  // |to_kind| elements.
  compiler::Node* LoadElementAndPrepareForStore(compiler::Node* array,
                                                compiler::Node* offset,
                                                ElementsKind from_kind,
                                                ElementsKind to_kind,
                                                Label* if_hole);

  compiler::Node* CalculateNewElementsCapacity(
      compiler::Node* old_capacity, ParameterMode mode = INTEGER_PARAMETERS);

  // Tries to grow the |elements| array of given |object| to store the |key|
  // or bails out if the growing gap is too big. Returns new elements.
  compiler::Node* TryGrowElementsCapacity(compiler::Node* object,
                                          compiler::Node* elements,
                                          ElementsKind kind,
                                          compiler::Node* key, Label* bailout);

  // Tries to grow the |capacity|-length |elements| array of given |object|
  // to store the |key| or bails out if the growing gap is too big. Returns
  // new elements.
  compiler::Node* TryGrowElementsCapacity(compiler::Node* object,
                                          compiler::Node* elements,
                                          ElementsKind kind,
                                          compiler::Node* key,
                                          compiler::Node* capacity,
                                          ParameterMode mode, Label* bailout);

  // Grows elements capacity of given object. Returns new elements.
  compiler::Node* GrowElementsCapacity(
      compiler::Node* object, compiler::Node* elements, ElementsKind from_kind,
      ElementsKind to_kind, compiler::Node* capacity,
      compiler::Node* new_capacity, ParameterMode mode, Label* bailout);

  // Allocation site manipulation
  void InitializeAllocationMemento(compiler::Node* base_allocation,
                                   int base_allocation_size,
                                   compiler::Node* allocation_site);

  compiler::Node* TruncateTaggedToFloat64(compiler::Node* context,
                                          compiler::Node* value);
  compiler::Node* TruncateTaggedToWord32(compiler::Node* context,
                                         compiler::Node* value);
  // Truncate the floating point value of a HeapNumber to an Int32.
  compiler::Node* TruncateHeapNumberValueToWord32(compiler::Node* object);

  // Conversions.
  compiler::Node* ChangeFloat64ToTagged(compiler::Node* value);
  compiler::Node* ChangeInt32ToTagged(compiler::Node* value);
  compiler::Node* ChangeUint32ToTagged(compiler::Node* value);

  // Type conversions.
  // Throws a TypeError for {method_name} if {value} is not coercible to Object,
  // or returns the {value} converted to a String otherwise.
  compiler::Node* ToThisString(compiler::Node* context, compiler::Node* value,
                               char const* method_name);
  // Throws a TypeError for {method_name} if {value} is neither of the given
  // {primitive_type} nor a JSValue wrapping a value of {primitive_type}, or
  // returns the {value} (or wrapped value) otherwise.
  compiler::Node* ToThisValue(compiler::Node* context, compiler::Node* value,
                              PrimitiveType primitive_type,
                              char const* method_name);

  // Throws a TypeError for {method_name} if {value} is not of the given
  // instance type. Returns {value}'s map.
  compiler::Node* ThrowIfNotInstanceType(compiler::Node* context,
                                         compiler::Node* value,
                                         InstanceType instance_type,
                                         char const* method_name);

  // Type checks.
  compiler::Node* IsStringInstanceType(compiler::Node* instance_type);
  compiler::Node* IsJSReceiverInstanceType(compiler::Node* instance_type);

  // String helpers.
  // Load a character from a String (might flatten a ConsString).
  compiler::Node* StringCharCodeAt(compiler::Node* string,
                                   compiler::Node* smi_index);
  // Return the single character string with only {code}.
  compiler::Node* StringFromCharCode(compiler::Node* code);
  // Return a new string object which holds a substring containing the range
  // [from,to[ of string.  |from| and |to| are expected to be tagged.
  compiler::Node* SubString(compiler::Node* context, compiler::Node* string,
                            compiler::Node* from, compiler::Node* to);

  compiler::Node* StringFromCodePoint(compiler::Node* codepoint,
                                      UnicodeEncoding encoding);

  // Type conversion helpers.
  // Convert a String to a Number.
  compiler::Node* StringToNumber(compiler::Node* context,
                                 compiler::Node* input);
  // Convert an object to a name.
  compiler::Node* ToName(compiler::Node* context, compiler::Node* input);
  // Convert a Non-Number object to a Number.
  compiler::Node* NonNumberToNumber(compiler::Node* context,
                                    compiler::Node* input);
  // Convert any object to a Number.
  compiler::Node* ToNumber(compiler::Node* context, compiler::Node* input);

  enum ToIntegerTruncationMode {
    kNoTruncation,
    kTruncateMinusZero,
  };

  // Convert any object to an Integer.
  compiler::Node* ToInteger(compiler::Node* context, compiler::Node* input,
                            ToIntegerTruncationMode mode = kNoTruncation);

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word32|. Returns result as an uint32 node.
  template <typename T>
  compiler::Node* BitFieldDecode(compiler::Node* word32) {
    return BitFieldDecode(word32, T::kShift, T::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word32|. Returns result as a word-size node.
  template <typename T>
  compiler::Node* BitFieldDecodeWord(compiler::Node* word32) {
    return ChangeUint32ToWord(BitFieldDecode<T>(word32));
  }

  // Decodes an unsigned (!) value from |word32| to an uint32 node.
  compiler::Node* BitFieldDecode(compiler::Node* word32, uint32_t shift,
                                 uint32_t mask);

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int delta);
  void DecrementCounter(StatsCounter* counter, int delta);

  // Generates "if (false) goto label" code. Useful for marking a label as
  // "live" to avoid assertion failures during graph building. In the resulting
  // code this check will be eliminated.
  void Use(Label* label);

  // Various building blocks for stubs doing property lookups.
  void TryToName(compiler::Node* key, Label* if_keyisindex, Variable* var_index,
                 Label* if_keyisunique, Label* if_bailout);

  // Calculates array index for given dictionary entry and entry field.
  // See Dictionary::EntryToIndex().
  template <typename Dictionary>
  compiler::Node* EntryToIndex(compiler::Node* entry, int field_index);
  template <typename Dictionary>
  compiler::Node* EntryToIndex(compiler::Node* entry) {
    return EntryToIndex<Dictionary>(entry, Dictionary::kEntryKeyIndex);
  }

  // Looks up an entry in a NameDictionaryBase successor. If the entry is found
  // control goes to {if_found} and {var_name_index} contains an index of the
  // key field of the entry found. If the key is not found control goes to
  // {if_not_found}.
  static const int kInlinedDictionaryProbes = 4;
  template <typename Dictionary>
  void NameDictionaryLookup(compiler::Node* dictionary,
                            compiler::Node* unique_name, Label* if_found,
                            Variable* var_name_index, Label* if_not_found,
                            int inlined_probes = kInlinedDictionaryProbes);

  compiler::Node* ComputeIntegerHash(compiler::Node* key, compiler::Node* seed);

  template <typename Dictionary>
  void NumberDictionaryLookup(compiler::Node* dictionary,
                              compiler::Node* intptr_index, Label* if_found,
                              Variable* var_entry, Label* if_not_found);

  // Tries to check if {object} has own {unique_name} property.
  void TryHasOwnProperty(compiler::Node* object, compiler::Node* map,
                         compiler::Node* instance_type,
                         compiler::Node* unique_name, Label* if_found,
                         Label* if_not_found, Label* if_bailout);

  // Tries to get {object}'s own {unique_name} property value. If the property
  // is an accessor then it also calls a getter. If the property is a double
  // field it re-wraps value in an immutable heap number.
  void TryGetOwnProperty(compiler::Node* context, compiler::Node* receiver,
                         compiler::Node* object, compiler::Node* map,
                         compiler::Node* instance_type,
                         compiler::Node* unique_name, Label* if_found,
                         Variable* var_value, Label* if_not_found,
                         Label* if_bailout);

  void LoadPropertyFromFastObject(compiler::Node* object, compiler::Node* map,
                                  compiler::Node* descriptors,
                                  compiler::Node* name_index,
                                  Variable* var_details, Variable* var_value);

  void LoadPropertyFromNameDictionary(compiler::Node* dictionary,
                                      compiler::Node* entry,
                                      Variable* var_details,
                                      Variable* var_value);

  void LoadPropertyFromGlobalDictionary(compiler::Node* dictionary,
                                        compiler::Node* entry,
                                        Variable* var_details,
                                        Variable* var_value, Label* if_deleted);

  // Generic property lookup generator. If the {object} is fast and
  // {unique_name} property is found then the control goes to {if_found_fast}
  // label and {var_meta_storage} and {var_name_index} will contain
  // DescriptorArray and an index of the descriptor's name respectively.
  // If the {object} is slow or global then the control goes to {if_found_dict}
  // or {if_found_global} and the {var_meta_storage} and {var_name_index} will
  // contain a dictionary and an index of the key field of the found entry.
  // If property is not found or given lookup is not supported then
  // the control goes to {if_not_found} or {if_bailout} respectively.
  //
  // Note: this code does not check if the global dictionary points to deleted
  // entry! This has to be done by the caller.
  void TryLookupProperty(compiler::Node* object, compiler::Node* map,
                         compiler::Node* instance_type,
                         compiler::Node* unique_name, Label* if_found_fast,
                         Label* if_found_dict, Label* if_found_global,
                         Variable* var_meta_storage, Variable* var_name_index,
                         Label* if_not_found, Label* if_bailout);

  void TryLookupElement(compiler::Node* object, compiler::Node* map,
                        compiler::Node* instance_type,
                        compiler::Node* intptr_index, Label* if_found,
                        Label* if_not_found, Label* if_bailout);

  // This is a type of a lookup in holder generator function. In case of a
  // property lookup the {key} is guaranteed to be a unique name and in case of
  // element lookup the key is an Int32 index.
  typedef std::function<void(compiler::Node* receiver, compiler::Node* holder,
                             compiler::Node* map, compiler::Node* instance_type,
                             compiler::Node* key, Label* next_holder,
                             Label* if_bailout)>
      LookupInHolder;

  // Generic property prototype chain lookup generator.
  // For properties it generates lookup using given {lookup_property_in_holder}
  // and for elements it uses {lookup_element_in_holder}.
  // Upon reaching the end of prototype chain the control goes to {if_end}.
  // If it can't handle the case {receiver}/{key} case then the control goes
  // to {if_bailout}.
  void TryPrototypeChainLookup(compiler::Node* receiver, compiler::Node* key,
                               LookupInHolder& lookup_property_in_holder,
                               LookupInHolder& lookup_element_in_holder,
                               Label* if_end, Label* if_bailout);

  // Instanceof helpers.
  // ES6 section 7.3.19 OrdinaryHasInstance (C, O)
  compiler::Node* OrdinaryHasInstance(compiler::Node* context,
                                      compiler::Node* callable,
                                      compiler::Node* object);

  // Load/StoreIC helpers.
  struct LoadICParameters {
    LoadICParameters(compiler::Node* context, compiler::Node* receiver,
                     compiler::Node* name, compiler::Node* slot,
                     compiler::Node* vector)
        : context(context),
          receiver(receiver),
          name(name),
          slot(slot),
          vector(vector) {}

    compiler::Node* context;
    compiler::Node* receiver;
    compiler::Node* name;
    compiler::Node* slot;
    compiler::Node* vector;
  };

  struct StoreICParameters : public LoadICParameters {
    StoreICParameters(compiler::Node* context, compiler::Node* receiver,
                      compiler::Node* name, compiler::Node* value,
                      compiler::Node* slot, compiler::Node* vector)
        : LoadICParameters(context, receiver, name, slot, vector),
          value(value) {}
    compiler::Node* value;
  };

  // Load type feedback vector from the stub caller's frame.
  compiler::Node* LoadTypeFeedbackVectorForStub();

  // Update the type feedback vector.
  void UpdateFeedback(compiler::Node* feedback,
                      compiler::Node* type_feedback_vector,
                      compiler::Node* slot_id);

  compiler::Node* LoadReceiverMap(compiler::Node* receiver);

  // Checks monomorphic case. Returns {feedback} entry of the vector.
  compiler::Node* TryMonomorphicCase(compiler::Node* slot,
                                     compiler::Node* vector,
                                     compiler::Node* receiver_map,
                                     Label* if_handler, Variable* var_handler,
                                     Label* if_miss);
  void HandlePolymorphicCase(compiler::Node* receiver_map,
                             compiler::Node* feedback, Label* if_handler,
                             Variable* var_handler, Label* if_miss,
                             int unroll_count);

  compiler::Node* StubCachePrimaryOffset(compiler::Node* name,
                                         compiler::Node* map);

  compiler::Node* StubCacheSecondaryOffset(compiler::Node* name,
                                           compiler::Node* seed);

  // This enum is used here as a replacement for StubCache::Table to avoid
  // including stub cache header.
  enum StubCacheTable : int;

  void TryProbeStubCacheTable(StubCache* stub_cache, StubCacheTable table_id,
                              compiler::Node* entry_offset,
                              compiler::Node* name, compiler::Node* map,
                              Label* if_handler, Variable* var_handler,
                              Label* if_miss);

  void TryProbeStubCache(StubCache* stub_cache, compiler::Node* receiver,
                         compiler::Node* name, Label* if_handler,
                         Variable* var_handler, Label* if_miss);

  // Extends properties backing store by JSObject::kFieldsAdded elements.
  void ExtendPropertiesBackingStore(compiler::Node* object);

  compiler::Node* PrepareValueForWrite(compiler::Node* value,
                                       Representation representation,
                                       Label* bailout);

  void StoreNamedField(compiler::Node* object, FieldIndex index,
                       Representation representation, compiler::Node* value,
                       bool transition_to_field);

  void StoreNamedField(compiler::Node* object, compiler::Node* offset,
                       bool is_inobject, Representation representation,
                       compiler::Node* value, bool transition_to_field);

  // Emits keyed sloppy arguments load. Returns either the loaded value.
  compiler::Node* LoadKeyedSloppyArguments(compiler::Node* receiver,
                                           compiler::Node* key,
                                           Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, nullptr, bailout);
  }

  // Emits keyed sloppy arguments store.
  void StoreKeyedSloppyArguments(compiler::Node* receiver, compiler::Node* key,
                                 compiler::Node* value, Label* bailout) {
    DCHECK_NOT_NULL(value);
    EmitKeyedSloppyArguments(receiver, key, value, bailout);
  }

  // Loads script context from the script context table.
  compiler::Node* LoadScriptContext(compiler::Node* context, int context_index);

  compiler::Node* Int32ToUint8Clamped(compiler::Node* int32_value);
  compiler::Node* Float64ToUint8Clamped(compiler::Node* float64_value);

  compiler::Node* PrepareValueForWriteToTypedArray(compiler::Node* key,
                                                   ElementsKind elements_kind,
                                                   Label* bailout);

  // Store value to an elements array with given elements kind.
  void StoreElement(compiler::Node* elements, ElementsKind kind,
                    compiler::Node* index, compiler::Node* value,
                    ParameterMode mode);

  void EmitElementStore(compiler::Node* object, compiler::Node* key,
                        compiler::Node* value, bool is_jsarray,
                        ElementsKind elements_kind,
                        KeyedAccessStoreMode store_mode, Label* bailout);

  compiler::Node* CheckForCapacityGrow(compiler::Node* object,
                                       compiler::Node* elements,
                                       ElementsKind kind,
                                       compiler::Node* length,
                                       compiler::Node* key, ParameterMode mode,
                                       bool is_js_array, Label* bailout);

  compiler::Node* CopyElementsOnWrite(compiler::Node* object,
                                      compiler::Node* elements,
                                      ElementsKind kind, compiler::Node* length,
                                      ParameterMode mode, Label* bailout);

  void LoadIC(const LoadICParameters* p);
  void LoadGlobalIC(const LoadICParameters* p);
  void KeyedLoadIC(const LoadICParameters* p);
  void KeyedLoadICGeneric(const LoadICParameters* p);
  void StoreIC(const StoreICParameters* p);

  void TransitionElementsKind(compiler::Node* object, compiler::Node* map,
                              ElementsKind from_kind, ElementsKind to_kind,
                              bool is_jsarray, Label* bailout);

  void TrapAllocationMemento(compiler::Node* object, Label* memento_found);

  compiler::Node* PageFromAddress(compiler::Node* address);

  // Get the enumerable length from |map| and return the result as a Smi.
  compiler::Node* EnumLength(compiler::Node* map);

  // Check the cache validity for |receiver|. Branch to |use_cache| if
  // the cache is valid, otherwise branch to |use_runtime|.
  void CheckEnumCache(compiler::Node* receiver,
                      CodeStubAssembler::Label* use_cache,
                      CodeStubAssembler::Label* use_runtime);

  // Create a new weak cell with a specified value and install it into a
  // feedback vector.
  compiler::Node* CreateWeakCellInFeedbackVector(
      compiler::Node* feedback_vector, compiler::Node* slot,
      compiler::Node* value);

  // Create a new AllocationSite and install it into a feedback vector.
  compiler::Node* CreateAllocationSiteInFeedbackVector(
      compiler::Node* feedback_vector, compiler::Node* slot);

  compiler::Node* GetFixedArrayAllocationSize(compiler::Node* element_count,
                                              ElementsKind kind,
                                              ParameterMode mode) {
    return ElementOffsetFromIndex(element_count, kind, mode,
                                  FixedArray::kHeaderSize);
  }

 private:
  enum ElementSupport { kOnlyProperties, kSupportElements };

  void DescriptorLookupLinear(compiler::Node* unique_name,
                              compiler::Node* descriptors, compiler::Node* nof,
                              Label* if_found, Variable* var_name_index,
                              Label* if_not_found);
  compiler::Node* CallGetterIfAccessor(compiler::Node* value,
                                       compiler::Node* details,
                                       compiler::Node* context,
                                       compiler::Node* receiver,
                                       Label* if_bailout);

  void HandleLoadICHandlerCase(
      const LoadICParameters* p, compiler::Node* handler, Label* miss,
      ElementSupport support_elements = kOnlyProperties);
  compiler::Node* TryToIntptr(compiler::Node* key, Label* miss);
  void EmitFastElementsBoundsCheck(compiler::Node* object,
                                   compiler::Node* elements,
                                   compiler::Node* intptr_index,
                                   compiler::Node* is_jsarray_condition,
                                   Label* miss);
  void EmitElementLoad(compiler::Node* object, compiler::Node* elements,
                       compiler::Node* elements_kind, compiler::Node* key,
                       compiler::Node* is_jsarray_condition, Label* if_hole,
                       Label* rebox_double, Variable* var_double_value,
                       Label* unimplemented_elements_kind, Label* out_of_bounds,
                       Label* miss);
  void BranchIfPrototypesHaveNoElements(compiler::Node* receiver_map,
                                        Label* definitely_no_elements,
                                        Label* possibly_elements);

  compiler::Node* ElementOffsetFromIndex(compiler::Node* index,
                                         ElementsKind kind, ParameterMode mode,
                                         int base_size = 0);

  compiler::Node* AllocateRawAligned(compiler::Node* size_in_bytes,
                                     AllocationFlags flags,
                                     compiler::Node* top_address,
                                     compiler::Node* limit_address);
  compiler::Node* AllocateRawUnaligned(compiler::Node* size_in_bytes,
                                       AllocationFlags flags,
                                       compiler::Node* top_adddress,
                                       compiler::Node* limit_address);
  // Allocate and return a JSArray of given total size in bytes with header
  // fields initialized.
  compiler::Node* AllocateUninitializedJSArray(ElementsKind kind,
                                               compiler::Node* array_map,
                                               compiler::Node* length,
                                               compiler::Node* allocation_site,
                                               compiler::Node* size_in_bytes);

  compiler::Node* SmiShiftBitsConstant();

  // Emits keyed sloppy arguments load if the |value| is nullptr or store
  // otherwise. Returns either the loaded value or |value|.
  compiler::Node* EmitKeyedSloppyArguments(compiler::Node* receiver,
                                           compiler::Node* key,
                                           compiler::Node* value,
                                           Label* bailout);

  static const int kElementLoopUnrollThreshold = 8;
};

DEFINE_OPERATORS_FOR_FLAGS(CodeStubAssembler::AllocationFlags);

}  // namespace internal
}  // namespace v8
#endif  // V8_CODE_STUB_ASSEMBLER_H_
