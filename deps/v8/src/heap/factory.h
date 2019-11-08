// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_FACTORY_H_
#define V8_HEAP_FACTORY_H_

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "src/builtins/builtins.h"
#include "src/common/globals.h"
#include "src/execution/messages.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/heap/heap.h"
#include "src/objects/code.h"
#include "src/objects/dictionary.h"
#include "src/objects/function-kind.h"
#include "src/objects/js-array.h"
#include "src/objects/js-regexp.h"
#include "src/objects/string.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AliasedArgumentsEntry;
class ObjectBoilerplateDescription;
class BreakPoint;
class BreakPointInfo;
class CallableTask;
class CallbackTask;
class CallHandlerInfo;
class Expression;
class EmbedderDataArray;
class ArrayBoilerplateDescription;
class CoverageInfo;
class DebugInfo;
class EnumCache;
class FreshlyAllocatedBigInt;
class Isolate;
class JSArrayBufferView;
class JSDataView;
class JSGeneratorObject;
class JSMap;
class JSMapIterator;
class JSModuleNamespace;
class JSPromise;
class JSProxy;
class JSSet;
class JSSetIterator;
class JSTypedArray;
class JSWeakMap;
class LoadHandler;
class NativeContext;
class NewFunctionArgs;
class PreparseData;
class PromiseResolveThenableJobTask;
class RegExpMatchInfo;
class ScriptContextTable;
class SourceTextModule;
class SourceTextModuleInfo;
class StackFrameInfo;
class StackTraceFrame;
class StoreHandler;
class SyntheticModule;
class TemplateObjectDescription;
class UncompiledDataWithoutPreparseData;
class UncompiledDataWithPreparseData;
class WasmCapiFunctionData;
class WasmExportedFunctionData;
class WasmJSFunctionData;
class WeakCell;
struct SourceRange;
template <typename T>
class ZoneVector;
enum class SharedFlag : uint8_t;
enum class InitializedFlag : uint8_t;

enum FunctionMode {
  kWithNameBit = 1 << 0,
  kWithHomeObjectBit = 1 << 1,
  kWithWritablePrototypeBit = 1 << 2,
  kWithReadonlyPrototypeBit = 1 << 3,
  kWithPrototypeBits = kWithWritablePrototypeBit | kWithReadonlyPrototypeBit,

  // Without prototype.
  FUNCTION_WITHOUT_PROTOTYPE = 0,
  METHOD_WITH_NAME = kWithNameBit,
  METHOD_WITH_HOME_OBJECT = kWithHomeObjectBit,
  METHOD_WITH_NAME_AND_HOME_OBJECT = kWithNameBit | kWithHomeObjectBit,

  // With writable prototype.
  FUNCTION_WITH_WRITEABLE_PROTOTYPE = kWithWritablePrototypeBit,
  FUNCTION_WITH_NAME_AND_WRITEABLE_PROTOTYPE =
      kWithWritablePrototypeBit | kWithNameBit,
  FUNCTION_WITH_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE =
      kWithWritablePrototypeBit | kWithHomeObjectBit,
  FUNCTION_WITH_NAME_AND_HOME_OBJECT_AND_WRITEABLE_PROTOTYPE =
      kWithWritablePrototypeBit | kWithNameBit | kWithHomeObjectBit,

  // With readonly prototype.
  FUNCTION_WITH_READONLY_PROTOTYPE = kWithReadonlyPrototypeBit,
  FUNCTION_WITH_NAME_AND_READONLY_PROTOTYPE =
      kWithReadonlyPrototypeBit | kWithNameBit,
};

// Interface for handle based allocation.
class V8_EXPORT_PRIVATE Factory {
 public:
  Handle<Oddball> NewOddball(Handle<Map> map, const char* to_string,
                             Handle<Object> to_number, const char* type_of,
                             byte kind);

  // Marks self references within code generation.
  Handle<Oddball> NewSelfReferenceMarker();

