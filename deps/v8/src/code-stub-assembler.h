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
class CodeStubAssembler;
class StatsCounter;
class StubCache;

enum class PrimitiveType { kBoolean, kNumber, kString, kSymbol };

#define HEAP_CONSTANT_LIST(V)                                                 \
  V(AccessorInfoMap, accessor_info_map, AccessorInfoMap)                      \
  V(AccessorPairMap, accessor_pair_map, AccessorPairMap)                      \
  V(AllocationSiteMap, allocation_site_map, AllocationSiteMap)                \
  V(BooleanMap, boolean_map, BooleanMap)                                      \
  V(CodeMap, code_map, CodeMap)                                               \
  V(EmptyPropertyDictionary, empty_property_dictionary,                       \
    EmptyPropertyDictionary)                                                  \
  V(EmptyFixedArray, empty_fixed_array, EmptyFixedArray)                      \
  V(EmptySlowElementDictionary, empty_slow_element_dictionary,                \
    EmptySlowElementDictionary)                                               \
  V(empty_string, empty_string, EmptyString)                                  \
  V(EmptyWeakCell, empty_weak_cell, EmptyWeakCell)                            \
  V(FalseValue, false_value, False)                                           \
  V(FeedbackVectorMap, feedback_vector_map, FeedbackVectorMap)                \
  V(FixedArrayMap, fixed_array_map, FixedArrayMap)                            \
  V(FixedCOWArrayMap, fixed_cow_array_map, FixedCOWArrayMap)                  \
  V(FixedDoubleArrayMap, fixed_double_array_map, FixedDoubleArrayMap)         \
  V(FunctionTemplateInfoMap, function_template_info_map,                      \
    FunctionTemplateInfoMap)                                                  \
  V(GlobalPropertyCellMap, global_property_cell_map, PropertyCellMap)         \
  V(has_instance_symbol, has_instance_symbol, HasInstanceSymbol)              \
  V(HeapNumberMap, heap_number_map, HeapNumberMap)                            \
  V(length_string, length_string, LengthString)                               \
  V(ManyClosuresCellMap, many_closures_cell_map, ManyClosuresCellMap)         \
  V(MetaMap, meta_map, MetaMap)                                               \
  V(MinusZeroValue, minus_zero_value, MinusZero)                              \
  V(MutableHeapNumberMap, mutable_heap_number_map, MutableHeapNumberMap)      \
  V(NanValue, nan_value, Nan)                                                 \
  V(NoClosuresCellMap, no_closures_cell_map, NoClosuresCellMap)               \
  V(NullValue, null_value, Null)                                              \
  V(OneClosureCellMap, one_closure_cell_map, OneClosureCellMap)               \
  V(prototype_string, prototype_string, PrototypeString)                      \
  V(SpeciesProtector, species_protector, SpeciesProtector)                    \
  V(StoreHandler0Map, store_handler0_map, StoreHandler0Map)                   \
  V(SymbolMap, symbol_map, SymbolMap)                                         \
  V(TheHoleValue, the_hole_value, TheHole)                                    \
  V(TrueValue, true_value, True)                                              \
  V(Tuple2Map, tuple2_map, Tuple2Map)                                         \
  V(Tuple3Map, tuple3_map, Tuple3Map)                                         \
  V(UndefinedValue, undefined_value, Undefined)                               \
  V(WeakCellMap, weak_cell_map, WeakCellMap)                                  \
  V(SharedFunctionInfoMap, shared_function_info_map, SharedFunctionInfoMap)   \
  V(promise_default_reject_handler_symbol,                                    \
    promise_default_reject_handler_symbol, PromiseDefaultRejectHandlerSymbol) \
  V(promise_default_resolve_handler_symbol,                                   \
    promise_default_resolve_handler_symbol,                                   \
    PromiseDefaultResolveHandlerSymbol)

// Returned from IteratorBuiltinsAssembler::GetIterator(). Struct is declared
// here to simplify use in other generated builtins.
struct IteratorRecord {
 public:
  // iteratorRecord.[[Iterator]]
  compiler::TNode<JSReceiver> object;

  // iteratorRecord.[[NextMethod]]
  compiler::TNode<Object> next;
};

// Provides JavaScript-specific "macro-assembler" functionality on top of the
// CodeAssembler. By factoring the JavaScript-isms out of the CodeAssembler,
// it's possible to add JavaScript-specific useful CodeAssembler "macros"
// without modifying files in the compiler directory (and requiring a review
// from a compiler directory OWNER).
class V8_EXPORT_PRIVATE CodeStubAssembler : public compiler::CodeAssembler {
 public:
  using Node = compiler::Node;
  template <class T>
  using TNode = compiler::TNode<T>;
  template <class T>
  using SloppyTNode = compiler::SloppyTNode<T>;

  template <typename T>
  using LazyNode = std::function<TNode<T>()>;

  CodeStubAssembler(compiler::CodeAssemblerState* state);

  enum AllocationFlag : uint8_t {
    kNone = 0,
    kDoubleAlignment = 1,
    kPretenured = 1 << 1,
    kAllowLargeObjectAllocation = 1 << 2,
  };

  enum SlackTrackingMode { kWithSlackTracking, kNoSlackTracking };

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

  Node* WordToParameter(SloppyTNode<IntPtrT> value, ParameterMode mode) {
    if (mode == SMI_PARAMETERS) return SmiTag(value);
    return value;
  }

  Node* Word32ToParameter(SloppyTNode<Int32T> value, ParameterMode mode) {
    return WordToParameter(ChangeInt32ToIntPtr(value), mode);
  }

  TNode<Smi> ParameterToTagged(Node* value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) return SmiTag(value);
    return UncheckedCast<Smi>(value);
  }

  Node* TaggedToParameter(SloppyTNode<Smi> value, ParameterMode mode) {
    if (mode != SMI_PARAMETERS) return SmiUntag(value);
    return value;
  }

  Node* MatchesParameterMode(Node* value, ParameterMode mode);

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
#define HEAP_CONSTANT_ACCESSOR(rootIndexName, rootAccessorName, name) \
  compiler::TNode<std::remove_reference<decltype(                     \
      *std::declval<Heap>().rootAccessorName())>::type>               \
      name##Constant();
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_ACCESSOR)
#undef HEAP_CONSTANT_ACCESSOR

#define HEAP_CONSTANT_TEST(rootIndexName, rootAccessorName, name) \
  TNode<BoolT> Is##name(SloppyTNode<Object> value);               \
  TNode<BoolT> IsNot##name(SloppyTNode<Object> value);
  HEAP_CONSTANT_LIST(HEAP_CONSTANT_TEST)
