// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The infrastructure used for (localized) message reporting in V8.
//
// Note: there's a big unresolved issue about ownership of the data
// structures used by this framework.

#ifndef V8_MESSAGES_H_
#define V8_MESSAGES_H_

// Forward declaration of MessageLocation.
namespace v8 {
namespace internal {
class MessageLocation;
} }  // namespace v8::internal


class V8Message {
 public:
  V8Message(char* type,
            v8::internal::Handle<v8::internal::JSArray> args,
            const v8::internal::MessageLocation* loc) :
      type_(type), args_(args), loc_(loc) { }
  char* type() const { return type_; }
  v8::internal::Handle<v8::internal::JSArray> args() const { return args_; }
  const v8::internal::MessageLocation* loc() const { return loc_; }
 private:
  char* type_;
  v8::internal::Handle<v8::internal::JSArray> const args_;
  const v8::internal::MessageLocation* loc_;
};


namespace v8 {
namespace internal {

struct Language;
class SourceInfo;

class MessageLocation {
 public:
  MessageLocation(Handle<Script> script, int start_pos, int end_pos,
                  Handle<JSFunction> function = Handle<JSFunction>())
      : script_(script),
        start_pos_(start_pos),
        end_pos_(end_pos),
        function_(function) {}
  MessageLocation() : start_pos_(-1), end_pos_(-1) { }

  Handle<Script> script() const { return script_; }
  int start_pos() const { return start_pos_; }
  int end_pos() const { return end_pos_; }
  Handle<JSFunction> function() const { return function_; }