  // Allocates a fixed array-like object with given map and initialized with
  // undefined values.
  template <typename T = FixedArray>
  Handle<T> NewFixedArrayWithMap(
      RootIndex map_root_index, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Allocates a weak fixed array-like object with given map and initialized
  // with undefined values.
  template <typename T = WeakFixedArray>
  Handle<T> NewWeakFixedArrayWithMap(
      RootIndex map_root_index, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Allocates a fixed array initialized with undefined values.
  Handle<FixedArray> NewFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a fixed array which may contain in-place weak references. The
  // array is initialized with undefined values
  Handle<WeakFixedArray> NewWeakFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates a property array initialized with undefined values.
  Handle<PropertyArray> NewPropertyArray(int length);
  // Tries allocating a fixed array initialized with undefined values.
  // In case of an allocation failure (OOM) an empty handle is returned.
  // The caller has to manually signal an
  // v8::internal::Heap::FatalProcessOutOfMemory typically by calling
  // NewFixedArray as a fallback.
  V8_WARN_UNUSED_RESULT
  MaybeHandle<FixedArray> TryNewFixedArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocate a new fixed array with non-existing entries (the hole).
  Handle<FixedArray> NewFixedArrayWithHoles(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Allocates an uninitialized fixed array. It must be filled by the caller.
  Handle<FixedArray> NewUninitializedFixedArray(int length);

  // Allocates a closure feedback cell array whose feedback cells are
  // initialized with undefined values.
  Handle<ClosureFeedbackCellArray> NewClosureFeedbackCellArray(int num_slots);

  // Allocates a feedback vector whose slots are initialized with undefined
  // values.
  Handle<FeedbackVector> NewFeedbackVector(
      Handle<SharedFunctionInfo> shared,
      Handle<ClosureFeedbackCellArray> closure_feedback_cell_array);

  // Allocates a clean embedder data array with given capacity.
  Handle<EmbedderDataArray> NewEmbedderDataArray(int length);

  // Allocates a fixed array for name-value pairs of boilerplate properties and
  // calculates the number of properties we need to store in the backing store.
  Handle<ObjectBoilerplateDescription> NewObjectBoilerplateDescription(
      int boilerplate, int all_properties, int index_keys, bool has_seen_proto);

  // Allocate a new uninitialized fixed double array.
  // The function returns a pre-allocated empty fixed array for length = 0,
  // so the return type must be the general fixed array class.
  Handle<FixedArrayBase> NewFixedDoubleArray(int length);

  // Allocate a new fixed double array with hole values.
  Handle<FixedArrayBase> NewFixedDoubleArrayWithHoles(int size);

  // Allocates a FeedbackMedata object and zeroes the data section.
  Handle<FeedbackMetadata> NewFeedbackMetadata(
      int slot_count, int feedback_cell_count,
      AllocationType allocation = AllocationType::kOld);

  Handle<FrameArray> NewFrameArray(int number_of_frames);

  Handle<OrderedHashSet> NewOrderedHashSet();
  Handle<OrderedHashMap> NewOrderedHashMap();
  Handle<OrderedNameDictionary> NewOrderedNameDictionary();

  Handle<SmallOrderedHashSet> NewSmallOrderedHashSet(
      int capacity = kSmallOrderedHashSetMinCapacity,
      AllocationType allocation = AllocationType::kYoung);
  Handle<SmallOrderedHashMap> NewSmallOrderedHashMap(
      int capacity = kSmallOrderedHashMapMinCapacity,
      AllocationType allocation = AllocationType::kYoung);
  Handle<SmallOrderedNameDictionary> NewSmallOrderedNameDictionary(
      int capacity = kSmallOrderedHashMapMinCapacity,
      AllocationType allocation = AllocationType::kYoung);

  // Create a new PrototypeInfo struct.
  Handle<PrototypeInfo> NewPrototypeInfo();

  // Create a new EnumCache struct.
  Handle<EnumCache> NewEnumCache(Handle<FixedArray> keys,
                                 Handle<FixedArray> indices);

  // Create a new Tuple2 struct.
  Handle<Tuple2> NewTuple2(Handle<Object> value1, Handle<Object> value2,
                           AllocationType allocation);

  // Create a new ArrayBoilerplateDescription struct.
  Handle<ArrayBoilerplateDescription> NewArrayBoilerplateDescription(
      ElementsKind elements_kind, Handle<FixedArrayBase> constant_values);

  // Create a new TemplateObjectDescription struct.
  Handle<TemplateObjectDescription> NewTemplateObjectDescription(
      Handle<FixedArray> raw_strings, Handle<FixedArray> cooked_strings);

  // Create a pre-tenured empty AccessorPair.
  Handle<AccessorPair> NewAccessorPair();

  // Finds the internalized copy for string in the string table.
  // If not found, a new string is added to the table and returned.
  Handle<String> InternalizeUtf8String(const Vector<const char>& str);
  Handle<String> InternalizeUtf8String(const char* str) {
    return InternalizeUtf8String(CStrVector(str));
  }

  template <typename Char>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Handle<String> InternalizeString(const Vector<const Char>& str,
                                   bool convert_encoding = false);

  template <typename SeqString>
  Handle<String> InternalizeString(Handle<SeqString>, int from, int length,
                                   bool convert_encoding = false);

  template <class StringTableKey>
  Handle<String> InternalizeStringWithKey(StringTableKey* key);

  // Internalized strings are created in the old generation (data space).
  inline Handle<String> InternalizeString(Handle<String> string);

  inline Handle<Name> InternalizeName(Handle<Name> name);

  // String creation functions.  Most of the string creation functions take
  // an AllocationType argument to optionally request that they be
  // allocated in the old generation. Otherwise the default is
  // AllocationType::kYoung.
  //
  // Creates a new String object.  There are two String encodings: one-byte and
  // two-byte.  One should choose between the three string factory functions
  // based on the encoding of the string buffer that the string is
  // initialized from.
  //   - ...FromOneByte initializes the string from a buffer that is Latin1
  //     encoded (it does not check that the buffer is Latin1 encoded) and
  //     the result will be Latin1 encoded.
  //   - ...FromUtf8 initializes the string from a buffer that is UTF-8
  //     encoded.  If the characters are all ASCII characters, the result
  //     will be Latin1 encoded, otherwise it will converted to two-byte.
  //   - ...FromTwoByte initializes the string from a buffer that is two-byte
  //     encoded.  If the characters are all Latin1 characters, the result
  //     will be converted to Latin1, otherwise it will be left as two-byte.
  //
  // One-byte strings are pretenured when used as keys in the SourceCodeCache.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromOneByte(
      const Vector<const uint8_t>& str,
      AllocationType allocation = AllocationType::kYoung);

  template <size_t N>
  inline Handle<String> NewStringFromStaticChars(
      const char (&str)[N],
      AllocationType allocation = AllocationType::kYoung) {
    DCHECK_EQ(N, strlen(str) + 1);
    return NewStringFromOneByte(StaticCharVector(str), allocation)
        .ToHandleChecked();
  }

  inline Handle<String> NewStringFromAsciiChecked(
      const char* str, AllocationType allocation = AllocationType::kYoung) {
    return NewStringFromOneByte(OneByteVector(str), allocation)
        .ToHandleChecked();
  }

  // UTF8 strings are pretenured when used for regexp literal patterns and
  // flags in the parser.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8(
      const Vector<const char>& str,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromUtf8SubString(
      Handle<SeqOneByteString> str, int begin, int end,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromTwoByte(
      const Vector<const uc16>& str,
      AllocationType allocation = AllocationType::kYoung);

  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewStringFromTwoByte(
      const ZoneVector<uc16>* str,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSStringIterator> NewJSStringIterator(Handle<String> string);

  Handle<String> NewOneByteInternalizedString(const Vector<const uint8_t>& str,
                                              uint32_t hash_field);

  Handle<SeqOneByteString> AllocateRawOneByteInternalizedString(
      int length, uint32_t hash_field);

  Handle<String> NewTwoByteInternalizedString(const Vector<const uc16>& str,
                                              uint32_t hash_field);

  Handle<SeqTwoByteString> AllocateRawTwoByteInternalizedString(
      int length, uint32_t hash_field);

  Handle<String> NewInternalizedStringImpl(Handle<String> string, int chars,
                                           uint32_t hash_field);

  // Compute the matching internalized string map for a string if possible.
  // Empty handle is returned if string is in new space or not flattened.
  V8_WARN_UNUSED_RESULT MaybeHandle<Map> InternalizedStringMapForString(
      Handle<String> string);

  // Creates an internalized copy of an external string. |string| must be
  // of type StringClass.
  template <class StringClass>
  Handle<StringClass> InternalizeExternalString(Handle<String> string);

  // Allocates and partially initializes an one-byte or two-byte String. The
  // characters of the string are uninitialized. Currently used in regexp code
  // only, where they are pretenured.
  V8_WARN_UNUSED_RESULT MaybeHandle<SeqOneByteString> NewRawOneByteString(
      int length, AllocationType allocation = AllocationType::kYoung);
  V8_WARN_UNUSED_RESULT MaybeHandle<SeqTwoByteString> NewRawTwoByteString(
      int length, AllocationType allocation = AllocationType::kYoung);

  // Creates a single character string where the character has given code.
  // A cache is used for Latin1 codes.
  Handle<String> LookupSingleCharacterStringFromCode(uint16_t code);

  // Create a new cons string object which consists of a pair of strings.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewConsString(Handle<String> left,
                                                          Handle<String> right);

  V8_WARN_UNUSED_RESULT Handle<String> NewConsString(Handle<String> left,
                                                     Handle<String> right,
                                                     int length, bool one_byte);

  // Create or lookup a single characters tring made up of a utf16 surrogate
  // pair.
  Handle<String> NewSurrogatePairString(uint16_t lead, uint16_t trail);

  // Create a new string object which holds a proper substring of a string.
  Handle<String> NewProperSubString(Handle<String> str, int begin, int end);

  // Create a new string object which holds a substring of a string.
  inline Handle<String> NewSubString(Handle<String> str, int begin, int end);

  // Creates a new external String object.  There are two String encodings
  // in the system: one-byte and two-byte.  Unlike other String types, it does
  // not make sense to have a UTF-8 factory function for external strings,
  // because we cannot change the underlying buffer.  Note that these strings
  // are backed by a string resource that resides outside the V8 heap.
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewExternalStringFromOneByte(
      const ExternalOneByteString::Resource* resource);
  V8_WARN_UNUSED_RESULT MaybeHandle<String> NewExternalStringFromTwoByte(
      const ExternalTwoByteString::Resource* resource);
  // Create a new external string object for one-byte encoded native script.
  // It does not cache the resource data pointer.
  Handle<ExternalOneByteString> NewNativeSourceString(
      const ExternalOneByteString::Resource* resource);

  // Create a symbol in old or read-only space.
  Handle<Symbol> NewSymbol(AllocationType allocation = AllocationType::kOld);
  Handle<Symbol> NewPrivateSymbol(
      AllocationType allocation = AllocationType::kOld);
  Handle<Symbol> NewPrivateNameSymbol(Handle<String> name);

  // Create a global (but otherwise uninitialized) context.
  Handle<NativeContext> NewNativeContext();

  // Create a script context.
  Handle<Context> NewScriptContext(Handle<NativeContext> outer,
                                   Handle<ScopeInfo> scope_info);

  // Create an empty script context table.
  Handle<ScriptContextTable> NewScriptContextTable();

  // Create a module context.
  Handle<Context> NewModuleContext(Handle<SourceTextModule> module,
                                   Handle<NativeContext> outer,
                                   Handle<ScopeInfo> scope_info);

  // Create a function or eval context.
  Handle<Context> NewFunctionContext(Handle<Context> outer,
                                     Handle<ScopeInfo> scope_info);

  // Create a catch context.
  Handle<Context> NewCatchContext(Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info,
                                  Handle<Object> thrown_object);

  // Create a 'with' context.
  Handle<Context> NewWithContext(Handle<Context> previous,
                                 Handle<ScopeInfo> scope_info,
                                 Handle<JSReceiver> extension);

  Handle<Context> NewDebugEvaluateContext(Handle<Context> previous,
                                          Handle<ScopeInfo> scope_info,
                                          Handle<JSReceiver> extension,
                                          Handle<Context> wrapped,
                                          Handle<StringSet> whitelist);

  // Create a block context.
  Handle<Context> NewBlockContext(Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info);

  // Create a context that's used by builtin functions.
  //
  // These are similar to function context but don't have a previous
  // context or any scope info. These are used to store spec defined
  // context values.
  Handle<Context> NewBuiltinContext(Handle<NativeContext> native_context,
                                    int length);

  Handle<Struct> NewStruct(InstanceType type,
                           AllocationType allocation = AllocationType::kYoung);

  Handle<AliasedArgumentsEntry> NewAliasedArgumentsEntry(
      int aliased_context_slot);

  Handle<AccessorInfo> NewAccessorInfo();

  Handle<Script> NewScript(Handle<String> source);
  Handle<Script> NewScriptWithId(Handle<String> source, int script_id);
  Handle<Script> CloneScript(Handle<Script> script);

  Handle<BreakPointInfo> NewBreakPointInfo(int source_position);
  Handle<BreakPoint> NewBreakPoint(int id, Handle<String> condition);
  Handle<StackTraceFrame> NewStackTraceFrame(Handle<FrameArray> frame_array,
                                             int index);
  Handle<StackFrameInfo> NewStackFrameInfo(Handle<FrameArray> frame_array,
                                           int index);
  Handle<SourcePositionTableWithFrameCache>
  NewSourcePositionTableWithFrameCache(
      Handle<ByteArray> source_position_table,
      Handle<SimpleNumberDictionary> stack_frame_cache);

  // Allocate various microtasks.
  Handle<CallableTask> NewCallableTask(Handle<JSReceiver> callable,
                                       Handle<Context> context);
  Handle<CallbackTask> NewCallbackTask(Handle<Foreign> callback,
                                       Handle<Foreign> data);
  Handle<PromiseResolveThenableJobTask> NewPromiseResolveThenableJobTask(
      Handle<JSPromise> promise_to_resolve, Handle<JSReceiver> then,
      Handle<JSReceiver> thenable, Handle<Context> context);

  // Foreign objects are pretenured when allocated by the bootstrapper.
  Handle<Foreign> NewForeign(Address addr);

  Handle<ByteArray> NewByteArray(
      int length, AllocationType allocation = AllocationType::kYoung);

  Handle<BytecodeArray> NewBytecodeArray(int length, const byte* raw_bytecodes,
                                         int frame_size, int parameter_count,
                                         Handle<FixedArray> constant_pool);

  Handle<Cell> NewCell(Handle<Object> value);

  Handle<PropertyCell> NewPropertyCell(
      Handle<Name> name, AllocationType allocation = AllocationType::kOld);

  Handle<FeedbackCell> NewNoClosuresCell(Handle<HeapObject> value);
  Handle<FeedbackCell> NewOneClosureCell(Handle<HeapObject> value);
  Handle<FeedbackCell> NewManyClosuresCell(Handle<HeapObject> value);

  Handle<DescriptorArray> NewDescriptorArray(int number_of_entries,
                                             int slack = 0);
  Handle<TransitionArray> NewTransitionArray(int number_of_transitions,
                                             int slack = 0);

  // Allocate a tenured AllocationSite. Its payload is null.
  Handle<AllocationSite> NewAllocationSite(bool with_weak_next);

  // Allocates and initializes a new Map.
  Handle<Map> NewMap(InstanceType type, int instance_size,
                     ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
                     int inobject_properties = 0);
  // Initializes the fields of a newly created Map. Exposed for tests and
  // heap setup; other code should just call NewMap which takes care of it.
  Map InitializeMap(Map map, InstanceType type, int instance_size,
                    ElementsKind elements_kind, int inobject_properties);

  // Allocate a block of memory of the given AllocationType (filled with a
  // filler). Used as a fall-back for generated code when the space is full.
  Handle<HeapObject> NewFillerObject(
      int size, bool double_align, AllocationType allocation,
      AllocationOrigin origin = AllocationOrigin::kRuntime);

  Handle<JSObject> NewFunctionPrototype(Handle<JSFunction> function);

  Handle<WeakCell> NewWeakCell();

  // Returns a deep copy of the JavaScript object.
  // Properties and elements are copied too.
  Handle<JSObject> CopyJSObject(Handle<JSObject> object);
  // Same as above, but also takes an AllocationSite to be appended in an
  // AllocationMemento.
  Handle<JSObject> CopyJSObjectWithAllocationSite(Handle<JSObject> object,
                                                  Handle<AllocationSite> site);

  Handle<FixedArray> CopyFixedArrayWithMap(Handle<FixedArray> array,
                                           Handle<Map> map);

  Handle<FixedArray> CopyFixedArrayAndGrow(Handle<FixedArray> array,
                                           int grow_by);

  Handle<WeakFixedArray> CopyWeakFixedArrayAndGrow(Handle<WeakFixedArray> array,
                                                   int grow_by);

  Handle<WeakArrayList> CopyWeakArrayListAndGrow(
      Handle<WeakArrayList> array, int grow_by,
      AllocationType allocation = AllocationType::kYoung);

  Handle<PropertyArray> CopyPropertyArrayAndGrow(Handle<PropertyArray> array,
                                                 int grow_by);

  Handle<FixedArray> CopyFixedArrayUpTo(
      Handle<FixedArray> array, int new_len,
      AllocationType allocation = AllocationType::kYoung);

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  // This method expects a COW array in new space, and creates a copy
  // of it in old space.
  Handle<FixedArray> CopyAndTenureFixedCOWArray(Handle<FixedArray> array);

  Handle<FixedDoubleArray> CopyFixedDoubleArray(Handle<FixedDoubleArray> array);

  // Numbers (e.g. literals) are pretenured by the parser.
  // The return value may be a smi or a heap number.
  template <AllocationType allocation = AllocationType::kYoung>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Handle<Object> NewNumber(double value);
  Handle<Object> NewNumberFromInt(int32_t value);
  Handle<Object> NewNumberFromUint(uint32_t value);
  inline Handle<Object> NewNumberFromSize(size_t value);
  inline Handle<Object> NewNumberFromInt64(int64_t value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<HeapNumber> NewHeapNumber(double value);
  template <AllocationType allocation = AllocationType::kYoung>
  inline Handle<HeapNumber> NewHeapNumberFromBits(uint64_t bits);

  // Creates heap number object with not yet set value field.
  template <AllocationType allocation = AllocationType::kYoung>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  Handle<HeapNumber> NewHeapNumber();

  // Creates a new HeapNumber in read-only space if possible otherwise old
  // space.
  Handle<HeapNumber> NewHeapNumberForCodeAssembler(double value);

  inline Handle<HeapNumber> NewHeapNumberWithHoleNaN();

  // Allocates a new BigInt with {length} digits. Only to be used by
  // MutableBigInt::New*.
  Handle<FreshlyAllocatedBigInt> NewBigInt(
      int length, AllocationType allocation = AllocationType::kYoung);

  Handle<JSObject> NewArgumentsObject(Handle<JSFunction> callee, int length);

  // Allocates and initializes a new JavaScript object based on a
  // constructor.
  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(
      Handle<JSFunction> constructor,
      AllocationType allocation = AllocationType::kYoung);
  // JSObject without a prototype.
  Handle<JSObject> NewJSObjectWithNullProto();

  // Global objects are pretenured and initialized based on a constructor.
  Handle<JSGlobalObject> NewJSGlobalObject(Handle<JSFunction> constructor);

  // Allocates and initializes a new JavaScript object based on a map.
  // Passing an allocation site means that a memento will be created that
  // points to the site.
  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(
      Handle<Map> map, AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Like NewJSObjectFromMap, but includes allocating a properties dictionary.
  Handle<JSObject> NewSlowJSObjectFromMap(
      Handle<Map> map,
      int number_of_slow_properties = NameDictionary::kInitialCapacity,
      AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Calls NewJSObjectFromMap or NewSlowJSObjectFromMap depending on whether the
  // map is a dictionary map.
  inline Handle<JSObject> NewFastOrSlowJSObjectFromMap(
      Handle<Map> map,
      int number_of_slow_properties = NameDictionary::kInitialCapacity,
      AllocationType allocation = AllocationType::kYoung,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  // Allocates and initializes a new JavaScript object with the given
  // {prototype} and {properties}. The newly created object will be
  // in dictionary properties mode. The {elements} can either be the
  // empty fixed array, in which case the resulting object will have
  // fast elements, or a NumberDictionary, in which case the resulting
  // object will have dictionary elements.
  Handle<JSObject> NewSlowJSObjectWithPropertiesAndElements(
      Handle<HeapObject> prototype, Handle<NameDictionary> properties,
      Handle<FixedArrayBase> elements);

  // JS arrays are pretenured when allocated by the parser.

  // Create a JSArray with a specified length and elements initialized
  // according to the specified mode.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSArray> NewJSArray(
      int capacity, ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      AllocationType allocation = AllocationType::kYoung) {
    if (capacity != 0) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
    return NewJSArray(elements_kind, 0, capacity,
                      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, allocation);
  }

  // Create a JSArray with the given elements.
  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements, ElementsKind elements_kind, int length,
      AllocationType allocation = AllocationType::kYoung);

  inline Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      AllocationType allocation = AllocationType::kYoung);

  void NewJSArrayStorage(
      Handle<JSArray> array, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<JSWeakMap> NewJSWeakMap();

  Handle<JSGeneratorObject> NewJSGeneratorObject(Handle<JSFunction> function);

  Handle<JSModuleNamespace> NewJSModuleNamespace();

  Handle<SourceTextModule> NewSourceTextModule(Handle<SharedFunctionInfo> code);
  Handle<SyntheticModule> NewSyntheticModule(
      Handle<String> module_name, Handle<FixedArray> export_names,
      v8::Module::SyntheticModuleEvaluationSteps evaluation_steps);

  Handle<JSArrayBuffer> NewJSArrayBuffer(
      std::shared_ptr<BackingStore> backing_store,
      AllocationType allocation = AllocationType::kYoung);

  MaybeHandle<JSArrayBuffer> NewJSArrayBufferAndBackingStore(
      size_t byte_length, InitializedFlag initialized,
      AllocationType allocation = AllocationType::kYoung);

  Handle<JSArrayBuffer> NewJSSharedArrayBuffer(
      std::shared_ptr<BackingStore> backing_store);

  static void TypeAndSizeForElementsKind(ElementsKind kind,
                                         ExternalArrayType* array_type,
                                         size_t* element_size);

  // Creates a new JSTypedArray with the specified buffer.
  Handle<JSTypedArray> NewJSTypedArray(ExternalArrayType type,
                                       Handle<JSArrayBuffer> buffer,
                                       size_t byte_offset, size_t length);

  Handle<JSDataView> NewJSDataView(Handle<JSArrayBuffer> buffer,
                                   size_t byte_offset, size_t byte_length);

  Handle<JSIteratorResult> NewJSIteratorResult(Handle<Object> value, bool done);
  Handle<JSAsyncFromSyncIterator> NewJSAsyncFromSyncIterator(
      Handle<JSReceiver> sync_iterator, Handle<Object> next);

  Handle<JSMap> NewJSMap();
  Handle<JSSet> NewJSSet();

  // Allocates a bound function.
  MaybeHandle<JSBoundFunction> NewJSBoundFunction(
      Handle<JSReceiver> target_function, Handle<Object> bound_this,
      Vector<Handle<Object>> bound_args);

  // Allocates a Harmony proxy.
  Handle<JSProxy> NewJSProxy(Handle<JSReceiver> target,
                             Handle<JSReceiver> handler);

  // Reinitialize an JSGlobalProxy based on a constructor.  The object
  // must have the same size as objects allocated using the
  // constructor.  The object is reinitialized and behaves as an
  // object that has been freshly allocated using the constructor.
  void ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> global,
                                 Handle<JSFunction> constructor);

  Handle<JSGlobalProxy> NewUninitializedJSGlobalProxy(int size);

  // Creates a new JSFunction according to the given args. This is the function
  // you'll probably want to use when creating a JSFunction from the runtime.
  Handle<JSFunction> NewFunction(const NewFunctionArgs& args);

  // For testing only. Creates a sloppy function without code.
  Handle<JSFunction> NewFunctionForTest(Handle<String> name);

  // Function creation from SharedFunctionInfo.

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<Map> initial_map, Handle<SharedFunctionInfo> function_info,
      Handle<Context> context, Handle<FeedbackCell> feedback_cell,
      AllocationType allocation = AllocationType::kOld);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info, Handle<Context> context,
      Handle<FeedbackCell> feedback_cell,
      AllocationType allocation = AllocationType::kOld);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<Map> initial_map, Handle<SharedFunctionInfo> function_info,
      Handle<Context> context,
      AllocationType allocation = AllocationType::kOld);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info, Handle<Context> context,
      AllocationType allocation = AllocationType::kOld);

