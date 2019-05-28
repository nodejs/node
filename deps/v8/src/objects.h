// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_H_
#define V8_OBJECTS_H_

#include <iosfwd>
#include <memory>

#include "include/v8-internal.h"
#include "include/v8.h"
#include "include/v8config.h"
#include "src/assert-scope.h"
#include "src/base/bits.h"
#include "src/base/build_config.h"
#include "src/base/flags.h"
#include "src/base/logging.h"
#include "src/checks.h"
#include "src/constants-arch.h"
#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/flags.h"
#include "src/message-template.h"
#include "src/objects-definitions.h"
#include "src/property-details.h"
#include "src/utils.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

//
// Most object types in the V8 JavaScript are described in this file.
//
// Inheritance hierarchy:
// - Object
//   - Smi          (immediate small integer)
//   - HeapObject   (superclass for everything allocated in the heap)
//     - JSReceiver  (suitable for property access)
//       - JSObject
//         - JSArray
//         - JSArrayBuffer
//         - JSArrayBufferView
//           - JSTypedArray
//           - JSDataView
//         - JSBoundFunction
//         - JSCollection
//           - JSSet
//           - JSMap
//         - JSStringIterator
//         - JSSetIterator
//         - JSMapIterator
//         - JSWeakCollection
//           - JSWeakMap
//           - JSWeakSet
//         - JSRegExp
//         - JSFunction
//         - JSGeneratorObject
//         - JSGlobalObject
//         - JSGlobalProxy
//         - JSValue
//           - JSDate
//         - JSMessageObject
//         - JSModuleNamespace
//         - JSV8BreakIterator     // If V8_INTL_SUPPORT enabled.
//         - JSCollator            // If V8_INTL_SUPPORT enabled.
//         - JSDateTimeFormat      // If V8_INTL_SUPPORT enabled.
//         - JSListFormat          // If V8_INTL_SUPPORT enabled.
//         - JSLocale              // If V8_INTL_SUPPORT enabled.
//         - JSNumberFormat        // If V8_INTL_SUPPORT enabled.
//         - JSPluralRules         // If V8_INTL_SUPPORT enabled.
//         - JSRelativeTimeFormat  // If V8_INTL_SUPPORT enabled.
//         - JSSegmentIterator     // If V8_INTL_SUPPORT enabled.
//         - JSSegmenter           // If V8_INTL_SUPPORT enabled.
//         - WasmExceptionObject
//         - WasmGlobalObject
//         - WasmInstanceObject
//         - WasmMemoryObject
//         - WasmModuleObject
//         - WasmTableObject
//       - JSProxy
//     - FixedArrayBase
//       - ByteArray
//       - BytecodeArray
//       - FixedArray
//         - FrameArray
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
//         - ModuleInfo
//         - ScriptContextTable
//         - ClosureFeedbackCellArray
//       - FixedDoubleArray
//     - Name
//       - String
//         - SeqString
//           - SeqOneByteString
//           - SeqTwoByteString
//         - SlicedString
//         - ConsString
//         - ThinString
//         - ExternalString
//           - ExternalOneByteString
//           - ExternalTwoByteString
//         - InternalizedString
//           - SeqInternalizedString
//             - SeqOneByteInternalizedString
//             - SeqTwoByteInternalizedString
//           - ConsInternalizedString
//           - ExternalInternalizedString
//             - ExternalOneByteInternalizedString
//             - ExternalTwoByteInternalizedString
//       - Symbol
//     - Context
//       - NativeContext
//     - HeapNumber
//     - BigInt
//     - Cell
//     - DescriptorArray
//     - PropertyCell
//     - PropertyArray
//     - Code
//     - AbstractCode, a wrapper around Code or BytecodeArray
//     - Map
//     - Oddball
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
//       - StackFrameInfo
//       - StackTraceFrame
//       - SourcePositionTableWithFrameCache
//       - CodeCache
//       - PrototypeInfo
//       - Microtask
//         - CallbackTask
//         - CallableTask
//         - PromiseReactionJobTask
//           - PromiseFulfillReactionJobTask
//           - PromiseRejectReactionJobTask
//         - PromiseResolveThenableJobTask
//       - Module
//       - ModuleInfoEntry
//     - FeedbackCell
//     - FeedbackVector
//     - PreparseData
//     - UncompiledData
//       - UncompiledDataWithoutPreparseData
//       - UncompiledDataWithPreparseData
//
// Formats of Object::ptr_:
//  Smi:        [31 bit signed int] 0
//  HeapObject: [32 bit direct pointer] (4 byte aligned) | 01