 private:
  Handle<Script> script_;
  int start_pos_;
  int end_pos_;
  Handle<JSFunction> function_;
};


// A message handler is a convenience interface for accessing the list
// of message listeners registered in an environment
class MessageHandler {
 public:
  // Returns a message object for the API to use.
  static Handle<JSMessageObject> MakeMessageObject(
      Isolate* isolate,
      const char* type,
      MessageLocation* loc,
      Vector< Handle<Object> > args,
      Handle<JSArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  static void ReportMessage(Isolate* isolate,
                            MessageLocation* loc,
                            Handle<Object> message);

  static void DefaultMessageReport(Isolate* isolate,
                                   const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static SmartArrayPointer<char> GetLocalizedMessage(Isolate* isolate,
                                                     Handle<Object> data);
};


class CallSite {
 public:
  CallSite(Handle<Object> receiver, Handle<JSFunction> fun, int pos)
      : receiver_(receiver), fun_(fun), pos_(pos) {}

  Handle<Object> GetFileName(Isolate* isolate);
  Handle<Object> GetFunctionName(Isolate* isolate);
  Handle<Object> GetScriptNameOrSourceUrl(Isolate* isolate);
  Handle<Object> GetMethodName(Isolate* isolate);
  // Return 1-based line number, including line offset.
  int GetLineNumber(Isolate* isolate);
  // Return 1-based column number, including column offset if first line.
  int GetColumnNumber(Isolate* isolate);
  bool IsNative(Isolate* isolate);
  bool IsToplevel(Isolate* isolate);
  bool IsEval(Isolate* isolate);
  bool IsConstructor(Isolate* isolate);

 private:
  Handle<Object> receiver_;
  Handle<JSFunction> fun_;
  int pos_;
};


#define MESSAGE_TEMPLATES(T)                                                   \
  /* Error */                                                                  \
  T(CyclicProto, "Cyclic __proto__ value")                                     \
  T(DefaultOptionsMissing, "Internal % error. Default options are missing.")   \
  T(Unsupported, "Not supported")                                              \
  T(WrongServiceType, "Internal error, wrong service type: %")                 \
  T(WrongValueType, "Internal error. Wrong value type.")                       \
  /* TypeError */                                                              \
  T(ApplyNonFunction,                                                          \
    "Function.prototype.apply was called on %, which is a % and not a "        \
    "function")                                                                \
  T(ArrayFunctionsOnFrozen, "Cannot modify frozen array elements")             \
  T(ArrayFunctionsOnSealed, "Cannot add/remove sealed array elements")         \
  T(ArrayNotSubclassable, "Subclassing Arrays is not currently supported.")    \
  T(CalledNonCallable, "% is not a function")                                  \
  T(CalledOnNonObject, "% called on non-object")                               \
  T(CalledOnNullOrUndefined, "% called on null or undefined")                  \
  T(CannotConvertToPrimitive, "Cannot convert object to primitive value")      \
  T(ConstructorNonCallable,                                                    \
    "Class constructors cannot be invoked without 'new'")                      \
  T(ConstructorNotFunction, "Constructor % requires 'new'")                    \
  T(CurrencyCode, "Currency code is required with currency style.")            \
  T(DataViewNotArrayBuffer,                                                    \
    "First argument to DataView constructor must be an ArrayBuffer")           \
  T(DateType, "this is not a Date object.")                                    \
  T(DefineDisallowed, "Cannot define property:%, object is not extensible.")   \
  T(FirstArgumentNotRegExp,                                                    \
    "First argument to % must not be a regular expression")                    \
  T(FlagsGetterNonObject,                                                      \
    "RegExp.prototype.flags getter called on non-object %")                    \
  T(FunctionBind, "Bind must be called on a function")                         \
  T(GeneratorRunning, "Generator is already running")                          \
  T(IncompatibleMethodReceiver, "Method % called on incompatible receiver %")  \
  T(InstanceofFunctionExpected,                                                \
    "Expecting a function in instanceof check, but got %")                     \
  T(InstanceofNonobjectProto,                                                  \
    "Function has non-object prototype '%' in instanceof check")               \
  T(InvalidArgument, "invalid_argument")                                       \
  T(InvalidInOperatorUse, "Cannot use 'in' operator to search for '%' in %")   \
  T(IteratorResultNotAnObject, "Iterator result % is not an object")           \
  T(IteratorValueNotAnObject, "Iterator value % is not an entry object")       \
  T(LanguageID, "Language ID should be string or object.")                     \
  T(MethodCalledOnWrongObject,                                                 \
    "Method % called on a non-object or on a wrong type of object.")           \
  T(MethodInvokedOnNullOrUndefined,                                            \
    "Method invoked on undefined or null value.")                              \
  T(MethodInvokedOnWrongType, "Method invoked on an object that is not %.")    \
  T(NotAnIterator, "% is not an iterator")                                     \
  T(NotAPromise, "% is not a promise")                                         \
  T(NotConstructor, "% is not a constructor")                                  \
  T(NotGeneric, "% is not generic")                                            \
  T(NotIterable, "% is not iterable")                                          \
  T(NotTypedArray, "this is not a typed array.")                               \
  T(ObjectGetterExpectingFunction,                                             \
    "Object.prototype.__defineGetter__: Expecting function")                   \
  T(ObjectGetterCallable, "Getter must be a function: %")                      \
  T(ObjectSetterExpectingFunction,                                             \
    "Object.prototype.__defineSetter__: Expecting function")                   \
  T(ObjectSetterCallable, "Setter must be a function: %")                      \
  T(ObserveCallbackFrozen,                                                     \
    "Object.observe cannot deliver to a frozen function object")               \
  T(ObserveGlobalProxy, "% cannot be called on the global proxy object")       \
  T(ObserveInvalidAccept,                                                      \
    "Third argument to Object.observe must be an array of strings.")           \
  T(ObserveNonFunction, "Object.% cannot deliver to non-function")             \
  T(ObserveNonObject, "Object.% cannot % non-object")                          \
  T(ObserveNotifyNonNotifier, "notify called on non-notifier object")          \
  T(ObservePerformNonFunction, "Cannot perform non-function")                  \
  T(ObservePerformNonString, "Invalid non-string changeType")                  \
  T(ObserveTypeNonString,                                                      \
    "Invalid changeRecord with non-string 'type' property")                    \
  T(OrdinaryFunctionCalledAsConstructor,                                       \
    "Function object that's not a constructor was created with new")           \
  T(PromiseCyclic, "Chaining cycle detected for promise %")                    \
  T(PropertyDescObject, "Property description must be an object: %")           \
  T(PropertyNotFunction, "Property '%' of object % is not a function")         \
  T(ProtoObjectOrNull, "Object prototype may only be an Object or null: %")    \
  T(ProxyHandlerNonObject, "Proxy.% called with non-object as handler")        \
  T(ProxyHandlerReturned, "Proxy handler % returned % from '%' trap")          \
  T(ProxyHandlerTrapMissing, "Proxy handler % has no '%' trap")                \
  T(ProxyHandlerTrapMustBeCallable,                                            \
    "Proxy handler %0 has non-callable '%' trap")                              \
  T(ProxyNonObjectPropNames, "Trap '%' returned non-object %")                 \
  T(ProxyProtoNonObject, "Proxy.create called with no-object as prototype")    \
  T(ProxyPropNotConfigurable,                                                  \
    "Proxy handler % returned non-configurable descriptor for property '%' "   \
    "from '%' trap")                                                           \
  T(ProxyRepeatedPropName, "Trap '%' returned repeated property name '%'")     \
  T(ProxyTrapFunctionExpected,                                                 \
    "Proxy.createFunction called with non-function for '%' trap")              \
  T(RedefineDisallowed, "Cannot redefine property: %")                         \
  T(ReduceNoInitial, "Reduce of empty array with no initial value")            \
  T(ReinitializeIntl, "Trying to re-initialize % object.")                     \
  T(ResolvedOptionsCalledOnNonObject,                                          \
    "resolvedOptions method called on a non-object or on a object that is "    \
    "not Intl.%.")                                                             \
  T(ResolverNotAFunction, "Promise resolver % is not a function")              \
  T(SymbolToPrimitive,                                                         \
    "Cannot convert a Symbol wrapper object to a primitive value")             \
  T(SymbolToNumber, "Cannot convert a Symbol value to a number")               \
  T(SymbolToString, "Cannot convert a Symbol value to a string")               \
  T(UndefinedOrNullToObject, "Cannot convert undefined or null to object")     \
  T(ValueAndAccessor,                                                          \
    "Invalid property.  A property cannot both have accessors and be "         \
    "writable or have a value, %")                                             \
  T(WithExpression, "% has no properties")                                     \
  T(WrongArgs, "%: Arguments list has wrong type")                             \
  /* ReferenceError */                                                         \
  T(NonMethod, "'super' is referenced from non-method")                        \
  T(NotDefined, "% is not defined")                                            \
  T(UnsupportedSuper, "Unsupported reference to 'super'")                      \
  /* RangeError */                                                             \
  T(ArrayLengthOutOfRange, "defineProperty() array length out of range")       \
  T(DateRange, "Provided date is not in valid range.")                         \
  T(ExpectedLocation, "Expected Area/Location for time zone, got %")           \
  T(InvalidArrayBufferLength, "Invalid array buffer length")                   \
  T(InvalidArrayLength, "Invalid array length")                                \
  T(InvalidCodePoint, "Invalid code point %")                                  \
  T(InvalidCountValue, "Invalid count value")                                  \
  T(InvalidCurrencyCode, "Invalid currency code: %")                           \
  T(InvalidDataViewAccessorOffset,                                             \
    "Offset is outside the bounds of the DataView")                            \
  T(InvalidDataViewLength, "Invalid data view length")                         \
  T(InvalidDataViewOffset, "Start offset is outside the bounds of the buffer") \
  T(InvalidLanguageTag, "Invalid language tag: %")                             \
  T(InvalidStringLength, "Invalid string length")                              \
  T(InvalidTimeValue, "Invalid time value")                                    \
  T(InvalidTypedArrayAlignment, "% of % should be a multiple of %")            \
  T(InvalidTypedArrayLength, "Invalid typed array length")                     \
  T(InvalidTypedArrayOffset, "Start offset is too large:")                     \
  T(LocaleMatcher, "Illegal value for localeMatcher:%")                        \
  T(NormalizationForm, "The normalization form should be one of %.")           \
  T(NumberFormatRange, "% argument must be between 0 and 20")                  \
  T(PropertyValueOutOfRange, "% value is out of range.")                       \
  T(StackOverflow, "Maximum call stack size exceeded")                         \
  T(ToPrecisionFormatRange, "toPrecision() argument must be between 1 and 21") \
  T(ToRadixFormatRange, "toString() radix argument must be between 2 and 36")  \
  T(TypedArraySetNegativeOffset, "Start offset is negative")                   \
  T(TypedArraySetSourceTooLarge, "Source is too large")                        \
  T(UnsupportedTimeZone, "Unsupported time zone specified %")                  \
  T(ValueOutOfRange, "Value % out of range for % options property %")          \
  /* SyntaxError */                                                            \
  T(ParenthesisInArgString, "Function arg string contains parenthesis")        \
  /* EvalError */                                                              \
  T(CodeGenFromStrings, "%")                                                   \
  /* URIError */                                                               \
  T(URIMalformed, "URI malformed")

class MessageTemplate {
 public:
  enum Template {
#define TEMPLATE(NAME, STRING) k##NAME,
    MESSAGE_TEMPLATES(TEMPLATE)
#undef TEMPLATE
        kLastMessage
  };

  static MaybeHandle<String> FormatMessage(int template_index,
                                           Handle<String> arg0,
                                           Handle<String> arg1,
                                           Handle<String> arg2);
};
} }  // namespace v8::internal

#endif  // V8_MESSAGES_H_