  // The choke-point for JSFunction creation. Handles allocation and
  // initialization. All other utility methods call into this.
  Handle<JSFunction> NewFunction(
      Handle<Map> map, Handle<SharedFunctionInfo> info, Handle<Context> context,
      AllocationType allocation = AllocationType::kOld);

  // Create a serialized scope info.
  Handle<ScopeInfo> NewScopeInfo(int length,
                                 AllocationType type = AllocationType::kOld);

  Handle<SourceTextModuleInfo> NewSourceTextModuleInfo();

  Handle<PreparseData> NewPreparseData(int data_length, int children_length);

  Handle<UncompiledDataWithoutPreparseData>
  NewUncompiledDataWithoutPreparseData(Handle<String> inferred_name,
                                       int32_t start_position,
                                       int32_t end_position);

  Handle<UncompiledDataWithPreparseData> NewUncompiledDataWithPreparseData(
      Handle<String> inferred_name, int32_t start_position,
      int32_t end_position, Handle<PreparseData>);

  // Create an External object for V8's external API.
  Handle<JSObject> NewExternal(void* value);

  // Creates a new CodeDataContainer for a Code object.
  Handle<CodeDataContainer> NewCodeDataContainer(int flags,
                                                 AllocationType allocation);

  // Allocates a new code object and initializes it as the trampoline to the
  // given off-heap entry point.
  Handle<Code> NewOffHeapTrampolineFor(Handle<Code> code,
                                       Address off_heap_entry);

