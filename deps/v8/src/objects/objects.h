// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_OBJECTS_H_
#define V8_OBJECTS_OBJECTS_H_

#include <iosfwd>
#include <memory>

#include "include/v8-internal.h"
#include "include/v8config.h"
#include "src/base/bits.h"
#include "src/base/build_config.h"
#include "src/base/flags.h"
#include "src/base/logging.h"
#include "src/base/memory.h"
#include "src/codegen/constants-arch.h"
#include "src/common/assert-scope.h"
#include "src/common/checks.h"
#include "src/common/message-template.h"
#include "src/common/operation.h"
#include "src/common/ptr-compr.h"
#include "src/flags/flags.h"
#include "src/objects/elements-kind.h"
#include "src/objects/field-index.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/objects-definitions.h"
#include "src/objects/property-details.h"
#include "src/objects/tagged-impl.h"
#include "src/utils/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

//
// Most object types in the V8 JavaScript are described in this file.
//
// Inheritance hierarchy:
// - Object
//   - Smi          (immediate small integer)
//   - TaggedIndex  (properly sign-extended immediate small integer)
//   - HeapObject   (superclass for everything allocated in the heap)
//     - JSReceiver  (suitable for property access)
//       - JSObject
//         - JSArray
//           - TemplateLiteralObject
//         - JSArrayBuffer
//         - JSArrayBufferView
//           - JSTypedArray
//           - JSDataView
//         - JSCollection
//           - JSSet
//           - JSMap
//         - JSCustomElementsObject (may have elements despite empty FixedArray)
//           - JSSpecialObject (requires custom property lookup handling)
//             - JSGlobalObject
//             - JSGlobalProxy
//             - JSModuleNamespace
//           - JSPrimitiveWrapper
//         - JSDate
//         - JSFunctionOrBoundFunctionOrWrappedFunction
//           - JSBoundFunction
//           - JSFunction
//           - JSWrappedFunction
//         - JSGeneratorObject
//         - JSMapIterator
//         - JSMessageObject
//         - JSRegExp
//         - JSSetIterator
//         - JSShadowRealm
//         - JSSharedStruct
//         - JSStringIterator
//         - JSTemporalCalendar
//         - JSTemporalDuration
//         - JSTemporalInstant
//         - JSTemporalPlainDate
//         - JSTemporalPlainDateTime
//         - JSTemporalPlainMonthDay
//         - JSTemporalPlainTime
//         - JSTemporalPlainYearMonth
//         - JSTemporalTimeZone
//         - JSTemporalZonedDateTime
//         - JSWeakCollection
//           - JSWeakMap
//           - JSWeakSet
//         - JSCollator            // If V8_INTL_SUPPORT enabled.
//         - JSDateTimeFormat      // If V8_INTL_SUPPORT enabled.
//         - JSDisplayNames        // If V8_INTL_SUPPORT enabled.
//         - JSDurationFormat      // If V8_INTL_SUPPORT enabled.
//         - JSListFormat          // If V8_INTL_SUPPORT enabled.
//         - JSLocale              // If V8_INTL_SUPPORT enabled.
//         - JSNumberFormat        // If V8_INTL_SUPPORT enabled.
//         - JSPluralRules         // If V8_INTL_SUPPORT enabled.
//         - JSRelativeTimeFormat  // If V8_INTL_SUPPORT enabled.
//         - JSSegmenter           // If V8_INTL_SUPPORT enabled.
//         - JSSegments            // If V8_INTL_SUPPORT enabled.
//         - JSSegmentIterator     // If V8_INTL_SUPPORT enabled.
//         - JSV8BreakIterator     // If V8_INTL_SUPPORT enabled.
//         - WasmExceptionPackage
//         - WasmTagObject
//         - WasmGlobalObject
//         - WasmInstanceObject
//         - WasmMemoryObject
//         - WasmModuleObject
//         - WasmTableObject
//         - WasmSuspenderObject
//       - JSProxy
//     - FixedArrayBase
//       - ByteArray
//       - BytecodeArray
//       - FixedArray
//         - HashTable
//           - Dictionary
//           - StringTable
//           - StringSet
//           - CompilationCacheTable
//           - MapCache
//         - OrderedHashTable
//           - OrderedHashSet
//           - OrderedHashMap
//         - FeedbackMetadata
//         - TemplateList
//         - TransitionArray
//         - ScopeInfo
//         - SourceTextModuleInfo
//         - ScriptContextTable
//         - ClosureFeedbackCellArray
//       - FixedDoubleArray
//     - PrimitiveHeapObject
//       - BigInt
//       - HeapNumber
//       - Name
//         - String
//           - SeqString
//             - SeqOneByteString
//             - SeqTwoByteString
//           - SlicedString
//           - ConsString
//           - ThinString
//           - ExternalString
//             - ExternalOneByteString
//             - ExternalTwoByteString
//           - InternalizedString
//             - SeqInternalizedString
//               - SeqOneByteInternalizedString
//               - SeqTwoByteInternalizedString
//             - ConsInternalizedString
//             - ExternalInternalizedString
//               - ExternalOneByteInternalizedString
//               - ExternalTwoByteInternalizedString
//         - Symbol
//       - Oddball
//     - Context
//       - NativeContext
//     - Cell
//     - DescriptorArray
//     - PropertyCell
//     - PropertyArray
//     - InstructionStream
//     - AbstractCode, a wrapper around Code or BytecodeArray
//     - GcSafeCode, a wrapper around Code
//     - Map
//     - Foreign
//     - SmallOrderedHashTable
//       - SmallOrderedHashMap
//       - SmallOrderedHashSet
//     - SharedFunctionInfo
//     - Struct
//       - AccessorInfo
//       - AsmWasmData
//       - PromiseReaction
//       - PromiseCapability
//       - AccessorPair
//       - AccessCheckInfo
//       - InterceptorInfo
//       - CallHandlerInfo
//       - EnumCache
//       - TemplateInfo
//         - FunctionTemplateInfo
//         - ObjectTemplateInfo
//       - Script
//       - DebugInfo
//       - BreakPoint
//       - BreakPointInfo
//       - CallSiteInfo
//       - CodeCache
//       - PropertyDescriptorObject
//       - PromiseOnStack
//       - PrototypeInfo
//       - Microtask
//         - CallbackTask
//         - CallableTask
//         - PromiseReactionJobTask
//           - PromiseFulfillReactionJobTask
//           - PromiseRejectReactionJobTask
//         - PromiseResolveThenableJobTask
//       - Module
//         - SourceTextModule
//         - SyntheticModule
//       - SourceTextModuleInfoEntry
//       - StackFrameInfo
//     - FeedbackCell
//     - FeedbackVector
//     - PreparseData
//     - UncompiledData
//       - UncompiledDataWithoutPreparseData
//       - UncompiledDataWithPreparseData
//     - SwissNameDictionary
//
// Formats of Object::ptr_:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01