#undef HEAP_CONSTANT_TEST

  Node* HashSeed();
  Node* StaleRegisterConstant();

  Node* IntPtrOrSmiConstant(int value, ParameterMode mode);

  bool IsIntPtrOrSmiConstantZero(Node* test, ParameterMode mode);
  bool TryGetIntPtrOrSmiConstantValue(Node* maybe_constant, int* value,
                                      ParameterMode mode);

  // Round the 32bits payload of the provided word up to the next power of two.
  Node* IntPtrRoundUpToPowerOfTwo32(Node* value);
  // Select the maximum of the two provided IntPtr values.
  TNode<IntPtrT> IntPtrMax(SloppyTNode<IntPtrT> left,
                           SloppyTNode<IntPtrT> right);
  // Select the minimum of the two provided IntPtr values.
  TNode<IntPtrT> IntPtrMin(SloppyTNode<IntPtrT> left,
                           SloppyTNode<IntPtrT> right);

  // Float64 operations.
  TNode<Float64T> Float64Ceil(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Floor(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Round(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64RoundToEven(SloppyTNode<Float64T> x);
  TNode<Float64T> Float64Trunc(SloppyTNode<Float64T> x);
  // Select the minimum of the two provided Number values.
  TNode<Object> NumberMax(SloppyTNode<Object> left, SloppyTNode<Object> right);
  // Select the minimum of the two provided Number values.
  TNode<Object> NumberMin(SloppyTNode<Object> left, SloppyTNode<Object> right);

  // Tag a Word as a Smi value.
  TNode<Smi> SmiTag(SloppyTNode<IntPtrT> value);
  // Untag a Smi value as a Word.
  TNode<IntPtrT> SmiUntag(SloppyTNode<Smi> value);

  // Smi conversions.
  TNode<Float64T> SmiToFloat64(SloppyTNode<Smi> value);
  TNode<Smi> SmiFromWord(SloppyTNode<IntPtrT> value) { return SmiTag(value); }
  TNode<Smi> SmiFromWord32(SloppyTNode<Int32T> value);
  TNode<IntPtrT> SmiToWord(SloppyTNode<Smi> value) { return SmiUntag(value); }
  TNode<Int32T> SmiToWord32(SloppyTNode<Smi> value);

  // Smi operations.
#define SMI_ARITHMETIC_BINOP(SmiOpName, IntPtrOpName)                  \
  TNode<Smi> SmiOpName(SloppyTNode<Smi> a, SloppyTNode<Smi> b) {       \
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
  TNode<Smi> SmiMax(SloppyTNode<Smi> a, SloppyTNode<Smi> b);
  TNode<Smi> SmiMin(SloppyTNode<Smi> a, SloppyTNode<Smi> b);
  // Computes a % b for Smi inputs a and b; result is not necessarily a Smi.
  Node* SmiMod(Node* a, Node* b);
  // Computes a * b for Smi inputs a and b; result is not necessarily a Smi.
  TNode<Number> SmiMul(SloppyTNode<Smi> a, SloppyTNode<Smi> b);
  // Tries to computes dividend / divisor for Smi inputs; branching to bailout
  // if the division needs to be performed as a floating point operation.
  Node* TrySmiDiv(Node* dividend, Node* divisor, Label* bailout);

  // Smi | HeapNumber operations.
  Node* NumberInc(Node* value);
  Node* NumberDec(Node* value);
  Node* NumberAdd(Node* a, Node* b);
  Node* NumberSub(Node* a, Node* b);
  void GotoIfNotNumber(Node* value, Label* is_not_number);
  void GotoIfNumber(Node* value, Label* is_number);

  Node* BitwiseOp(Node* left32, Node* right32, Operation bitwise_op);

  // Allocate an object of the given size.
  Node* AllocateInNewSpace(Node* size, AllocationFlags flags = kNone);
  Node* AllocateInNewSpace(int size, AllocationFlags flags = kNone);
  Node* Allocate(Node* size, AllocationFlags flags = kNone);
  Node* Allocate(int size, AllocationFlags flags = kNone);
  Node* InnerAllocate(Node* previous, int offset);
  Node* InnerAllocate(Node* previous, Node* offset);
  Node* IsRegularHeapObjectSize(Node* size);

  typedef std::function<Node*()> NodeGenerator;

  void Assert(const NodeGenerator& condition_body,
              const char* message = nullptr, const char* file = nullptr,
              int line = 0, Node* extra_node1 = nullptr,
              const char* extra_node1_name = "", Node* extra_node2 = nullptr,
              const char* extra_node2_name = "", Node* extra_node3 = nullptr,
              const char* extra_node3_name = "", Node* extra_node4 = nullptr,
              const char* extra_node4_name = "", Node* extra_node5 = nullptr,
              const char* extra_node5_name = "");
  void Check(const NodeGenerator& condition_body, const char* message = nullptr,
             const char* file = nullptr, int line = 0,
             Node* extra_node1 = nullptr, const char* extra_node1_name = "",
             Node* extra_node2 = nullptr, const char* extra_node2_name = "",
             Node* extra_node3 = nullptr, const char* extra_node3_name = "",
             Node* extra_node4 = nullptr, const char* extra_node4_name = "",
             Node* extra_node5 = nullptr, const char* extra_node5_name = "");

  Node* Select(SloppyTNode<BoolT> condition, const NodeGenerator& true_body,
               const NodeGenerator& false_body, MachineRepresentation rep);
  template <class A, class F, class G>
  TNode<A> Select(SloppyTNode<BoolT> condition, const F& true_body,
                  const G& false_body, MachineRepresentation rep) {
    return UncheckedCast<A>(
        Select(condition,
               [&]() -> Node* {
                 return base::implicit_cast<SloppyTNode<A>>(true_body());
               },
               [&]() -> Node* {
                 return base::implicit_cast<SloppyTNode<A>>(false_body());
               },
               rep));
  }

  Node* SelectConstant(Node* condition, Node* true_value, Node* false_value,
                       MachineRepresentation rep);
  template <class A>
  TNode<A> SelectConstant(TNode<BoolT> condition, TNode<A> true_value,
                          TNode<A> false_value, MachineRepresentation rep) {
    return UncheckedCast<A>(
        SelectConstant(condition, static_cast<Node*>(true_value),
                       static_cast<Node*>(false_value), rep));
  }

  Node* SelectInt32Constant(Node* condition, int true_value, int false_value);
  Node* SelectIntPtrConstant(Node* condition, int true_value, int false_value);
  Node* SelectBooleanConstant(Node* condition);
  template <class A>
  TNode<A> SelectTaggedConstant(SloppyTNode<BoolT> condition,
                                TNode<A> true_value,
                                SloppyTNode<A> false_value) {
    static_assert(std::is_base_of<Object, A>::value, "not a tagged type");
    return SelectConstant(condition, true_value, false_value,
                          MachineRepresentation::kTagged);
  }
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

  TNode<Int32T> TruncateWordToWord32(SloppyTNode<IntPtrT> value);

  // Check a value for smi-ness
  TNode<BoolT> TaggedIsSmi(SloppyTNode<Object> a);
  TNode<BoolT> TaggedIsNotSmi(SloppyTNode<Object> a);
  // Check that the value is a non-negative smi.
  TNode<BoolT> TaggedIsPositiveSmi(SloppyTNode<Object> a);
  // Check that a word has a word-aligned address.
  TNode<BoolT> WordIsWordAligned(SloppyTNode<WordT> word);
  TNode<BoolT> WordIsPowerOfTwo(SloppyTNode<IntPtrT> value);

  Node* IsNotTheHole(Node* value) { return Word32BinaryNot(IsTheHole(value)); }

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

  void BranchIfFastJSArray(Node* object, Node* context, Label* if_true,
                           Label* if_false);
  void BranchIfFastJSArrayForCopy(Node* object, Node* context, Label* if_true,
                                  Label* if_false);

  // Branches to {if_true} when --force-slow-path flag has been passed.
  // It's used for testing to ensure that slow path implementation behave
  // equivalent to corresponding fast paths (where applicable).
  //
  // Works only with V8_ENABLE_FORCE_SLOW_PATH compile time flag. Nop otherwise.
  void GotoIfForceSlowPath(Label* if_true);

  // Load value from current frame by given offset in bytes.
  Node* LoadFromFrame(int offset, MachineType rep = MachineType::AnyTagged());
  // Load value from current parent frame by given offset in bytes.
  Node* LoadFromParentFrame(int offset,
                            MachineType rep = MachineType::AnyTagged());

  // Load an object pointer from a buffer that isn't in the heap.
  Node* LoadBufferObject(Node* buffer, int offset,
                         MachineType rep = MachineType::AnyTagged());
  // Load a field from an object on the heap.
  Node* LoadObjectField(SloppyTNode<HeapObject> object, int offset,
                        MachineType rep);
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<Object>>::value,
                         int>::type = 0>
  TNode<T> LoadObjectField(TNode<HeapObject> object, int offset) {
    return CAST(LoadObjectField(object, offset, MachineTypeOf<T>::value));
  }
  template <class T, typename std::enable_if<
                         std::is_convertible<TNode<T>, TNode<UntaggedT>>::value,
                         int>::type = 0>
  TNode<T> LoadObjectField(TNode<HeapObject> object, int offset) {
    return UncheckedCast<T>(
        LoadObjectField(object, offset, MachineTypeOf<T>::value));
  }
  TNode<Object> LoadObjectField(SloppyTNode<HeapObject> object, int offset) {
    return UncheckedCast<Object>(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
  Node* LoadObjectField(SloppyTNode<HeapObject> object,
                        SloppyTNode<IntPtrT> offset, MachineType rep);
  TNode<Object> LoadObjectField(SloppyTNode<HeapObject> object,
                                SloppyTNode<IntPtrT> offset) {
    return UncheckedCast<Object>(
        LoadObjectField(object, offset, MachineType::AnyTagged()));
  }
  // Load a SMI field and untag it.
  TNode<IntPtrT> LoadAndUntagObjectField(SloppyTNode<HeapObject> object,
                                         int offset);
  // Load a SMI field, untag it, and convert to Word32.
  TNode<Int32T> LoadAndUntagToWord32ObjectField(Node* object, int offset);
  // Load a SMI and untag it.
  TNode<IntPtrT> LoadAndUntagSmi(Node* base, int index);
  // Load a SMI root, untag it, and convert to Word32.
  Node* LoadAndUntagToWord32Root(Heap::RootListIndex root_index);

  // Tag a smi and store it.
  Node* StoreAndTagSmi(Node* base, int offset, Node* value);

  // Load the floating point value of a HeapNumber.
  TNode<Float64T> LoadHeapNumberValue(SloppyTNode<HeapNumber> object);
  // Load the Map of an HeapObject.
  TNode<Map> LoadMap(SloppyTNode<HeapObject> object);
  // Load the instance type of an HeapObject.
  TNode<Int32T> LoadInstanceType(SloppyTNode<HeapObject> object);
  // Compare the instance the type of the object against the provided one.
  Node* HasInstanceType(Node* object, InstanceType type);
  Node* DoesntHaveInstanceType(Node* object, InstanceType type);
  Node* TaggedDoesntHaveInstanceType(Node* any_tagged, InstanceType type);
  // Load the properties backing store of a JSObject.
  TNode<HeapObject> LoadSlowProperties(SloppyTNode<JSObject> object);
  TNode<HeapObject> LoadFastProperties(SloppyTNode<JSObject> object);
  // Load the elements backing store of a JSObject.
  TNode<FixedArrayBase> LoadElements(SloppyTNode<JSObject> object);
  // Load the length of a JSArray instance.
  TNode<Object> LoadJSArrayLength(SloppyTNode<JSArray> array);
  // Load the length of a fast JSArray instance. Returns a positive Smi.
  TNode<Smi> LoadFastJSArrayLength(SloppyTNode<JSArray> array);
  // Load the length of a fixed array base instance.
  TNode<Smi> LoadFixedArrayBaseLength(SloppyTNode<FixedArrayBase> array);
  // Load the length of a fixed array base instance.
  TNode<IntPtrT> LoadAndUntagFixedArrayBaseLength(
      SloppyTNode<FixedArrayBase> array);
  // Load the bit field of a Map.
  TNode<Int32T> LoadMapBitField(SloppyTNode<Map> map);
  // Load bit field 2 of a map.
  TNode<Int32T> LoadMapBitField2(SloppyTNode<Map> map);
  // Load bit field 3 of a map.
  TNode<Uint32T> LoadMapBitField3(SloppyTNode<Map> map);
  // Load the instance type of a map.
  TNode<Int32T> LoadMapInstanceType(SloppyTNode<Map> map);
  // Load the ElementsKind of a map.
  TNode<Int32T> LoadMapElementsKind(SloppyTNode<Map> map);
  // Load the instance descriptors of a map.
  TNode<DescriptorArray> LoadMapDescriptors(SloppyTNode<Map> map);
  // Load the prototype of a map.
  TNode<Object> LoadMapPrototype(SloppyTNode<Map> map);
  // Load the prototype info of a map. The result has to be checked if it is a
  // prototype info object or not.
  TNode<PrototypeInfo> LoadMapPrototypeInfo(SloppyTNode<Map> map,
                                            Label* if_has_no_proto_info);
  // Load the instance size of a Map.
  TNode<IntPtrT> LoadMapInstanceSizeInWords(SloppyTNode<Map> map);
  // Load the inobject properties start of a Map (valid only for JSObjects).
  TNode<IntPtrT> LoadMapInobjectPropertiesStartInWords(SloppyTNode<Map> map);
  // Load the constructor function index of a Map (only for primitive maps).
  TNode<IntPtrT> LoadMapConstructorFunctionIndex(SloppyTNode<Map> map);
  // Load the constructor of a Map (equivalent to Map::GetConstructor()).
  TNode<Object> LoadMapConstructor(SloppyTNode<Map> map);
  // Load the EnumLength of a Map.
  Node* LoadMapEnumLength(SloppyTNode<Map> map);
  // Load the back-pointer of a Map.
  Node* LoadMapBackPointer(SloppyTNode<Map> map);
  // Load the identity hash of a JSRececiver.
  TNode<IntPtrT> LoadJSReceiverIdentityHash(SloppyTNode<Object> receiver,
                                            Label* if_no_hash = nullptr);

  // This is only used on a newly allocated PropertyArray which
  // doesn't have an existing hash.
  void InitializePropertyArrayLength(Node* property_array, Node* length,
                                     ParameterMode mode);

  // Check if the map is set for slow properties.
  TNode<BoolT> IsDictionaryMap(SloppyTNode<Map> map);

  // Load the hash field of a name as an uint32 value.
  TNode<Uint32T> LoadNameHashField(SloppyTNode<Name> name);
  // Load the hash value of a name as an uint32 value.
  // If {if_hash_not_computed} label is specified then it also checks if
  // hash is actually computed.
  TNode<Uint32T> LoadNameHash(SloppyTNode<Name> name,
                              Label* if_hash_not_computed = nullptr);

  // Load length field of a String object as intptr_t value.
  TNode<IntPtrT> LoadStringLengthAsWord(SloppyTNode<String> object);
  // Load length field of a String object as Smi value.
  TNode<Smi> LoadStringLengthAsSmi(SloppyTNode<String> object);
  // Loads a pointer to the sequential String char array.
  Node* PointerToSeqStringData(Node* seq_string);
  // Load value field of a JSValue object.
  Node* LoadJSValueValue(Node* object);
  // Load value field of a WeakCell object.
  TNode<Object> LoadWeakCellValueUnchecked(Node* weak_cell);
  TNode<Object> LoadWeakCellValue(SloppyTNode<WeakCell> weak_cell,
                                  Label* if_cleared = nullptr);

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

  // Load a feedback slot from a FeedbackVector.
  TNode<Object> LoadFeedbackVectorSlot(
      Node* object, Node* index, int additional_offset = 0,
      ParameterMode parameter_mode = INTPTR_PARAMETERS);

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
  TNode<Object> LoadContextElement(SloppyTNode<Context> context,
                                   int slot_index);
  TNode<Object> LoadContextElement(SloppyTNode<Context> context,
                                   SloppyTNode<IntPtrT> slot_index);
  void StoreContextElement(SloppyTNode<Context> context, int slot_index,
                           SloppyTNode<Object> value);
  void StoreContextElement(SloppyTNode<Context> context,
                           SloppyTNode<IntPtrT> slot_index,
                           SloppyTNode<Object> value);
  void StoreContextElementNoWriteBarrier(SloppyTNode<Context> context,
                                         int slot_index,
                                         SloppyTNode<Object> value);
  TNode<Context> LoadNativeContext(SloppyTNode<Context> context);
  // Calling this is only valid if there's a module context in the chain.
  TNode<Context> LoadModuleContext(SloppyTNode<Context> context);

  void GotoIfContextElementEqual(Node* value, Node* native_context,
                                 int slot_index, Label* if_equal) {
    GotoIf(WordEqual(value, LoadContextElement(native_context, slot_index)),
           if_equal);
  }

  TNode<Map> LoadJSArrayElementsMap(ElementsKind kind,
                                    SloppyTNode<Context> native_context);
  TNode<Map> LoadJSArrayElementsMap(SloppyTNode<Int32T> kind,
                                    SloppyTNode<Context> native_context);

  // Load the "prototype" property of a JSFunction.
  Node* LoadJSFunctionPrototype(Node* function, Label* if_bailout);

  // Store the floating point value of a HeapNumber.
  void StoreHeapNumberValue(SloppyTNode<HeapNumber> object,
                            SloppyTNode<Float64T> value);
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

  Node* StoreFeedbackVectorSlot(
      Node* object, Node* index, Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0,
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
  TNode<Smi> BuildAppendJSArray(ElementsKind kind, SloppyTNode<JSArray> array,
                                CodeStubArguments* args,
                                TVariable<IntPtrT>* arg_index, Label* bailout);
  // Pushes value onto the end of array.
  void BuildAppendJSArray(ElementsKind kind, Node* array, Node* value,
                          Label* bailout);

  void StoreFieldsNoWriteBarrier(Node* start_address, Node* end_address,
                                 Node* value);

  Node* AllocateCellWithValue(Node* value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  Node* AllocateSmiCell(int value = 0) {
    return AllocateCellWithValue(SmiConstant(value), SKIP_WRITE_BARRIER);
  }

  Node* LoadCellValue(Node* cell);

  Node* StoreCellValue(Node* cell, Node* value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Allocate a HeapNumber without initializing its value.
  TNode<HeapNumber> AllocateHeapNumber(MutableMode mode = IMMUTABLE);
  // Allocate a HeapNumber with a specific value.
  TNode<HeapNumber> AllocateHeapNumberWithValue(SloppyTNode<Float64T> value,
                                                MutableMode mode = IMMUTABLE);
  // Allocate a SeqOneByteString with the given length.
  Node* AllocateSeqOneByteString(int length, AllocationFlags flags = kNone);
  Node* AllocateSeqOneByteString(Node* context, TNode<Smi> length,
                                 AllocationFlags flags = kNone);
  // Allocate a SeqTwoByteString with the given length.
  Node* AllocateSeqTwoByteString(int length, AllocationFlags flags = kNone);
  Node* AllocateSeqTwoByteString(Node* context, TNode<Smi> length,
                                 AllocationFlags flags = kNone);

  // Allocate a SlicedOneByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  Node* AllocateSlicedOneByteString(TNode<Smi> length, Node* parent,
                                    Node* offset);
  // Allocate a SlicedTwoByteString with the given length, parent and offset.
  // |length| and |offset| are expected to be tagged.
  Node* AllocateSlicedTwoByteString(TNode<Smi> length, Node* parent,
                                    Node* offset);

  // Allocate a one-byte ConsString with the given length, first and second
  // parts. |length| is expected to be tagged, and |first| and |second| are
  // expected to be one-byte strings.
  Node* AllocateOneByteConsString(TNode<Smi> length, Node* first, Node* second,
                                  AllocationFlags flags = kNone);
  // Allocate a two-byte ConsString with the given length, first and second
  // parts. |length| is expected to be tagged, and |first| and |second| are
  // expected to be two-byte strings.
  Node* AllocateTwoByteConsString(TNode<Smi> length, Node* first, Node* second,
                                  AllocationFlags flags = kNone);

  // Allocate an appropriate one- or two-byte ConsString with the first and
  // second parts specified by |left| and |right|.
  Node* NewConsString(Node* context, TNode<Smi> length, Node* left, Node* right,
                      AllocationFlags flags = kNone);

  Node* AllocateNameDictionary(int at_least_space_for);
  Node* AllocateNameDictionary(Node* at_least_space_for);
  Node* AllocateNameDictionaryWithCapacity(Node* capacity);
  Node* CopyNameDictionary(Node* dictionary, Label* large_object_fallback);

  Node* AllocateStruct(Node* map, AllocationFlags flags = kNone);
  void InitializeStructBody(Node* object, Node* map, Node* size,
                            int start_offset = Struct::kHeaderSize);

  Node* AllocateJSObjectFromMap(
      Node* map, Node* properties = nullptr, Node* elements = nullptr,
      AllocationFlags flags = kNone,
      SlackTrackingMode slack_tracking_mode = kNoSlackTracking);

  void InitializeJSObjectFromMap(
      Node* object, Node* map, Node* instance_size, Node* properties = nullptr,
      Node* elements = nullptr,
      SlackTrackingMode slack_tracking_mode = kNoSlackTracking);

  void InitializeJSObjectBodyWithSlackTracking(Node* object, Node* map,
                                               Node* instance_size);
  void InitializeJSObjectBodyNoSlackTracking(
      Node* object, Node* map, Node* instance_size,
      int start_offset = JSObject::kHeaderSize);

  // Allocate a JSArray without elements and initialize the header fields.
  Node* AllocateUninitializedJSArrayWithoutElements(Node* array_map,
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

  Node* CloneFastJSArray(Node* context, Node* array,
                         ParameterMode mode = INTPTR_PARAMETERS,
                         Node* allocation_site = nullptr);

  Node* ExtractFastJSArray(Node* context, Node* array, Node* begin, Node* count,
                           ParameterMode mode = INTPTR_PARAMETERS,
                           Node* capacity = nullptr,
                           Node* allocation_site = nullptr);

  Node* AllocateFixedArray(ElementsKind kind, Node* capacity,
                           ParameterMode mode = INTPTR_PARAMETERS,
                           AllocationFlags flags = kNone,
                           Node* fixed_array_map = nullptr);

  Node* AllocatePropertyArray(Node* capacity,
                              ParameterMode mode = INTPTR_PARAMETERS,
                              AllocationFlags flags = kNone);
  // Perform CreateArrayIterator (ES6 #sec-createarrayiterator).
  Node* CreateArrayIterator(Node* array, Node* array_map, Node* array_type,
                            Node* context, IterationKind mode);

  Node* AllocateJSArrayIterator(Node* array, Node* array_map, Node* map);
  Node* AllocateJSIteratorResult(Node* context, Node* value, Node* done);
  Node* AllocateJSIteratorResultForEntry(Node* context, Node* key, Node* value);

  Node* TypedArraySpeciesCreateByLength(Node* context, Node* originalArray,
                                        Node* len);

  void FillFixedArrayWithValue(ElementsKind kind, Node* array, Node* from_index,
                               Node* to_index,
                               Heap::RootListIndex value_root_index,
                               ParameterMode mode = INTPTR_PARAMETERS);

  void FillPropertyArrayWithUndefined(Node* array, Node* from_index,
                                      Node* to_index,
                                      ParameterMode mode = INTPTR_PARAMETERS);

  void CopyPropertyArrayValues(
      Node* from_array, Node* to_array, Node* length,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS);

  // Copies all elements from |from_array| of |length| size to
  // |to_array| of the same size respecting the elements kind.
  void CopyFixedArrayElements(
      ElementsKind kind, Node* from_array, Node* to_array, Node* length,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS) {
    CopyFixedArrayElements(kind, from_array, kind, to_array,
                           IntPtrOrSmiConstant(0, mode), length, length,
                           barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| starting from element
  // zero to |to_array| of |capacity| size respecting both array's elements
  // kinds.
  void CopyFixedArrayElements(
      ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
      Node* to_array, Node* element_count, Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS) {
    CopyFixedArrayElements(from_kind, from_array, to_kind, to_array,
                           IntPtrOrSmiConstant(0, mode), element_count,
                           capacity, barrier_mode, mode);
  }

  // Copies |element_count| elements from |from_array| starting from element
  // |first_element| to |to_array| of |capacity| size respecting both array's
  // elements kinds.
  void CopyFixedArrayElements(
      ElementsKind from_kind, Node* from_array, ElementsKind to_kind,
      Node* to_array, Node* first_element, Node* element_count, Node* capacity,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode mode = INTPTR_PARAMETERS);

  enum class ExtractFixedArrayFlag {
    kFixedArrays = 1,
    kFixedDoubleArrays = 2,
    kDontCopyCOW = 4,
    kNewSpaceAllocationOnly = 8,
    kAllFixedArrays = kFixedArrays | kFixedDoubleArrays,
    kAllFixedArraysDontCopyCOW = kAllFixedArrays | kDontCopyCOW
  };

  typedef base::Flags<ExtractFixedArrayFlag> ExtractFixedArrayFlags;

  // Copy a portion of an existing FixedArray or FixedDoubleArray into a new
  // FixedArray, including special appropriate handling for empty arrays and COW
  // arrays.
  //
  // * |source| is either a FixedArray or FixedDoubleArray from which to copy
  // elements.
  // * |first| is the starting element index to copy from, if nullptr is passed
  // then index zero is used by default.
  // * |count| is the number of elements to copy out of the source array
  // starting from and including the element indexed by |start|. If |count| is
  // nullptr, then all of the elements from |start| to the end of |source| are
  // copied.
  // * |capacity| determines the size of the allocated result array, with
  // |capacity| >= |count|. If |capacity| is nullptr, then |count| is used as
  // the destination array's capacity.
  // * |extract_flags| determines whether FixedArrays, FixedDoubleArrays or both
  // are detected and copied. Although it's always correct to pass
  // kAllFixedArrays, the generated code is more compact and efficient if the
  // caller can specify whether only FixedArrays or FixedDoubleArrays will be
  // passed as the |source| parameter.
  // * |parameter_mode| determines the parameter mode of |first|, |count| and
  // |capacity|.
  Node* ExtractFixedArray(Node* source, Node* first, Node* count = nullptr,
                          Node* capacity = nullptr,
                          ExtractFixedArrayFlags extract_flags =
                              ExtractFixedArrayFlag::kAllFixedArrays,
                          ParameterMode parameter_mode = INTPTR_PARAMETERS);

  // Copy the entire contents of a FixedArray or FixedDoubleArray to a new
  // array, including special appropriate handling for empty arrays and COW
  // arrays.
  //
  // * |source| is either a FixedArray or FixedDoubleArray from which to copy
  // elements.
  // * |extract_flags| determines whether FixedArrays, FixedDoubleArrays or both
  // are detected and copied. Although it's always correct to pass
  // kAllFixedArrays, the generated code is more compact and efficient if the
  // caller can specify whether only FixedArrays or FixedDoubleArrays will be
  // passed as the |source| parameter.
  Node* CloneFixedArray(Node* source,
                        ExtractFixedArrayFlags flags =
                            ExtractFixedArrayFlag::kAllFixedArraysDontCopyCOW) {
    ParameterMode mode = OptimalParameterMode();
    return ExtractFixedArray(source, IntPtrOrSmiConstant(0, mode), nullptr,
                             nullptr, flags, mode);
  }

  // Copies |character_count| elements from |from_string| to |to_string|
  // starting at the |from_index|'th character. |from_string| and |to_string|
  // can either be one-byte strings or two-byte strings, although if
  // |from_string| is two-byte, then |to_string| must be two-byte.
  // |from_index|, |to_index| and |character_count| must be intptr_ts s.t. 0 <=
  // |from_index| <= |from_index| + |character_count| <= from_string.length and
  // 0 <= |to_index| <= |to_index| + |character_count| <= to_string.length.
  void CopyStringCharacters(Node* from_string, Node* to_string,
                            TNode<IntPtrT> from_index, TNode<IntPtrT> to_index,
                            TNode<IntPtrT> character_count,
                            String::Encoding from_encoding,
                            String::Encoding to_encoding);

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
                                   Node* base_allocation_size,
                                   Node* allocation_site);

  Node* TryTaggedToFloat64(Node* value, Label* if_valueisnotnumber);
  Node* TruncateTaggedToFloat64(Node* context, Node* value);
  Node* TruncateTaggedToWord32(Node* context, Node* value);
  void TaggedToWord32OrBigInt(Node* context, Node* value, Label* if_number,
                              Variable* var_word32, Label* if_bigint,
                              Variable* var_bigint);
  void TaggedToWord32OrBigIntWithFeedback(
      Node* context, Node* value, Label* if_number, Variable* var_word32,
      Label* if_bigint, Variable* var_bigint, Variable* var_feedback);

  // Truncate the floating point value of a HeapNumber to an Int32.
  Node* TruncateHeapNumberValueToWord32(Node* object);

  // Conversions.
  TNode<Number> ChangeFloat64ToTagged(SloppyTNode<Float64T> value);
  TNode<Number> ChangeInt32ToTagged(SloppyTNode<Int32T> value);
  TNode<Number> ChangeUint32ToTagged(SloppyTNode<Uint32T> value);
  TNode<Float64T> ChangeNumberToFloat64(SloppyTNode<Number> value);
  TNode<UintPtrT> ChangeNonnegativeNumberToUintPtr(TNode<Number> value);

  void TaggedToNumeric(Node* context, Node* value, Label* done,
                       Variable* var_numeric);
  void TaggedToNumericWithFeedback(Node* context, Node* value, Label* done,
                                   Variable* var_numeric,
                                   Variable* var_feedback);

  Node* TimesPointerSize(Node* value);

  // Type conversions.
  // Throws a TypeError for {method_name} if {value} is not coercible to Object,
  // or returns the {value} converted to a String otherwise.
  TNode<String> ToThisString(Node* context, Node* value,
                             char const* method_name);
  // Throws a TypeError for {method_name} if {value} is neither of the given
  // {primitive_type} nor a JSValue wrapping a value of {primitive_type}, or
  // returns the {value} (or wrapped value) otherwise.
  Node* ToThisValue(Node* context, Node* value, PrimitiveType primitive_type,
                    char const* method_name);

  // Throws a TypeError for {method_name}. Terminates the current block.
  void ThrowIncompatibleMethodReceiver(Node* context, char const* method_name,
                                       Node* receiver);

  // Throws a TypeError for {method_name} if {value} is not of the given
  // instance type. Returns {value}'s map.
  Node* ThrowIfNotInstanceType(Node* context, Node* value,
                               InstanceType instance_type,
                               char const* method_name);
  // Throws a TypeError for {method_name} if {value} is not a JSReceiver.
  // Returns the {value}'s map.
  Node* ThrowIfNotJSReceiver(Node* context, Node* value,
                             MessageTemplate::Template msg_template,
                             const char* method_name = nullptr);

  void ThrowRangeError(Node* context, MessageTemplate::Template message,
                       Node* arg0 = nullptr, Node* arg1 = nullptr,
                       Node* arg2 = nullptr);
  void ThrowTypeError(Node* context, MessageTemplate::Template message,
                      char const* arg0 = nullptr, char const* arg1 = nullptr);
  void ThrowTypeError(Node* context, MessageTemplate::Template message,
                      Node* arg0, Node* arg1 = nullptr, Node* arg2 = nullptr);

  // Type checks.
  // Check whether the map is for an object with special properties, such as a
  // JSProxy or an object with interceptors.
  Node* InstanceTypeEqual(Node* instance_type, int type);
  Node* IsAccessorInfo(Node* object);
  Node* IsAccessorPair(Node* object);
  Node* IsAllocationSite(Node* object);
  Node* IsAnyHeapNumber(Node* object);
  Node* IsArrayIteratorInstanceType(Node* instance_type);
  Node* IsNoElementsProtectorCellInvalid();
  Node* IsBigIntInstanceType(Node* instance_type);
  Node* IsBigInt(Node* object);
  Node* IsBoolean(Node* object);
  Node* IsCallableMap(Node* map);
  Node* IsCallable(Node* object);
  Node* IsCell(Node* object);
  Node* IsConsStringInstanceType(Node* instance_type);
  Node* IsConstructorMap(Node* map);
  Node* IsConstructor(Node* object);
  Node* IsDeprecatedMap(Node* map);
  Node* IsDictionary(Node* object);
  Node* IsExtensibleMap(Node* map);
  Node* IsExternalStringInstanceType(Node* instance_type);
  TNode<BoolT> IsFastJSArray(SloppyTNode<Object> object,
                             SloppyTNode<Context> context);
  Node* IsFeedbackVector(Node* object);
  Node* IsFixedArray(Node* object);
  Node* IsFixedArraySubclass(Node* object);
  Node* IsFixedArrayWithKind(Node* object, ElementsKind kind);
  Node* IsFixedArrayWithKindOrEmpty(Node* object, ElementsKind kind);
  Node* IsFixedDoubleArray(Node* object);
  Node* IsFixedTypedArray(Node* object);
  Node* IsFunctionWithPrototypeSlotMap(Node* map);
  Node* IsHashTable(Node* object);
  Node* IsHeapNumber(Node* object);
  Node* IsIndirectStringInstanceType(Node* instance_type);
  Node* IsJSArrayBuffer(Node* object);
  Node* IsJSArrayInstanceType(Node* instance_type);
  Node* IsJSArrayMap(Node* object);
  Node* IsJSArray(Node* object);
  Node* IsJSFunctionInstanceType(Node* instance_type);
  Node* IsJSFunctionMap(Node* object);
  Node* IsJSFunction(Node* object);
  Node* IsJSGlobalProxyInstanceType(Node* instance_type);
  Node* IsJSGlobalProxy(Node* object);
  Node* IsJSObjectInstanceType(Node* instance_type);
  Node* IsJSObjectMap(Node* map);
  Node* IsJSObject(Node* object);
  Node* IsJSProxy(Node* object);
  Node* IsJSReceiverInstanceType(Node* instance_type);
  Node* IsJSReceiverMap(Node* map);
  Node* IsJSReceiver(Node* object);
  Node* IsJSRegExp(Node* object);
  Node* IsJSTypedArray(Node* object);
  Node* IsJSValueInstanceType(Node* instance_type);
  Node* IsJSValueMap(Node* map);
  Node* IsJSValue(Node* object);
  Node* IsMap(Node* object);
  Node* IsMutableHeapNumber(Node* object);
  Node* IsName(Node* object);
  Node* IsNativeContext(Node* object);
  Node* IsNullOrJSReceiver(Node* object);
  Node* IsNullOrUndefined(Node* object);
  Node* IsNumberDictionary(Node* object);
  Node* IsOneByteStringInstanceType(Node* instance_type);
  Node* IsPrimitiveInstanceType(Node* instance_type);
  Node* IsPrivateSymbol(Node* object);
  Node* IsPropertyArray(Node* object);
  Node* IsPropertyCell(Node* object);
  Node* IsPrototypeInitialArrayPrototype(Node* context, Node* map);
  Node* IsSequentialStringInstanceType(Node* instance_type);
  Node* IsShortExternalStringInstanceType(Node* instance_type);
  Node* IsSpecialReceiverInstanceType(Node* instance_type);
  Node* IsSpeciesProtectorCellInvalid();
  Node* IsStringInstanceType(Node* instance_type);
  Node* IsString(Node* object);
  Node* IsSymbolInstanceType(Node* instance_type);
  Node* IsSymbol(Node* object);
  Node* IsUndetectableMap(Node* map);
  Node* IsWeakCell(Node* object);
  Node* IsZeroOrFixedArray(Node* object);

  inline Node* IsSharedFunctionInfo(Node* object) {
    return IsSharedFunctionInfoMap(LoadMap(object));
  }

  // True iff |object| is a Smi or a HeapNumber.
  Node* IsNumber(Node* object);
  // True iff |object| is a Smi or a HeapNumber or a BigInt.
  Node* IsNumeric(Node* object);

  // True iff |number| is either a Smi, or a HeapNumber whose value is not
  // within Smi range.
  Node* IsNumberNormalized(Node* number);
  Node* IsNumberPositive(Node* number);
  // True iff {number} is a positive number and a valid array index in the range
  // [0, 2^32-1).
  Node* IsNumberArrayIndex(Node* number);

  // ElementsKind helpers:
  Node* IsFastElementsKind(Node* elements_kind);
  Node* IsFastSmiOrTaggedElementsKind(Node* elements_kind);
  Node* IsHoleyFastElementsKind(Node* elements_kind);
  Node* IsElementsKindGreaterThan(Node* target_kind,
                                  ElementsKind reference_kind);

  Node* FixedArraySizeDoesntFitInNewSpace(
      Node* element_count, int base_size = FixedArray::kHeaderSize,
      ParameterMode mode = INTPTR_PARAMETERS);

  // String helpers.
  // Load a character from a String (might flatten a ConsString).
  TNode<Int32T> StringCharCodeAt(SloppyTNode<String> string,
                                 SloppyTNode<IntPtrT> index);
  // Return the single character string with only {code}.
  TNode<String> StringFromCharCode(TNode<Int32T> code);

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

  // Check if |string| is an indirect (thin or flat cons) string type that can
  // be dereferenced by DerefIndirectString.
  void BranchIfCanDerefIndirectString(Node* string, Node* instance_type,
                                      Label* can_deref, Label* cannot_deref);
  // Unpack an indirect (thin or flat cons) string type.
  void DerefIndirectString(Variable* var_string, Node* instance_type);
  // Check if |var_string| has an indirect (thin or flat cons) string type,
  // and unpack it if so.
  void MaybeDerefIndirectString(Variable* var_string, Node* instance_type,
                                Label* did_deref, Label* cannot_deref);
  // Check if |var_left| or |var_right| has an indirect (thin or flat cons)
  // string type, and unpack it/them if so. Fall through if nothing was done.
  void MaybeDerefIndirectStrings(Variable* var_left, Node* left_instance_type,
                                 Variable* var_right, Node* right_instance_type,
                                 Label* did_something);

  TNode<String> StringFromCodePoint(TNode<Int32T> codepoint,
                                    UnicodeEncoding encoding);

  // Type conversion helpers.
  enum class BigIntHandling { kConvertToNumber, kThrow };
  // Convert a String to a Number.
  TNode<Number> StringToNumber(SloppyTNode<String> input);
  // Convert a Number to a String.
  Node* NumberToString(Node* input);
  // Convert an object to a name.
  Node* ToName(Node* context, Node* input);
  // Convert a Non-Number object to a Number.
  TNode<Number> NonNumberToNumber(
      SloppyTNode<Context> context, SloppyTNode<HeapObject> input,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);
  // Convert a Non-Number object to a Numeric.
  TNode<Numeric> NonNumberToNumeric(SloppyTNode<Context> context,
                                    SloppyTNode<HeapObject> input);
  // Convert any object to a Number.
  // Conforms to ES#sec-tonumber if {bigint_handling} == kThrow.
  // With {bigint_handling} == kConvertToNumber, matches behavior of
  // tc39.github.io/proposal-bigint/#sec-number-constructor-number-value.
  TNode<Number> ToNumber(
      SloppyTNode<Context> context, SloppyTNode<Object> input,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);

  // Converts |input| to one of 2^32 integer values in the range 0 through
  // 2^32-1, inclusive.
  // ES#sec-touint32
  TNode<Number> ToUint32(SloppyTNode<Context> context,
                         SloppyTNode<Object> input);

  // Convert any object to a String.
  TNode<String> ToString(SloppyTNode<Context> context,
                         SloppyTNode<Object> input);
  TNode<String> ToString_Inline(SloppyTNode<Context> context,
                                SloppyTNode<Object> input);

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

  // ES6 7.1.4 ToInteger ( argument )
  TNode<Number> ToInteger_Inline(TNode<Context> context, TNode<Object> input,
                                 ToIntegerTruncationMode mode = kNoTruncation);
  TNode<Number> ToInteger(SloppyTNode<Context> context,
                          SloppyTNode<Object> input,
                          ToIntegerTruncationMode mode = kNoTruncation);

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word32|. Returns result as an uint32 node.
  template <typename BitField>
  TNode<Uint32T> DecodeWord32(SloppyTNode<Word32T> word32) {
    return DecodeWord32(word32, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word|. Returns result as a word-size node.
  template <typename BitField>
  TNode<UintPtrT> DecodeWord(SloppyTNode<WordT> word) {
    return DecodeWord(word, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word32|. Returns result as a word-size node.
  template <typename BitField>
  TNode<UintPtrT> DecodeWordFromWord32(SloppyTNode<Word32T> word32) {
    return DecodeWord<BitField>(ChangeUint32ToWord(word32));
  }

  // Returns a node that contains a decoded (unsigned!) value of a bit
  // field |BitField| in |word|. Returns result as an uint32 node.
  template <typename BitField>
  TNode<Uint32T> DecodeWord32FromWord(SloppyTNode<WordT> word) {
    return UncheckedCast<Uint32T>(
        TruncateWordToWord32(Signed(DecodeWord<BitField>(word))));
  }

  // Decodes an unsigned (!) value from |word32| to an uint32 node.
  TNode<Uint32T> DecodeWord32(SloppyTNode<Word32T> word32, uint32_t shift,
                              uint32_t mask);

  // Decodes an unsigned (!) value from |word| to a word-size node.
  TNode<UintPtrT> DecodeWord(SloppyTNode<WordT> word, uint32_t shift,
                             uint32_t mask);

  // Returns a node that contains the updated values of a |BitField|.
  template <typename BitField>
  Node* UpdateWord(Node* word, Node* value) {
    return UpdateWord(word, value, BitField::kShift, BitField::kMask);
  }

  // Returns a node that contains the updated {value} inside {word} starting
  // at {shift} and fitting in {mask}.
  Node* UpdateWord(Node* word, Node* value, uint32_t shift, uint32_t mask);

  // Returns true if any of the |T|'s bits in given |word32| are set.
  template <typename T>
  TNode<BoolT> IsSetWord32(SloppyTNode<Word32T> word32) {
    return IsSetWord32(word32, T::kMask);
  }

  // Returns true if any of the mask's bits in given |word32| are set.
  TNode<BoolT> IsSetWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    return Word32NotEqual(Word32And(word32, Int32Constant(mask)),
                          Int32Constant(0));
  }

  // Returns true if none of the mask's bits in given |word32| are set.
  TNode<BoolT> IsNotSetWord32(SloppyTNode<Word32T> word32, uint32_t mask) {
    return Word32Equal(Word32And(word32, Int32Constant(mask)),
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

  void Increment(Variable* variable, int value = 1,
                 ParameterMode mode = INTPTR_PARAMETERS);
  void Decrement(Variable* variable, int value = 1,
                 ParameterMode mode = INTPTR_PARAMETERS) {
    Increment(variable, -value, mode);
  }

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
  TNode<IntPtrT> HashTableComputeCapacity(
      SloppyTNode<IntPtrT> at_least_space_for);

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
  Node* LoadName(Node* key);

  template <typename Dictionary>
  void NameDictionaryLookup(Node* dictionary, Node* unique_name,
                            Label* if_found, Variable* var_name_index,
                            Label* if_not_found,
                            int inlined_probes = kInlinedDictionaryProbes,
                            LookupMode mode = kFindExisting);

  Node* ComputeIntegerHash(Node* key);
  Node* ComputeIntegerHash(Node* key, Node* seed);

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

  // Operating mode for TryGetOwnProperty and CallGetterIfAccessor
  // kReturnAccessorPair is used when we're only getting the property descriptor
  enum GetOwnPropertyMode { kCallJSGetter, kReturnAccessorPair };
  // Tries to get {object}'s own {unique_name} property value. If the property
  // is an accessor then it also calls a getter. If the property is a double
  // field it re-wraps value in an immutable heap number.
  void TryGetOwnProperty(Node* context, Node* receiver, Node* object, Node* map,
                         Node* instance_type, Node* unique_name,
                         Label* if_found, Variable* var_value,
                         Label* if_not_found, Label* if_bailout);
  void TryGetOwnProperty(Node* context, Node* receiver, Node* object, Node* map,
                         Node* instance_type, Node* unique_name,
                         Label* if_found, Variable* var_value,
                         Variable* var_details, Variable* var_raw_value,
                         Label* if_not_found, Label* if_bailout,
                         GetOwnPropertyMode mode);

  Node* GetProperty(Node* context, Node* receiver, Handle<Name> name) {
    return GetProperty(context, receiver, HeapConstant(name));
  }

  Node* GetProperty(Node* context, Node* receiver, Node* const name) {
    return CallStub(CodeFactory::GetProperty(isolate()), context, receiver,
                    name);
  }

  Node* GetMethod(Node* context, Node* object, Handle<Name> name,
                  Label* if_null_or_undefined);

  template <class... TArgs>
  Node* CallBuiltin(Builtins::Name id, Node* context, TArgs... args) {
    DCHECK_IMPLIES(Builtins::KindOf(id) == Builtins::TFJ,
                   !Builtins::IsLazy(id));
    return CallStub(Builtins::CallableFor(isolate(), id), context, args...);
  }

  template <class... TArgs>
  Node* TailCallBuiltin(Builtins::Name id, Node* context, TArgs... args) {
    DCHECK_IMPLIES(Builtins::KindOf(id) == Builtins::TFJ,
                   !Builtins::IsLazy(id));
    return TailCallStub(Builtins::CallableFor(isolate(), id), context, args...);
  }

  void LoadPropertyFromFastObject(Node* object, Node* map, Node* descriptors,
                                  Node* name_index, Variable* var_details,
                                  Variable* var_value);

  void LoadPropertyFromFastObject(Node* object, Node* map, Node* descriptors,
                                  Node* name_index, Node* details,
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

  // For integer indexed exotic cases, check if the given string cannot be a
  // special index. If we are not sure that the given string is not a special
  // index with a simple check, return False. Note that "False" return value
  // does not mean that the name_string is a special index in the current
  // implementation.
  void BranchIfMaybeSpecialIndex(TNode<String> name_string,
                                 Label* if_maybe_special_index,
                                 Label* if_not_special_index);

  // Generic property prototype chain lookup generator.
  // For properties it generates lookup using given {lookup_property_in_holder}
  // and for elements it uses {lookup_element_in_holder}.
  // Upon reaching the end of prototype chain the control goes to {if_end}.
  // If it can't handle the case {receiver}/{key} case then the control goes
  // to {if_bailout}.
  // If {if_proxy} is nullptr, proxies go to if_bailout.
  void TryPrototypeChainLookup(Node* receiver, Node* key,
                               const LookupInHolder& lookup_property_in_holder,
                               const LookupInHolder& lookup_element_in_holder,
                               Label* if_end, Label* if_bailout,
                               Label* if_proxy = nullptr);

  // Instanceof helpers.
  // Returns true if {object} has {prototype} somewhere in it's prototype
  // chain, otherwise false is returned. Might cause arbitrary side effects
  // due to [[GetPrototypeOf]] invocations.
  Node* HasInPrototypeChain(Node* context, Node* object, Node* prototype);
  // ES6 section 7.3.19 OrdinaryHasInstance (C, O)
  Node* OrdinaryHasInstance(Node* context, Node* callable, Node* object);

  // Load type feedback vector from the stub caller's frame.
  Node* LoadFeedbackVectorForStub();

  // Load type feedback vector for the given closure.
  Node* LoadFeedbackVector(Node* closure);

  // Update the type feedback vector.
  void UpdateFeedback(Node* feedback, Node* feedback_vector, Node* slot_id);

  // Report that there was a feedback update, performing any tasks that should
  // be done after a feedback update.
  void ReportFeedbackUpdate(SloppyTNode<FeedbackVector> feedback_vector,
                            SloppyTNode<IntPtrT> slot_id, const char* reason);

  // Combine the new feedback with the existing_feedback. Do nothing if
  // existing_feedback is nullptr.
  void CombineFeedback(Variable* existing_feedback, int feedback);
  void CombineFeedback(Variable* existing_feedback, Node* feedback);

  // Overwrite the existing feedback with new_feedback. Do nothing if
  // existing_feedback is nullptr.
  void OverwriteFeedback(Variable* existing_feedback, int new_feedback);

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
  TNode<Context> LoadScriptContext(TNode<Context> context,
                                   TNode<IntPtrT> context_index);

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

  Node* GetPropertyArrayAllocationSize(Node* element_count,
                                       ParameterMode mode) {
    return GetArrayAllocationSize(element_count, PACKED_ELEMENTS, mode,
                                  PropertyArray::kHeaderSize);
  }

  void GotoIfFixedArraySizeDoesntFitInNewSpace(Node* element_count,
                                               Label* doesnt_fit, int base_size,
                                               ParameterMode mode);

  void InitializeFieldsWithRoot(Node* object, Node* start_offset,
                                Node* end_offset, Heap::RootListIndex root);

  Node* RelationalComparison(Operation op, Node* left, Node* right,
                             Node* context,
                             Variable* var_type_feedback = nullptr);

  void BranchIfNumberRelationalComparison(Operation op, Node* left, Node* right,
                                          Label* if_true, Label* if_false);

  void BranchIfAccessorPair(Node* value, Label* if_accessor_pair,
                            Label* if_not_accessor_pair) {
    GotoIf(TaggedIsSmi(value), if_not_accessor_pair);
    Branch(IsAccessorPair(value), if_accessor_pair, if_not_accessor_pair);
  }

  void GotoIfNumberGreaterThanOrEqual(Node* left, Node* right, Label* if_false);

  Node* Equal(Node* lhs, Node* rhs, Node* context,
              Variable* var_type_feedback = nullptr);

  Node* StrictEqual(Node* lhs, Node* rhs,
                    Variable* var_type_feedback = nullptr);

  // ECMA#sec-samevalue
  // Similar to StrictEqual except that NaNs are treated as equal and minus zero
  // differs from positive zero.
  void BranchIfSameValue(Node* lhs, Node* rhs, Label* if_true, Label* if_false);

  enum HasPropertyLookupMode { kHasProperty, kForInHasProperty };

  TNode<Oddball> HasProperty(SloppyTNode<HeapObject> object,
                             SloppyTNode<Name> key,
                             SloppyTNode<Context> context,
                             HasPropertyLookupMode mode);

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

  // for..in helpers
  void CheckPrototypeEnumCache(Node* receiver, Node* receiver_map,
                               Label* if_fast, Label* if_slow);
  Node* CheckEnumCache(Node* receiver, Label* if_empty, Label* if_runtime);

  // Support for printf-style debugging
  void Print(const char* s);
  void Print(const char* prefix, Node* tagged_value);
  inline void Print(SloppyTNode<Object> tagged_value) {
    return Print(nullptr, tagged_value);
  }

  template <class... TArgs>
  Node* MakeTypeError(MessageTemplate::Template message, Node* context,
                      TArgs... args) {
    STATIC_ASSERT(sizeof...(TArgs) <= 3);
    Node* const make_type_error = LoadContextElement(
        LoadNativeContext(context), Context::MAKE_TYPE_ERROR_INDEX);
    return CallJS(CodeFactory::Call(isolate()), context, make_type_error,
                  UndefinedConstant(), SmiConstant(message), args...);
  }

  void Abort(AbortReason reason) {
    CallRuntime(Runtime::kAbort, NoContextConstant(), SmiConstant(reason));
    Unreachable();
  }

  void PerformStackCheck(Node* context);

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
  Node* DescriptorNumberToIndex(SloppyTNode<Uint32T> descriptor_number);
  // Implements DescriptorArray::ToKeyIndex.
  // Returns an untagged IntPtr.
  Node* DescriptorArrayToKeyIndex(Node* descriptor_number);
  // Implements DescriptorArray::GetKey.
  Node* DescriptorArrayGetKey(Node* descriptors, Node* descriptor_number);
  // Implements DescriptorArray::GetKey.
  TNode<Uint32T> DescriptorArrayGetDetails(TNode<DescriptorArray> descriptors,
                                           TNode<Uint32T> descriptor_number);

  Node* CallGetterIfAccessor(Node* value, Node* details, Node* context,
                             Node* receiver, Label* if_bailout,
                             GetOwnPropertyMode mode = kCallJSGetter);

  Node* TryToIntptr(Node* key, Label* miss);

  void BranchIfPrototypesHaveNoElements(Node* receiver_map,
                                        Label* definitely_no_elements,
                                        Label* possibly_elements);

  void InitializeFunctionContext(Node* native_context, Node* context,
                                 int slots);

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
  Node* AllocateUninitializedJSArray(Node* array_map, Node* length,
                                     Node* allocation_site,
                                     Node* size_in_bytes);

  Node* SmiShiftBitsConstant();

  // Emits keyed sloppy arguments load if the |value| is nullptr or store
  // otherwise. Returns either the loaded value or |value|.
  Node* EmitKeyedSloppyArguments(Node* receiver, Node* key, Node* value,
                                 Label* bailout);

  Node* AllocateSlicedString(Heap::RootListIndex map_root_index,
                             TNode<Smi> length, Node* parent, Node* offset);

  Node* AllocateConsString(Heap::RootListIndex map_root_index,
                           TNode<Smi> length, Node* first, Node* second,
                           AllocationFlags flags);

  // Implements DescriptorArray::number_of_entries.
  // Returns an untagged int32.
  Node* DescriptorArrayNumberOfEntries(Node* descriptors);
  // Implements DescriptorArray::GetSortedKeyIndex.
  // Returns an untagged int32.
  Node* DescriptorArrayGetSortedKeyIndex(Node* descriptors,
                                         Node* descriptor_number);

  Node* CollectFeedbackForString(Node* instance_type);
  void GenerateEqual_Same(Node* value, Label* if_equal, Label* if_notequal,
                          Variable* var_type_feedback = nullptr);
  Node* AllocAndCopyStringCharacters(Node* context, Node* from,
                                     Node* from_instance_type,
                                     TNode<IntPtrT> from_index,
                                     TNode<Smi> character_count);

  static const int kElementLoopUnrollThreshold = 8;

  // {convert_bigint} is only meaningful when {mode} == kToNumber.
  Node* NonNumberToNumberOrNumeric(
      Node* context, Node* input, Object::Conversion mode,
      BigIntHandling bigint_handling = BigIntHandling::kThrow);

  void TaggedToNumeric(Node* context, Node* value, Label* done,
                       Variable* var_numeric, Variable* var_feedback);

  template <Object::Conversion conversion>
  void TaggedToWord32OrBigIntImpl(Node* context, Node* value, Label* if_number,
                                  Variable* var_word32,
                                  Label* if_bigint = nullptr,
                                  Variable* var_bigint = nullptr,
                                  Variable* var_feedback = nullptr);
};

class CodeStubArguments {
 public:
  typedef compiler::Node Node;
  template <class T>
  using TNode = compiler::TNode<T>;
  template <class T>
  using SloppyTNode = compiler::SloppyTNode<T>;
  enum ReceiverMode { kHasReceiver, kNoReceiver };

  // |argc| is an intptr value which specifies the number of arguments passed
  // to the builtin excluding the receiver. The arguments will include a
  // receiver iff |receiver_mode| is kHasReceiver.
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc,
                    ReceiverMode receiver_mode = ReceiverMode::kHasReceiver)
      : CodeStubArguments(assembler, argc, nullptr,
                          CodeStubAssembler::INTPTR_PARAMETERS, receiver_mode) {
  }

  // |argc| is either a smi or intptr depending on |param_mode|. The arguments
  // include a receiver iff |receiver_mode| is kHasReceiver.
  CodeStubArguments(CodeStubAssembler* assembler, Node* argc, Node* fp,
                    CodeStubAssembler::ParameterMode param_mode,
                    ReceiverMode receiver_mode = ReceiverMode::kHasReceiver);

  TNode<Object> GetReceiver() const;

  TNode<RawPtr<Object>> AtIndexPtr(
      Node* index, CodeStubAssembler::ParameterMode mode =
                       CodeStubAssembler::INTPTR_PARAMETERS) const;

  // |index| is zero-based and does not include the receiver
  TNode<Object> AtIndex(Node* index,
                        CodeStubAssembler::ParameterMode mode =
                            CodeStubAssembler::INTPTR_PARAMETERS) const;

  TNode<Object> AtIndex(int index) const;

  TNode<Object> GetOptionalArgumentValue(int index) {
    return GetOptionalArgumentValue(index, assembler_->UndefinedConstant());
  }
  TNode<Object> GetOptionalArgumentValue(int index,
                                         SloppyTNode<Object> default_value);

  Node* GetLength(CodeStubAssembler::ParameterMode mode) const {
    DCHECK_EQ(mode, argc_mode_);
    return argc_;
  }

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
  ReceiverMode receiver_mode_;
  Node* argc_;
  TNode<RawPtr<Object>> arguments_;
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

#define CSA_CHECK(csa, x)                                              \
  (csa)->Check(                                                        \
      [&]() -> compiler::Node* {                                       \
        return base::implicit_cast<compiler::SloppyTNode<Word32T>>(x); \
      },                                                               \
      #x, __FILE__, __LINE__)

#ifdef DEBUG
// Add stringified versions to the given values, except the first. That is,
// transform
//   x, a, b, c, d, e, f
// to
//   a, "a", b, "b", c, "c", d, "d", e, "e", f, "f"
//
// __VA_ARGS__  is ignored to allow the caller to pass through too many
// parameters, and the first element is ignored to support having no extra
// values without empty __VA_ARGS__ (which cause all sorts of problems with
// extra commas).
#define CSA_ASSERT_STRINGIFY_EXTRA_VALUES_5(_, v1, v2, v3, v4, v5, ...) \
  v1, #v1, v2, #v2, v3, #v3, v4, #v4, v5, #v5

// Stringify the given variable number of arguments. The arguments are trimmed
// to 5 if there are too many, and padded with nullptr if there are not enough.
#define CSA_ASSERT_STRINGIFY_EXTRA_VALUES(...)                                \
  CSA_ASSERT_STRINGIFY_EXTRA_VALUES_5(__VA_ARGS__, nullptr, nullptr, nullptr, \
                                      nullptr, nullptr)

#define CSA_ASSERT_GET_CONDITION(x, ...) (x)
#define CSA_ASSERT_GET_CONDITION_STR(x, ...) #x

// CSA_ASSERT(csa, <condition>, <extra values to print...>)

// We have to jump through some hoops to allow <extra values to print...> to be
// empty.
#define CSA_ASSERT(csa, ...)                                                 \
  (csa)->Assert(                                                             \
      [&]() -> compiler::Node* {                                             \
        return base::implicit_cast<compiler::SloppyTNode<Word32T>>(          \
            EXPAND(CSA_ASSERT_GET_CONDITION(__VA_ARGS__)));                  \
      },                                                                     \
      EXPAND(CSA_ASSERT_GET_CONDITION_STR(__VA_ARGS__)), __FILE__, __LINE__, \
      CSA_ASSERT_STRINGIFY_EXTRA_VALUES(__VA_ARGS__))

#define CSA_ASSERT_JS_ARGC_OP(csa, Op, op, expected)                      \
  (csa)->Assert(                                                          \
      [&]() -> compiler::Node* {                                          \
        compiler::Node* const argc =                                      \
            (csa)->Parameter(Descriptor::kActualArgumentsCount);          \
        return (csa)->Op(argc, (csa)->Int32Constant(expected));           \
      },                                                                  \
      "argc " #op " " #expected, __FILE__, __LINE__,                      \
      SmiFromWord32((csa)->Parameter(Descriptor::kActualArgumentsCount)), \
      "argc")

#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) \
  CSA_ASSERT_JS_ARGC_OP(csa, Word32Equal, ==, expected)

#define CSA_DEBUG_INFO(name) \
  { #name, __FILE__, __LINE__ }
#define BIND(label) Bind(label, CSA_DEBUG_INFO(label))
#define VARIABLE(name, ...) \
  Variable name(this, CSA_DEBUG_INFO(name), __VA_ARGS__)
#define VARIABLE_CONSTRUCTOR(name, ...) \
  name(this, CSA_DEBUG_INFO(name), __VA_ARGS__)
#define TYPED_VARIABLE_DEF(type, name, ...) \
  TVariable<type> name(CSA_DEBUG_INFO(name), __VA_ARGS__)
#else  // DEBUG
#define CSA_ASSERT(csa, ...) ((void)0)
#define CSA_ASSERT_JS_ARGC_EQ(csa, expected) ((void)0)
#define BIND(label) Bind(label)
#define VARIABLE(name, ...) Variable name(this, __VA_ARGS__)
#define VARIABLE_CONSTRUCTOR(name, ...) name(this, __VA_ARGS__)
#define TYPED_VARIABLE_DEF(type, name, ...) TVariable<type> name(__VA_ARGS__)
#endif  // DEBUG

#define TVARIABLE(...) EXPAND(TYPED_VARIABLE_DEF(__VA_ARGS__, this))

#ifdef ENABLE_SLOW_DCHECKS
#define CSA_SLOW_ASSERT(csa, ...) \
  if (FLAG_enable_slow_asserts) { \
    CSA_ASSERT(csa, __VA_ARGS__); \
  }
#else
#define CSA_SLOW_ASSERT(csa, ...) ((void)0)
#endif

DEFINE_OPERATORS_FOR_FLAGS(CodeStubAssembler::AllocationFlags);

}  // namespace internal
}  // namespace v8
#endif  // V8_CODE_STUB_ASSEMBLER_H_