  Handle<Code> CopyCode(Handle<Code> code);

  Handle<BytecodeArray> CopyBytecodeArray(Handle<BytecodeArray>);

  // Interface for creating error objects.
  Handle<Object> NewError(Handle<JSFunction> constructor,
                          Handle<String> message);

  Handle<Object> NewInvalidStringLengthError();

  inline Handle<Object> NewURIError();

  Handle<Object> NewError(Handle<JSFunction> constructor,
                          MessageTemplate template_index,
                          Handle<Object> arg0 = Handle<Object>(),
                          Handle<Object> arg1 = Handle<Object>(),
                          Handle<Object> arg2 = Handle<Object>());

#define DECLARE_ERROR(NAME)                                        \
  Handle<Object> New##NAME(MessageTemplate template_index,         \
                           Handle<Object> arg0 = Handle<Object>(), \
                           Handle<Object> arg1 = Handle<Object>(), \
                           Handle<Object> arg2 = Handle<Object>());
  DECLARE_ERROR(Error)
  DECLARE_ERROR(EvalError)
  DECLARE_ERROR(RangeError)
  DECLARE_ERROR(ReferenceError)
  DECLARE_ERROR(SyntaxError)
  DECLARE_ERROR(TypeError)
  DECLARE_ERROR(WasmCompileError)
  DECLARE_ERROR(WasmLinkError)
  DECLARE_ERROR(WasmRuntimeError)
