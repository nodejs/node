// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FACTORY_H_
#define V8_FACTORY_H_

#include "src/feedback-vector.h"
#include "src/globals.h"
#include "src/isolate.h"
#include "src/messages.h"
#include "src/objects/descriptor-array.h"
#include "src/objects/dictionary.h"
#include "src/objects/js-array.h"
#include "src/objects/js-regexp.h"
#include "src/objects/scope-info.h"
#include "src/objects/string.h"
#include "src/string-hasher.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AliasedArgumentsEntry;
class BreakPointInfo;
class BreakPoint;
class BoilerplateDescription;
class ConstantElementsPair;
class CoverageInfo;
class DebugInfo;
class FreshlyAllocatedBigInt;
class JSModuleNamespace;
class NewFunctionArgs;
struct SourceRange;
class PreParsedScopeData;
class TemplateObjectDescription;

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
class V8_EXPORT_PRIVATE Factory final {
 public:
  Handle<Oddball> NewOddball(Handle<Map> map, const char* to_string,
                             Handle<Object> to_number, const char* type_of,
                             byte kind);

  // Allocates a fixed array-like object with given map and initialized with
  // undefined values.
  Handle<FixedArray> NewFixedArrayWithMap(Heap::RootListIndex map_root_index,
                                          int length, PretenureFlag pretenure);

