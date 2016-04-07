// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FACTORY_H_
#define V8_FACTORY_H_

#include "src/isolate.h"
#include "src/messages.h"
#include "src/type-feedback-vector.h"

namespace v8 {
namespace internal {

// Interface for handle based allocation.
class Factory final {
 public:
  Handle<Oddball> NewOddball(Handle<Map> map, const char* to_string,
                             Handle<Object> to_number, const char* type_of,
                             byte kind);

  // Allocates a fixed array initialized with undefined values.
  Handle<FixedArray> NewFixedArray(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new fixed array with non-existing entries (the hole).
  Handle<FixedArray> NewFixedArrayWithHoles(
      int size,
      PretenureFlag pretenure = NOT_TENURED);

  // Allocates an uninitialized fixed array. It must be filled by the caller.
  Handle<FixedArray> NewUninitializedFixedArray(int size);

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

  Handle<OrderedHashSet> NewOrderedHashSet();
  Handle<OrderedHashMap> NewOrderedHashMap();

  // Create a new boxed value.
  Handle<Box> NewBox(Handle<Object> value);

  // Create a new PrototypeInfo struct.
  Handle<PrototypeInfo> NewPrototypeInfo();

  // Create a new SloppyBlockWithEvalContextExtension struct.
  Handle<SloppyBlockWithEvalContextExtension>
  NewSloppyBlockWithEvalContextExtension(Handle<ScopeInfo> scope_info,
                                         Handle<JSObject> extension);

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
  Handle<String> InternalizeString(Handle<String> string) {
    if (string->IsInternalizedString()) return string;
    return StringTable::LookupString(isolate(), string);
  }

  Handle<Name> InternalizeName(Handle<Name> name) {
    if (name->IsUniqueName()) return name;
    return StringTable::LookupString(isolate(), Handle<String>::cast(name));
  }

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
      Vector<const uint8_t> str,
      PretenureFlag pretenure = NOT_TENURED);

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


  // Allocates and fully initializes a String.  There are two String encodings:
  // one-byte and two-byte. One should choose between the threestring
  // allocation functions based on the encoding of the string buffer used to
  // initialized the string.
  //   - ...FromOneByte initializes the string from a buffer that is Latin1
  //     encoded (it does not check that the buffer is Latin1 encoded) and the
  //     result will be Latin1 encoded.
  //   - ...FromUTF8 initializes the string from a buffer that is UTF-8
  //     encoded.  If the characters are all ASCII characters, the result
  //     will be Latin1 encoded, otherwise it will converted to two-byte.
  //   - ...FromTwoByte initializes the string from a buffer that is two-byte
  //     encoded.  If the characters are all Latin1 characters, the
  //     result will be converted to Latin1, otherwise it will be left as
  //     two-byte.

  // TODO(dcarney): remove this function.
  MUST_USE_RESULT inline MaybeHandle<String> NewStringFromAscii(
      Vector<const char> str,
      PretenureFlag pretenure = NOT_TENURED) {
    return NewStringFromOneByte(Vector<const uint8_t>::cast(str), pretenure);
  }