namespace v8 {
namespace internal {

struct InliningPosition;
class PropertyDescriptorObject;

// SKIP_WRITE_BARRIER skips the write barrier.
// UPDATE_WEAK_WRITE_BARRIER skips the marking part of the write barrier and
// only performs the generational part.
// UPDATE_WRITE_BARRIER is doing the full barrier, marking and generational.
enum WriteBarrierMode {
  SKIP_WRITE_BARRIER,
  UPDATE_WEAK_WRITE_BARRIER,
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
enum TransitionFlag {
  INSERT_TRANSITION,
  OMIT_TRANSITION
};


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
enum DescriptorFlag {
  ALL_DESCRIPTORS,
  OWN_DESCRIPTORS
};

// Instance size sentinel for objects of variable size.
const int kVariableSizeSentinel = 0;

// We may store the unsigned bit field as signed Smi value and do not
// use the sign bit.
const int kStubMajorKeyBits = 8;
const int kStubMinorKeyBits = kSmiValueSize - kStubMajorKeyBits - 1;

// Result of an abstract relational comparison of x and y, implemented according
// to ES6 section 7.2.11 Abstract Relational Comparison.
enum class ComparisonResult {
  kLessThan,     // x < y
  kEqual,        // x = y
  kGreaterThan,  // x > y
  kUndefined     // at least one of x or y was undefined or NaN
};

// (Returns false whenever {result} is kUndefined.)
bool ComparisonResultToBool(Operation op, ComparisonResult result);

enum class OnNonExistent { kThrowReferenceError, kReturnUndefined };

class AbstractCode;
class AccessorPair;
class AccessCheckInfo;
class AllocationSite;
class ByteArray;
class CachedTemplateObject;
class Cell;
class ClosureFeedbackCellArray;
class ConsString;
class DependentCode;
class ElementsAccessor;
class EnumCache;
class FixedArrayBase;
class FixedDoubleArray;
class FreeSpace;
class FunctionLiteral;
class FunctionTemplateInfo;
class JSAsyncGeneratorObject;
class JSGlobalProxy;
class JSPromise;
class JSProxy;
class JSProxyRevocableResult;
class KeyAccumulator;
class LayoutDescriptor;
class LookupIterator;
class FieldType;
class Module;
class ModuleInfoEntry;
class MutableHeapNumber;
class ObjectHashTable;
class ObjectTemplateInfo;
class ObjectVisitor;
class PreparseData;
class PropertyArray;
class PropertyCell;
class PropertyDescriptor;
class PrototypeInfo;
class ReadOnlyRoots;
class RegExpMatchInfo;
class RootVisitor;
class SafepointEntry;
class ScriptContextTable;
class SharedFunctionInfo;
class StringStream;
class Symbol;
class FeedbackCell;
class FeedbackMetadata;
class FeedbackVector;
class UncompiledData;
class TemplateInfo;
class TransitionArray;
class TemplateList;
class WasmInstanceObject;
class WasmMemoryObject;
template <typename T>
class ZoneForwardList;

#ifdef OBJECT_PRINT
#define DECL_PRINTER(Name) void Name##Print(std::ostream& os);  // NOLINT
#else
#define DECL_PRINTER(Name)
#endif

#define OBJECT_TYPE_LIST(V) \
  V(Smi)                    \
  V(LayoutDescriptor)       \
  V(HeapObject)             \
  V(Primitive)              \
  V(Number)                 \
  V(Numeric)

#define HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V) \
  V(AbstractCode)                              \
  V(AccessCheckNeeded)                         \
  V(AllocationSite)                            \
  V(ArrayList)                                 \
  V(BigInt)                                    \
  V(BigIntWrapper)                             \
  V(ObjectBoilerplateDescription)              \
  V(Boolean)                                   \
  V(BooleanWrapper)                            \
  V(BreakPoint)                                \
  V(BreakPointInfo)                            \
  V(ByteArray)                                 \
  V(BytecodeArray)                             \
  V(CachedTemplateObject)                      \
  V(CallHandlerInfo)                           \
  V(Callable)                                  \
  V(Cell)                                      \
  V(ClassBoilerplate)                          \
  V(Code)                                      \
  V(CodeDataContainer)                         \
  V(CompilationCacheTable)                     \
  V(ConsString)                                \
  V(Constructor)                               \
  V(Context)                                   \
  V(CoverageInfo)                              \
  V(ClosureFeedbackCellArray)                  \
  V(DataHandler)                               \
  V(DeoptimizationData)                        \
  V(DependentCode)                             \
  V(DescriptorArray)                           \
  V(EmbedderDataArray)                         \
  V(EphemeronHashTable)                        \
  V(ExternalOneByteString)                     \
  V(ExternalString)                            \
  V(ExternalTwoByteString)                     \
  V(FeedbackCell)                              \
  V(FeedbackMetadata)                          \
  V(FeedbackVector)                            \
  V(Filler)                                    \
  V(FixedArray)                                \
  V(FixedArrayBase)                            \
  V(FixedArrayExact)                           \
  V(FixedBigInt64Array)                        \
  V(FixedBigUint64Array)                       \
  V(FixedDoubleArray)                          \
  V(FixedFloat32Array)                         \
  V(FixedFloat64Array)                         \
  V(FixedInt16Array)                           \
  V(FixedInt32Array)                           \
  V(FixedInt8Array)                            \
  V(FixedTypedArrayBase)                       \
  V(FixedUint16Array)                          \
  V(FixedUint32Array)                          \
  V(FixedUint8Array)                           \
  V(FixedUint8ClampedArray)                    \
  V(Foreign)                                   \
  V(FrameArray)                                \
  V(FreeSpace)                                 \
  V(Function)                                  \
  V(GlobalDictionary)                          \
  V(HandlerTable)                              \
  V(HeapNumber)                                \
  V(InternalizedString)                        \
  V(JSArgumentsObject)                         \
  V(JSArgumentsObjectWithLength)               \
  V(JSArray)                                   \
  V(JSArrayBuffer)                             \
  V(JSArrayBufferView)                         \
  V(JSArrayIterator)                           \
  V(JSAsyncFromSyncIterator)                   \
  V(JSAsyncFunctionObject)                     \
  V(JSAsyncGeneratorObject)                    \
  V(JSBoundFunction)                           \
  V(JSCollection)                              \
  V(JSContextExtensionObject)                  \
  V(JSDataView)                                \
  V(JSDate)                                    \
  V(JSError)                                   \
  V(JSFunction)                                \
  V(JSGeneratorObject)                         \
  V(JSGlobalObject)                            \
  V(JSGlobalProxy)                             \
  V(JSMap)                                     \
  V(JSMapIterator)                             \
  V(JSMessageObject)                           \
  V(JSModuleNamespace)                         \
  V(JSObject)                                  \
  V(JSPromise)                                 \
  V(JSProxy)                                   \
  V(JSReceiver)                                \
  V(JSRegExp)                                  \
  V(JSRegExpResult)                            \
  V(JSRegExpStringIterator)                    \
  V(JSSet)                                     \
  V(JSSetIterator)                             \
  V(JSSloppyArgumentsObject)                   \
  V(JSStringIterator)                          \
  V(JSTypedArray)                              \
  V(JSValue)                                   \
  V(JSWeakRef)                                 \
  V(JSWeakCollection)                          \
  V(JSFinalizationGroup)                       \
  V(JSFinalizationGroupCleanupIterator)        \
  V(JSWeakMap)                                 \
  V(JSWeakSet)                                 \
  V(LoadHandler)                               \
  V(Map)                                       \
  V(MapCache)                                  \
  V(Microtask)                                 \
  V(ModuleInfo)                                \
  V(MutableHeapNumber)                         \
  V(Name)                                      \
  V(NameDictionary)                            \
  V(NativeContext)                             \
  V(NormalizedMapCache)                        \
  V(NumberDictionary)                          \
  V(NumberWrapper)                             \
  V(ObjectHashSet)                             \
  V(ObjectHashTable)                           \
  V(Oddball)                                   \
  V(OrderedHashMap)                            \
  V(OrderedHashSet)                            \
  V(OrderedNameDictionary)                     \
  V(PreparseData)                              \
  V(PromiseReactionJobTask)                    \
  V(PropertyArray)                             \
  V(PropertyCell)                              \
  V(PropertyDescriptorObject)                  \
  V(RegExpMatchInfo)                           \
  V(ScopeInfo)                                 \
  V(ScriptContextTable)                        \
  V(ScriptWrapper)                             \
  V(SeqOneByteString)                          \
  V(SeqString)                                 \
  V(SeqTwoByteString)                          \
  V(SharedFunctionInfo)                        \
  V(SimpleNumberDictionary)                    \
  V(SlicedString)                              \
  V(SloppyArgumentsElements)                   \
  V(SmallOrderedHashMap)                       \
  V(SmallOrderedHashSet)                       \
  V(SmallOrderedNameDictionary)                \
  V(SourcePositionTableWithFrameCache)         \
  V(StoreHandler)                              \
  V(String)                                    \
  V(StringSet)                                 \
  V(StringTable)                               \
  V(StringWrapper)                             \
  V(Struct)                                    \
  V(Symbol)                                    \
  V(SymbolWrapper)                             \
  V(TemplateInfo)                              \
  V(TemplateList)                              \
  V(TemplateObjectDescription)                 \
  V(ThinString)                                \
  V(TransitionArray)                           \
  V(UncompiledData)                            \
  V(UncompiledDataWithPreparseData)            \
  V(UncompiledDataWithoutPreparseData)         \
  V(Undetectable)                              \
  V(UniqueName)                                \
  V(WasmExceptionObject)                       \
  V(WasmGlobalObject)                          \
  V(WasmInstanceObject)                        \
  V(WasmMemoryObject)                          \
  V(WasmModuleObject)                          \
  V(WasmTableObject)                           \
  V(WeakFixedArray)                            \
  V(WeakArrayList)                             \
  V(WeakCell)

#ifdef V8_INTL_SUPPORT
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)  \
  V(JSV8BreakIterator)                    \
  V(JSCollator)                           \
  V(JSDateTimeFormat)                     \
  V(JSListFormat)                         \
  V(JSLocale)                             \
  V(JSNumberFormat)                       \
  V(JSPluralRules)                        \
  V(JSRelativeTimeFormat)                 \
  V(JSSegmentIterator)                    \
  V(JSSegmenter)
#else
#define HEAP_OBJECT_ORDINARY_TYPE_LIST(V) HEAP_OBJECT_ORDINARY_TYPE_LIST_BASE(V)
#endif  // V8_INTL_SUPPORT

#define HEAP_OBJECT_TEMPLATE_TYPE_LIST(V) \
  V(Dictionary)                           \
  V(HashTable)

#define HEAP_OBJECT_TYPE_LIST(V)    \
  HEAP_OBJECT_ORDINARY_TYPE_LIST(V) \
  HEAP_OBJECT_TEMPLATE_TYPE_LIST(V)

#define ODDBALL_LIST(V)                 \
  V(Undefined, undefined_value)         \
  V(Null, null_value)                   \
  V(TheHole, the_hole_value)            \
  V(Exception, exception)               \
  V(Uninitialized, uninitialized_value) \
  V(True, true_value)                   \
  V(False, false_value)                 \
  V(ArgumentsMarker, arguments_marker)  \
  V(OptimizedOut, optimized_out)        \
  V(StaleRegister, stale_register)

// The element types selection for CreateListFromArrayLike.
enum class ElementTypes { kAll, kStringAndSymbol };

// TODO(mythria): Move this to a better place.
ShouldThrow GetShouldThrow(Isolate* isolate, Maybe<ShouldThrow> should_throw);

// Object is the abstract superclass for all classes in the
// object hierarchy.
// Object does not use any virtual functions to avoid the
// allocation of the C++ vtable.
// There must only be a single data member in Object: the Address ptr,
// containing the tagged heap pointer that this Object instance refers to.
// For a design overview, see https://goo.gl/Ph4CGz.
class Object {
 public:
  constexpr Object() : ptr_(kNullAddress) {}
  explicit constexpr Object(Address ptr) : ptr_(ptr) {}

