// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUB_ASSEMBLER_H_
#define V8_CODE_STUB_ASSEMBLER_H_

#include <functional>

#include "src/compiler/code-assembler.h"
#include "src/globals.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class CodeStubArguments;
class StatsCounter;
class StubCache;

enum class PrimitiveType { kBoolean, kNumber, kString, kSymbol };

#define HEAP_CONSTANT_LIST(V)                         \
  V(AccessorInfoMap, AccessorInfoMap)                 \
  V(AccessorPairMap, AccessorPairMap)                 \
  V(AllocationSiteMap, AllocationSiteMap)             \
  V(BooleanMap, BooleanMap)                           \
  V(CodeMap, CodeMap)                                 \
  V(empty_string, EmptyString)                        \
  V(length_string, LengthString)                      \
  V(prototype_string, PrototypeString)                \
  V(EmptyFixedArray, EmptyFixedArray)                 \
  V(EmptyWeakCell, EmptyWeakCell)                     \
  V(FalseValue, False)                                \
  V(FeedbackVectorMap, FeedbackVectorMap)             \
  V(FixedArrayMap, FixedArrayMap)                     \
  V(FixedCOWArrayMap, FixedCOWArrayMap)               \
  V(FixedDoubleArrayMap, FixedDoubleArrayMap)         \
  V(FunctionTemplateInfoMap, FunctionTemplateInfoMap) \
  V(has_instance_symbol, HasInstanceSymbol)           \
  V(HeapNumberMap, HeapNumberMap)                     \
  V(NoClosuresCellMap, NoClosuresCellMap)             \
  V(OneClosureCellMap, OneClosureCellMap)             \
  V(ManyClosuresCellMap, ManyClosuresCellMap)         \
  V(MinusZeroValue, MinusZero)                        \
  V(NanValue, Nan)                                    \
  V(NullValue, Null)                                  \
  V(GlobalPropertyCellMap, PropertyCellMap)           \
  V(SymbolMap, SymbolMap)                             \
  V(TheHoleValue, TheHole)                            \
  V(TrueValue, True)                                  \
  V(Tuple2Map, Tuple2Map)                             \
  V(Tuple3Map, Tuple3Map)                             \
  V(UndefinedValue, Undefined)                        \
  V(WeakCellMap, WeakCellMap)                         \
  V(SpeciesProtector, SpeciesProtector)

// Provides JavaScript-specific "macro-assembler" functionality on top of the
// CodeAssembler. By factoring the JavaScript-isms out of the CodeAssembler,
// it's possible to add JavaScript-specific useful CodeAssembler "macros"
// without modifying files in the compiler directory (and requiring a review
// from a compiler directory OWNER).
class V8_EXPORT_PRIVATE CodeStubAssembler : public compiler::CodeAssembler {
 public:
  typedef compiler::Node Node;

  CodeStubAssembler(compiler::CodeAssemblerState* state);

  enum AllocationFlag : uint8_t {
    kNone = 0,
    kDoubleAlignment = 1,
    kPretenured = 1 << 1,
    kAllowLargeObjectAllocation = 1 << 2,
  };

  typedef base::Flags<AllocationFlag> AllocationFlags;

  enum ParameterMode { SMI_PARAMETERS, INTPTR_PARAMETERS };
  // On 32-bit platforms, there is a slight performance advantage to doing all
  // of the array offset/index arithmetic with SMIs, since it's possible
  // to save a few tag/untag operations without paying an extra expense when
  // calculating array offset (the smi math can be folded away) and there are
  // fewer live ranges. Thus only convert indices to untagged value on 64-bit
  // platforms.
  ParameterMode OptimalParameterMode() const {
    return Is64() ? INTPTR_PARAMETERS : SMI_PARAMETERS;
  }

  MachineRepresentation ParameterRepresentation(ParameterMode mode) const {
    return mode == INTPTR_PARAMETERS ? MachineType::PointerRepresentation()
                                     : MachineRepresentation::kTaggedSigned;
  }

  MachineRepresentation OptimalParameterRepresentation() const {
    return ParameterRepresentation(OptimalParameterMode());
  }

  Node* ParameterToWord(Node* value, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) value = SmiUntag(value);
    return value;
  }

  Node* WordToParameter(Node* value, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) value = SmiTag(value);
    return value;
  }

  Node* ParameterToTagged(Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) value = SmiTag(value);
    return value;
  }

  Node* TaggedToParameter(Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) value = SmiUntag(value);
    return value;
  }

#define PARAMETER_BINOP(OpName, IntPtrOpName, SmiOpName) \
  Node* OpName(Node* a, Node* b, ParameterMode mode) {   \
    if (mode == SMI_PARAMETERS) {                        \
      return SmiOpName(a, b);                            \
    } else {                                             \
      DCHECK_EQ(INTPTR_PARAMETERS, mode);                \
      return IntPtrOpName(a, b);                         \
    }                                                    \
  }
  PARAMETER_BINOP(IntPtrOrSmiMin, IntPtrMin, SmiMin)
  PARAMETER_BINOP(IntPtrOrSmiAdd, IntPtrAdd, SmiAdd)
  PARAMETER_BINOP(IntPtrOrSmiSub, IntPtrSub, SmiSub)
  PARAMETER_BINOP(IntPtrOrSmiLessThan, IntPtrLessThan, SmiLessThan)
  PARAMETER_BINOP(IntPtrOrSmiLessThanOrEqual, IntPtrLessThanOrEqual,
                  SmiLessThanOrEqual)
  PARAMETER_BINOP(IntPtrOrSmiGreaterThan, IntPtrGreaterThan, SmiGreaterThan)
  PARAMETER_BINOP(IntPtrOrSmiGreaterThanOrEqual, IntPtrGreaterThanOrEqual,
                  SmiGreaterThanOrEqual)
  PARAMETER_BINOP(UintPtrOrSmiLessThan, UintPtrLessThan, SmiBelow)
  PARAMETER_BINOP(UintPtrOrSmiGreaterThanOrEqual, UintPtrGreaterThanOrEqual,
                  SmiAboveOrEqual)
#undef PARAMETER_BINOP

  Node* NoContextConstant();
#define HEAP_CONSTANT_ACCESSOR(rootName, name) Node* name##Constant();
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootName, name) Node* Is##name(Node* value);
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_TEST)
#undef HEAP_CONSTANT_TEST

  Node* HashSeed();
  Node* StaleRegisterConstant();

  Node* IntPtrOrSmiConstant(int value, ParameterMode mode);

  bool IsIntPtrOrSmiConstantZero(Node* test);

  // Round the 32bits payload of the provided word up to the next power of two.
  Node* IntPtrRoundUpToPowerOfTwo32(Node* value);
  // Select the maximum of the two provided IntPtr values.
  Node* IntPtrMax(Node* left, Node* right);
  // Select the minimum of the two provided IntPtr values.
  Node* IntPtrMin(Node* left, Node* right);

  // Float64 operations.
  Node* Float64Ceil(Node* x);
  Node* Float64Floor(Node* x);
  Node* Float64Round(Node* x);
  Node* Float64RoundToEven(Node* x);
  Node* Float64Trunc(Node* x);

  // Tag a Word as a Smi value.
  Node* SmiTag(Node* value);
  // Untag a Smi value as a Word.
  Node* SmiUntag(Node* value);

  // Smi conversions.
  Node* SmiToFloat64(Node* value);
  Node* SmiFromWord(Node* value) { return SmiTag(value); }
  Node* SmiFromWord32(Node* value);
  Node* SmiToWord(Node* value) { return SmiUntag(value); }
  Node* SmiToWord32(Node* value);

  // Smi operations.
#define SMI_ARITHMETIC_BINOP(SmiOpName, IntPtrOpName)                  \
  Node* SmiOpName(Node* a, Node* b) {                                  \
    return BitcastWordToTaggedSigned(                                  \
        IntPtrOpName(BitcastTaggedToWord(a), BitcastTaggedToWord(b))); \
  }
  SMI_ARITHMETIC_BINOP(SmiAdd, IntPtrAdd)
  SMI_ARITHMETIC_BINOP(SmiSub, IntPtrSub)
  SMI_ARITHMETIC_BINOP(SmiAnd, WordAnd)
  SMI_ARITHMETIC_BINOP(SmiOr, WordOr)
#undef SMI_ARITHMETIC_BINOP

  Node* SmiShl(Node* a, int shift) {
    return BitcastWordToTaggedSigned(WordShl(BitcastTaggedToWord(a), shift));
  }

  Node* SmiShr(Node* a, int shift) {
    return BitcastWordToTaggedSigned(
        WordAnd(WordShr(BitcastTaggedToWord(a), shift),
                BitcastTaggedToWord(SmiConstant(-1))));
  }

  Node* WordOrSmiShl(Node* a, int shift, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) {
      return SmiShl(a, shift);
    } else {
      DCHECK_EQ(INTPTR_PARAMETERS, mode);
      return WordShl(a, shift);
    }
  }

  Node* WordOrSmiShr(Node* a, int shift, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) {
      return SmiShr(a, shift);
    } else {
      DCHECK_EQ(INTPTR_PARAMETERS, mode);
      return WordShr(a, shift);
    }
  }

