// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FACTORY_H_
#define V8_FACTORY_H_

#include "src/isolate.h"

namespace v8 {
namespace internal {

// Interface for handle based allocation.

class Factory FINAL {
 public:
  Handle<Oddball> NewOddball(Handle<Map> map,
                             const char* to_string,
                             Handle<Object> to_number,
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

  Handle<ConstantPoolArray> NewConstantPoolArray(
      const ConstantPoolArray::NumberOfEntries& small);

  Handle<ConstantPoolArray> NewExtendedConstantPoolArray(
      const ConstantPoolArray::NumberOfEntries& small,
      const ConstantPoolArray::NumberOfEntries& extended);

  Handle<OrderedHashSet> NewOrderedHashSet();
  Handle<OrderedHashMap> NewOrderedHashMap();

  // Create a new boxed value.
  Handle<Box> NewBox(Handle<Object> value);

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
  Handle<String> InternalizeString(Handle<String> str);
  Handle<String> InternalizeOneByteString(Vector<const uint8_t> str);
  Handle<String> InternalizeOneByteString(
      Handle<SeqOneByteString>, int from, int length);

  Handle<String> InternalizeTwoByteString(Vector<const uc16> str);

  template<class StringTableKey>
  Handle<String> InternalizeStringWithKey(StringTableKey* key);


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
  MUST_USE_RESULT Handle<String> NewInternalizedStringFromUtf8(
      Vector<const char> str,
      int chars,
      uint32_t hash_field);

  MUST_USE_RESULT Handle<String> NewOneByteInternalizedString(
      Vector<const uint8_t> str, uint32_t hash_field);

  MUST_USE_RESULT Handle<String> NewOneByteInternalizedSubString(
      Handle<SeqOneByteString> string, int offset, int length,
      uint32_t hash_field);

  MUST_USE_RESULT Handle<String> NewTwoByteInternalizedString(
        Vector<const uc16> str,
        uint32_t hash_field);

  MUST_USE_RESULT Handle<String> NewInternalizedStringImpl(
      Handle<String> string, int chars, uint32_t hash_field);

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
  Handle<Symbol> NewPrivateOwnSymbol();

  // Create a global (but otherwise uninitialized) context.
  Handle<Context> NewNativeContext();

  // Create a global context.
  Handle<Context> NewGlobalContext(Handle<JSFunction> function,
                                   Handle<ScopeInfo> scope_info);

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

  Handle<DeclaredAccessorDescriptor> NewDeclaredAccessorDescriptor();

  Handle<DeclaredAccessorInfo> NewDeclaredAccessorInfo();

  Handle<ExecutableAccessorInfo> NewExecutableAccessorInfo();

  Handle<Script> NewScript(Handle<String> source);

  // Foreign objects are pretenured when allocated by the bootstrapper.
  Handle<Foreign> NewForeign(Address addr,
                             PretenureFlag pretenure = NOT_TENURED);

  // Allocate a new foreign object.  The foreign is pretenured (allocated
  // directly in the old generation).
  Handle<Foreign> NewForeign(const AccessorDescriptor* foreign);

  Handle<ByteArray> NewByteArray(int length,
                                 PretenureFlag pretenure = NOT_TENURED);

  Handle<ExternalArray> NewExternalArray(
      int length,
      ExternalArrayType array_type,
      void* external_pointer,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<FixedTypedArrayBase> NewFixedTypedArray(
      int length,
      ExternalArrayType array_type,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<Cell> NewCell(Handle<Object> value);

  Handle<PropertyCell> NewPropertyCellWithHole();

  Handle<PropertyCell> NewPropertyCell(Handle<Object> value);

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

  Handle<FixedArray> CopyFixedArray(Handle<FixedArray> array);

  // This method expects a COW array in new space, and creates a copy
  // of it in old space.
  Handle<FixedArray> CopyAndTenureFixedCOWArray(Handle<FixedArray> array);

  Handle<FixedDoubleArray> CopyFixedDoubleArray(
      Handle<FixedDoubleArray> array);

  Handle<ConstantPoolArray> CopyConstantPoolArray(
      Handle<ConstantPoolArray> array);

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
    if (Smi::IsValid(static_cast<intptr_t>(value))) {
      return Handle<Object>(Smi::FromIntptr(static_cast<intptr_t>(value)),
                            isolate());
    }
    return NewNumber(static_cast<double>(value), pretenure);
  }
  Handle<HeapNumber> NewHeapNumber(double value,
                                   MutableMode mode = IMMUTABLE,
                                   PretenureFlag pretenure = NOT_TENURED);

  // These objects are used by the api to create env-independent data
  // structures in the heap.
  inline Handle<JSObject> NewNeanderObject() {
    return NewJSObjectFromMap(neander_map());
  }

  Handle<JSObject> NewArgumentsObject(Handle<JSFunction> callee, int length);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObject(Handle<JSFunction> constructor,
                               PretenureFlag pretenure = NOT_TENURED);
  // JSObject that should have a memento pointing to the allocation site.
  Handle<JSObject> NewJSObjectWithMemento(Handle<JSFunction> constructor,
                                          Handle<AllocationSite> site);

  // Global objects are pretenured and initialized based on a constructor.
  Handle<GlobalObject> NewGlobalObject(Handle<JSFunction> constructor);

  // JS objects are pretenured when allocated by the bootstrapper and
  // runtime.
  Handle<JSObject> NewJSObjectFromMap(
      Handle<Map> map,
      PretenureFlag pretenure = NOT_TENURED,
      bool allocate_properties = true,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());

  // JS modules are pretenured.
  Handle<JSModule> NewJSModule(Handle<Context> context,
                               Handle<ScopeInfo> scope_info);

  // JS arrays are pretenured when allocated by the parser.

  // Create a JSArray with no elements.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind,
      PretenureFlag pretenure = NOT_TENURED);

  // Create a JSArray with a specified length and elements initialized
  // according to the specified mode.
  Handle<JSArray> NewJSArray(
      ElementsKind elements_kind, int length, int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArray(
      int capacity,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      PretenureFlag pretenure = NOT_TENURED) {
    if (capacity != 0) {
      elements_kind = GetHoleyElementsKind(elements_kind);
    }
    return NewJSArray(elements_kind, 0, capacity,
                      INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE, pretenure);
  }

  // Create a JSArray with the given elements.
  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind,
      int length,
      PretenureFlag pretenure = NOT_TENURED);

  Handle<JSArray> NewJSArrayWithElements(
      Handle<FixedArrayBase> elements,
      ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND,
      PretenureFlag pretenure = NOT_TENURED) {
    return NewJSArrayWithElements(
        elements, elements_kind, elements->length(), pretenure);
  }

  void NewJSArrayStorage(
      Handle<JSArray> array,
      int length,
      int capacity,
      ArrayStorageAllocationMode mode = DONT_INITIALIZE_ARRAY_ELEMENTS);

  Handle<JSGeneratorObject> NewJSGeneratorObject(Handle<JSFunction> function);

  Handle<JSArrayBuffer> NewJSArrayBuffer();

  Handle<JSTypedArray> NewJSTypedArray(ExternalArrayType type);

  Handle<JSDataView> NewJSDataView();

  // Allocates a Harmony proxy.
  Handle<JSProxy> NewJSProxy(Handle<Object> handler, Handle<Object> prototype);

  // Allocates a Harmony function proxy.
  Handle<JSProxy> NewJSFunctionProxy(Handle<Object> handler,
                                     Handle<Object> call_trap,
                                     Handle<Object> construct_trap,
                                     Handle<Object> prototype);

  // Reinitialize an JSGlobalProxy based on a constructor.  The object
  // must have the same size as objects allocated using the
  // constructor.  The object is reinitialized and behaves as an
  // object that has been freshly allocated using the constructor.
  void ReinitializeJSGlobalProxy(Handle<JSGlobalProxy> global,
                                 Handle<JSFunction> constructor);

  // Change the type of the argument into a JS object/function and reinitialize.
  void BecomeJSObject(Handle<JSProxy> object);
  void BecomeJSFunction(Handle<JSProxy> object);

  Handle<JSFunction> NewFunction(Handle<String> name,
                                 Handle<Code> code,
                                 Handle<Object> prototype,
                                 bool read_only_prototype = false);
  Handle<JSFunction> NewFunction(Handle<String> name);
  Handle<JSFunction> NewFunctionWithoutPrototype(Handle<String> name,
                                                 Handle<Code> code);

  Handle<JSFunction> NewFunctionFromSharedFunctionInfo(
      Handle<SharedFunctionInfo> function_info,
      Handle<Context> context,
      PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunction(Handle<String> name,
                                 Handle<Code> code,
                                 Handle<Object> prototype,
                                 InstanceType type,
                                 int instance_size,
                                 bool read_only_prototype = false);
  Handle<JSFunction> NewFunction(Handle<String> name,
                                 Handle<Code> code,
                                 InstanceType type,
                                 int instance_size);

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

  // Interface for creating error objects.

  MaybeHandle<Object> NewError(const char* maker, const char* message,
                               Handle<JSArray> args);
  Handle<String> EmergencyNewError(const char* message, Handle<JSArray> args);
  MaybeHandle<Object> NewError(const char* maker, const char* message,
                               Vector<Handle<Object> > args);
  MaybeHandle<Object> NewError(const char* message,
                               Vector<Handle<Object> > args);
  MaybeHandle<Object> NewError(Handle<String> message);
  MaybeHandle<Object> NewError(const char* constructor, Handle<String> message);

  MaybeHandle<Object> NewTypeError(const char* message,
                                   Vector<Handle<Object> > args);
  MaybeHandle<Object> NewTypeError(Handle<String> message);

  MaybeHandle<Object> NewRangeError(const char* message,
                                    Vector<Handle<Object> > args);
  MaybeHandle<Object> NewRangeError(Handle<String> message);

  MaybeHandle<Object> NewInvalidStringLengthError() {
    return NewRangeError("invalid_string_length",
                         HandleVector<Object>(NULL, 0));
  }

  MaybeHandle<Object> NewSyntaxError(const char* message, Handle<JSArray> args);
  MaybeHandle<Object> NewSyntaxError(Handle<String> message);

  MaybeHandle<Object> NewReferenceError(const char* message,
                                        Vector<Handle<Object> > args);
  MaybeHandle<Object> NewReferenceError(const char* message,
                                        Handle<JSArray> args);
  MaybeHandle<Object> NewReferenceError(Handle<String> message);

  MaybeHandle<Object> NewEvalError(const char* message,
                                   Vector<Handle<Object> > args);

  Handle<String> NumberToString(Handle<Object> number,
                                bool check_number_string_cache = true);

  Handle<String> Uint32ToString(uint32_t value) {
    return NumberToString(NewNumberFromUint(value));
  }

  enum ApiInstanceType {
    JavaScriptObjectType,
    GlobalObjectType,
    GlobalProxyType
  };

  Handle<JSFunction> CreateApiFunction(
      Handle<FunctionTemplateInfo> data,
      Handle<Object> prototype,
      ApiInstanceType type = JavaScriptObjectType);

  Handle<JSFunction> InstallMembers(Handle<JSFunction> function);

  // Installs interceptors on the instance.  'desc' is a function template,
  // and instance is an object instance created by the function of this
  // function template.
  MUST_USE_RESULT MaybeHandle<FunctionTemplateInfo> ConfigureInstance(
      Handle<FunctionTemplateInfo> desc, Handle<JSObject> instance);

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

  inline void set_string_table(Handle<StringTable> table) {
    isolate()->heap()->set_string_table(*table);
  }

  Handle<String> hidden_string() {
    return Handle<String>(&isolate()->heap()->hidden_string_);
  }

  // Allocates a new SharedFunctionInfo object.
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(
      Handle<String> name, int number_of_literals, FunctionKind kind,
      Handle<Code> code, Handle<ScopeInfo> scope_info,
      Handle<TypeFeedbackVector> feedback_vector);
  Handle<SharedFunctionInfo> NewSharedFunctionInfo(Handle<String> name,
                                                   MaybeHandle<Code> code);

  // Allocate a new type feedback vector
  Handle<TypeFeedbackVector> NewTypeFeedbackVector(int slot_count);

  // Allocates a new JSMessageObject object.
  Handle<JSMessageObject> NewJSMessageObject(
      Handle<String> type,
      Handle<JSArray> arguments,
      int start_position,
      int end_position,
      Handle<Object> script,
      Handle<Object> stack_frames);

  Handle<DebugInfo> NewDebugInfo(Handle<SharedFunctionInfo> shared);

  // Return a map using the map cache in the native context.
  // The key the an ordered set of property names.
  Handle<Map> ObjectLiteralMapFromCache(Handle<Context> context,
                                        Handle<FixedArray> keys);

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
  Handle<Object> GlobalConstantFor(Handle<String> name);

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

  // Create a new map cache.
  Handle<MapCache> NewMapCache(int at_least_space_for);

  // Update the map cache in the native context with (keys, map)
  Handle<MapCache> AddToMapCache(Handle<Context> context,
                                 Handle<FixedArray> keys,
                                 Handle<Map> map);

  // Attempt to find the number in a small cache.  If we finds it, return
  // the string representation of the number.  Otherwise return undefined.
  Handle<Object> GetNumberStringCache(Handle<Object> number);

  // Update the cache with a new number-string pair.
  void SetNumberStringCache(Handle<Object> number, Handle<String> string);

  // Initializes a function with a shared part and prototype.
  // Note: this code was factored out of NewFunction such that other parts of
  // the VM could use it. Specifically, a function that creates instances of
  // type JS_FUNCTION_TYPE benefit from the use of this function.
  inline void InitializeFunction(Handle<JSFunction> function,
                                 Handle<SharedFunctionInfo> info,
                                 Handle<Context> context);

  // Creates a function initialized with a shared part.
  Handle<JSFunction> NewFunction(Handle<Map> map,
                                 Handle<SharedFunctionInfo> info,
                                 Handle<Context> context,
                                 PretenureFlag pretenure = TENURED);

  Handle<JSFunction> NewFunction(Handle<Map> map,
                                 Handle<String> name,
                                 MaybeHandle<Code> maybe_code);

  // Reinitialize a JSProxy into an (empty) JS object of respective type and
  // size, but keeping the original prototype.  The receiver must have at least
  // the size of the new object.  The object is reinitialized and behaves as an
  // object that has been freshly allocated.
  void ReinitializeJSProxy(Handle<JSProxy> proxy, InstanceType type, int size);
};

} }  // namespace v8::internal

#endif  // V8_FACTORY_H_