namespace v8 {
namespace internal {

struct InliningPosition;
class PropertyDescriptorObject;

// UNSAFE_SKIP_WRITE_BARRIER skips the write barrier.
// SKIP_WRITE_BARRIER skips the write barrier and asserts that this is safe in
// the MemoryOptimizer
// UPDATE_WRITE_BARRIER is doing the full barrier, marking and generational.
enum WriteBarrierMode {
  SKIP_WRITE_BARRIER,
  UNSAFE_SKIP_WRITE_BARRIER,
  UPDATE_EPHEMERON_KEY_WRITE_BARRIER,
  UPDATE_WRITE_BARRIER
};

// PropertyNormalizationMode is used to specify whether to keep
// inobject properties when normalizing properties of a JSObject.
enum PropertyNormalizationMode {
  CLEAR_INOBJECT_PROPERTIES,
  KEEP_INOBJECT_PROPERTIES
};

// Indicates whether transitions can be added to a source map or not.
enum TransitionFlag { INSERT_TRANSITION, OMIT_TRANSITION };

// Indicates whether the transition is simple: the target map of the transition
// either extends the current map with a new property, or it modifies the
// property that was added last to the current map.
enum SimpleTransitionFlag {
  SIMPLE_PROPERTY_TRANSITION,
  PROPERTY_TRANSITION,
  SPECIAL_TRANSITION
};

// Indicates whether we are only interested in the descriptors of a particular
// map, or in all descriptors in the descriptor array.
enum DescriptorFlag { ALL_DESCRIPTORS, OWN_DESCRIPTORS };

// Instance size sentinel for objects of variable size.
const int kVariableSizeSentinel = 0;

// We may store the unsigned bit field as signed Smi value and do not
// use the sign bit.
const int kStubMajorKeyBits = 8;
const int kStubMinorKeyBits = kSmiValueSize - kStubMajorKeyBits - 1;

// Result of an abstract relational comparison of x and y, implemented according
// to ES6 section 7.2.11 Abstract Relational Comparison.
enum class ComparisonResult {
  kLessThan = -1,    // x < y
  kEqual = 0,        // x = y
  kGreaterThan = 1,  // x > y
  kUndefined = 2     // at least one of x or y was undefined or NaN
};

// (Returns false whenever {result} is kUndefined.)
bool ComparisonResultToBool(Operation op, ComparisonResult result);

enum class OnNonExistent { kThrowReferenceError, kReturnUndefined };

// The element types selection for CreateListFromArrayLike.
enum class ElementTypes { kAll, kStringAndSymbol };

// Currently DefineOwnPropertyIgnoreAttributes invokes the setter
// interceptor and user-defined setters during define operations,
// even in places where it makes more sense to invoke the definer
// interceptor and not invoke the setter: e.g. both the definer and
// the setter interceptors are called in Object.defineProperty().
// kDefine allows us to implement the define semantics correctly
// in selected locations.
// TODO(joyee): see if we can deprecate the old behavior.
enum class EnforceDefineSemantics { kSet, kDefine };

// TODO(mythria): Move this to a better place.
ShouldThrow GetShouldThrow(Isolate* isolate, Maybe<ShouldThrow> should_throw);

// Object is the abstract superclass for all classes in the
// object hierarchy.
// Object does not use any virtual functions to avoid the
// allocation of the C++ vtable.
// There must only be a single data member in Object: the Address ptr,
// containing the tagged heap pointer that this Object instance refers to.
// For a design overview, see https://goo.gl/Ph4CGz.
class Object : public TaggedImpl<HeapObjectReferenceType::STRONG, Address> {
 public:
  constexpr Object() : TaggedImpl(kNullAddress) {}
  explicit constexpr Object(Address ptr) : TaggedImpl(ptr) {}