#define SMI_COMPARISON_OP(SmiOpName, IntPtrOpName)                       \
  Node* SmiOpName(Node* a, Node* b) {                                    \
    return IntPtrOpName(BitcastTaggedToWord(a), BitcastTaggedToWord(b)); \
  }
  SMI_COMPARISON_OP(SmiEqual, WordEqual)
  SMI_COMPARISON_OP(SmiNotEqual, WordNotEqual)
  SMI_COMPARISON_OP(SmiAbove, UintPtrGreaterThan)
  SMI_COMPARISON_OP(SmiAboveOrEqual, UintPtrGreaterThanOrEqual)
  SMI_COMPARISON_OP(SmiBelow, UintPtrLessThan)
  SMI_COMPARISON_OP(SmiLessThan, IntPtrLessThan)
  SMI_COMPARISON_OP(SmiLessThanOrEqual, IntPtrLessThanOrEqual)
  SMI_COMPARISON_OP(SmiGreaterThan, IntPtrGreaterThan)
  SMI_COMPARISON_OP(SmiGreaterThanOrEqual, IntPtrGreaterThanOrEqual)
#undef SMI_COMPARISON_OP
  Node* SmiMax(Node* a, Node* b);
  Node* SmiMin(Node* a, Node* b);
  // Computes a % b for Smi inputs a and b; result is not necessarily a Smi.
  Node* SmiMod(Node* a, Node* b);
  // Computes a * b for Smi inputs a and b; result is not necessarily a Smi.
  Node* SmiMul(Node* a, Node* b);
  // Tries to computes dividend / divisor for Smi inputs; branching to bailout
  // if the division needs to be performed as a floating point operation.
  Node* TrySmiDiv(Node* dividend, Node* divisor, Label* bailout);

  // Smi | HeapNumber operations.
  Node* NumberInc(Node* value);
  Node* NumberDec(Node* value);
  void GotoIfNotNumber(Node* value, Label* is_not_number);
  void GotoIfNumber(Node* value, Label* is_number);

  // Allocate an object of the given size.
  Node* AllocateInNewSpace(Node* size, AllocationFlags flags = kNone);
  Node* AllocateInNewSpace(int size, AllocationFlags flags = kNone);
  Node* Allocate(Node* size, AllocationFlags flags = kNone);
  Node* Allocate(int size, AllocationFlags flags = kNone);
  Node* InnerAllocate(Node* previous, int offset);
  Node* InnerAllocate(Node* previous, Node* offset);
  Node* IsRegularHeapObjectSize(Node* size);

  typedef std::function<Node*()> NodeGenerator;

  void Assert(const NodeGenerator& condition_body, const char* string = nullptr,
              const char* file = nullptr, int line = 0);
  void Check(const NodeGenerator& condition_body, const char* string = nullptr,
             const char* file = nullptr, int line = 0);

  Node* Select(Node* condition, const NodeGenerator& true_body,
               const NodeGenerator& false_body, MachineRepresentation rep);

  Node* SelectConstant(Node* condition, Node* true_value, Node* false_value,
                       MachineRepresentation rep);

  Node* SelectInt32Constant(Node* condition, int true_value, int false_value);
  Node* SelectIntPtrConstant(Node* condition, int true_value, int false_value);
  Node* SelectBooleanConstant(Node* condition);
  Node* SelectTaggedConstant(Node* condition, Node* true_value,
                             Node* false_value);
  Node* SelectSmiConstant(Node* condition, Smi* true_value, Smi* false_value);
  Node* SelectSmiConstant(Node* condition, int true_value, Smi* false_value) {
    return SelectSmiConstant(condition, Smi::FromInt(true_value), false_value);
  }
  Node* SelectSmiConstant(Node* condition, Smi* true_value, int false_value) {
    return SelectSmiConstant(condition, true_value, Smi::FromInt(false_value));
  }
  Node* SelectSmiConstant(Node* condition, int true_value, int false_value) {
    return SelectSmiConstant(condition, Smi::FromInt(true_value),
                             Smi::FromInt(false_value));
  }

  Node* TruncateWordToWord32(Node* value);

  // Check a value for smi-ness
  Node* TaggedIsSmi(Node* a);
  Node* TaggedIsNotSmi(Node* a);
  // Check that the value is a non-negative smi.
  Node* TaggedIsPositiveSmi(Node* a);
  // Check that a word has a word-aligned address.
  Node* WordIsWordAligned(Node* word);
  Node* WordIsPowerOfTwo(Node* value);

#if DEBUG
  void Bind(Label* label, AssemblerDebugInfo debug_info);
#else
  void Bind(Label* label);