  // Allocates a fixed array initialized with undefined values.
  Handle<FixedArray> NewFixedArray(int length,
                                   PretenureFlag pretenure = NOT_TENURED);
  Handle<PropertyArray> NewPropertyArray(int length,
                                         PretenureFlag pretenure = NOT_TENURED);
  // Tries allocating a fixed array initialized with undefined values.
  // In case of an allocation failure (OOM) an empty handle is returned.
  // The caller has to manually signal an
  // v8::internal::Heap::FatalProcessOutOfMemory typically by calling
  // NewFixedArray as a fallback.
  MUST_USE_RESULT
  MaybeHandle<FixedArray> TryNewFixedArray(
      int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new fixed array with non-existing entries (the hole).
  Handle<FixedArray> NewFixedArrayWithHoles(
      int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocates an uninitialized fixed array. It must be filled by the caller.
  Handle<FixedArray> NewUninitializedFixedArray(int length);

  // Allocates a feedback vector whose slots are initialized with undefined
  // values.
  Handle<FeedbackVector> NewFeedbackVector(
      Handle<SharedFunctionInfo> shared, PretenureFlag pretenure = NOT_TENURED);

  // Allocates a fixed array for name-value pairs of boilerplate properties and
  // calculates the number of properties we need to store in the backing store.
  Handle<BoilerplateDescription> NewBoilerplateDescription(int boilerplate,
                                                           int all_properties,
                                                           int index_keys,
                                                           bool has_seen_proto);

  // Allocate a new uninitialized fixed double array.
  // The function returns a pre-allocated empty fixed array for capacity = 0,
  // so the return type must be the general fixed array class.
  Handle<FixedArrayBase> NewFixedDoubleArray(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new fixed double array with hole values.
  Handle<FixedArrayBase> NewFixedDoubleArrayWithHoles(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<FrameArray> NewFrameArray(int number_of_frames,
                                   PretenureFlag pretenure = NOT_TENURED);

  Handle<OrderedHashSet> NewOrderedHashSet();
  Handle<OrderedHashMap> NewOrderedHashMap();

  Handle<SmallOrderedHashSet> NewSmallOrderedHashSet(
      int size = SmallOrderedHashSet::kMinCapacity,
      PretenureFlag pretenure = NOT_TENURED);
  Handle<SmallOrderedHashMap> NewSmallOrderedHashMap(
      int size = SmallOrderedHashMap::kMinCapacity,
      PretenureFlag pretenure = NOT_TENURED);

  // Create a new PrototypeInfo struct.
  Handle<PrototypeInfo> NewPrototypeInfo();

  // Create a new EnumCache struct.
  Handle<EnumCache> NewEnumCache(Handle<FixedArray> keys,
                                 Handle<FixedArray> indices);

  // Create a new Tuple2 struct.
  Handle<Tuple2> NewTuple2(Handle<Object> value1, Handle<Object> value2,
                           PretenureFlag pretenure);

  // Create a new Tuple3 struct.
  Handle<Tuple3> NewTuple3(Handle<Object> value1, Handle<Object> value2,
                           Handle<Object> value3, PretenureFlag pretenure);

  // Create a new ContextExtension struct.
  Handle<ContextExtension> NewContextExtension(Handle<ScopeInfo> scope_info,
                                               Handle<Object> extension);

  // Create a new ConstantElementsPair struct.
  Handle<ConstantElementsPair> NewConstantElementsPair(
      ElementsKind elements_kind, Handle<FixedArrayBase> constant_values);

  // Create a new TemplateObjectDescription struct.
  Handle<TemplateObjectDescription> NewTemplateObjectDescription(
      int hash, Handle<FixedArray> raw_strings,
      Handle<FixedArray> cooked_strings);

  // Create a pre-tenured empty AccessorPair.
  Handle<AccessorPair> NewAccessorPair();

  // Create an empty TypeFeedbackInfo.
  Handle<TypeFeedbackInfo> NewTypeFeedbackInfo();

  // Finds the internalized copy for string in the string table.
  // If not found, a new string is added to the table and returned.
  Handle<String> InternalizeUtf8String(Vector<const char> str);
  Handle<String> InternalizeUtf8String(const char* str) {
    return InternalizeUtf8String(CStrVector(str));
  }

  Handle<String> InternalizeOneByteString(Vector<const uint8_t> str);
  Handle<String> InternalizeOneByteString(
      Handle<SeqOneByteString>, int from, int length);

  Handle<String> InternalizeTwoByteString(Vector<const uc16> str);

  template<class StringTableKey>
  Handle<String> InternalizeStringWithKey(StringTableKey* key);

  // Internalized strings are created in the old generation (data space).
  inline Handle<String> InternalizeString(Handle<String> string);

  inline Handle<Name> InternalizeName(Handle<Name> name);

  // String creation functions.  Most of the string creation functions take
  // a Heap::PretenureFlag argument to optionally request that they be
  // allocated in the old generation.  The pretenure flag defaults to
  // DONT_TENURE.
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
  MUST_USE_RESULT MaybeHandle<String> NewStringFromOneByte(
      Vector<const uint8_t> str, PretenureFlag pretenure = NOT_TENURED);

  template <size_t N>
  inline Handle<String> NewStringFromStaticChars(
      const char (&str)[N], PretenureFlag pretenure = NOT_TENURED) {
    DCHECK(N == StrLength(str) + 1);
    return NewStringFromOneByte(STATIC_CHAR_VECTOR(str), pretenure)
        .ToHandleChecked();
  }

  inline Handle<String> NewStringFromAsciiChecked(
      const char* str,
      PretenureFlag pretenure = NOT_TENURED) {
    return NewStringFromOneByte(
        OneByteVector(str), pretenure).ToHandleChecked();
  }

  // UTF8 strings are pretenured when used for regexp literal patterns and
  // flags in the parser.
  MUST_USE_RESULT MaybeHandle<String> NewStringFromUtf8(
      Vector<const char> str, PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT MaybeHandle<String> NewStringFromUtf8SubString(
      Handle<SeqOneByteString> str, int begin, int end,
      PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT MaybeHandle<String> NewStringFromTwoByte(
      Vector<const uc16> str, PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT MaybeHandle<String> NewStringFromTwoByte(
      const ZoneVector<uc16>* str, PretenureFlag pretenure = NOT_TENURED);

  Handle<JSStringIterator> NewJSStringIterator(Handle<String> string);

  // Allocates an internalized string in old space based on the character
  // stream.
  Handle<String> NewInternalizedStringFromUtf8(Vector<const char> str,
                                               int chars, uint32_t hash_field);

  Handle<String> NewOneByteInternalizedString(Vector<const uint8_t> str,
                                              uint32_t hash_field);

  Handle<String> NewOneByteInternalizedSubString(
      Handle<SeqOneByteString> string, int offset, int length,
      uint32_t hash_field);

  Handle<String> NewTwoByteInternalizedString(Vector<const uc16> str,
                                              uint32_t hash_field);

  Handle<String> NewInternalizedStringImpl(Handle<String> string, int chars,
                                           uint32_t hash_field);

  // Compute the matching internalized string map for a string if possible.
  // Empty handle is returned if string is in new space or not flattened.
  MUST_USE_RESULT MaybeHandle<Map> InternalizedStringMapForString(
      Handle<String> string);

  // Creates an internalized copy of an external string. |string| must be
  // of type StringClass.
  template <class StringClass>
  Handle<StringClass> InternalizeExternalString(Handle<String> string);

  // Allocates and partially initializes an one-byte or two-byte String. The
  // characters of the string are uninitialized. Currently used in regexp code
  // only, where they are pretenured.
  MUST_USE_RESULT MaybeHandle<SeqOneByteString> NewRawOneByteString(
      int length,
      PretenureFlag pretenure = NOT_TENURED);
  MUST_USE_RESULT MaybeHandle<SeqTwoByteString> NewRawTwoByteString(
      int length,
      PretenureFlag pretenure = NOT_TENURED);

  // Creates a single character string where the character has given code.
  // A cache is used for Latin1 codes.
  Handle<String> LookupSingleCharacterStringFromCode(uint32_t code);

  // Create a new cons string object which consists of a pair of strings.
  MUST_USE_RESULT MaybeHandle<String> NewConsString(Handle<String> left,
                                                    Handle<String> right);

  MUST_USE_RESULT Handle<String> NewConsString(Handle<String> left,
                                               Handle<String> right, int length,
                                               bool one_byte);

  // Create or lookup a single characters tring made up of a utf16 surrogate
  // pair.
  Handle<String> NewSurrogatePairString(uint16_t lead, uint16_t trail);

  // Create a new string object which holds a proper substring of a string.
  Handle<String> NewProperSubString(Handle<String> str,
                                    int begin,
                                    int end);

  // Create a new string object which holds a substring of a string.
  inline Handle<String> NewSubString(Handle<String> str, int begin, int end);

  // Creates a new external String object.  There are two String encodings
  // in the system: one-byte and two-byte.  Unlike other String types, it does
  // not make sense to have a UTF-8 factory function for external strings,
  // because we cannot change the underlying buffer.  Note that these strings
  // are backed by a string resource that resides outside the V8 heap.
  MUST_USE_RESULT MaybeHandle<String> NewExternalStringFromOneByte(
      const ExternalOneByteString::Resource* resource);
  MUST_USE_RESULT MaybeHandle<String> NewExternalStringFromTwoByte(
      const ExternalTwoByteString::Resource* resource);
  // Create a new external string object for one-byte encoded native script.
  // It does not cache the resource data pointer.
  Handle<ExternalOneByteString> NewNativeSourceString(
      const ExternalOneByteString::Resource* resource);

  // Create a symbol.
  Handle<Symbol> NewSymbol();
  Handle<Symbol> NewPrivateSymbol();

  // Create a global (but otherwise uninitialized) context.
  Handle<Context> NewNativeContext();

  // Create a script context.
  Handle<Context> NewScriptContext(Handle<JSFunction> function,
                                   Handle<ScopeInfo> scope_info);

  // Create an empty script context table.
  Handle<ScriptContextTable> NewScriptContextTable();

  // Create a module context.
  Handle<Context> NewModuleContext(Handle<Module> module,
                                   Handle<JSFunction> function,
                                   Handle<ScopeInfo> scope_info);

  // Create a function or eval context.
  Handle<Context> NewFunctionContext(int length, Handle<JSFunction> function,
                                     ScopeType scope_type);

  // Create a catch context.
  Handle<Context> NewCatchContext(Handle<JSFunction> function,
                                  Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info,
                                  Handle<String> name,
                                  Handle<Object> thrown_object);

  // Create a 'with' context.
  Handle<Context> NewWithContext(Handle<JSFunction> function,
                                 Handle<Context> previous,
                                 Handle<ScopeInfo> scope_info,
                                 Handle<JSReceiver> extension);

  Handle<Context> NewDebugEvaluateContext(Handle<Context> previous,
                                          Handle<ScopeInfo> scope_info,
                                          Handle<JSReceiver> extension,
                                          Handle<Context> wrapped,
                                          Handle<StringSet> whitelist);

  // Create a block context.
  Handle<Context> NewBlockContext(Handle<JSFunction> function,
                                  Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info);

  Handle<Struct> NewStruct(InstanceType type,
                           PretenureFlag pretenure = NOT_TENURED);

  Handle<AliasedArgumentsEntry> NewAliasedArgumentsEntry(
      int aliased_context_slot);

  Handle<AccessorInfo> NewAccessorInfo();

  Handle<Script> NewScript(Handle<String> source);

  Handle<BreakPointInfo> NewBreakPointInfo(int source_position);
  Handle<BreakPoint> NewBreakPoint(int id, Handle<String> condition);
  Handle<StackFrameInfo> NewStackFrameInfo();
  Handle<SourcePositionTableWithFrameCache>
  NewSourcePositionTableWithFrameCache(
      Handle<ByteArray> source_position_table,
      Handle<NumberDictionary> stack_frame_cache);

  // Foreign objects are pretenured when allocated by the bootstrapper.
  Handle<Foreign> NewForeign(Address addr,
                             PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new foreign object.  The foreign is pretenured (allocated
  // directly in the old generation).
  Handle<Foreign> NewForeign(const AccessorDescriptor* foreign);

  Handle<ByteArray> NewByteArray(int length,
                                 PretenureFlag pretenure = NOT_TENURED);

  Handle<BytecodeArray> NewBytecodeArray(int length, const byte* raw_bytecodes,
                                         int frame_size, int parameter_count,
                                         Handle<FixedArray> constant_pool);

  Handle<FixedTypedArrayBase> NewFixedTypedArrayWithExternalPointer(
      int length, ExternalArrayType array_type, void* external_pointer,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<FixedTypedArrayBase> NewFixedTypedArray(
      int length, ExternalArrayType array_type, bool initialize,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<Cell> NewCell(Handle<Object> value);

  Handle<PropertyCell> NewPropertyCell(Handle<Name> name);

  Handle<WeakCell> NewWeakCell(Handle<HeapObject> value);

  Handle<Cell> NewNoClosuresCell(Handle<Object> value);
  Handle<Cell> NewOneClosureCell(Handle<Object> value);
  Handle<Cell> NewManyClosuresCell(Handle<Object> value);

  Handle<TransitionArray> NewTransitionArray(int capacity);

  // Allocate a tenured AllocationSite. It's payload is null.
  Handle<AllocationSite> NewAllocationSite();

  Handle<Map> NewMap(InstanceType type, int instance_size,
                     ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
                     int inobject_properties = 0);

  Handle<HeapObject> NewFillerObject(int size,
                                     bool double_align,
                                     AllocationSpace space);

  Handle<JSObject> NewFunctionPrototype(Handle<JSFunction> function);

  Handle<JSObject> CopyJSObject(Handle<JSObject> object);

  Handle<JSObject> CopyJSObjectWithAllocationSite(Handle<JSObject> object,
                                                  Handle<AllocationSite> site);

  Handle<FixedArray> CopyFixedArrayWithMap(Handle<FixedArray> array,
                                           Handle<Map> map);

  Handle<FixedArray> CopyFixedArrayAndGrow(
      Handle<FixedArray> array, int grow_by,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<PropertyArray> CopyPropertyArrayAndGrow(
      Handle<PropertyArray> array, int grow_by,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<FixedArray> CopyFixedArrayUpTo(Handle<FixedArray> array, int new_len,
                                        PretenureFlag pretenure = NOT_TENURED);

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  // This method expects a COW array in new space, and creates a copy
  // of it in old space.
  Handle<FixedArray> CopyAndTenureFixedCOWArray(Handle<FixedArray> array);

  Handle<FixedDoubleArray> CopyFixedDoubleArray(
      Handle<FixedDoubleArray> array);

  Handle<FeedbackVector> CopyFeedbackVector(Handle<FeedbackVector> array);

  // Numbers (e.g. literals) are pretenured by the parser.
  // The return value may be a smi or a heap number.
  Handle<Object> NewNumber(double value,
                           PretenureFlag pretenure = NOT_TENURED);

  Handle<Object> NewNumberFromInt(int32_t value,
                                  PretenureFlag pretenure = NOT_TENURED);
  Handle<Object> NewNumberFromUint(uint32_t value,
                                  PretenureFlag pretenure = NOT_TENURED);
  inline Handle<Object> NewNumberFromSize(
      size_t value, PretenureFlag pretenure = NOT_TENURED);
  inline Handle<Object> NewNumberFromInt64(
      int64_t value, PretenureFlag pretenure = NOT_TENURED);
  inline Handle<HeapNumber> NewHeapNumber(
      double value, MutableMode mode = IMMUTABLE,
      PretenureFlag pretenure = NOT_TENURED);
  inline Handle<HeapNumber> NewHeapNumberFromBits(
      uint64_t bits, MutableMode mode = IMMUTABLE,
      PretenureFlag pretenure = NOT_TENURED);
  // Creates mutable heap number object with value field set to hole NaN.
  inline Handle<HeapNumber> NewMutableHeapNumber(
      PretenureFlag pretenure = NOT_TENURED);

  // Creates heap number object with not yet set value field.
  Handle<HeapNumber> NewHeapNumber(MutableMode mode,
                                   PretenureFlag pretenure = NOT_TENURED);

  // Allocates a new BigInt with {length} digits. Only to be used by
  // MutableBigInt::New*.
  Handle<FreshlyAllocatedBigInt> NewBigInt(int length);

  Handle<JSObject> NewArgumentsObject(Handle<JSFunction> callee, int length);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(Handle<JSFunction> constructor,
                               PretenureFlag pretenure = NOT_TENURED);
  // JSObject without a prototype.
  Handle<JSObject> NewJSObjectWithNullProto(
      PretenureFlag pretenure = NOT_TENURED);

  // Global objects are pretenured and initialized based on a constructor.
  Handle<JSGlobalObject> NewJSGlobalObject(Handle<JSFunction> constructor);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(
      Handle<Map> map,
      PretenureFlag pretenure = NOT_TENURED,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());
  Handle<JSObject> NewSlowJSObjectFromMap(
      Handle<Map> map,
      int number_of_slow_properties = NameDictionary::kInitialCapacity,
      PretenureFlag pretenure = NOT_TENURED);

  // JS arrays are pretenured when allocated by the parser.

  // Create a JSArray with a specified length and elements initialized
  // according to the specified mode.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArray(
      int capacity, ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      PretenureFlag pretenure = NOT_TENURED) {
    if (capacity != 0) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
    return NewJSArray(elements_kind, 0, capacity,
                      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, pretenure);
  }

  // Create a JSArray with the given elements.
  Handle<JSArray> NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                         ElementsKind elements_kind, int length,
                                         PretenureFlag pretenure = NOT_TENURED);

  inline Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      PretenureFlag pretenure = NOT_TENURED);

  void NewJSArrayStorage(
      Handle<JSArray> array,
      int length,
      int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<JSGeneratorObject> NewJSGeneratorObject(Handle<JSFunction> function);

  Handle<JSModuleNamespace> NewJSModuleNamespace();

  Handle<Module> NewModule(Handle<SharedFunctionInfo> code);

  Handle<JSArrayBuffer> NewJSArrayBuffer(
      SharedFlag shared = SharedFlag::kNotShared,
      PretenureFlag pretenure = NOT_TENURED);

  ExternalArrayType GetArrayTypeFromElementsKind(ElementsKind kind);
  size_t GetExternalArrayElementSize(ExternalArrayType type);

  Handle<JSTypedArray> NewJSTypedArray(ExternalArrayType type,
                                       PretenureFlag pretenure = NOT_TENURED);

  Handle<JSTypedArray> NewJSTypedArray(ElementsKind elements_kind,
                                       PretenureFlag pretenure = NOT_TENURED);

  // Creates a new JSTypedArray with the specified buffer.
  Handle<JSTypedArray> NewJSTypedArray(ExternalArrayType type,
                                       Handle<JSArrayBuffer> buffer,
                                       size_t byte_offset, size_t length,
                                       PretenureFlag pretenure = NOT_TENURED);

  // Creates a new on-heap JSTypedArray.
  Handle<JSTypedArray> NewJSTypedArray(ElementsKind elements_kind,
                                       size_t number_of_elements,
                                       PretenureFlag pretenure = NOT_TENURED);

  Handle<JSDataView> NewJSDataView();
  Handle<JSDataView> NewJSDataView(Handle<JSArrayBuffer> buffer,
                                   size_t byte_offset, size_t byte_length);

  Handle<JSIteratorResult> NewJSIteratorResult(Handle<Object> value, bool done);
  Handle<JSAsyncFromSyncIterator> NewJSAsyncFromSyncIterator(
      Handle<JSReceiver> sync_iterator);

  Handle<JSMap> NewJSMap();
  Handle<JSSet> NewJSSet();

  Handle<JSMapIterator> NewJSMapIterator(Handle<Map> map,
                                         Handle<OrderedHashMap> table,
                                         int index);
  Handle<JSSetIterator> NewJSSetIterator(Handle<Map> map,
                                         Handle<OrderedHashSet> table,
                                         int index);

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
      Handle<Object> context_or_undefined, Handle<Cell> vector,
      PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info, Handle<Context> context,
      Handle<Cell> vector, PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<Map> initial_map, Handle<SharedFunctionInfo> function_info,
      Handle<Object> context_or_undefined, PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info, Handle<Context> context,
      PretenureFlag pretenure = TENURED);

  // The choke-point for JSFunction creation. Handles allocation and
  // initialization. All other utility methods call into this.
  Handle<JSFunction> NewFunction(Handle<Map> map,
                                 Handle<SharedFunctionInfo> info,
                                 Handle<Object> context_or_undefined,
                                 PretenureFlag pretenure = TENURED);

  // Create a serialized scope info.
  Handle<ScopeInfo> NewScopeInfo(int length);

  Handle<ModuleInfoEntry> NewModuleInfoEntry();
  Handle<ModuleInfo> NewModuleInfo();

  Handle<PreParsedScopeData> NewPreParsedScopeData();

  // Create an External object for V8's external API.
  Handle<JSObject> NewExternal(void* value);

  // Creates a new CodeDataContainer for a Code object.
  Handle<CodeDataContainer> NewCodeDataContainer(int flags);

  // The reference to the Code object is stored in self_reference.
  // This allows generated code to reference its own Code object
  // by containing this handle.
  Handle<Code> NewCode(const CodeDesc& desc, Code::Kind kind,
                       Handle<Object> self_reference,
                       int32_t builtin_index = Builtins::kNoBuiltinId,
                       MaybeHandle<HandlerTable> maybe_handler_table =
                           MaybeHandle<HandlerTable>(),
                       MaybeHandle<ByteArray> maybe_source_position_table =
                           MaybeHandle<ByteArray>(),
                       MaybeHandle<DeoptimizationData> maybe_deopt_data =
                           MaybeHandle<DeoptimizationData>(),
                       Movability movability = kMovable, uint32_t stub_key = 0,
                       bool is_turbofanned = false, int stack_slots = 0,
                       int safepoint_table_offset = 0);

  // Allocates a new, empty code object for use by builtin deserialization. The
  // given {size} argument specifies the size of the entire code object.
  Handle<Code> NewCodeForDeserialization(uint32_t size);

  Handle<Code> CopyCode(Handle<Code> code);

  Handle<BytecodeArray> CopyBytecodeArray(Handle<BytecodeArray>);

  // Interface for creating error objects.
  Handle<Object> NewError(Handle<JSFunction> constructor,
                          Handle<String> message);

  Handle<Object> NewInvalidStringLengthError();

  inline Handle<Object> NewURIError();

  Handle<Object> NewError(Handle<JSFunction> constructor,
                          MessageTemplate::Template template_index,
                          Handle<Object> arg0 = Handle<Object>(),
                          Handle<Object> arg1 = Handle<Object>(),
                          Handle<Object> arg2 = Handle<Object>());

#define DECLARE_ERROR(NAME)                                          \
  Handle<Object> New##NAME(MessageTemplate::Template template_index, \
                           Handle<Object> arg0 = Handle<Object>(),   \
                           Handle<Object> arg1 = Handle<Object>(),   \
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

  Handle<String> NumberToString(Handle<Object> number,
                                bool check_number_string_cache = true);

  inline Handle<String> Uint32ToString(uint32_t value);

  Handle<JSFunction> InstallMembers(Handle<JSFunction> function);

#define ROOT_ACCESSOR(type, name, camel_name) inline Handle<type> name();
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name) inline Handle<Map> name##_map();
  STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) inline Handle<String> name();
  INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) inline Handle<Symbol> name();
  PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) inline Handle<Symbol> name();
  PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
  WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define ACCESSOR_INFO_ACCESSOR(accessor_name, AccessorName) \
  inline Handle<AccessorInfo> accessor_name##_accessor();
  ACCESSOR_INFO_LIST(ACCESSOR_INFO_ACCESSOR)
#undef ACCESSOR_INFO_ACCESSOR

  // Allocates a new SharedFunctionInfo object.
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      MaybeHandle<String> name, FunctionKind kind, Handle<Code> code,
      Handle<ScopeInfo> scope_info);
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      MaybeHandle<String> name, MaybeHandle<Code> code, bool is_constructor,
      FunctionKind kind = kNormalFunction,
      int maybe_builtin_index = Builtins::kNoBuiltinId);

  Handle<SharedFunctionInfo> NewSharedFunctionInfoForLiteral(
      FunctionLiteral* literal, Handle<Script> script);

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
  Handle<JSMessageObject> NewJSMessageObject(MessageTemplate::Template message,
                                             Handle<Object> argument,
                                             int start_position,
                                             int end_position,
                                             Handle<Object> script,
                                             Handle<Object> stack_frames);

  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);

  Handle<CoverageInfo> NewCoverageInfo(const ZoneVector<SourceRange>& slots);

  // Return a map for given number of properties using the map cache in the
  // native context.
  Handle<Map> ObjectLiteralMapFromCache(Handle<Context> native_context,
                                        int number_of_properties);

  Handle<RegExpMatchInfo> NewRegExpMatchInfo();

  // Creates a new FixedArray that holds the data associated with the
  // atom regexp and stores it in the regexp.
  void SetRegExpAtomData(Handle<JSRegExp> regexp,
                         JSRegExp::Type type,
                         Handle<String> source,
                         JSRegExp::Flags flags,
                         Handle<Object> match_pattern);

  // Creates a new FixedArray that holds the data associated with the
  // irregexp regexp and stores it in the regexp.
  void SetRegExpIrregexpData(Handle<JSRegExp> regexp,
                             JSRegExp::Type type,
                             Handle<String> source,
                             JSRegExp::Flags flags,
                             int capture_count);

  // Returns the value for a known global constant (a property of the global
  // object which is neither configurable nor writable) like 'undefined'.
  // Returns a null handle when the given name is unknown.
  Handle<Object> GlobalConstantFor(Handle<Name> name);

  // Converts the given boolean condition to JavaScript boolean value.
  Handle<Object> ToBoolean(bool value);

  // Converts the given ToPrimitive hint to it's string representation.
  Handle<String> ToPrimitiveHintString(ToPrimitiveHint hint);

 private:
  Isolate* isolate() { return reinterpret_cast<Isolate*>(this); }

  // Creates a heap object based on the map. The fields of the heap object are
  // not initialized by New<>() functions. It's the responsibility of the caller
  // to do that.
  template<typename T>
  Handle<T> New(Handle<Map> map, AllocationSpace space);

  template<typename T>
  Handle<T> New(Handle<Map> map,
                AllocationSpace space,
                Handle<AllocationSite> allocation_site);

  MaybeHandle<String> NewStringFromTwoByte(const uc16* string, int length,
                                           PretenureFlag pretenure);

  // Attempt to find the number in a small cache.  If we finds it, return
  // the string representation of the number.  Otherwise return undefined.
  Handle<Object> GetNumberStringCache(Handle<Object> number);

  // Update the cache with a new number-string pair.
  void SetNumberStringCache(Handle<Object> number, Handle<String> string);

  // Create a JSArray with no elements and no length.
  Handle<JSArray> NewJSArray(ElementsKind elements_kind,
                             PretenureFlag pretenure = NOT_TENURED);
};

// Utility class to simplify argument handling around JSFunction creation.
class NewFunctionArgs final {
 public:
  static NewFunctionArgs ForWasm(Handle<String> name, Handle<Code> code,
                                 Handle<Map> map);
  static NewFunctionArgs ForBuiltin(Handle<String> name, Handle<Code> code,
                                    Handle<Map> map, int builtin_id);
  static NewFunctionArgs ForFunctionWithoutCode(Handle<String> name,
                                                Handle<Map> map,
                                                LanguageMode language_mode);
  static NewFunctionArgs ForBuiltinWithPrototype(
      Handle<String> name, Handle<Code> code, Handle<Object> prototype,
      InstanceType type, int instance_size, int inobject_properties,
      int builtin_id, MutableMode prototype_mutability);
  static NewFunctionArgs ForBuiltinWithoutPrototype(Handle<String> name,
                                                    Handle<Code> code,
                                                    int builtin_id,
                                                    LanguageMode language_mode);

  Handle<Map> GetMap(Isolate* isolate) const;

 private:
  NewFunctionArgs() {}  // Use the static factory constructors.

  void SetShouldCreateAndSetInitialMap();
  void SetShouldSetPrototype();
  void SetShouldSetLanguageMode();

  // Sentinel value.
  static const int kUninitialized = -1;

  Handle<String> name_;
  MaybeHandle<Map> maybe_map_;
  MaybeHandle<Code> maybe_code_;

  bool should_create_and_set_initial_map_ = false;
  InstanceType type_;
  int instance_size_ = kUninitialized;
  int inobject_properties_ = kUninitialized;

  bool should_set_prototype_ = false;
  MaybeHandle<Object> maybe_prototype_;

  bool should_set_language_mode_ = false;
  LanguageMode language_mode_;

  int maybe_builtin_id_ = kUninitialized;

  MutableMode prototype_mutability_;

  friend class Factory;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FACTORY_H_