  // Make clang on Linux catch what MSVC complains about on Windows:
  operator bool() const = delete;

  bool operator==(const Object that) const { return this->ptr() == that.ptr(); }
  bool operator!=(const Object that) const { return this->ptr() != that.ptr(); }
  // Usage in std::set requires operator<.
  bool operator<(const Object that) const { return this->ptr() < that.ptr(); }

  // Returns the tagged "(heap) object pointer" representation of this object.
  constexpr Address ptr() const { return ptr_; }

  // These operator->() overloads are required for handlified code.
  Object* operator->() { return this; }
  const Object* operator->() const { return this; }

  // Type testing.
  bool IsObject() const { return true; }

#define IS_TYPE_FUNCTION_DECL(Type) V8_INLINE bool Is##Type() const;
  OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
  HEAP_OBJECT_TYPE_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

// Oddball checks are faster when they are raw pointer comparisons, so the
// isolate/read-only roots overloads should be preferred where possible.
#define IS_TYPE_FUNCTION_DECL(Type, Value)            \
  V8_INLINE bool Is##Type(Isolate* isolate) const;    \
  V8_INLINE bool Is##Type(ReadOnlyRoots roots) const; \
  V8_INLINE bool Is##Type() const;
  ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
#undef IS_TYPE_FUNCTION_DECL