  V8_INLINE bool IsTaggedIndex() const;

  // Whether the object is in the RO heap and the RO heap is shared, or in the
  // writable shared heap.
  V8_INLINE bool InSharedHeap() const;

  V8_INLINE bool InWritableSharedSpace() const;

#define IS_TYPE_FUNCTION_DECL(Type) \
  V8_INLINE bool Is##Type() const;  \
  V8_INLINE bool Is##Type(PtrComprCageBase cage_base) const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  IS_TYPE_FUNCTION_DECL(HashTableBase)
  IS_TYPE_FUNCTION_DECL(SmallOrderedHashTable)
#undef IS_TYPE_FUNCTION_DECL
  V8_INLINE bool IsNumber(ReadOnlyRoots roots) const;

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, Value, _)           \
  V8_INLINE bool Is##Type(Isolate* isolate) const;      \
  V8_INLINE bool Is##Type(LocalIsolate* isolate) const; \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const;   \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
  IS_TYPE_FUNCTION_DECL(NullOrUndefined, , /* unused */)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsZero() const;
  V8_INLINE bool IsNoSharedNameSentinel() const;
  V8_INLINE bool IsPrivateSymbol() const;
  V8_INLINE bool IsPublicSymbol() const;

#if !V8_ENABLE_WEBASSEMBLY
  // Dummy implementation on builds without WebAssembly.
  bool IsWasmObject(Isolate* = nullptr) const { return false; }
#endif

  V8_INLINE bool IsJSObjectThatCanBeTrackedAsPrototype() const;

  enum class Conversion { kToNumber, kToNumeric };

#define DECL_STRUCT_PREDICATE(NAME, Name, name) \
  V8_INLINE bool Is##Name() const;              \
  V8_INLINE bool Is##Name(PtrComprCageBase cage_base) const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  V8_INLINE
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsArray(Handle<Object> object);

  // Extract the number.
  inline double Number() const;
  V8_INLINE bool IsNaN() const;
  V8_INLINE bool IsMinusZero() const;
  V8_EXPORT_PRIVATE bool ToInt32(int32_t* value);
  inline bool ToUint32(uint32_t* value) const;

  inline Representation OptimalRepresentation(PtrComprCageBase cage_base) const;

  inline ElementsKind OptimalElementsKind(PtrComprCageBase cage_base) const;

  // If {allow_coercion} is true, then a Smi will be considered to fit
  // a Double representation, since it can be converted to a HeapNumber
  // and stored.
  inline bool FitsRepresentation(Representation representation,
                                 bool allow_coercion = true) const;

  inline bool FilterKey(PropertyFilter filter);

  Handle<FieldType> OptimalType(Isolate* isolate,
                                Representation representation);

  V8_EXPORT_PRIVATE static Handle<Object> NewStorageFor(
      Isolate* isolate, Handle<Object> object, Representation representation);

  template <AllocationType allocation_type = AllocationType::kYoung,
            typename IsolateT>
  static Handle<Object> WrapForRead(IsolateT* isolate, Handle<Object> object,
                                    Representation representation);

  // Returns true if the object is of the correct type to be used as a
  // implementation of a JSObject's elements.
  inline bool HasValidElements();

  // ECMA-262 9.2.
  template <typename IsolateT>
  V8_EXPORT_PRIVATE bool BooleanValue(IsolateT* isolate);
  Object ToBoolean(Isolate* isolate);

  // ES6 section 7.2.11 Abstract Relational Comparison
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<ComparisonResult>
  Compare(Isolate* isolate, Handle<Object> x, Handle<Object> y);