#undef DECLARE_ERROR

  Handle<String> NumberToString(Handle<Object> number, bool check_cache = true);
  Handle<String> NumberToString(Smi number, bool check_cache = true);

  inline Handle<String> Uint32ToString(uint32_t value, bool check_cache = true);

#define ROOT_ACCESSOR(Type, name, CamelName) inline Handle<Type> name();
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // Allocates a new SharedFunctionInfo object.
  Handle<SharedFunctionInfo> NewSharedFunctionInfoForApiFunction(
      MaybeHandle<String> maybe_name,
      Handle<FunctionTemplateInfo> function_template_info, FunctionKind kind);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForWasmCapiFunction(
      Handle<WasmCapiFunctionData> data);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForBuiltin(
      MaybeHandle<String> name, int builtin_index,
      FunctionKind kind = kNormalFunction);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForLiteral(
      FunctionLiteral* literal, Handle<Script> script, bool is_toplevel);

  static bool IsFunctionModeWithPrototype(FunctionMode function_mode) {
    return (function_mode & kWithPrototypeBits) != 0;
  }

  static bool IsFunctionModeWithWritablePrototype(FunctionMode function_mode) {
    return (function_mode & kWithWritablePrototypeBit) != 0;
  }

  static bool IsFunctionModeWithName(FunctionMode function_mode) {
    return (function_mode & kWithNameBit) != 0;
  }

  static bool IsFunctionModeWithHomeObject(FunctionMode function_mode) {
    return (function_mode & kWithHomeObjectBit) != 0;
  }

  Handle<Map> CreateSloppyFunctionMap(
      FunctionMode function_mode, MaybeHandle<JSFunction> maybe_empty_function);

  Handle<Map> CreateStrictFunctionMap(FunctionMode function_mode,
                                      Handle<JSFunction> empty_function);

  Handle<Map> CreateClassFunctionMap(Handle<JSFunction> empty_function);

  // Allocates a new JSMessageObject object.
  Handle<JSMessageObject> NewJSMessageObject(
      MessageTemplate message, Handle<Object> argument, int start_position,
      int end_position, Handle<SharedFunctionInfo> shared_info,
      int bytecode_offset, Handle<Script> script, Handle<Object> stack_frames);

  Handle<ClassPositions> NewClassPositions(int start, int end);
  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);

  Handle<CoverageInfo> NewCoverageInfo(const ZoneVector<SourceRange>& slots);

  // Return a map for given number of properties using the map cache in the
  // native context.
  Handle<Map> ObjectLiteralMapFromCache(Handle<NativeContext> native_context,
                                        int number_of_properties);

  Handle<LoadHandler> NewLoadHandler(
      int data_count, AllocationType allocation = AllocationType::kOld);
  Handle<StoreHandler> NewStoreHandler(int data_count);

  Handle<RegExpMatchInfo> NewRegExpMatchInfo();

  // Creates a new FixedArray that holds the data associated with the
  // atom regexp and stores it in the regexp.
  void SetRegExpAtomData(Handle<JSRegExp> regexp, JSRegExp::Type type,
                         Handle<String> source, JSRegExp::Flags flags,
                         Handle<Object> match_pattern);

  // Creates a new FixedArray that holds the data associated with the
  // irregexp regexp and stores it in the regexp.
  void SetRegExpIrregexpData(Handle<JSRegExp> regexp, JSRegExp::Type type,
                             Handle<String> source, JSRegExp::Flags flags,
                             int capture_count);

  // Returns the value for a known global constant (a property of the global
  // object which is neither configurable nor writable) like 'undefined'.
  // Returns a null handle when the given name is unknown.
  Handle<Object> GlobalConstantFor(Handle<Name> name);

  // Converts the given boolean condition to JavaScript boolean value.
  Handle<Object> ToBoolean(bool value);

  // Converts the given ToPrimitive hint to it's string representation.
  Handle<String> ToPrimitiveHintString(ToPrimitiveHint hint);

  Handle<JSPromise> NewJSPromiseWithoutHook();
  Handle<JSPromise> NewJSPromise();

  Handle<CallHandlerInfo> NewCallHandlerInfo(bool has_no_side_effect = false);

  HeapObject NewForTest(Handle<Map> map, AllocationType allocation) {
    return New(map, allocation);
  }

  // Allows creation of Code objects. It provides two build methods, one of
  // which tries to gracefully handle allocation failure.
  class V8_EXPORT_PRIVATE CodeBuilder final {
   public:
    CodeBuilder(Isolate* isolate, const CodeDesc& desc, Code::Kind kind);

    // Builds a new code object (fully initialized). All header fields of the
    // returned object are immutable and the code object is write protected.
    V8_WARN_UNUSED_RESULT Handle<Code> Build();
    // Like Build, builds a new code object. May return an empty handle if the
    // allocation fails.
    V8_WARN_UNUSED_RESULT MaybeHandle<Code> TryBuild();

    // Sets the self-reference object in which a reference to the code object is
    // stored. This allows generated code to reference its own Code object by
    // using this handle.
    CodeBuilder& set_self_reference(Handle<Object> self_reference) {
      DCHECK(!self_reference.is_null());
      self_reference_ = self_reference;
      return *this;
    }

    CodeBuilder& set_builtin_index(int32_t builtin_index) {
      builtin_index_ = builtin_index;
      return *this;
    }

    CodeBuilder& set_source_position_table(Handle<ByteArray> table) {
      DCHECK(!table.is_null());
      source_position_table_ = table;
      return *this;
    }

    CodeBuilder& set_deoptimization_data(
        Handle<DeoptimizationData> deopt_data) {
      DCHECK(!deopt_data.is_null());
      deoptimization_data_ = deopt_data;
      return *this;
    }

    CodeBuilder& set_immovable() {
      is_movable_ = false;
      return *this;
    }

    CodeBuilder& set_is_turbofanned() {
      is_turbofanned_ = true;
      return *this;
    }

    // Indicates the CodeDataContainer should be allocated in read-only space.
    // As an optimization, if the kind-specific flags match that of a canonical
    // container, it will be used instead.
    CodeBuilder& set_read_only_data_container(int32_t flags) {
      read_only_data_container_ = true;
      kind_specific_flags_ = flags;
      return *this;
    }

    CodeBuilder& set_stack_slots(int stack_slots) {
      stack_slots_ = stack_slots;
      return *this;
    }

   private:
    MaybeHandle<Code> BuildInternal(bool retry_allocation_or_fail);

    Isolate* const isolate_;
    const CodeDesc& code_desc_;
    const Code::Kind kind_;

    MaybeHandle<Object> self_reference_;
    int32_t builtin_index_ = Builtins::kNoBuiltinId;
    int32_t kind_specific_flags_ = 0;
    Handle<ByteArray> source_position_table_;
    Handle<DeoptimizationData> deoptimization_data_ =
        DeoptimizationData::Empty(isolate_);
    bool read_only_data_container_ = false;
    bool is_movable_ = true;
    bool is_turbofanned_ = false;
    int stack_slots_ = 0;
  };

 private:
  Isolate* isolate() {
    // Downcast to the privately inherited sub-class using c-style casts to
    // avoid undefined behavior (as static_cast cannot cast across private
    // bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (Isolate*)this;  // NOLINT(readability/casting)
  }

  HeapObject AllocateRawWithImmortalMap(
      int size, AllocationType allocation, Map map,
      AllocationAlignment alignment = kWordAligned);
  HeapObject AllocateRawWithAllocationSite(
      Handle<Map> map, AllocationType allocation,
      Handle<AllocationSite> allocation_site);

  Handle<JSArrayBufferView> NewJSArrayBufferView(
      Handle<Map> map, Handle<FixedArrayBase> elements,
      Handle<JSArrayBuffer> buffer, size_t byte_offset, size_t byte_length);

  // Allocate memory for an uninitialized array (e.g., a FixedArray or similar).
  HeapObject AllocateRawArray(int size, AllocationType allocation);
  HeapObject AllocateRawFixedArray(int length, AllocationType allocation);
  HeapObject AllocateRawWeakArrayList(int length, AllocationType allocation);
  Handle<FixedArray> NewFixedArrayWithFiller(RootIndex map_root_index,
                                             int length, Object filler,
                                             AllocationType allocation);

  // Allocates new context with given map, sets length and initializes the
  // after-header part with uninitialized values and leaves the context header
  // uninitialized.
  Handle<Context> NewContext(RootIndex map_root_index, int size,
                             int variadic_part_length,
                             AllocationType allocation);

  template <typename T>
  Handle<T> AllocateSmallOrderedHashTable(Handle<Map> map, int capacity,
                                          AllocationType allocation);

  // Creates a heap object based on the map. The fields of the heap object are
  // not initialized, it's the responsibility of the caller to do that.
  HeapObject New(Handle<Map> map, AllocationType allocation);

  template <typename T>
  Handle<T> CopyArrayWithMap(Handle<T> src, Handle<Map> map);
  template <typename T>
  Handle<T> CopyArrayAndGrow(Handle<T> src, int grow_by,
                             AllocationType allocation);

  template <bool is_one_byte, typename T>
  Handle<String> AllocateInternalizedStringImpl(T t, int chars,
                                                uint32_t hash_field);

  Handle<String> AllocateTwoByteInternalizedString(
      const Vector<const uc16>& str, uint32_t hash_field);

  MaybeHandle<String> NewStringFromTwoByte(const uc16* string, int length,
                                           AllocationType allocation);

  // Attempt to find the number in a small cache.  If we finds it, return
  // the string representation of the number.  Otherwise return undefined.
  Handle<Object> NumberToStringCacheGet(Object number, int hash);

  // Update the cache with a new number-string pair.
  Handle<String> NumberToStringCacheSet(Handle<Object> number, int hash,
                                        const char* string, bool check_cache);

  // Creates a new JSArray with the given backing storage. Performs no
  // verification of the backing storage because it may not yet be filled.
  Handle<JSArray> NewJSArrayWithUnverifiedElements(
      Handle<FixedArrayBase> elements, ElementsKind elements_kind, int length,
      AllocationType allocation = AllocationType::kYoung);

  // Creates the backing storage for a JSArray. This handle must be discarded
  // before returning the JSArray reference to code outside Factory, which might
  // decide to left-trim the backing store. To avoid unnecessary HandleScopes,
  // this method requires capacity greater than zero.
  Handle<FixedArrayBase> NewJSArrayStorage(
      ElementsKind elements_kind, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      MaybeHandle<String> name, MaybeHandle<HeapObject> maybe_function_data,
      int maybe_builtin_index, FunctionKind kind = kNormalFunction);

  void InitializeAllocationMemento(AllocationMemento memento,
                                   AllocationSite allocation_site);

  // Initializes a JSObject based on its map.
  void InitializeJSObjectFromMap(Handle<JSObject> obj,
                                 Handle<Object> properties, Handle<Map> map);
  // Initializes JSObject body starting at given offset.
  void InitializeJSObjectBody(Handle<JSObject> obj, Handle<Map> map,
                              int start_offset);
};