  V8_INLINE bool IsNullOrUndefined(Isolate* isolate) const;
  V8_INLINE bool IsNullOrUndefined(ReadOnlyRoots roots) const;
  V8_INLINE bool IsNullOrUndefined() const;

  enum class Conversion { kToNumber, kToNumeric };

#define RETURN_FAILURE(isolate, should_throw, call) \
  do {                                              \
    if ((should_throw) == kDontThrow) {             \
      return Just(false);                           \
    } else {                                        \
      isolate->Throw(*isolate->factory()->call);    \
      return Nothing<bool>();                       \
    }                                               \
  } while (false)

#define MAYBE_RETURN(call, value)         \
  do {                                    \
    if ((call).IsNothing()) return value; \
  } while (false)

#define MAYBE_RETURN_NULL(call) MAYBE_RETURN(call, MaybeHandle<Object>())

#define MAYBE_ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, dst, call) \
  do {                                                               \
    Isolate* __isolate__ = (isolate);                                \
    if (!(call).To(&dst)) {                                          \
      DCHECK(__isolate__->has_pending_exception());                  \
      return ReadOnlyRoots(__isolate__).exception();                 \
    }                                                                \
  } while (false)

#define DECL_STRUCT_PREDICATE(NAME, Name, name) V8_INLINE bool Is##Name() const;
  STRUCT_LIST(DECL_STRUCT_PREDICATE)
#undef DECL_STRUCT_PREDICATE