  // UTF8 strings are pretenured when used for regexp literal patterns and
  // flags in the parser.
  MUST_USE_RESULT MaybeHandle<String> NewStringFromUtf8(
      Vector<const char> str,
      PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT MaybeHandle<String> NewStringFromTwoByte(
      Vector<const uc16> str,
      PretenureFlag pretenure = NOT_TENURED);

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

  // Create a new string object which holds a proper substring of a string.
  Handle<String> NewProperSubString(Handle<String> str,
                                    int begin,
                                    int end);

  // Create a new string object which holds a substring of a string.
  Handle<String> NewSubString(Handle<String> str, int begin, int end) {
    if (begin == 0 && end == str->length()) return str;
    return NewProperSubString(str, begin, end);
  }

  // Creates a new external String object.  There are two String encodings
  // in the system: one-byte and two-byte.  Unlike other String types, it does
  // not make sense to have a UTF-8 factory function for external strings,
  // because we cannot change the underlying buffer.  Note that these strings
  // are backed by a string resource that resides outside the V8 heap.
  MUST_USE_RESULT MaybeHandle<String> NewExternalStringFromOneByte(
      const ExternalOneByteString::Resource* resource);
  MUST_USE_RESULT MaybeHandle<String> NewExternalStringFromTwoByte(
      const ExternalTwoByteString::Resource* resource);

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
  Handle<Context> NewModuleContext(Handle<ScopeInfo> scope_info);

  // Create a function context.
  Handle<Context> NewFunctionContext(int length, Handle<JSFunction> function);

  // Create a catch context.
  Handle<Context> NewCatchContext(Handle<JSFunction> function,
                                  Handle<Context> previous,
                                  Handle<String> name,
                                  Handle<Object> thrown_object);

  // Create a 'with' context.
  Handle<Context> NewWithContext(Handle<JSFunction> function,
                                 Handle<Context> previous,
                                 Handle<JSReceiver> extension);

  // Create a block context.
  Handle<Context> NewBlockContext(Handle<JSFunction> function,
                                  Handle<Context> previous,
                                  Handle<ScopeInfo> scope_info);

  // Allocate a new struct.  The struct is pretenured (allocated directly in
  // the old generation).
  Handle<Struct> NewStruct(InstanceType type);

  Handle<CodeCache> NewCodeCache();

  Handle<AliasedArgumentsEntry> NewAliasedArgumentsEntry(
      int aliased_context_slot);

  Handle<AccessorInfo> NewAccessorInfo();

  Handle<Script> NewScript(Handle<String> source);

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

  Handle<PropertyCell> NewPropertyCell();

  Handle<WeakCell> NewWeakCell(Handle<HeapObject> value);

  Handle<TransitionArray> NewTransitionArray(int capacity);

  // Allocate a tenured AllocationSite. It's payload is null.
  Handle<AllocationSite> NewAllocationSite();

  Handle<Map> NewMap(
      InstanceType type,
      int instance_size,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND);

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

  Handle<FixedArray> CopyFixedArrayUpTo(Handle<FixedArray> array, int new_len,
                                        PretenureFlag pretenure = NOT_TENURED);

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  // This method expects a COW array in new space, and creates a copy
  // of it in old space.
  Handle<FixedArray> CopyAndTenureFixedCOWArray(Handle<FixedArray> array);

  Handle<FixedDoubleArray> CopyFixedDoubleArray(
      Handle<FixedDoubleArray> array);

  // Numbers (e.g. literals) are pretenured by the parser.
  // The return value may be a smi or a heap number.
  Handle<Object> NewNumber(double value,
                           PretenureFlag pretenure = NOT_TENURED);

  Handle<Object> NewNumberFromInt(int32_t value,
                                  PretenureFlag pretenure = NOT_TENURED);
  Handle<Object> NewNumberFromUint(uint32_t value,
                                  PretenureFlag pretenure = NOT_TENURED);
  Handle<Object> NewNumberFromSize(size_t value,
                                   PretenureFlag pretenure = NOT_TENURED) {
    // We can't use Smi::IsValid() here because that operates on a signed
    // intptr_t, and casting from size_t could create a bogus sign bit.
    if (value <= static_cast<size_t>(Smi::kMaxValue)) {
      return Handle<Object>(Smi::FromIntptr(static_cast<intptr_t>(value)),
                            isolate());
    }
    return NewNumber(static_cast<double>(value), pretenure);
  }
  Handle<HeapNumber> NewHeapNumber(double value,
                                   MutableMode mode = IMMUTABLE,
                                   PretenureFlag pretenure = NOT_TENURED);

#define SIMD128_NEW_DECL(TYPE, Type, type, lane_count, lane_type) \
  Handle<Type> New##Type(lane_type lanes[lane_count],             \
                         PretenureFlag pretenure = NOT_TENURED);
  SIMD128_TYPES(SIMD128_NEW_DECL)
#undef SIMD128_NEW_DECL