  // ES6 section 7.2.12 Abstract Equality Comparison
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> Equals(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);

  // ES6 section 7.2.13 Strict Equality Comparison
  V8_EXPORT_PRIVATE bool StrictEquals(Object that);

  // ES6 section 7.1.13 ToObject
  // Convert to a JSObject if needed.
  // native_context is used when creating wrapper object.
  //
  // Passing a non-null method_name allows us to give a more informative
  // error message for those cases where ToObject is being called on
  // the receiver of a built-in method.
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<JSReceiver> ToObject(
      Isolate* isolate, Handle<Object> object,
      const char* method_name = nullptr);
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> ToObjectImpl(
      Isolate* isolate, Handle<Object> object,
      const char* method_name = nullptr);

  // ES6 section 9.2.1.2, OrdinaryCallBindThis for sloppy callee.
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSReceiver> ConvertReceiver(
      Isolate* isolate, Handle<Object> object);

  // ES6 section 7.1.14 ToPropertyKey
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Name> ToName(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.1 ToPrimitive
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToPrimitive(
      Isolate* isolate, Handle<Object> input,
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

  // ES6 section 7.1.3 ToNumber
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToNumber(
      Isolate* isolate, Handle<Object> input);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToNumeric(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.4 ToInteger
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToInteger(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.5 ToInt32
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToInt32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.6 ToUint32
  V8_WARN_UNUSED_RESULT inline static MaybeHandle<Object> ToUint32(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.12 ToString
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<String> ToString(
      Isolate* isolate, Handle<Object> input);

  V8_EXPORT_PRIVATE static MaybeHandle<String> NoSideEffectsToMaybeString(
      Isolate* isolate, Handle<Object> input);

  V8_EXPORT_PRIVATE static Handle<String> NoSideEffectsToString(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.14 ToPropertyKey
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToPropertyKey(
      Isolate* isolate, Handle<Object> value);

  // ES6 section 7.1.15 ToLength
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToLength(
      Isolate* isolate, Handle<Object> input);

  // ES6 section 7.1.17 ToIndex
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> ToIndex(
      Isolate* isolate, Handle<Object> input, MessageTemplate error_index);

  // ES6 section 7.3.9 GetMethod
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetMethod(
      Handle<JSReceiver> receiver, Handle<Name> name);

  // ES6 section 7.3.17 CreateListFromArrayLike
  V8_WARN_UNUSED_RESULT static MaybeHandle<FixedArray> CreateListFromArrayLike(
      Isolate* isolate, Handle<Object> object, ElementTypes element_types);

  // Get length property and apply ToLength.
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetLengthFromArrayLike(
      Isolate* isolate, Handle<JSReceiver> object);

  // ES6 section 12.5.6 The typeof Operator
  static Handle<String> TypeOf(Isolate* isolate, Handle<Object> object);

  // ES6 section 12.7 Additive Operators
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> Add(Isolate* isolate,
                                                       Handle<Object> lhs,
                                                       Handle<Object> rhs);

  // ES6 section 12.9 Relational Operators
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> GreaterThan(Isolate* isolate,
                                                              Handle<Object> x,
                                                              Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> GreaterThanOrEqual(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> LessThan(Isolate* isolate,
                                                           Handle<Object> x,
                                                           Handle<Object> y);
  V8_WARN_UNUSED_RESULT static inline Maybe<bool> LessThanOrEqual(
      Isolate* isolate, Handle<Object> x, Handle<Object> y);

  // ES6 section 7.3.19 OrdinaryHasInstance (C, O).
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> OrdinaryHasInstance(
      Isolate* isolate, Handle<Object> callable, Handle<Object> object);

  // ES6 section 12.10.4 Runtime Semantics: InstanceofOperator(O, C)
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> InstanceOf(
      Isolate* isolate, Handle<Object> object, Handle<Object> callable);

  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  GetProperty(LookupIterator* it, bool is_global_reference = false);

  // ES6 [[Set]] (when passed kDontThrow)
  // Invariants for this and related functions (unless stated otherwise):
  // 1) When the result is Nothing, an exception is pending.
  // 2) When passed kThrowOnError, the result is never Just(false).
  // In some cases, an exception is thrown regardless of the ShouldThrow
  // argument.  These cases are either in accordance with the spec or not
  // covered by it (eg., concerning API callbacks).
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> SetProperty(
      LookupIterator* it, Handle<Object> value, StoreOrigin store_origin,
      Maybe<ShouldThrow> should_throw = Nothing<ShouldThrow>());
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  SetProperty(Isolate* isolate, Handle<Object> object, Handle<Name> name,
              Handle<Object> value,
              StoreOrigin store_origin = StoreOrigin::kMaybeKeyed,
              Maybe<ShouldThrow> should_throw = Nothing<ShouldThrow>());
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> SetPropertyOrElement(
      Isolate* isolate, Handle<Object> object, Handle<Name> name,
      Handle<Object> value,
      Maybe<ShouldThrow> should_throw = Nothing<ShouldThrow>(),
      StoreOrigin store_origin = StoreOrigin::kMaybeKeyed);

  V8_WARN_UNUSED_RESULT static Maybe<bool> SetSuperProperty(
      LookupIterator* it, Handle<Object> value, StoreOrigin store_origin,
      Maybe<ShouldThrow> should_throw = Nothing<ShouldThrow>());

  V8_WARN_UNUSED_RESULT static Maybe<bool> CannotCreateProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      LookupIterator* it, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> WriteToReadOnlyProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<Object> name,
      Handle<Object> value, ShouldThrow should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> RedefineIncompatibleProperty(
      Isolate* isolate, Handle<Object> name, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetDataProperty(
      LookupIterator* it, Handle<Object> value);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static Maybe<bool> AddDataProperty(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      Maybe<ShouldThrow> should_throw, StoreOrigin store_origin,
      EnforceDefineSemantics semantics = EnforceDefineSemantics::kSet);

  V8_WARN_UNUSED_RESULT static Maybe<bool> TransitionAndWriteDataProperty(
      LookupIterator* it, Handle<Object> value, PropertyAttributes attributes,
      Maybe<ShouldThrow> should_throw, StoreOrigin store_origin);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Isolate* isolate, Handle<Object> object, Handle<Name> name);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetPropertyOrElement(
      Handle<Object> receiver, Handle<Name> name, Handle<JSReceiver> holder);
  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetProperty(
      Isolate* isolate, Handle<Object> object, Handle<Name> name);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithAccessor(
      LookupIterator* it);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithAccessor(
      LookupIterator* it, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetPropertyWithDefinedGetter(
      Handle<Object> receiver, Handle<JSReceiver> getter);
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyWithDefinedSetter(
      Handle<Object> receiver, Handle<JSReceiver> setter, Handle<Object> value,
      Maybe<ShouldThrow> should_throw);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> GetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index);

  V8_WARN_UNUSED_RESULT static inline MaybeHandle<Object> SetElement(
      Isolate* isolate, Handle<Object> object, uint32_t index,
      Handle<Object> value, ShouldThrow should_throw);

  // Returns the permanent hash code associated with this object. May return
  // undefined if not yet created.
  inline Object GetHash();

  // Returns the permanent hash code associated with this object depending on
  // the actual object type. May create and store a hash code if needed and none
  // exists.
  V8_EXPORT_PRIVATE Smi GetOrCreateHash(Isolate* isolate);

  // Checks whether this object has the same value as the given one.  This
  // function is implemented according to ES5, section 9.12 and can be used
  // to implement the Object.is function.
  V8_EXPORT_PRIVATE bool SameValue(Object other);

  // A part of SameValue which handles Number vs. Number case.
  // Treats NaN == NaN and +0 != -0.
  inline static bool SameNumberValue(double number1, double number2);

  // Checks whether this object has the same value as the given one.
  // +0 and -0 are treated equal. Everything else is the same as SameValue.
  // This function is implemented according to ES6, section 7.2.4 and is used
  // by ES6 Map and Set.
  bool SameValueZero(Object other);

  // ES6 section 9.4.2.3 ArraySpeciesCreate (part of it)
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ArraySpeciesConstructor(
      Isolate* isolate, Handle<Object> original_array);

  // ES6 section 7.3.20 SpeciesConstructor ( O, defaultConstructor )
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SpeciesConstructor(
      Isolate* isolate, Handle<JSReceiver> recv,
      Handle<JSFunction> default_ctor);

  // Tries to convert an object to an array length. Returns true and sets the
  // output parameter if it succeeds.
  inline bool ToArrayLength(uint32_t* index) const;

  // Tries to convert an object to an array index. Returns true and sets the
  // output parameter if it succeeds. Equivalent to ToArrayLength, but does not
  // allow kMaxUInt32.
  V8_WARN_UNUSED_RESULT inline bool ToArrayIndex(uint32_t* index) const;

  // Tries to convert an object to an index (in the range 0..size_t::max).
  // Returns true and sets the output parameter if it succeeds.
  inline bool ToIntegerIndex(size_t* index) const;

  // Returns true if the result of iterating over the object is the same
  // (including observable effects) as simply accessing the properties between 0
  // and length.
  V8_EXPORT_PRIVATE bool IterationHasObservableEffects();

  // TC39 "Dynamic Code Brand Checks"
  bool IsCodeLike(Isolate* isolate) const;

  EXPORT_DECL_VERIFIER(Object)

#ifdef VERIFY_HEAP
  // Verify a pointer is a valid (non-InstructionStream) object pointer.
  // When V8_EXTERNAL_CODE_SPACE is enabled InstructionStream objects are not
  // allowed.
  static void VerifyPointer(Isolate* isolate, Object p);
  // Verify a pointer is a valid object pointer.
  // InstructionStream objects are allowed regardless of the
  // V8_EXTERNAL_CODE_SPACE mode.
  static void VerifyAnyTagged(Isolate* isolate, Object p);
#endif

#ifdef DEBUG
  inline bool IsApiCallResultType() const;
#endif  // DEBUG

  // Prints this object without details.
  V8_EXPORT_PRIVATE void ShortPrint(FILE* out = stdout) const;

  // Prints this object without details to a message accumulator.
  V8_EXPORT_PRIVATE void ShortPrint(StringStream* accumulator) const;

  V8_EXPORT_PRIVATE void ShortPrint(std::ostream& os) const;

  inline static Object cast(Object object) { return object; }
  inline static Object unchecked_cast(Object object) { return object; }

  // Layout description.
  static const int kHeaderSize = 0;  // Object does not take up any space.

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  V8_EXPORT_PRIVATE void Print() const;

  // Prints this object with details.
  V8_EXPORT_PRIVATE void Print(std::ostream& os) const;
#else
  void Print() const { ShortPrint(); }
  void Print(std::ostream& os) const { ShortPrint(os); }
#endif

  // For use with std::unordered_set.
  struct Hasher {
    size_t operator()(const Object o) const {
      return std::hash<v8::internal::Address>{}(static_cast<Tagged_t>(o.ptr()));
    }
  };

  // For use with std::unordered_set/unordered_map when using both
  // InstructionStream and non-InstructionStream objects as keys.
  struct KeyEqualSafe {
    bool operator()(const Object a, const Object b) const {
      return a.SafeEquals(b);
    }
  };

  // For use with std::map.
  struct Comparer {
    bool operator()(const Object a, const Object b) const { return a < b; }
  };

  template <class T, typename std::enable_if<std::is_arithmetic<T>::value ||
                                                 std::is_enum<T>::value,
                                             int>::type = 0>
  inline T ReadField(size_t offset) const {
    return ReadMaybeUnalignedValue<T>(field_address(offset));
  }

  template <class T, typename std::enable_if<std::is_arithmetic<T>::value ||
                                                 std::is_enum<T>::value,
                                             int>::type = 0>
  inline void WriteField(size_t offset, T value) const {
    return WriteMaybeUnalignedValue<T>(field_address(offset), value);
  }

  // Atomically reads a field using relaxed memory ordering. Can only be used
  // with integral types whose size is <= kTaggedSize (to guarantee alignment).
  template <class T,
            typename std::enable_if<(std::is_arithmetic<T>::value ||
                                     std::is_enum<T>::value) &&
                                        !std::is_floating_point<T>::value,
                                    int>::type = 0>
  inline T Relaxed_ReadField(size_t offset) const;

  // Atomically writes a field using relaxed memory ordering. Can only be used
  // with integral types whose size is <= kTaggedSize (to guarantee alignment).
  template <class T,
            typename std::enable_if<(std::is_arithmetic<T>::value ||
                                     std::is_enum<T>::value) &&
                                        !std::is_floating_point<T>::value,
                                    int>::type = 0>
  inline void Relaxed_WriteField(size_t offset, T value);

  //
  // SandboxedPointer_t field accessors.
  //
  inline Address ReadSandboxedPointerField(size_t offset,
                                           PtrComprCageBase cage_base) const;
  inline void WriteSandboxedPointerField(size_t offset,
                                         PtrComprCageBase cage_base,
                                         Address value);
  inline void WriteSandboxedPointerField(size_t offset, Isolate* isolate,
                                         Address value);

  //
  // BoundedSize field accessors.
  //
  inline size_t ReadBoundedSizeField(size_t offset) const;
  inline void WriteBoundedSizeField(size_t offset, size_t value);

  //
  // ExternalPointer_t field accessors.
  //
  template <ExternalPointerTag tag>
  inline void InitExternalPointerField(size_t offset, Isolate* isolate,
                                       Address value);
  template <ExternalPointerTag tag>
  inline Address ReadExternalPointerField(size_t offset,
                                          Isolate* isolate) const;
  template <ExternalPointerTag tag>
  inline void WriteExternalPointerField(size_t offset, Isolate* isolate,
                                        Address value);

  // If the receiver is the JSGlobalObject, the store was contextual. In case
  // the property did not exist yet on the global object itself, we have to
  // throw a reference error in strict mode.  In sloppy mode, we continue.
  // Returns false if the exception was thrown, otherwise true.
  static bool CheckContextualStoreToJSGlobalObject(
      LookupIterator* it, Maybe<ShouldThrow> should_throw);

  // Returns whether the object is safe to share across Isolates.
  //
  // Currently, the following kinds of values can be safely shared across
  // Isolates:
  // - Smis
  // - Objects in RO space when the RO space is shared
  // - HeapNumbers in the shared old space
  // - Strings for which String::IsShared() is true
  // - JSSharedStructs
  // - JSSharedArrays
  inline bool IsShared() const;

  // Returns an equivalent value that's safe to share across Isolates if
  // possible. Acts as the identity function when value->IsShared().
  static inline MaybeHandle<Object> Share(
      Isolate* isolate, Handle<Object> value,
      ShouldThrow throw_if_cannot_be_shared);

  static MaybeHandle<Object> ShareSlow(Isolate* isolate,
                                       Handle<HeapObject> value,
                                       ShouldThrow throw_if_cannot_be_shared);

  // Whether this Object can be held weakly, i.e. whether it can be used as a
  // key in WeakMap, as a key in WeakSet, as the target of a WeakRef, or as a
  // target or unregister token of a FinalizationRegistry.
  inline bool CanBeHeldWeakly() const;

 protected:
  inline Address field_address(size_t offset) const {
    return ptr() + offset - kHeapObjectTag;
  }

 private:
  friend class CompressedObjectSlot;
  friend class FullObjectSlot;
  friend class LookupIterator;
  friend class StringStream;

  // Return the map of the root of object's prototype chain.
  Map GetPrototypeChainRootMap(Isolate* isolate) const;

  // Returns a non-SMI for JSReceivers, but returns the hash code for
  // simple objects.  This avoids a double lookup in the cases where
  // we know we will add the hash to the JSReceiver if it does not
  // already exist.
  //
  // Despite its size, this needs to be inlined for performance
  // reasons.
  static inline Object GetSimpleHash(Object object);

  // Helper for SetProperty and SetSuperProperty.
  // Return value is only meaningful if [found] is set to true on return.
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPropertyInternal(
      LookupIterator* it, Handle<Object> value, Maybe<ShouldThrow> should_throw,
      StoreOrigin store_origin, bool* found);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Name> ConvertToName(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToPropertyKey(
      Isolate* isolate, Handle<Object> value);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<String>
  ConvertToString(Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToNumberOrNumeric(
      Isolate* isolate, Handle<Object> input, Conversion mode);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  ConvertToInteger(Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToInt32(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToUint32(
      Isolate* isolate, Handle<Object> input);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  ConvertToLength(Isolate* isolate, Handle<Object> input);
  V8_EXPORT_PRIVATE V8_WARN_UNUSED_RESULT static MaybeHandle<Object>
  ConvertToIndex(Isolate* isolate, Handle<Object> input,
                 MessageTemplate error_index);
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Object& obj);

struct Brief {
  template <typename TObject>
  explicit Brief(TObject v) : value{v.ptr()} {}
  // {value} is a tagged heap object reference (weak or strong), equivalent to
  // a MaybeObject's payload. It has a plain Address type to keep #includes
  // lightweight.
  const Address value;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Brief& v);

// Objects should never have the weak tag; this variant is for overzealous
// checking.
V8_INLINE static bool HasWeakHeapObjectTag(const Object value) {
  return HAS_WEAK_HEAP_OBJECT_TAG(value.ptr());
}

// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (e.g. mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
//
// When external code space is enabled forwarding pointers are encoded as
// Smi values representing a diff from the source or map word host object
// address in kObjectAlignment chunks. Such a representation has the following
// properties:
// a) it can hold both positive an negative diffs for full pointer compression
//    cage size (HeapObject address has only valuable 30 bits while Smis have
//    31 bits),
// b) it's independent of the pointer compression base and pointer compression
//    scheme.
class MapWord {
 public:
  // Normal state: the map word contains a map pointer.

  // Create a map word from a map pointer.
  static inline MapWord FromMap(const Map map);

  // View this map word as a map pointer.
  inline Map ToMap() const;

  // Scavenge collection: the map word of live objects in the from space
  // contains a forwarding address (a heap object pointer in the to space).

  // True if this map word is a forwarding address for a scavenge
  // collection.  Only valid during a scavenge collection (specifically,
  // when all map words are heap object pointers, i.e. not during a full GC).
  inline bool IsForwardingAddress() const;

  // Create a map word from a forwarding address.
  static inline MapWord FromForwardingAddress(HeapObject map_word_host,
                                              HeapObject object);

  // View this map word as a forwarding address.
  inline HeapObject ToForwardingAddress(HeapObject map_word_host);

  constexpr inline Address ptr() const { return value_; }

  // When pointer compression is enabled, MapWord is uniquely identified by
  // the lower 32 bits. On the other hand full-value comparison is not correct
  // because map word in a forwarding state might have corrupted upper part.
  constexpr bool operator==(MapWord other) const {
    return static_cast<Tagged_t>(ptr()) == static_cast<Tagged_t>(other.ptr());
  }
  constexpr bool operator!=(MapWord other) const {
    return static_cast<Tagged_t>(ptr()) != static_cast<Tagged_t>(other.ptr());
  }

#ifdef V8_MAP_PACKING
  static constexpr Address Pack(Address map) {
    return map ^ Internals::kMapWordXorMask;
  }
  static constexpr Address Unpack(Address mapword) {
    // TODO(wenyuzhao): Clear header metadata.
    return mapword ^ Internals::kMapWordXorMask;
  }
  static constexpr bool IsPacked(Address mapword) {
    return (static_cast<intptr_t>(mapword) & Internals::kMapWordXorMask) ==
               Internals::kMapWordSignature &&
           (0xffffffff00000000 & static_cast<intptr_t>(mapword)) != 0;
  }
#else
  static constexpr bool IsPacked(Address) { return false; }
#endif

 private:
  // HeapObject calls the private constructor and directly reads the value.
  friend class HeapObject;
  template <typename TFieldType, int kFieldOffset, typename CompressionScheme>
  friend class TaggedField;

  explicit constexpr MapWord(Address value) : value_(value) {}

  Address value_;
};

template <int start_offset, int end_offset, int size>
class FixedBodyDescriptor;

template <int start_offset>
class FlexibleBodyDescriptor;

template <int start_offset>
class FlexibleWeakBodyDescriptor;

template <class ParentBodyDescriptor, class ChildBodyDescriptor>
class SubclassBodyDescriptor;

enum EnsureElementsMode {
  DONT_ALLOW_DOUBLE_ELEMENTS,
  ALLOW_COPIED_DOUBLE_ELEMENTS,
  ALLOW_CONVERTED_DOUBLE_ELEMENTS
};

// Indicator for one component of an AccessorPair.
enum AccessorComponent { ACCESSOR_GETTER, ACCESSOR_SETTER };

// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable {
 public:
  explicit inline Relocatable(Isolate* isolate);
  inline virtual ~Relocatable();
  virtual void IterateInstance(RootVisitor* v) {}
  virtual void PostGarbageCollection() {}

  static void PostGarbageCollectionProcessing(Isolate* isolate);
  static int ArchiveSpacePerThread();
  static char* ArchiveState(Isolate* isolate, char* to);
  static char* RestoreState(Isolate* isolate, char* from);
  static void Iterate(Isolate* isolate, RootVisitor* v);
  static void Iterate(RootVisitor* v, Relocatable* top);
  static char* Iterate(RootVisitor* v, char* t);

 private:
  Isolate* isolate_;
  Relocatable* prev_;
};

// BooleanBit is a helper class for setting and getting a bit in an integer.
class BooleanBit : public AllStatic {
 public:
  static inline bool get(int value, int bit_position) {
    return (value & (1 << bit_position)) != 0;
  }

  static inline int set(int value, int bit_position, bool v) {
    if (v) {
      value |= (1 << bit_position);
    } else {
      value &= ~(1 << bit_position);
    }
    return value;
  }
};

// This is an RAII helper class to emit a store-store memory barrier when
// publishing objects allocated in the shared heap.
//
// This helper must be used in every Factory method that allocates a shared
// JSObject visible user JS code. This is also used in Object::ShareSlow when
// publishing newly shared JS primitives.
//
// While there is no default ordering guarantee for shared JS objects
// (e.g. without the use of Atomics methods or postMessage, data races on
// fields are observable), the internal VM state of a JS object must be safe
// for publishing so that other threads do not crash.
//
// This barrier does not provide synchronization for publishing JS shared
// objects. It only ensures the weaker "do not crash the VM" guarantee.
//
// In particular, note that memory barriers are invisible to TSAN. When
// concurrent marking is active, field accesses are performed with relaxed
// atomics, and TSAN is unable to detect data races in shared JS objects. When
// concurrent marking is inactive, unordered publishes of shared JS objects in
// JS code are reported as data race warnings by TSAN.
class V8_NODISCARD SharedObjectSafePublishGuard final {
 public:
  ~SharedObjectSafePublishGuard() {
    // A release fence is used to prevent store-store reorderings of stores to
    // VM-internal state of shared objects past any subsequent stores (i.e. the
    // publish).
    //
    // On the loading side, we rely on neither the compiler nor the CPU
    // reordering loads that are dependent on observing the address of the
    // published shared object, like fields of the shared object.
    std::atomic_thread_fence(std::memory_order_release);
  }
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_OBJECTS_H_