// Utility class to simplify argument handling around JSFunction creation.
class NewFunctionArgs final {
 public:
  static NewFunctionArgs ForWasm(
      Handle<String> name,
      Handle<WasmExportedFunctionData> exported_function_data, Handle<Map> map);
  static NewFunctionArgs ForWasm(Handle<String> name,
                                 Handle<WasmJSFunctionData> js_function_data,
                                 Handle<Map> map);
  V8_EXPORT_PRIVATE static NewFunctionArgs ForBuiltin(Handle<String> name,
                                                      Handle<Map> map,
                                                      int builtin_id);
  static NewFunctionArgs ForFunctionWithoutCode(Handle<String> name,
                                                Handle<Map> map,
                                                LanguageMode language_mode);
  static NewFunctionArgs ForBuiltinWithPrototype(
      Handle<String> name, Handle<HeapObject> prototype, InstanceType type,
      int instance_size, int inobject_properties, int builtin_id,
      MutableMode prototype_mutability);
  static NewFunctionArgs ForBuiltinWithoutPrototype(Handle<String> name,
                                                    int builtin_id,
                                                    LanguageMode language_mode);

  Handle<Map> GetMap(Isolate* isolate) const;

 private:
  NewFunctionArgs() = default;  // Use the static factory constructors.

  void SetShouldCreateAndSetInitialMap();
  void SetShouldSetPrototype();
  void SetShouldSetLanguageMode();

  // Sentinel value.
  static const int kUninitialized = -1;

  Handle<String> name_;
  MaybeHandle<Map> maybe_map_;
  MaybeHandle<Struct> maybe_wasm_function_data_;

  bool should_create_and_set_initial_map_ = false;
  InstanceType type_;
  int instance_size_ = kUninitialized;
  int inobject_properties_ = kUninitialized;

  bool should_set_prototype_ = false;
  MaybeHandle<HeapObject> maybe_prototype_;

  bool should_set_language_mode_ = false;
  LanguageMode language_mode_;

  int maybe_builtin_id_ = kUninitialized;

  MutableMode prototype_mutability_;

  friend class Factory;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_FACTORY_H_