  // These objects are used by the api to create env-independent data
  // structures in the heap.
  inline Handle<JSObject> NewNeanderObject() {
    return NewJSObjectFromMap(neander_map());
  }

  Handle<JSWeakMap> NewJSWeakMap();

  Handle<JSObject> NewArgumentsObject(Handle<JSFunction> callee, int length);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(Handle<JSFunction> constructor,
                               PretenureFlag pretenure = NOT_TENURED);
  // JSObject that should have a memento pointing to the allocation site.
  Handle<JSObject> NewJSObjectWithMemento(Handle<JSFunction> constructor,
                                          Handle<AllocationSite> site);

  // Global objects are pretenured and initialized based on a constructor.
  Handle<JSGlobalObject> NewJSGlobalObject(Handle<JSFunction> constructor);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(
      Handle<Map> map,
      PretenureFlag pretenure = NOT_TENURED,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());

  // JS modules are pretenured.
  Handle<JSModule> NewJSModule(Handle<Context> context,
                               Handle<ScopeInfo> scope_info);

  // JS arrays are pretenured when allocated by the parser.

  // Create a JSArray with a specified length and elements initialized
  // according to the specified mode.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind, int length, int capacity,
      Strength strength = Strength::WEAK,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArray(
      int capacity, ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      Strength strength = Strength::WEAK,
      PretenureFlag pretenure = NOT_TENURED) {
    if (capacity != 0) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
    return NewJSArray(elements_kind, 0, capacity, strength,
                      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, pretenure);
  }

  // Create a JSArray with the given elements.
  Handle<JSArray> NewJSArrayWithElements(Handle<FixedArrayBase> elements,
                                         ElementsKind elements_kind, int length,
                                         Strength strength = Strength::WEAK,
                                         PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      Strength strength = Strength::WEAK,
      PretenureFlag pretenure = NOT_TENURED) {
    return NewJSArrayWithElements(elements, elements_kind, elements->length(),
                                  strength, pretenure);
  }