#endif  // DEBUG

  void BranchIfSmiEqual(Node* a, Node* b, Label* if_true, Label* if_false) {
    Branch(SmiEqual(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThan(Node* a, Node* b, Label* if_true, Label* if_false) {
    Branch(SmiLessThan(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThanOrEqual(Node* a, Node* b, Label* if_true,
                                  Label* if_false) {
    Branch(SmiLessThanOrEqual(a, b), if_true, if_false);
  }

  void BranchIfFloat64IsNaN(Node* value, Label* if_true, Label* if_false) {
    Branch(Float64Equal(value, value), if_false, if_true);
  }

  // Branches to {if_true} if ToBoolean applied to {value} yields true,
  // otherwise goes to {if_false}.
  void BranchIfToBooleanIsTrue(Node* value, Label* if_true, Label* if_false);

  void BranchIfJSReceiver(Node* object, Label* if_true, Label* if_false);
  void BranchIfJSObject(Node* object, Label* if_true, Label* if_false);

  enum class FastJSArrayAccessMode { INBOUNDS_READ, ANY_ACCESS };
  void BranchIfFastJSArray(Node* object, Node* context,
                           FastJSArrayAccessMode mode, Label* if_true,
                           Label* if_false);

  // Load value from current frame by given offset in bytes.
  Node* LoadFromFrame(int offset, MachineType rep = MachineType::AnyTagged());
  // Load value from current parent frame by given offset in bytes.
  Node* LoadFromParentFrame(int offset,
                            MachineType rep = MachineType::AnyTagged());

  // Load an object pointer from a buffer that isn't in the heap.
  Node* LoadBufferObject(Node* buffer, int offset,
                         MachineType rep = MachineType::AnyTagged());
  // Load a field from an object on the heap.
  Node* LoadObjectField(Node* object, int offset,
                        MachineType rep = MachineType::AnyTagged());
  Node* LoadObjectField(Node* object, Node* offset,
                        MachineType rep = MachineType::AnyTagged());
  // Load a SMI field and untag it.
  Node* LoadAndUntagObjectField(Node* object, int offset);
  // Load a SMI field, untag it, and convert to Word32.
  Node* LoadAndUntagToWord32ObjectField(Node* object, int offset);
  // Load a SMI and untag it.
  Node* LoadAndUntagSmi(Node* base, int index);
  // Load a SMI root, untag it, and convert to Word32.
  Node* LoadAndUntagToWord32Root(Heap::RootListIndex root_index);

  // Tag a smi and store it.
  Node* StoreAndTagSmi(Node* base, int offset, Node* value);

  // Load the floating point value of a HeapNumber.
  Node* LoadHeapNumberValue(Node* object);
  // Load the Map of an HeapObject.
  Node* LoadMap(Node* object);
  // Load the instance type of an HeapObject.
  Node* LoadInstanceType(Node* object);
  // Compare the instance the type of the object against the provided one.
  Node* HasInstanceType(Node* object, InstanceType type);
  Node* DoesntHaveInstanceType(Node* object, InstanceType type);
  // Load the properties backing store of a JSObject.
  Node* LoadProperties(Node* object);
  // Load the elements backing store of a JSObject.
  Node* LoadElements(Node* object);
  // Load the length of a JSArray instance.
  Node* LoadJSArrayLength(Node* array);
  // Load the length of a fixed array base instance.
  Node* LoadFixedArrayBaseLength(Node* array);
  // Load the length of a fixed array base instance.
  Node* LoadAndUntagFixedArrayBaseLength(Node* array);
  // Load the bit field of a Map.
  Node* LoadMapBitField(Node* map);
  // Load bit field 2 of a map.
  Node* LoadMapBitField2(Node* map);
  // Load bit field 3 of a map.
  Node* LoadMapBitField3(Node* map);
  // Load the instance type of a map.
  Node* LoadMapInstanceType(Node* map);
  // Load the ElementsKind of a map.
  Node* LoadMapElementsKind(Node* map);
  // Load the instance descriptors of a map.
  Node* LoadMapDescriptors(Node* map);
  // Load the prototype of a map.
  Node* LoadMapPrototype(Node* map);
  // Load the prototype info of a map. The result has to be checked if it is a
  // prototype info object or not.
  Node* LoadMapPrototypeInfo(Node* map, Label* if_has_no_proto_info);
  // Load the instance size of a Map.
  Node* LoadMapInstanceSize(Node* map);
  // Load the inobject properties count of a Map (valid only for JSObjects).
  Node* LoadMapInobjectProperties(Node* map);
  // Load the constructor function index of a Map (only for primitive maps).
  Node* LoadMapConstructorFunctionIndex(Node* map);
  // Load the constructor of a Map (equivalent to
  // Map::GetConstructor()).
  Node* LoadMapConstructor(Node* map);
  // Loads a value from the specially encoded integer fields in the
  // SharedFunctionInfo object.
  // TODO(danno): This currently only works for the integer fields that are
  // mapped to the upper part of 64-bit words. We should customize
  // SFI::BodyDescriptor and store int32 values directly.
  Node* LoadSharedFunctionInfoSpecialField(Node* shared, int offset,
                                           ParameterMode param_mode);

  // Check if the map is set for slow properties.
  Node* IsDictionaryMap(Node* map);

  // Load the hash field of a name as an uint32 value.
  Node* LoadNameHashField(Node* name);
  // Load the hash value of a name as an uint32 value.
  // If {if_hash_not_computed} label is specified then it also checks if
  // hash is actually computed.
  Node* LoadNameHash(Node* name, Label* if_hash_not_computed = nullptr);

  // Load length field of a String object.
  Node* LoadStringLength(Node* object);
  // Loads a pointer to the sequential String char array.
  Node* PointerToSeqStringData(Node* seq_string);
  // Load value field of a JSValue object.
  Node* LoadJSValueValue(Node* object);
  // Load value field of a WeakCell object.
  Node* LoadWeakCellValueUnchecked(Node* weak_cell);
  Node* LoadWeakCellValue(Node* weak_cell, Label* if_cleared = nullptr);

  // Load an array element from a FixedArray.
  Node* LoadFixedArrayElement(Node* object, Node* index,
                              int additional_offset = 0,
                              ParameterMode parameter_mode = INTPTR_PARAMETERS);
  Node* LoadFixedArrayElement(Node* object, int index,
                              int additional_offset = 0) {
    return LoadFixedArrayElement(object, IntPtrConstant(index),
                                 additional_offset);
  }
  // Load an array element from a FixedArray, untag it and return it as Word32.
  Node* LoadAndUntagToWord32FixedArrayElement(
      Node* object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);
  // Load an array element from a FixedDoubleArray.
  Node* LoadFixedDoubleArrayElement(
      Node* object, Node* index, MachineType machine_type,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS,
      Label* if_hole = nullptr);

  // Load Float64 value by |base| + |offset| address. If the value is a double
  // hole then jump to |if_hole|. If |machine_type| is None then only the hole
  // check is generated.
  Node* LoadDoubleWithHoleCheck(
      Node* base, Node* offset, Label* if_hole,
      MachineType machine_type = MachineType::Float64());
  Node* LoadFixedTypedArrayElement(
      Node* data_pointer, Node* index_node, ElementsKind elements_kind,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);
  Node* LoadFixedTypedArrayElementAsTagged(
      Node* data_pointer, Node* index_node, ElementsKind elements_kind,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Context manipulation
  Node* LoadContextElement(Node* context, int slot_index);
  Node* LoadContextElement(Node* context, Node* slot_index);
  Node* StoreContextElement(Node* context, int slot_index, Node* value);
  Node* StoreContextElement(Node* context, Node* slot_index, Node* value);
  Node* StoreContextElementNoWriteBarrier(Node* context, int slot_index,
                                          Node* value);
  Node* LoadNativeContext(Node* context);

  Node* LoadJSArrayElementsMap(ElementsKind kind, Node* native_context);

  // Load the "prototype" property of a JSFunction.
  Node* LoadJSFunctionPrototype(Node* function, Label* if_bailout);

  // Store the floating point value of a HeapNumber.
  Node* StoreHeapNumberValue(Node* object, Node* value);
  // Store a field to an object on the heap.
  Node* StoreObjectField(Node* object, int offset, Node* value);
  Node* StoreObjectField(Node* object, Node* offset, Node* value);
  Node* StoreObjectFieldNoWriteBarrier(
      Node* object, int offset, Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  Node* StoreObjectFieldNoWriteBarrier(
      Node* object, Node* offset, Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  // Store the Map of an HeapObject.
  Node* StoreMap(Node* object, Node* map);
  Node* StoreMapNoWriteBarrier(Node* object,
                               Heap::RootListIndex map_root_index);
  Node* StoreMapNoWriteBarrier(Node* object, Node* map);
  Node* StoreObjectFieldRoot(Node* object, int offset,
                             Heap::RootListIndex root);
  // Store an array element to a FixedArray.
  Node* StoreFixedArrayElement(
      Node* object, int index, Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER) {
    return StoreFixedArrayElement(object, IntPtrConstant(index), value,
                                  barrier_mode);
  }

  Node* StoreFixedArrayElement(
      Node* object, Node* index, Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  Node* StoreFixedDoubleArrayElement(
      Node* object, Node* index, Node* value,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

  void EnsureArrayLengthWritable(Node* map, Label* bailout);

  // EnsureArrayPushable verifies that receiver is:
  //   1. Is not a prototype.
  //   2. Is not a dictionary.
  //   3. Has a writeable length property.
  // It returns ElementsKind as a node for further division into cases.
  Node* EnsureArrayPushable(Node* receiver, Label* bailout);

  void TryStoreArrayElement(ElementsKind kind, ParameterMode mode,
                            Label* bailout, Node* elements, Node* index,
                            Node* value);
  // Consumes args into the array, and returns tagged new length.
  Node* BuildAppendJSArray(ElementsKind kind, Node* array,
                           CodeStubArguments& args, Variable& arg_index,
                           Label* bailout);
  // Pushes value onto the end of array.
  void BuildAppendJSArray(ElementsKind kind, Node* array, Node* value,
                          Label* bailout);

  void StoreFieldsNoWriteBarrier(Node* start_address, Node* end_address,
                                 Node* value);

  // Allocate a HeapNumber without initializing its value.
  Node* AllocateHeapNumber(MutableMode mode = IMMUTABLE);
  // Allocate a HeapNumber with a specific value.
  Node* AllocateHeapNumberWithValue(Node* value, MutableMode mode = IMMUTABLE);
  // Allocate a SeqOneByteString with the given length.
  Node* AllocateSeqOneByteString(int length, AllocationFlags flags = kNone);
  Node* AllocateSeqOneByteString(Node* context, Node* length,
                                 ParameterMode mode = INTPTR_PARAMETERS,
                                 AllocationFlags flags = kNone);
  // Allocate a SeqTwoByteString with the given length.
  Node* AllocateSeqTwoByteString(int length, AllocationFlags flags = kNone);
  Node* AllocateSeqTwoByteString(Node* context, Node* length,
                                 ParameterMode mode = INTPTR_PARAMETERS,
                                 AllocationFlags flags = kNone);

  // Allocate a SlicedOneByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  Node* AllocateSlicedOneByteString(Node* length, Node* parent, Node* offset);
  // Allocate a SlicedTwoByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  Node* AllocateSlicedTwoByteString(Node* length, Node* parent, Node* offset);

  // Allocate a one-byte ConsString with the given length, first and second
  // parts. |length| is expected to be tagged, and |first| and |second| are
  // expected to be one-byte strings.
  Node* AllocateOneByteConsString(Node* length, Node* first, Node* second,
                                  AllocationFlags flags = kNone);
  // Allocate a two-byte ConsString with the given length, first and second
  // parts. |length| is expected to be tagged, and |first| and |second| are
  // expected to be two-byte strings.
  Node* AllocateTwoByteConsString(Node* length, Node* first, Node* second,
                                  AllocationFlags flags = kNone);

  // Allocate an appropriate one- or two-byte ConsString with the first and
  // second parts specified by |first| and |second|.
  Node* NewConsString(Node* context, Node* length, Node* left, Node* right,
                      AllocationFlags flags = kNone);

  // Allocate a RegExpResult with the given length (the number of captures,
  // including the match itself), index (the index where the match starts),
  // and input string. |length| and |index| are expected to be tagged, and
  // |input| must be a string.
  Node* AllocateRegExpResult(Node* context, Node* length, Node* index,
                             Node* input);

  Node* AllocateNameDictionary(int at_least_space_for);
  Node* AllocateNameDictionary(Node* at_least_space_for);
  Node* AllocateNameDictionaryWithCapacity(Node* capacity);
  Node* CopyNameDictionary(Node* dictionary, Label* large_object_fallback);

  Node* AllocateJSObjectFromMap(Node* map, Node* properties = nullptr,
                                Node* elements = nullptr,
                                AllocationFlags flags = kNone);

  void InitializeJSObjectFromMap(Node* object, Node* map, Node* size,
                                 Node* properties = nullptr,
                                 Node* elements = nullptr);

  void InitializeJSObjectBody(Node* object, Node* map, Node* size,
                              int start_offset = JSObject::kHeaderSize);

  // Allocate a JSArray without elements and initialize the header fields.
  Node* AllocateUninitializedJSArrayWithoutElements(ElementsKind kind,
                                                    Node* array_map,
                                                    Node* length,
                                                    Node* allocation_site);
  // Allocate and return a JSArray with initialized header fields and its
  // uninitialized elements.
  // The ParameterMode argument is only used for the capacity parameter.
  std::pair<Node*, Node*> AllocateUninitializedJSArrayWithElements(
      ElementsKind kind, Node* array_map, Node* length, Node* allocation_site,
      Node* capacity, ParameterMode capacity_mode = INTPTR_PARAMETERS);
  // Allocate a JSArray and fill elements with the hole.
  // The ParameterMode argument is only used for the capacity parameter.
  Node* AllocateJSArray(ElementsKind kind, Node* array_map, Node* capacity,
                        Node* length, Node* allocation_site = nullptr,
                        ParameterMode capacity_mode = INTPTR_PARAMETERS);

  Node* AllocateFixedArray(ElementsKind kind, Node* capacity,
                           ParameterMode mode = INTPTR_PARAMETERS,
                           AllocationFlags flags = kNone);

  // Perform CreateArrayIterator (ES6 #sec-createarrayiterator).
  Node* CreateArrayIterator(Node* array, Node* array_map, Node* array_type,
                            Node* context, IterationKind mode);

  Node* AllocateJSArrayIterator(Node* array, Node* array_map, Node* map);

  Node* TypedArraySpeciesCreateByLength(Node* context, Node* originalArray,
                                        Node* len);

  void FillFixedArrayWithValue(ElementsKind kind, Node* array, Node* from_index,
                               Node* to_index,
                               Heap::RootListIndex value_root_index,
                               ParameterMode mode = INTPTR_PARAMETERS);

  // Copies all elements from |from_array| of |length| size to
  // |to_array| of the same size respecting the elements kind.
  void CopyFixedArrayElements(
      ElementsKind kind, Node* from_array, Node* to_array, Node* length,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS) {
    CopyFixedArrayElements(kind, from_array, kind, to_array, length, length,
                           barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| to |to_array| of
  // |capacity| size respecting both array's elements kinds.
  void CopyFixedArrayElements(
      ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
      Node* to_array, Node* element_count, Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS);

  // Copies |character_count| elements from |from_string| to |to_string|
  // starting at the |from_index|'th character. |from_string| and |to_string|
  // can either be one-byte strings or two-byte strings, although if
  // |from_string| is two-byte, then |to_string| must be two-byte.
  // |from_index|, |to_index| and |character_count| must be either Smis or
  // intptr_ts depending on |mode| s.t. 0 <= |from_index| <= |from_index| +
  // |character_count| <= from_string.length and 0 <= |to_index| <= |to_index| +
  // |character_count| <= to_string.length.
  void CopyStringCharacters(Node* from_string, Node* to_string,
                            Node* from_index, Node* to_index,
                            Node* character_count,
                            String::Encoding from_encoding,
                            String::Encoding to_encoding, ParameterMode mode);

  // Loads an element from |array| of |from_kind| elements by given |offset|
  // (NOTE: not index!), does a hole check if |if_hole| is provided and
  // converts the value so that it becomes ready for storing to array of
  // |to_kind| elements.
  Node* LoadElementAndPrepareForStore(Node* array, Node* offset,
                                      ElementsKind from_kind,
                                      ElementsKind to_kind, Label* if_hole);

  Node* CalculateNewElementsCapacity(Node* old_capacity,
                                     ParameterMode mode = INTPTR_PARAMETERS);

  // Tries to grow the |elements| array of given |object| to store the |key|
  // or bails out if the growing gap is too big. Returns new elements.
  Node* TryGrowElementsCapacity(Node* object, Node* elements, ElementsKind kind,
                                Node* key, Label* bailout);

  // Tries to grow the |capacity|-length |elements| array of given |object|
  // to store the |key| or bails out if the growing gap is too big. Returns
  // new elements.
  Node* TryGrowElementsCapacity(Node* object, Node* elements, ElementsKind kind,
                                Node* key, Node* capacity, ParameterMode mode,
                                Label* bailout);

  // Grows elements capacity of given object. Returns new elements.
  Node* GrowElementsCapacity(Node* object, Node* elements,
                             ElementsKind from_kind, ElementsKind to_kind,
                             Node* capacity, Node* new_capacity,
                             ParameterMode mode, Label* bailout);

  // Given a need to grow by |growth|, allocate an appropriate new capacity
  // if necessary, and return a new elements FixedArray object. Label |bailout|
  // is followed for allocation failure.
  void PossiblyGrowElementsCapacity(ParameterMode mode, ElementsKind kind,
                                    Node* array, Node* length,
                                    Variable* var_elements, Node* growth,
                                    Label* bailout);

  // Allocation site manipulation
  void InitializeAllocationMemento(Node* base_allocation,
                                   int base_allocation_size,
                                   Node* allocation_site);

  Node* TryTaggedToFloat64(Node* value, Label* if_valueisnotnumber);
  Node* TruncateTaggedToFloat64(Node* context, Node* value);
  Node* TruncateTaggedToWord32(Node* context, Node* value);
  // Truncate the floating point value of a HeapNumber to an Int32.
  Node* TruncateHeapNumberValueToWord32(Node* object);

  // Conversions.
  Node* ChangeFloat64ToTagged(Node* value);
  Node* ChangeInt32ToTagged(Node* value);
  Node* ChangeUint32ToTagged(Node* value);
  Node* ChangeNumberToFloat64(Node* value);
  Node* ChangeNumberToIntPtr(Node* value);

  Node* TimesPointerSize(Node* value);

  // Type conversions.
  // Throws a TypeError for {method_name} if {value} is not coercible to Object,
  // or returns the {value} converted to a String otherwise.
  Node* ToThisString(Node* context, Node* value, char const* method_name);
  // Throws a TypeError for {method_name} if {value} is neither of the given
  // {primitive_type} nor a JSValue wrapping a value of {primitive_type}, or
  // returns the {value} (or wrapped value) otherwise.
  Node* ToThisValue(Node* context, Node* value, PrimitiveType primitive_type,
                    char const* method_name);

  // Throws a TypeError for {method_name} if {value} is not of the given
  // instance type. Returns {value}'s map.
  Node* ThrowIfNotInstanceType(Node* context, Node* value,
                               InstanceType instance_type,
                               char const* method_name);

  // Type checks.
  // Check whether the map is for an object with special properties, such as a
  // JSProxy or an object with interceptors.
  Node* InstanceTypeEqual(Node* instance_type, int type);
  Node* IsSpecialReceiverMap(Node* map);
  Node* IsSpecialReceiverInstanceType(Node* instance_type);
  Node* IsStringInstanceType(Node* instance_type);
  Node* IsOneByteStringInstanceType(Node* instance_type);
  Node* IsExternalStringInstanceType(Node* instance_type);
  Node* IsShortExternalStringInstanceType(Node* instance_type);
  Node* IsSequentialStringInstanceType(Node* instance_type);
  Node* IsConsStringInstanceType(Node* instance_type);
  Node* IsIndirectStringInstanceType(Node* instance_type);
  Node* IsString(Node* object);
  Node* IsJSObjectMap(Node* map);
  Node* IsJSObject(Node* object);
  Node* IsJSGlobalProxy(Node* object);
  Node* IsJSReceiverInstanceType(Node* instance_type);
  Node* IsJSReceiver(Node* object);
  Node* IsJSReceiverMap(Node* map);
  Node* IsMap(Node* object);
  Node* IsCallableMap(Node* map);
  Node* IsDeprecatedMap(Node* map);
  Node* IsCallable(Node* object);
  Node* IsBoolean(Node* object);
  Node* IsPropertyCell(Node* object);
  Node* IsAccessorInfo(Node* object);
  Node* IsAccessorPair(Node* object);
  Node* IsHeapNumber(Node* object);
  Node* IsName(Node* object);
  Node* IsSymbol(Node* object);
  Node* IsPrivateSymbol(Node* object);
  Node* IsJSValueInstanceType(Node* instance_type);
  Node* IsJSValue(Node* object);
  Node* IsJSValueMap(Node* map);
  Node* IsJSArrayInstanceType(Node* instance_type);
  Node* IsJSArray(Node* object);
  Node* IsJSArrayMap(Node* object);
  Node* IsNativeContext(Node* object);
  Node* IsWeakCell(Node* object);
  Node* IsFixedDoubleArray(Node* object);
  Node* IsHashTable(Node* object);
  Node* IsDictionary(Node* object);
  Node* IsUnseededNumberDictionary(Node* object);
  Node* IsConstructorMap(Node* map);
  Node* IsJSFunctionInstanceType(Node* instance_type);
  Node* IsJSFunction(Node* object);
  Node* IsJSFunctionMap(Node* object);
  Node* IsJSTypedArray(Node* object);
  Node* IsJSArrayBuffer(Node* object);
  Node* IsFixedTypedArray(Node* object);
  Node* IsJSRegExp(Node* object);
  Node* IsFeedbackVector(Node* object);

  // True iff |object| is a Smi or a HeapNumber.
  Node* IsNumber(Node* object);

  // True iff |number| is either a Smi, or a HeapNumber whose value is not
  // within Smi range.
  Node* IsNumberNormalized(Node* number);
  Node* IsNumberPositive(Node* number);

  // ElementsKind helpers:
  Node* IsFastElementsKind(Node* elements_kind);
  Node* IsHoleyFastElementsKind(Node* elements_kind);
  Node* IsElementsKindGreaterThan(Node* target_kind,
                                  ElementsKind reference_kind);

  // String helpers.
  // Load a character from a String (might flatten a ConsString).
  Node* StringCharCodeAt(Node* string, Node* index,
                         ParameterMode parameter_mode = SMI_PARAMETERS);
  // Return the single character string with only {code}.
  Node* StringFromCharCode(Node* code);

  enum class SubStringFlags { NONE, FROM_TO_ARE_BOUNDED };

  // Return a new string object which holds a substring containing the range
  // [from,to[ of string.  |from| and |to| are expected to be tagged.
  // If flags has the value FROM_TO_ARE_BOUNDED then from and to are in
  // the range [0, string-length)
  Node* SubString(Node* context, Node* string, Node* from, Node* to,
                  SubStringFlags flags = SubStringFlags::NONE);

  // Return a new string object produced by concatenating |first| with |second|.
  Node* StringAdd(Node* context, Node* first, Node* second,
                  AllocationFlags flags = kNone);

  // Unpack the external string, returning a pointer that (offset-wise) looks
  // like a sequential string.
  // Note that this pointer is not tagged and does not point to a real
  // sequential string instance, and may only be used to access the string
  // data. The pointer is GC-safe as long as a reference to the container
  // ExternalString is live.
  // |string| must be an external string. Bailout for short external strings.
  Node* TryDerefExternalString(Node* const string, Node* const instance_type,
                               Label* if_bailout);

  // Check if |var_string| has an indirect (thin or flat cons) string type,
  // and unpack it if so.
  void MaybeDerefIndirectString(Variable* var_string, Node* instance_type,
                                Variable* var_did_something);
  // Check if |var_left| or |var_right| has an indirect (thin or flat cons)
  // string type, and unpack it/them if so. Fall through if nothing was done.
  void MaybeDerefIndirectStrings(Variable* var_left, Node* left_instance_type,
                                 Variable* var_right, Node* right_instance_type,
                                 Label* did_something);

  Node* StringFromCodePoint(Node* codepoint, UnicodeEncoding encoding);

  // Type conversion helpers.
  // Convert a String to a Number.
  Node* StringToNumber(Node* context, Node* input);
  Node* NumberToString(Node* context, Node* input);
  // Convert an object to a name.
  Node* ToName(Node* context, Node* input);
  // Convert a Non-Number object to a Number.
  Node* NonNumberToNumber(Node* context, Node* input);
  // Convert any object to a Number.
  Node* ToNumber(Node* context, Node* input);

  // Converts |input| to one of 2^32 integer values in the range 0 through
  // 2^32-1, inclusive.
  // ES#sec-touint32
  compiler::Node* ToUint32(compiler::Node* context, compiler::Node* input);

  // Convert any object to a String.
  Node* ToString(Node* context, Node* input);
  Node* ToString_Inline(Node* const context, Node* const input);

  // Convert any object to a Primitive.
  Node* JSReceiverToPrimitive(Node* context, Node* input);

  enum ToIntegerTruncationMode {
    kNoTruncation,
    kTruncateMinusZero,
  };

  // ES6 7.1.17 ToIndex, but jumps to range_error if the result is not a Smi.
  Node* ToSmiIndex(Node* const input, Node* const context, Label* range_error);

  // ES6 7.1.15 ToLength, but jumps to range_error if the result is not a Smi.
  Node* ToSmiLength(Node* input, Node* const context, Label* range_error);

  // ES6 7.1.15 ToLength, but with inlined fast path.
  Node* ToLength_Inline(Node* const context, Node* const input);

  // Convert any object to an Integer.
  Node* ToInteger(Node* context, Node* input,
                  ToIntegerTruncationMode mode = kNoTruncation);

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word32|. Returns result as an uint32 node.
  template <typename T>
  Node* DecodeWord32(Node* word32) {
    return DecodeWord32(word32, T::kShift, T::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word|. Returns result as a word-size node.
  template <typename T>
  Node* DecodeWord(Node* word) {
    return DecodeWord(word, T::kShift, T::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word32|. Returns result as a word-size node.
  template <typename T>
  Node* DecodeWordFromWord32(Node* word32) {
    return DecodeWord<T>(ChangeUint32ToWord(word32));
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |T| in |word|. Returns result as an uint32 node.
  template <typename T>
  Node* DecodeWord32FromWord(Node* word) {
    return TruncateWordToWord32(DecodeWord<T>(word));
  }

  // Decodes an unsigned (!) value from |word32| to an uint32 node.
  Node* DecodeWord32(Node* word32, uint32_t shift, uint32_t mask);

  // Decodes an unsigned (!) value from |word| to a word-size node.
  Node* DecodeWord(Node* word, uint32_t shift, uint32_t mask);

  // Returns true if any of the |T|'s bits in given |word32| are set.
  template <typename T>
  Node* IsSetWord32(Node* word32) {
    return IsSetWord32(word32, T::kMask);
  }

  // Returns true if any of the mask's bits in given |word32| are set.
  Node* IsSetWord32(Node* word32, uint32_t mask) {
    return Word32NotEqual(Word32And(word32, Int32Constant(mask)),
                          Int32Constant(0));
  }

  // Returns true if any of the |T|'s bits in given |word| are set.
  template <typename T>
  Node* IsSetWord(Node* word) {
    return IsSetWord(word, T::kMask);
  }

  // Returns true if any of the mask's bits in given |word| are set.
  Node* IsSetWord(Node* word, uint32_t mask) {
    return WordNotEqual(WordAnd(word, IntPtrConstant(mask)), IntPtrConstant(0));
  }

  // Returns true if any of the mask's bit are set in the given Smi.
  // Smi-encoding of the mask is performed implicitly!
  Node* IsSetSmi(Node* smi, int untagged_mask) {
    intptr_t mask_word = bit_cast<intptr_t>(Smi::FromInt(untagged_mask));
    return WordNotEqual(
        WordAnd(BitcastTaggedToWord(smi), IntPtrConstant(mask_word)),
        IntPtrConstant(0));
  }

  // Returns true if all of the |T|'s bits in given |word32| are clear.
  template <typename T>
  Node* IsClearWord32(Node* word32) {
    return IsClearWord32(word32, T::kMask);
  }

  // Returns true if all of the mask's bits in given |word32| are clear.
  Node* IsClearWord32(Node* word32, uint32_t mask) {
    return Word32Equal(Word32And(word32, Int32Constant(mask)),
                       Int32Constant(0));
  }

  // Returns true if all of the |T|'s bits in given |word| are clear.
  template <typename T>
  Node* IsClearWord(Node* word) {
    return IsClearWord(word, T::kMask);
  }

  // Returns true if all of the mask's bits in given |word| are clear.
  Node* IsClearWord(Node* word, uint32_t mask) {
    return WordEqual(WordAnd(word, IntPtrConstant(mask)), IntPtrConstant(0));
  }

  void SetCounter(StatsCounter* counter, int value);
  void IncrementCounter(StatsCounter* counter, int delta);
  void DecrementCounter(StatsCounter* counter, int delta);

  void Increment(Variable& variable, int value = 1,
                 ParameterMode mode = INTPTR_PARAMETERS);

  // Generates "if (false) goto label" code. Useful for marking a label as
  // "live" to avoid assertion failures during graph building. In the resulting
  // code this check will be eliminated.
  void Use(Label* label);

  // Various building blocks for stubs doing property lookups.

  // |if_notinternalized| is optional; |if_bailout| will be used by default.
  void TryToName(Node* key, Label* if_keyisindex, Variable* var_index,
                 Label* if_keyisunique, Variable* var_unique, Label* if_bailout,
                 Label* if_notinternalized = nullptr);

  // Performs a hash computation and string table lookup for the given string,
  // and jumps to:
  // - |if_index| if the string is an array index like "123"; |var_index|
  //              will contain the intptr representation of that index.
  // - |if_internalized| if the string exists in the string table; the
  //                     internalized version will be in |var_internalized|.
  // - |if_not_internalized| if the string is not in the string table (but
  //                         does not add it).
  // - |if_bailout| for unsupported cases (e.g. uncachable array index).
  void TryInternalizeString(Node* string, Label* if_index, Variable* var_index,
                            Label* if_internalized, Variable* var_internalized,
                            Label* if_not_internalized, Label* if_bailout);

  // Calculates array index for given dictionary entry and entry field.
  // See Dictionary::EntryToIndex().
  template <typename Dictionary>
  Node* EntryToIndex(Node* entry, int field_index);
  template <typename Dictionary>
  Node* EntryToIndex(Node* entry) {
    return EntryToIndex<Dictionary>(entry, Dictionary::kEntryKeyIndex);
  }

  // Loads the details for the entry with the given key_index.
  // Returns an untagged int32.
  template <class ContainerType>
  Node* LoadDetailsByKeyIndex(Node* container, Node* key_index) {
    const int kKeyToDetailsOffset =
        (ContainerType::kEntryDetailsIndex - ContainerType::kEntryKeyIndex) *
        kPointerSize;
    return LoadAndUntagToWord32FixedArrayElement(container, key_index,
                                                 kKeyToDetailsOffset);
  }

  // Loads the value for the entry with the given key_index.
  // Returns a tagged value.
  template <class ContainerType>
  Node* LoadValueByKeyIndex(Node* container, Node* key_index) {
    const int kKeyToValueOffset =
        (ContainerType::kEntryValueIndex - ContainerType::kEntryKeyIndex) *
        kPointerSize;
    return LoadFixedArrayElement(container, key_index, kKeyToValueOffset);
  }

  // Stores the details for the entry with the given key_index.
  // |details| must be a Smi.
  template <class ContainerType>
  void StoreDetailsByKeyIndex(Node* container, Node* key_index, Node* details) {
    const int kKeyToDetailsOffset =
        (ContainerType::kEntryDetailsIndex - ContainerType::kEntryKeyIndex) *
        kPointerSize;
    StoreFixedArrayElement(container, key_index, details, SKIP_WRITE_BARRIER,
                           kKeyToDetailsOffset);
  }

  // Stores the value for the entry with the given key_index.
  template <class ContainerType>
  void StoreValueByKeyIndex(
      Node* container, Node* key_index, Node* value,
      WriteBarrierMode write_barrier = UPDATE_WRITE_BARRIER) {
    const int kKeyToValueOffset =
        (ContainerType::kEntryValueIndex - ContainerType::kEntryKeyIndex) *
        kPointerSize;
    StoreFixedArrayElement(container, key_index, value, write_barrier,
                           kKeyToValueOffset);
  }

  // Calculate a valid size for the a hash table.
  Node* HashTableComputeCapacity(Node* at_least_space_for);

  template <class Dictionary>
  Node* GetNumberOfElements(Node* dictionary) {
    return LoadFixedArrayElement(dictionary,
                                 Dictionary::kNumberOfElementsIndex);
  }

  template <class Dictionary>
  void SetNumberOfElements(Node* dictionary, Node* num_elements_smi) {
    StoreFixedArrayElement(dictionary, Dictionary::kNumberOfElementsIndex,
                           num_elements_smi, SKIP_WRITE_BARRIER);
  }

  template <class Dictionary>
  Node* GetNumberOfDeletedElements(Node* dictionary) {
    return LoadFixedArrayElement(dictionary,
                                 Dictionary::kNumberOfDeletedElementsIndex);
  }

  template <class Dictionary>
  void SetNumberOfDeletedElements(Node* dictionary, Node* num_deleted_smi) {
    StoreFixedArrayElement(dictionary,
                           Dictionary::kNumberOfDeletedElementsIndex,
                           num_deleted_smi, SKIP_WRITE_BARRIER);
  }

  template <class Dictionary>
  Node* GetCapacity(Node* dictionary) {
    return LoadFixedArrayElement(dictionary, Dictionary::kCapacityIndex);
  }

  template <class Dictionary>
  Node* GetNextEnumerationIndex(Node* dictionary);

  template <class Dictionary>
  void SetNextEnumerationIndex(Node* dictionary, Node* next_enum_index_smi);

  // Looks up an entry in a NameDictionaryBase successor. If the entry is found
  // control goes to {if_found} and {var_name_index} contains an index of the
  // key field of the entry found. If the key is not found control goes to
  // {if_not_found}.
  static const int kInlinedDictionaryProbes = 4;
  enum LookupMode { kFindExisting, kFindInsertionIndex };
  template <typename Dictionary>
  void NameDictionaryLookup(Node* dictionary, Node* unique_name,
                            Label* if_found, Variable* var_name_index,
                            Label* if_not_found,
                            int inlined_probes = kInlinedDictionaryProbes,
                            LookupMode mode = kFindExisting);

  Node* ComputeIntegerHash(Node* key, Node* seed);

  template <typename Dictionary>
  void NumberDictionaryLookup(Node* dictionary, Node* intptr_index,
                              Label* if_found, Variable* var_entry,
                              Label* if_not_found);

  template <class Dictionary>
  void FindInsertionEntry(Node* dictionary, Node* key, Variable* var_key_index);

  template <class Dictionary>
  void InsertEntry(Node* dictionary, Node* key, Node* value, Node* index,
                   Node* enum_index);

  template <class Dictionary>
  void Add(Node* dictionary, Node* key, Node* value, Label* bailout);

  // Tries to check if {object} has own {unique_name} property.
  void TryHasOwnProperty(Node* object, Node* map, Node* instance_type,
                         Node* unique_name, Label* if_found,
                         Label* if_not_found, Label* if_bailout);

  // Tries to get {object}'s own {unique_name} property value. If the property
  // is an accessor then it also calls a getter. If the property is a double
  // field it re-wraps value in an immutable heap number.
  void TryGetOwnProperty(Node* context, Node* receiver, Node* object, Node* map,
                         Node* instance_type, Node* unique_name,
                         Label* if_found, Variable* var_value,
                         Label* if_not_found, Label* if_bailout);

  Node* GetProperty(Node* context, Node* receiver, Handle<Name> name) {
    return GetProperty(context, receiver, HeapConstant(name));
  }

  Node* GetProperty(Node* context, Node* receiver, Node* const name) {
    return CallStub(CodeFactory::GetProperty(isolate()), context, receiver,
                    name);
  }

  template <class... TArgs>
  Node* CallBuiltin(Builtins::Name id, Node* context, TArgs... args) {
    return CallStub(Builtins::CallableFor(isolate(), id), context, args...);
  }

  template <class... TArgs>
  Node* TailCallBuiltin(Builtins::Name id, Node* context, TArgs... args) {
    return TailCallStub(Builtins::CallableFor(isolate(), id), context, args...);
  }

  void LoadPropertyFromFastObject(Node* object, Node* map, Node* descriptors,
                                  Node* name_index, Variable* var_details,
                                  Variable* var_value);

  void LoadPropertyFromNameDictionary(Node* dictionary, Node* entry,
                                      Variable* var_details,
                                      Variable* var_value);

  void LoadPropertyFromGlobalDictionary(Node* dictionary, Node* entry,
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
  void TryLookupProperty(Node* object, Node* map, Node* instance_type,
                         Node* unique_name, Label* if_found_fast,
                         Label* if_found_dict, Label* if_found_global,
                         Variable* var_meta_storage, Variable* var_name_index,
                         Label* if_not_found, Label* if_bailout);

  // This method jumps to if_found if the element is known to exist. To
  // if_absent if it's known to not exist. To if_not_found if the prototype
  // chain needs to be checked. And if_bailout if the lookup is unsupported.
  void TryLookupElement(Node* object, Node* map, Node* instance_type,
                        Node* intptr_index, Label* if_found, Label* if_absent,
                        Label* if_not_found, Label* if_bailout);

  // This is a type of a lookup in holder generator function. In case of a
  // property lookup the {key} is guaranteed to be an unique name and in case of
  // element lookup the key is an Int32 index.
  typedef std::function<void(Node* receiver, Node* holder, Node* map,
                             Node* instance_type, Node* key, Label* next_holder,
                             Label* if_bailout)>
      LookupInHolder;

  // Generic property prototype chain lookup generator.
  // For properties it generates lookup using given {lookup_property_in_holder}
  // and for elements it uses {lookup_element_in_holder}.
  // Upon reaching the end of prototype chain the control goes to {if_end}.
  // If it can't handle the case {receiver}/{key} case then the control goes
  // to {if_bailout}.
  void TryPrototypeChainLookup(Node* receiver, Node* key,
                               const LookupInHolder& lookup_property_in_holder,
                               const LookupInHolder& lookup_element_in_holder,
                               Label* if_end, Label* if_bailout);

  // Instanceof helpers.
  // ES6 section 7.3.19 OrdinaryHasInstance (C, O)
  Node* OrdinaryHasInstance(Node* context, Node* callable, Node* object);

  // Load type feedback vector from the stub caller's frame.
  Node* LoadFeedbackVectorForStub();

  // Update the type feedback vector.
  void UpdateFeedback(Node* feedback, Node* feedback_vector, Node* slot_id);

  // Combine the new feedback with the existing_feedback.
  void CombineFeedback(Variable* existing_feedback, Node* feedback);

  // Check if a property name might require protector invalidation when it is
  // used for a property store or deletion.
  void CheckForAssociatedProtector(Node* name, Label* if_protector);

  Node* LoadReceiverMap(Node* receiver);

  // Emits keyed sloppy arguments load. Returns either the loaded value.
  Node* LoadKeyedSloppyArguments(Node* receiver, Node* key, Label* bailout) {
    return EmitKeyedSloppyArguments(receiver, key, nullptr, bailout);
  }

  // Emits keyed sloppy arguments store.
  void StoreKeyedSloppyArguments(Node* receiver, Node* key, Node* value,
                                 Label* bailout) {
    DCHECK_NOT_NULL(value);
    EmitKeyedSloppyArguments(receiver, key, value, bailout);
  }

  // Loads script context from the script context table.
  Node* LoadScriptContext(Node* context, int context_index);

  Node* Int32ToUint8Clamped(Node* int32_value);
  Node* Float64ToUint8Clamped(Node* float64_value);

  Node* PrepareValueForWriteToTypedArray(Node* key, ElementsKind elements_kind,
                                         Label* bailout);

  // Store value to an elements array with given elements kind.
  void StoreElement(Node* elements, ElementsKind kind, Node* index, Node* value,
                    ParameterMode mode);

  void EmitElementStore(Node* object, Node* key, Node* value, bool is_jsarray,
                        ElementsKind elements_kind,
                        KeyedAccessStoreMode store_mode, Label* bailout);

  Node* CheckForCapacityGrow(Node* object, Node* elements, ElementsKind kind,
                             Node* length, Node* key, ParameterMode mode,
                             bool is_js_array, Label* bailout);

  Node* CopyElementsOnWrite(Node* object, Node* elements, ElementsKind kind,
                            Node* length, ParameterMode mode, Label* bailout);

  void TransitionElementsKind(Node* object, Node* map, ElementsKind from_kind,
                              ElementsKind to_kind, bool is_jsarray,
                              Label* bailout);

  void TrapAllocationMemento(Node* object, Label* memento_found);

  Node* PageFromAddress(Node* address);

  // Create a new weak cell with a specified value and install it into a
  // feedback vector.
  Node* CreateWeakCellInFeedbackVector(Node* feedback_vector, Node* slot,
                                       Node* value);

  // Create a new AllocationSite and install it into a feedback vector.
  Node* CreateAllocationSiteInFeedbackVector(Node* feedback_vector, Node* slot);

  // Given a recently allocated object {object}, with map {initial_map},
  // initialize remaining fields appropriately to comply with slack tracking.
  void HandleSlackTracking(Node* context, Node* object, Node* initial_map,
                           int start_offset);

  enum class IndexAdvanceMode { kPre, kPost };

  typedef std::function<void(Node* index)> FastLoopBody;

  Node* BuildFastLoop(const VariableList& var_list, Node* start_index,
                      Node* end_index, const FastLoopBody& body, int increment,
                      ParameterMode parameter_mode,
                      IndexAdvanceMode advance_mode = IndexAdvanceMode::kPre);

  Node* BuildFastLoop(Node* start_index, Node* end_index,
                      const FastLoopBody& body, int increment,
                      ParameterMode parameter_mode,
                      IndexAdvanceMode advance_mode = IndexAdvanceMode::kPre) {
    return BuildFastLoop(VariableList(0, zone()), start_index, end_index, body,
                         increment, parameter_mode, advance_mode);
  }

  enum class ForEachDirection { kForward, kReverse };

  typedef std::function<void(Node* fixed_array, Node* offset)>
      FastFixedArrayForEachBody;

  void BuildFastFixedArrayForEach(
      const CodeStubAssembler::VariableList& vars, Node* fixed_array,
      ElementsKind kind, Node* first_element_inclusive,
      Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
      ParameterMode mode = INTPTR_PARAMETERS,
      ForEachDirection direction = ForEachDirection::kReverse);

  void BuildFastFixedArrayForEach(
      Node* fixed_array, ElementsKind kind, Node* first_element_inclusive,
      Node* last_element_exclusive, const FastFixedArrayForEachBody& body,
      ParameterMode mode = INTPTR_PARAMETERS,
      ForEachDirection direction = ForEachDirection::kReverse) {
    CodeStubAssembler::VariableList list(0, zone());
    BuildFastFixedArrayForEach(list, fixed_array, kind, first_element_inclusive,
                               last_element_exclusive, body, mode, direction);
  }

  Node* GetArrayAllocationSize(Node* element_count, ElementsKind kind,
                               ParameterMode mode, int header_size) {
    return ElementOffsetFromIndex(element_count, kind, mode, header_size);
  }

  Node* GetFixedArrayAllocationSize(Node* element_count, ElementsKind kind,
                                    ParameterMode mode) {
    return GetArrayAllocationSize(element_count, kind, mode,
                                  FixedArray::kHeaderSize);
  }

  void GotoIfFixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                               Label* doesnt_fit, int base_size,
                                               ParameterMode mode);

  void InitializeFieldsWithRoot(Node* object, Node* start_offset,
                                Node* end_offset, Heap::RootListIndex root);

  enum RelationalComparisonMode {
    kLessThan,
    kLessThanOrEqual,
    kGreaterThan,
    kGreaterThanOrEqual
  };

  Node* RelationalComparison(RelationalComparisonMode mode, Node* lhs,
                             Node* rhs, Node* context,
                             Variable* var_type_feedback = nullptr);

  void BranchIfNumericRelationalComparison(RelationalComparisonMode mode,
                                           Node* lhs, Node* rhs, Label* if_true,
                                           Label* if_false);

  void GotoUnlessNumberLessThan(Node* lhs, Node* rhs, Label* if_false);

  Node* Equal(Node* lhs, Node* rhs, Node* context,
              Variable* var_type_feedback = nullptr);

  Node* StrictEqual(Node* lhs, Node* rhs,
                    Variable* var_type_feedback = nullptr);

  // ECMA#sec-samevalue
  // Similar to StrictEqual except that NaNs are treated as equal and minus zero
  // differs from positive zero.
  // Unlike Equal and StrictEqual, returns a value suitable for use in Branch
  // instructions, e.g. Branch(SameValue(...), &label).
  Node* SameValue(Node* lhs, Node* rhs);

  Node* HasProperty(
      Node* object, Node* key, Node* context,
      Runtime::FunctionId fallback_runtime_function_id = Runtime::kHasProperty);

  Node* ClassOf(Node* object);

  Node* Typeof(Node* value);

  Node* GetSuperConstructor(Node* value, Node* context);

  Node* InstanceOf(Node* object, Node* callable, Node* context);

  // Debug helpers
  Node* IsDebugActive();

  // TypedArray/ArrayBuffer helpers
  Node* IsDetachedBuffer(Node* buffer);

  Node* ElementOffsetFromIndex(Node* index, ElementsKind kind,
                               ParameterMode mode, int base_size = 0);

  Node* AllocateFunctionWithMapAndContext(Node* map, Node* shared_info,
                                          Node* context);

  // Promise helpers
  Node* IsPromiseHookEnabledOrDebugIsActive();

  Node* AllocatePromiseReactionJobInfo(Node* value, Node* tasks,
                                       Node* deferred_promise,
                                       Node* deferred_on_resolve,
                                       Node* deferred_on_reject, Node* context);

  // Helpers for StackFrame markers.
  Node* MarkerIsFrameType(Node* marker_or_function,
                          StackFrame::Type frame_type);
  Node* MarkerIsNotFrameType(Node* marker_or_function,
                             StackFrame::Type frame_type);

  // Support for printf-style debugging
  void Print(const char* s);
  void Print(const char* prefix, Node* tagged_value);
  inline void Print(Node* tagged_value) { return Print(nullptr, tagged_value); }

  template <class... TArgs>
  Node* MakeTypeError(MessageTemplate::Template message, Node* context,
                      TArgs... args) {
    STATIC_ASSERT(sizeof...(TArgs) <= 3);
    Node* const make_type_error = LoadContextElement(
        LoadNativeContext(context), Context::MAKE_TYPE_ERROR_INDEX);
    return CallJS(CodeFactory::Call(isolate()), context, make_type_error,
                  UndefinedConstant(), SmiConstant(message), args...);
  }

 protected:
  void DescriptorLookup(Node* unique_name, Node* descriptors, Node* bitfield3,
                        Label* if_found, Variable* var_name_index,
                        Label* if_not_found);
  void DescriptorLookupLinear(Node* unique_name, Node* descriptors, Node* nof,
                              Label* if_found, Variable* var_name_index,
                              Label* if_not_found);
  void DescriptorLookupBinary(Node* unique_name, Node* descriptors, Node* nof,
                              Label* if_found, Variable* var_name_index,
                              Label* if_not_found);
  // Implements DescriptorArray::ToKeyIndex.
  // Returns an untagged IntPtr.
  Node* DescriptorArrayToKeyIndex(Node* descriptor_number);

  Node* CallGetterIfAccessor(Node* value, Node* details, Node* context,
                             Node* receiver, Label* if_bailout);

  Node* TryToIntptr(Node* key, Label* miss);

  void BranchIfPrototypesHaveNoElements(Node* receiver_map,
                                        Label* definitely_no_elements,
                                        Label* possibly_elements);

 private:
  friend class CodeStubArguments;

  void HandleBreakOnNode();

  Node* AllocateRawDoubleAligned(Node* size_in_bytes, AllocationFlags flags,
                                 Node* top_address, Node* limit_address);
  Node* AllocateRawUnaligned(Node* size_in_bytes, AllocationFlags flags,
                             Node* top_adddress, Node* limit_address);
  Node* AllocateRaw(Node* size_in_bytes, AllocationFlags flags,
                    Node* top_address, Node* limit_address);
  // Allocate and return a JSArray of given total size in bytes with header
  // fields initialized.
  Node* AllocateUninitializedJSArray(ElementsKind kind, Node* array_map,
                                     Node* length, Node* allocation_site,
                                     Node* size_in_bytes);

  Node* SmiShiftBitsConstant();

  // Emits keyed sloppy arguments load if the |value| is nullptr or store
  // otherwise. Returns either the loaded value or |value|.
  Node* EmitKeyedSloppyArguments(Node* receiver, Node* key, Node* value,
                                 Label* bailout);

  Node* AllocateSlicedString(Heap::RootListIndex map_root_index, Node* length,
                             Node* parent, Node* offset);

  Node* AllocateConsString(Heap::RootListIndex map_root_index, Node* length,
                           Node* first, Node* second, AllocationFlags flags);

  // Implements DescriptorArray::number_of_entries.
  // Returns an untagged int32.
  Node* DescriptorArrayNumberOfEntries(Node* descriptors);
  // Implements DescriptorArray::GetSortedKeyIndex.
  // Returns an untagged int32.
  Node* DescriptorArrayGetSortedKeyIndex(Node* descriptors,
                                         Node* descriptor_number);
  // Implements DescriptorArray::GetKey.
  Node* DescriptorArrayGetKey(Node* descriptors, Node* descriptor_number);

  Node* CollectFeedbackForString(Node* instance_type);
  void GenerateEqual_Same(Node* value, Label* if_equal, Label* if_notequal,
                          Variable* var_type_feedback = nullptr);
  Node* AllocAndCopyStringCharacters(Node* context, Node* from,
                                     Node* from_instance_type, Node* from_index,
                                     Node* character_count);

  static const int kElementLoopUnrollThreshold = 8;
};

class CodeStubArguments {
 public:
  typedef compiler::Node Node;

  // |argc| is an intptr value which specifies the number of arguments passed
  // to the builtin excluding the receiver.
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc)
      : CodeStubArguments(assembler, argc, nullptr,
                          CodeStubAssembler::INTPTR_PARAMETERS) {}
  // |argc| is either a smi or intptr depending on |param_mode|
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc, Node* fp,
                    CodeStubAssembler::ParameterMode param_mode);

  Node* GetReceiver() const;

  Node* AtIndexPtr(Node* index, CodeStubAssembler::ParameterMode mode =
                                    CodeStubAssembler::INTPTR_PARAMETERS) const;

  // |index| is zero-based and does not include the receiver
  Node* AtIndex(Node* index, CodeStubAssembler::ParameterMode mode =
                                 CodeStubAssembler::INTPTR_PARAMETERS) const;

  Node* AtIndex(int index) const;

  Node* GetOptionalArgumentValue(int index, Node* default_value);

  Node* GetLength() const { return argc_; }

  typedef std::function<void(Node* arg)> ForEachBodyFunction;

  // Iteration doesn't include the receiver. |first| and |last| are zero-based.
  void ForEach(const ForEachBodyFunction& body, Node* first = nullptr,
               Node* last = nullptr, CodeStubAssembler::ParameterMode mode =
                                         CodeStubAssembler::INTPTR_PARAMETERS) {
    CodeStubAssembler::VariableList list(0, assembler_->zone());
    ForEach(list, body, first, last);
  }

  // Iteration doesn't include the receiver. |first| and |last| are zero-based.
  void ForEach(const CodeStubAssembler::VariableList& vars,
               const ForEachBodyFunction& body, Node* first = nullptr,
               Node* last = nullptr, CodeStubAssembler::ParameterMode mode =
                                         CodeStubAssembler::INTPTR_PARAMETERS);

  void PopAndReturn(Node* value);

 private:
  Node* GetArguments();

  CodeStubAssembler* assembler_;
  CodeStubAssembler::ParameterMode argc_mode_;
  Node* argc_;
  Node* arguments_;
  Node* fp_;
};

class ToDirectStringAssembler : public CodeStubAssembler {
 private:
  enum StringPointerKind { PTR_TO_DATA, PTR_TO_STRING };

 public:
  enum Flag {
    kDontUnpackSlicedStrings = 1 << 0,
  };
  typedef base::Flags<Flag> Flags;

  ToDirectStringAssembler(compiler::CodeAssemblerState* state, Node* string,
                          Flags flags = Flags());

  // Converts flat cons, thin, and sliced strings and returns the direct
  // string. The result can be either a sequential or external string.
  // Jumps to if_bailout if the string if the string is indirect and cannot
  // be unpacked.
  Node* TryToDirect(Label* if_bailout);

  // Returns a pointer to the beginning of the string data.
  // Jumps to if_bailout if the external string cannot be unpacked.
  Node* PointerToData(Label* if_bailout) {
    return TryToSequential(PTR_TO_DATA, if_bailout);
  }

  // Returns a pointer that, offset-wise, looks like a String.
  // Jumps to if_bailout if the external string cannot be unpacked.
  Node* PointerToString(Label* if_bailout) {
    return TryToSequential(PTR_TO_STRING, if_bailout);
  }

  Node* string() { return var_string_.value(); }
  Node* instance_type() { return var_instance_type_.value(); }
  Node* offset() { return var_offset_.value(); }
  Node* is_external() { return var_is_external_.value(); }

 private:
  Node* TryToSequential(StringPointerKind ptr_kind, Label* if_bailout);

  Variable var_string_;
  Variable var_instance_type_;
  Variable var_offset_;
  Variable var_is_external_;

  const Flags flags_;
};

#define CSA_CHECK(csa, x) \
  (csa)->Check([&] { return (x); }, #x, __FILE__, __LINE__)

#ifdef DEBUG
#define CSA_ASSERT(csa, x) \
  (csa)->Assert([&] { return (x); }, #x, __FILE__, __LINE__)
#define CSA_ASSERT_JS_ARGC_OP(csa, Op, op, expected)             \
  (csa)->Assert(                                                 \
      [&] {                                                      \
        compiler::Node* const argc =                             \
            (csa)->Parameter(Descriptor::kActualArgumentsCount); \
        return (csa)->Op(argc, (csa)->Int32Constant(expected));  \
      },                                                         \
      "argc " #op " " #expected, __FILE__, __LINE__)

#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) \
  CSA_ASSERT_JS_ARGC_OP(csa, Word32Equal, ==, expected)

#define CSA_DEBUG_INFO(name) \
  , { #name, __FILE__, __LINE__ }
#define BIND(label) Bind(label CSA_DEBUG_INFO(label))
#define VARIABLE(name, ...) \
  Variable name(this CSA_DEBUG_INFO(name), __VA_ARGS__);

#else  // DEBUG
#define CSA_ASSERT(csa, x) ((void)0)
#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) ((void)0)
#define CSA_DEBUG_INFO(name)
#define BIND(label) Bind(label);
#define VARIABLE(name, ...) Variable name(this, __VA_ARGS__);
#endif  // DEBUG

#ifdef ENABLE_SLOW_DCHECKS
#define CSA_SLOW_ASSERT(csa, x)   \
  if (FLAG_enable_slow_asserts) { \
    CSA_ASSERT(csa, x);           \
  }
#else
#define CSA_SLOW_ASSERT(csa, x) ((void)0)
#endif

DEFINE_OPERATORS_FOR_FLAGS(CodeStubAssembler::AllocationFlags);

}  // namespace internal
}  // namespace v8
#endif  // V8_CODE_STUB_ASSEMBLER_H_