  // ES6, #sec-isarray.  NOT to be confused with %_IsArray.
  V8_INLINE
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsArray(Handle<Object> object);

  V8_INLINE bool IsHashTableBase() const;
  V8_INLINE bool IsSmallOrderedHashTable() const;

  // Extract the number.
  inline double Number() const;
  V8_INLINE bool IsNaN() const;
  V8_INLINE bool IsMinusZero() const;
  V8_EXPORT_PRIVATE bool ToInt32(int32_t* value);
  inline bool ToUint32(uint32_t* value) const;

  inline Representation OptimalRepresentation();

  inline ElementsKind OptimalElementsKind();

  inline bool FitsRepresentation(Representation representation);

  inline bool FilterKey(PropertyFilter filter);

  Handle<FieldType> OptimalType(Isolate* isolate,
                                Representation representation);

  V8_EXPORT_PRIVATE static Handle<Object> NewStorageFor(
      Isolate* isolate, Handle<Object> object, Representation representation);

  static Handle<Object> WrapForRead(Isolate* isolate, Handle<Object> object,
                                    Representation representation);

  // Returns true if the object is of the correct type to be used as a
  // implementation of a JSObject's elements.
  inline bool HasValidElements();

  // ECMA-262 9.2.
  V8_EXPORT_PRIVATE bool BooleanValue(Isolate* isolate);
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
      Handle<Object> input, ToPrimitiveHint hint = ToPrimitiveHint::kDefault);

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
  GetProperty(LookupIterator* it,
              OnNonExistent on_non_existent = OnNonExistent::kReturnUndefined);

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
  V8_WARN_UNUSED_RESULT static Maybe<bool> AddDataProperty(
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

  // Returns true if the result of iterating over the object is the same
  // (including observable effects) as simply accessing the properties between 0
  // and length.
  bool IterationHasObservableEffects();

  //
  // The following GetHeapObjectXX methods mimic corresponding functionality
  // in MaybeObject. Having them here allows us to unify code that processes
  // ObjectSlots and MaybeObjectSlots.
  //

  // If this Object is a strong pointer to a HeapObject, returns true and
  // sets *result. Otherwise returns false.
  inline bool GetHeapObjectIfStrong(HeapObject* result) const;

  // If this Object is a strong pointer to a HeapObject (weak pointers are not
  // expected), returns true and sets *result. Otherwise returns false.
  inline bool GetHeapObject(HeapObject* result) const;

  // DCHECKs that this Object is a strong pointer to a HeapObject and returns
  // the HeapObject.
  inline HeapObject GetHeapObject() const;

  // Always returns false because Object is not expected to be a weak pointer
  // to a HeapObject.
  inline bool GetHeapObjectIfWeak(HeapObject* result) const {
    DCHECK(!HasWeakHeapObjectTag(ptr()));
    return false;
  }
  // Always returns false because Object is not expected to be a weak pointer
  // to a HeapObject.
  inline bool IsCleared() const { return false; }

  EXPORT_DECL_VERIFIER(Object)

#ifdef VERIFY_HEAP
  // Verify a pointer is a valid object pointer.
  static void VerifyPointer(Isolate* isolate, Object p);
#endif

  inline void VerifyApiCallResultType();

  // Prints this object without details.
  V8_EXPORT_PRIVATE void ShortPrint(FILE* out = stdout) const;

  // Prints this object without details to a message accumulator.
  V8_EXPORT_PRIVATE void ShortPrint(StringStream* accumulator) const;

  V8_EXPORT_PRIVATE void ShortPrint(std::ostream& os) const;  // NOLINT

  inline static Object cast(Object object) { return object; }
  inline static Object unchecked_cast(Object object) { return object; }

  // Layout description.
  static const int kHeaderSize = 0;  // Object does not take up any space.

#ifdef OBJECT_PRINT
  // For our gdb macros, we should perhaps change these in the future.
  V8_EXPORT_PRIVATE void Print() const;

  // Prints this object with details.
  V8_EXPORT_PRIVATE void Print(std::ostream& os) const;  // NOLINT
#else
  void Print() const { ShortPrint(); }
  void Print(std::ostream& os) const { ShortPrint(os); }  // NOLINT
#endif

  // For use with std::unordered_set.
  struct Hasher {
    size_t operator()(const Object o) const {
      return std::hash<v8::internal::Address>{}(o.ptr());
    }
  };

  // For use with std::map.
  struct Comparer {
    bool operator()(const Object a, const Object b) const {
      return a.ptr() < b.ptr();
    }
  };

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
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToInteger(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToInt32(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToUint32(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToLength(
      Isolate* isolate, Handle<Object> input);
  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ConvertToIndex(
      Isolate* isolate, Handle<Object> input, MessageTemplate error_index);

  Address ptr_;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Object& obj);

// In objects.h to be usable without objects-inl.h inclusion.
bool Object::IsSmi() const { return HAS_SMI_TAG(ptr()); }
bool Object::IsHeapObject() const {
  DCHECK_EQ(!IsSmi(), Internals::HasHeapObjectTag(ptr()));
  return !IsSmi();
}

struct Brief {
  V8_EXPORT_PRIVATE explicit Brief(const Object v);
  explicit Brief(const MaybeObject v);
  // {value} is a tagged heap object reference (weak or strong), equivalent to
  // a MaybeObject's payload. It has a plain Address type to keep #includes
  // lightweight.
  const Address value;
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& os, const Brief& v);

// Objects should never have the weak tag; this variant is for overzealous
// checking.
V8_INLINE static bool HasWeakHeapObjectTag(const Object value) {
  return ((value->ptr() & kHeapObjectTagMask) == kWeakHeapObjectTag);
}

// Heap objects typically have a map pointer in their first word.  However,
// during GC other data (e.g. mark bits, forwarding addresses) is sometimes
// encoded in the first word.  The class MapWord is an abstraction of the
// value in a heap object's first word.
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
  static inline MapWord FromForwardingAddress(HeapObject object);

  // View this map word as a forwarding address.
  inline HeapObject ToForwardingAddress();

  static inline MapWord FromRawValue(uintptr_t value) {
    return MapWord(value);
  }

  inline uintptr_t ToRawValue() {
    return value_;
  }

 private:
  // HeapObject calls the private constructor and directly reads the value.
  friend class HeapObject;

  explicit MapWord(Address value) : value_(value) {}

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
enum AccessorComponent {
  ACCESSOR_GETTER,
  ACCESSOR_SETTER
};

enum class GetKeysConversion {
  kKeepNumbers = static_cast<int>(v8::KeyConversionMode::kKeepNumbers),
  kConvertToString = static_cast<int>(v8::KeyConversionMode::kConvertToString)
};

enum class KeyCollectionMode {
  kOwnOnly = static_cast<int>(v8::KeyCollectionMode::kOwnOnly),
  kIncludePrototypes =
      static_cast<int>(v8::KeyCollectionMode::kIncludePrototypes)
};

// Utility superclass for stack-allocated objects that must be updated
// on gc.  It provides two ways for the gc to update instances, either
// iterating or updating after gc.
class Relocatable {
 public:
  explicit inline Relocatable(Isolate* isolate);
  inline virtual ~Relocatable();
  virtual void IterateInstance(RootVisitor* v) {}
  virtual void PostGarbageCollection() { }

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


}  // NOLINT, false-positive due to second-order macros.
}  // NOLINT, false-positive due to second-order macros.

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_H_