  void NewJSArrayStorage(
      Handle<JSArray> array,
      int length,
      int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<JSGeneratorObject> NewJSGeneratorObject(Handle<JSFunction> function);

  Handle<JSArrayBuffer> NewJSArrayBuffer(
      SharedFlag shared = SharedFlag::kNotShared,
      PretenureFlag pretenure = NOT_TENURED);

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

  Handle<JSMap> NewJSMap();
  Handle<JSSet> NewJSSet();

  // TODO(aandrey): Maybe these should take table, index and kind arguments.
  Handle<JSMapIterator> NewJSMapIterator();
  Handle<JSSetIterator> NewJSSetIterator();

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

  Handle<JSGlobalProxy> NewUninitializedJSGlobalProxy();

  Handle<JSFunction> NewFunction(Handle<String> name, Handle<Code> code,
                                 Handle<Object> prototype,
                                 bool read_only_prototype = false,
                                 bool is_strict = false);
  Handle<JSFunction> NewFunction(Handle<String> name);
  Handle<JSFunction> NewFunctionWithoutPrototype(Handle<String> name,
                                                 Handle<Code> code,
                                                 bool is_strict = false);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<Map> initial_map, Handle<SharedFunctionInfo> function_info,
      Handle<Context> context, PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info, Handle<Context> context,
      PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunction(Handle<String> name, Handle<Code> code,
                                 Handle<Object> prototype, InstanceType type,
                                 int instance_size,
                                 bool read_only_prototype = false,
                                 bool install_constructor = false,
                                 bool is_strict = false);
  Handle<JSFunction> NewFunction(Handle<String> name,
                                 Handle<Code> code,
                                 InstanceType type,
                                 int instance_size);
  Handle<JSFunction> NewFunction(Handle<Map> map, Handle<String> name,
                                 MaybeHandle<Code> maybe_code);

  // Create a serialized scope info.
  Handle<ScopeInfo> NewScopeInfo(int length);

  // Create an External object for V8's external API.
  Handle<JSObject> NewExternal(void* value);

  // The reference to the Code object is stored in self_reference.
  // This allows generated code to reference its own Code object
  // by containing this handle.
  Handle<Code> NewCode(const CodeDesc& desc,
                       Code::Flags flags,
                       Handle<Object> self_reference,
                       bool immovable = false,
                       bool crankshafted = false,
                       int prologue_offset = Code::kPrologueOffsetNotSet,
                       bool is_debug = false);

  Handle<Code> CopyCode(Handle<Code> code);

  Handle<Code> CopyCode(Handle<Code> code, Vector<byte> reloc_info);

  Handle<BytecodeArray> CopyBytecodeArray(Handle<BytecodeArray>);

  // Interface for creating error objects.
  Handle<Object> NewError(Handle<JSFunction> constructor,
                          Handle<String> message);

  Handle<Object> NewInvalidStringLengthError() {
    return NewRangeError(MessageTemplate::kInvalidStringLength);
  }

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
#undef DEFINE_ERROR

  Handle<String> NumberToString(Handle<Object> number,
                                bool check_number_string_cache = true);

  Handle<String> Uint32ToString(uint32_t value) {
    return NumberToString(NewNumberFromUint(value));
  }

  Handle<JSFunction> InstallMembers(Handle<JSFunction> function);

#define ROOT_ACCESSOR(type, name, camel_name)                         \
  inline Handle<type> name() {                                        \
    return Handle<type>(bit_cast<type**>(                             \
        &isolate()->heap()->roots_[Heap::k##camel_name##RootIndex])); \
  }
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

#define STRUCT_MAP_ACCESSOR(NAME, Name, name)                      \
  inline Handle<Map> name##_map() {                                \
    return Handle<Map>(bit_cast<Map**>(                            \
        &isolate()->heap()->roots_[Heap::k##Name##MapRootIndex])); \
  }
  STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str)                              \
  inline Handle<String> name() {                                \
    return Handle<String>(bit_cast<String**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
  INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name)                                   \
  inline Handle<Symbol> name() {                                \
    return Handle<Symbol>(bit_cast<Symbol**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
  PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description)                      \
  inline Handle<Symbol> name() {                                \
    return Handle<Symbol>(bit_cast<Symbol**>(                   \
        &isolate()->heap()->roots_[Heap::k##name##RootIndex])); \
  }
  PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
  WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

  // Allocates a new SharedFunctionInfo object.
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      Handle<String> name, int number_of_literals, FunctionKind kind,
      Handle<Code> code, Handle<ScopeInfo> scope_info,
      Handle<TypeFeedbackVector> feedback_vector);
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(Handle<String> name,
                                                   MaybeHandle<Code> code,
                                                   bool is_constructor);

  // Allocates a new JSMessageObject object.
  Handle<JSMessageObject> NewJSMessageObject(MessageTemplate::Template message,
                                             Handle<Object> argument,
                                             int start_position,
                                             int end_position,
                                             Handle<Object> script,
                                             Handle<Object> stack_frames);

  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);

  // Return a map for given number of properties using the map cache in the
  // native context.
  Handle<Map> ObjectLiteralMapFromCache(Handle<Context> context,
                                        int number_of_properties,
                                        bool is_strong,
                                        bool* is_result_from_cache);

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

  // Creates a code object that is not yet fully initialized yet.
  inline Handle<Code> NewCodeRaw(int object_size, bool immovable);

  // Attempt to find the number in a small cache.  If we finds it, return
  // the string representation of the number.  Otherwise return undefined.
  Handle<Object> GetNumberStringCache(Handle<Object> number);

  // Update the cache with a new number-string pair.
  void SetNumberStringCache(Handle<Object> number, Handle<String> string);

  // Creates a function initialized with a shared part.
  Handle<JSFunction> NewFunction(Handle<Map> map,
                                 Handle<SharedFunctionInfo> info,
                                 Handle<Context> context,
                                 PretenureFlag pretenure = TENURED);

  // Create a JSArray with no elements and no length.
  Handle<JSArray> NewJSArray(ElementsKind elements_kind,
                             Strength strength = Strength::WEAK,
                             PretenureFlag pretenure = NOT_TENURED);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_FACTORY_H_
