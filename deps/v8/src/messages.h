// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The infrastructure used for (localized) message reporting in V8.
//
// Note: there's a big unresolved issue about ownership of the data
// structures used by this framework.

#ifndef V8_MESSAGES_H_
#define V8_MESSAGES_H_

#include <memory>

#include "src/handles.h"
#include "src/wasm/wasm-code-wrapper.h"

namespace v8 {
namespace internal {
namespace wasm {
class WasmCode;
}

// Forward declarations.
class AbstractCode;
class FrameArray;
class JSMessageObject;
class LookupIterator;
class SharedFunctionInfo;
class SourceInfo;
class WasmInstanceObject;

class MessageLocation {
 public:
  MessageLocation(Handle<Script> script, int start_pos, int end_pos);
  MessageLocation(Handle<Script> script, int start_pos, int end_pos,
                  Handle<SharedFunctionInfo> shared);
  MessageLocation();

  Handle<Script> script() const { return script_; }
  int start_pos() const { return start_pos_; }
  int end_pos() const { return end_pos_; }
  Handle<SharedFunctionInfo> shared() const { return shared_; }

 private:
  Handle<Script> script_;
  int start_pos_;
  int end_pos_;
  Handle<SharedFunctionInfo> shared_;
};

class StackFrameBase {
 public:
  virtual ~StackFrameBase() {}

  virtual Handle<Object> GetReceiver() const = 0;
  virtual Handle<Object> GetFunction() const = 0;

  virtual Handle<Object> GetFileName() = 0;
  virtual Handle<Object> GetFunctionName() = 0;
  virtual Handle<Object> GetScriptNameOrSourceUrl() = 0;
  virtual Handle<Object> GetMethodName() = 0;
  virtual Handle<Object> GetTypeName() = 0;
  virtual Handle<Object> GetEvalOrigin();

  virtual int GetPosition() const = 0;
  // Return 1-based line number, including line offset.
  virtual int GetLineNumber() = 0;
  // Return 1-based column number, including column offset if first line.
  virtual int GetColumnNumber() = 0;

  virtual bool IsNative() = 0;
  virtual bool IsToplevel() = 0;
  virtual bool IsEval();
  virtual bool IsConstructor() = 0;
  virtual bool IsStrict() const = 0;

  virtual MaybeHandle<String> ToString() = 0;

 protected:
  StackFrameBase() {}
  explicit StackFrameBase(Isolate* isolate) : isolate_(isolate) {}
  Isolate* isolate_;

 private:
  virtual bool HasScript() const = 0;
  virtual Handle<Script> GetScript() const = 0;
};

class JSStackFrame : public StackFrameBase {
 public:
  JSStackFrame(Isolate* isolate, Handle<Object> receiver,
               Handle<JSFunction> function, Handle<AbstractCode> code,
               int offset);
  virtual ~JSStackFrame() {}

  Handle<Object> GetReceiver() const override { return receiver_; }
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override;
  Handle<Object> GetFunctionName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override;
  Handle<Object> GetMethodName() override;
  Handle<Object> GetTypeName() override;

  int GetPosition() const override;
  int GetLineNumber() override;
  int GetColumnNumber() override;

  bool IsNative() override;
  bool IsToplevel() override;
  bool IsConstructor() override { return is_constructor_; }
  bool IsStrict() const override { return is_strict_; }

  MaybeHandle<String> ToString() override;

 private:
  JSStackFrame();
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  bool HasScript() const override;
  Handle<Script> GetScript() const override;

  Handle<Object> receiver_;
  Handle<JSFunction> function_;
  Handle<AbstractCode> code_;
  int offset_;

  bool is_constructor_;
  bool is_strict_;

  friend class FrameArrayIterator;
};

class WasmStackFrame : public StackFrameBase {
 public:
  virtual ~WasmStackFrame() {}

  Handle<Object> GetReceiver() const override;
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override { return Null(); }
  Handle<Object> GetFunctionName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override { return Null(); }
  Handle<Object> GetMethodName() override { return Null(); }
  Handle<Object> GetTypeName() override { return Null(); }

  int GetPosition() const override;
  int GetLineNumber() override { return wasm_func_index_; }
  int GetColumnNumber() override { return -1; }

  bool IsNative() override { return false; }
  bool IsToplevel() override { return false; }
  bool IsConstructor() override { return false; }
  bool IsStrict() const override { return false; }
  bool IsInterpreted() const { return code_.is_null(); }

  MaybeHandle<String> ToString() override;

 protected:
  Handle<Object> Null() const;

  bool HasScript() const override;
  Handle<Script> GetScript() const override;

  Handle<WasmInstanceObject> wasm_instance_;
  uint32_t wasm_func_index_;
  WasmCodeWrapper code_;  // null for interpreted frames.
  int offset_;

 private:
  WasmStackFrame();
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  friend class FrameArrayIterator;
  friend class AsmJsWasmStackFrame;
};

class AsmJsWasmStackFrame : public WasmStackFrame {
 public:
  virtual ~AsmJsWasmStackFrame() {}

  Handle<Object> GetReceiver() const override;
  Handle<Object> GetFunction() const override;

  Handle<Object> GetFileName() override;
  Handle<Object> GetScriptNameOrSourceUrl() override;

  int GetPosition() const override;
  int GetLineNumber() override;
  int GetColumnNumber() override;

  MaybeHandle<String> ToString() override;

 private:
  friend class FrameArrayIterator;
  AsmJsWasmStackFrame();
  void FromFrameArray(Isolate* isolate, Handle<FrameArray> array, int frame_ix);

  bool is_at_number_conversion_;
};

class FrameArrayIterator {
 public:
  FrameArrayIterator(Isolate* isolate, Handle<FrameArray> array,
                     int frame_ix = 0);

  StackFrameBase* Frame();

  bool HasNext() const;
  void Next();

 private:
  Isolate* isolate_;

  Handle<FrameArray> array_;
  int next_frame_ix_;

  WasmStackFrame wasm_frame_;
  AsmJsWasmStackFrame asm_wasm_frame_;
  JSStackFrame js_frame_;
};

// Determines how stack trace collection skips frames.
enum FrameSkipMode {
  // Unconditionally skips the first frame. Used e.g. when the Error constructor
  // is called, in which case the first frame is always a BUILTIN_EXIT frame.
  SKIP_FIRST,
  // Skip all frames until a specified caller function is seen.
  SKIP_UNTIL_SEEN,
  SKIP_NONE,
};

class ErrorUtils : public AllStatic {
 public:
  static MaybeHandle<Object> Construct(
      Isolate* isolate, Handle<JSFunction> target, Handle<Object> new_target,
      Handle<Object> message, FrameSkipMode mode, Handle<Object> caller,
      bool suppress_detailed_trace);

  static MaybeHandle<String> ToString(Isolate* isolate, Handle<Object> recv);

  static MaybeHandle<Object> MakeGenericError(
      Isolate* isolate, Handle<JSFunction> constructor, int template_index,
      Handle<Object> arg0, Handle<Object> arg1, Handle<Object> arg2,
      FrameSkipMode mode);

  // Formats a textual stack trace from the given structured stack trace.
  // Note that this can call arbitrary JS code through Error.prepareStackTrace.
  static MaybeHandle<Object> FormatStackTrace(Isolate* isolate,
                                              Handle<JSObject> error,
                                              Handle<Object> stack_trace);
};

#define MESSAGE_TEMPLATES(T)                                                   \
  /* Error */                                                                  \
  T(None, "")                                                                  \
  T(CyclicProto, "Cyclic __proto__ value")                                     \
  T(Debugger, "Debugger: %")                                                   \
  T(DebuggerLoading, "Error loading debugger")                                 \
  T(DefaultOptionsMissing, "Internal % error. Default options are missing.")   \
  T(UncaughtException, "Uncaught %")                                           \
  T(Unsupported, "Not supported")                                              \
  T(WrongServiceType, "Internal error, wrong service type: %")                 \
  T(WrongValueType, "Internal error. Wrong value type.")                       \
  /* TypeError */                                                              \
  T(ApplyNonFunction,                                                          \
    "Function.prototype.apply was called on %, which is a % and not a "        \
    "function")                                                                \
  T(ArgumentsDisallowedInInitializer,                                          \
    "'arguments' is not allowed in class field initializer")                   \
  T(ArrayBufferTooShort,                                                       \
    "Derived ArrayBuffer constructor created a buffer which was too small")    \
  T(ArrayBufferSpeciesThis,                                                    \
    "ArrayBuffer subclass returned this from species constructor")             \
  T(ArrayFunctionsOnFrozen, "Cannot modify frozen array elements")             \
  T(ArrayFunctionsOnSealed, "Cannot add/remove sealed array elements")         \
  T(AwaitNotInAsyncFunction, "await is only valid in async function")          \
  T(AtomicsWaitNotAllowed, "Atomics.wait cannot be called in this context")    \
  T(BadSortComparisonFunction,                                                 \
    "The comparison function must be either a function or undefined")          \
  T(BigIntFromNumber,                                                          \
    "The number % is not a safe integer and thus cannot be converted to a "    \
    "BigInt")                                                                  \
  T(BigIntFromObject, "Cannot convert % to a BigInt")                          \
  T(BigIntMixedTypes,                                                          \
    "Cannot mix BigInt and other types, use explicit conversions")             \
  T(BigIntSerializeJSON, "Do not know how to serialize a BigInt")              \
  T(BigIntShr, "BigInts have no unsigned right shift, use >> instead")         \
  T(BigIntToNumber, "Cannot convert a BigInt value to a number")               \
  T(CalledNonCallable, "% is not a function")                                  \
  T(CalledOnNonObject, "% called on non-object")                               \
  T(CalledOnNullOrUndefined, "% called on null or undefined")                  \
  T(CallSiteExpectsFunction,                                                   \
    "CallSite expects wasm object as first or function as second argument, "   \
    "got <%, %>")                                                              \
  T(CallSiteMethod, "CallSite method % expects CallSite as receiver")          \
  T(CannotConvertToPrimitive, "Cannot convert object to primitive value")      \
  T(CannotPreventExt, "Cannot prevent extensions")                             \
  T(CannotFreeze, "Cannot freeze")                                             \
  T(CannotFreezeArrayBufferView,                                               \
    "Cannot freeze array buffer views with elements")                          \
  T(CannotSeal, "Cannot seal")                                                 \
  T(CircularStructure, "Converting circular structure to JSON")                \
  T(ConstructAbstractClass, "Abstract class % not directly constructable")     \
  T(ConstAssign, "Assignment to constant variable.")                           \
  T(ConstructorClassField, "Classes may not have a field named 'constructor'") \
  T(ConstructorNonCallable,                                                    \
    "Class constructor % cannot be invoked without 'new'")                     \
  T(ConstructorNotFunction, "Constructor % requires 'new'")                    \
  T(ConstructorNotReceiver, "The .constructor property is not an object")      \
  T(CurrencyCode, "Currency code is required with currency style.")            \
  T(CyclicModuleDependency, "Detected cycle while resolving name '%' in '%'")  \
  T(DataViewNotArrayBuffer,                                                    \
    "First argument to DataView constructor must be an ArrayBuffer")           \
  T(DateType, "this is not a Date object.")                                    \
  T(DebuggerFrame, "Debugger: Invalid frame index.")                           \
  T(DebuggerType, "Debugger: Parameters have wrong types.")                    \
  T(DeclarationMissingInitializer, "Missing initializer in % declaration")     \
  T(DefineDisallowed, "Cannot define property %, object is not extensible")    \
  T(DetachedOperation, "Cannot perform % on a detached ArrayBuffer")           \
  T(DuplicateTemplateProperty, "Object template has duplicate property '%'")   \
  T(ExtendsValueNotConstructor,                                                \
    "Class extends value % is not a constructor or null")                      \
  T(FirstArgumentNotRegExp,                                                    \
    "First argument to % must not be a regular expression")                    \
  T(FunctionBind, "Bind must be called on a function")                         \
  T(GeneratorRunning, "Generator is already running")                          \
  T(IllegalInvocation, "Illegal invocation")                                   \
  T(ImmutablePrototypeSet,                                                     \
    "Immutable prototype object '%' cannot have their prototype set")          \
  T(ImportCallNotNewExpression, "Cannot use new with import")                  \
  T(ImportMetaOutsideModule, "Cannot use 'import.meta' outside a module")      \
  T(ImportMissingSpecifier, "import() requires a specifier")                   \
  T(IncompatibleMethodReceiver, "Method % called on incompatible receiver %")  \
  T(InstanceofNonobjectProto,                                                  \
    "Function has non-object prototype '%' in instanceof check")               \
  T(InvalidArgument, "invalid_argument")                                       \
  T(InvalidInOperatorUse, "Cannot use 'in' operator to search for '%' in %")   \
  T(InvalidRegExpExecResult,                                                   \
    "RegExp exec method returned something other than an Object or null")      \
  T(IteratorResultNotAnObject, "Iterator result % is not an object")           \
  T(IteratorValueNotAnObject, "Iterator value % is not an entry object")       \
  T(LanguageID, "Language ID should be string or object.")                     \
  T(MethodCalledOnWrongObject,                                                 \
    "Method % called on a non-object or on a wrong type of object.")           \
  T(MethodInvokedOnNullOrUndefined,                                            \
    "Method invoked on undefined or null value.")                              \
  T(MethodInvokedOnWrongType, "Method invoked on an object that is not %.")    \
  T(NoAccess, "no access")                                                     \
  T(NonCallableInInstanceOfCheck,                                              \
    "Right-hand side of 'instanceof' is not callable")                         \
  T(NonCoercible, "Cannot destructure 'undefined' or 'null'.")                 \
  T(NonCoercibleWithProperty,                                                  \
    "Cannot destructure property `%` of 'undefined' or 'null'.")               \
  T(NonExtensibleProto, "% is not extensible")                                 \
  T(NonObjectInInstanceOfCheck,                                                \
    "Right-hand side of 'instanceof' is not an object")                        \
  T(NonObjectPropertyLoad, "Cannot read property '%' of %")                    \
  T(NonObjectPropertyStore, "Cannot set property '%' of %")                    \
  T(NoSetterInCallback, "Cannot set property % of % which has only a getter")  \
  T(NotAnIterator, "% is not an iterator")                                     \
  T(NotAPromise, "% is not a promise")                                         \
  T(NotConstructor, "% is not a constructor")                                  \
  T(NotDateObject, "this is not a Date object.")                               \
  T(NotGeneric, "% requires that 'this' be a %")                               \
  T(NotCallableOrIterable,                                                     \
    "% is not a function or its return value is not iterable")                 \
  T(NotCallableOrAsyncIterable,                                                \
    "% is not a function or its return value is not async iterable")           \
  T(NotIterable, "% is not iterable")                                          \
  T(NotAsyncIterable, "% is not async iterable")                               \
  T(NotPropertyName, "% is not a valid property name")                         \
  T(NotTypedArray, "this is not a typed array.")                               \
  T(NotSuperConstructor, "Super constructor % of % is not a constructor")      \
  T(NotSuperConstructorAnonymousClass,                                         \
    "Super constructor % of anonymous class is not a constructor")             \
  T(NotIntegerSharedTypedArray, "% is not an integer shared typed array.")     \
  T(NotInt32SharedTypedArray, "% is not an int32 shared typed array.")         \
  T(ObjectGetterExpectingFunction,                                             \
    "Object.prototype.__defineGetter__: Expecting function")                   \
  T(ObjectGetterCallable, "Getter must be a function: %")                      \
  T(ObjectNotExtensible, "Cannot add property %, object is not extensible")    \
  T(ObjectSetterExpectingFunction,                                             \
    "Object.prototype.__defineSetter__: Expecting function")                   \
  T(ObjectSetterCallable, "Setter must be a function: %")                      \
  T(OrdinaryFunctionCalledAsConstructor,                                       \
    "Function object that's not a constructor was created with new")           \
  T(PromiseCyclic, "Chaining cycle detected for promise %")                    \
  T(PromiseExecutorAlreadyInvoked,                                             \
    "Promise executor has already been invoked with non-undefined arguments")  \
  T(PromiseNonCallable, "Promise resolve or reject function is not callable")  \
  T(PropertyDescObject, "Property description must be an object: %")           \
  T(PropertyNotFunction,                                                       \
    "'%' returned for property '%' of object '%' is not a function")           \
  T(ProtoObjectOrNull, "Object prototype may only be an Object or null: %")    \
  T(PrototypeParentNotAnObject,                                                \
    "Class extends value does not have valid prototype property %")            \
  T(ProxyConstructNonObject,                                                   \
    "'construct' on proxy: trap returned non-object ('%')")                    \
  T(ProxyDefinePropertyNonConfigurable,                                        \
    "'defineProperty' on proxy: trap returned truish for defining "            \
    "non-configurable property '%' which is either non-existant or "           \
    "configurable in the proxy target")                                        \
  T(ProxyDefinePropertyNonExtensible,                                          \
    "'defineProperty' on proxy: trap returned truish for adding property '%' " \
    " to the non-extensible proxy target")                                     \
  T(ProxyDefinePropertyIncompatible,                                           \
    "'defineProperty' on proxy: trap returned truish for adding property '%' " \
    " that is incompatible with the existing property in the proxy target")    \
  T(ProxyDeletePropertyNonConfigurable,                                        \
    "'deleteProperty' on proxy: trap returned truish for property '%' which "  \
    "is non-configurable in the proxy target")                                 \
  T(ProxyGetNonConfigurableData,                                               \
    "'get' on proxy: property '%' is a read-only and "                         \
    "non-configurable data property on the proxy target but the proxy "        \
    "did not return its actual value (expected '%' but got '%')")              \
  T(ProxyGetNonConfigurableAccessor,                                           \
    "'get' on proxy: property '%' is a non-configurable accessor "             \
    "property on the proxy target and does not have a getter function, but "   \
    "the trap did not return 'undefined' (got '%')")                           \
  T(ProxyGetOwnPropertyDescriptorIncompatible,                                 \
    "'getOwnPropertyDescriptor' on proxy: trap returned descriptor for "       \
    "property '%' that is incompatible with the existing property in the "     \
    "proxy target")                                                            \
  T(ProxyGetOwnPropertyDescriptorInvalid,                                      \
    "'getOwnPropertyDescriptor' on proxy: trap returned neither object nor "   \
    "undefined for property '%'")                                              \
  T(ProxyGetOwnPropertyDescriptorNonConfigurable,                              \
    "'getOwnPropertyDescriptor' on proxy: trap reported non-configurability "  \
    "for property '%' which is either non-existant or configurable in the "    \
    "proxy target")                                                            \
  T(ProxyGetOwnPropertyDescriptorNonExtensible,                                \
    "'getOwnPropertyDescriptor' on proxy: trap returned undefined for "        \
    "property '%' which exists in the non-extensible proxy target")            \
  T(ProxyGetOwnPropertyDescriptorUndefined,                                    \
    "'getOwnPropertyDescriptor' on proxy: trap returned undefined for "        \
    "property '%' which is non-configurable in the proxy target")              \
  T(ProxyGetPrototypeOfInvalid,                                                \
    "'getPrototypeOf' on proxy: trap returned neither object nor null")        \
  T(ProxyGetPrototypeOfNonExtensible,                                          \
    "'getPrototypeOf' on proxy: proxy target is non-extensible but the "       \
    "trap did not return its actual prototype")                                \
  T(ProxyHandlerOrTargetRevoked,                                               \
    "Cannot create proxy with a revoked proxy as target or handler")           \
  T(ProxyHasNonConfigurable,                                                   \
    "'has' on proxy: trap returned falsish for property '%' which exists in "  \
    "the proxy target as non-configurable")                                    \
  T(ProxyHasNonExtensible,                                                     \
    "'has' on proxy: trap returned falsish for property '%' but the proxy "    \
    "target is not extensible")                                                \
  T(ProxyIsExtensibleInconsistent,                                             \
    "'isExtensible' on proxy: trap result does not reflect extensibility of "  \
    "proxy target (which is '%')")                                             \
  T(ProxyNonObject,                                                            \
    "Cannot create proxy with a non-object as target or handler")              \
  T(ProxyOwnKeysMissing,                                                       \
    "'ownKeys' on proxy: trap result did not include '%'")                     \
  T(ProxyOwnKeysNonExtensible,                                                 \
    "'ownKeys' on proxy: trap returned extra keys but proxy target is "        \
    "non-extensible")                                                          \
  T(ProxyPreventExtensionsExtensible,                                          \
    "'preventExtensions' on proxy: trap returned truish but the proxy target " \
    "is extensible")                                                           \
  T(ProxyPrivate, "Cannot pass private property name to proxy trap")           \
  T(ProxyRevoked, "Cannot perform '%' on a proxy that has been revoked")       \
  T(ProxySetFrozenData,                                                        \
    "'set' on proxy: trap returned truish for property '%' which exists in "   \
    "the proxy target as a non-configurable and non-writable data property "   \
    "with a different value")                                                  \
  T(ProxySetFrozenAccessor,                                                    \
    "'set' on proxy: trap returned truish for property '%' which exists in "   \
    "the proxy target as a non-configurable and non-writable accessor "        \
    "property without a setter")                                               \
  T(ProxySetPrototypeOfNonExtensible,                                          \
    "'setPrototypeOf' on proxy: trap returned truish for setting a new "       \
    "prototype on the non-extensible proxy target")                            \
  T(ProxyTrapReturnedFalsish, "'%' on proxy: trap returned falsish")           \
  T(ProxyTrapReturnedFalsishFor,                                               \
    "'%' on proxy: trap returned falsish for property '%'")                    \
  T(RedefineDisallowed, "Cannot redefine property: %")                         \
  T(RedefineExternalArray,                                                     \
    "Cannot redefine a property of an object with external array elements")    \
  T(ReduceNoInitial, "Reduce of empty array with no initial value")            \
  T(RegExpFlags,                                                               \
    "Cannot supply flags when constructing one RegExp from another")           \
  T(RegExpNonObject, "% getter called on non-object %")                        \
  T(RegExpNonRegExp, "% getter called on non-RegExp object")                   \
  T(ResolverNotAFunction, "Promise resolver % is not a function")              \
  T(ReturnMethodNotCallable, "The iterator's 'return' method is not callable") \
  T(SharedArrayBufferTooShort,                                                 \
    "Derived SharedArrayBuffer constructor created a buffer which was too "    \
    "small")                                                                   \
  T(SharedArrayBufferSpeciesThis,                                              \
    "SharedArrayBuffer subclass returned this from species constructor")       \
  T(StaticPrototype,                                                           \
    "Classes may not have a static property named 'prototype'")                \
  T(StrictDeleteProperty, "Cannot delete property '%' of %")                   \
  T(StrictPoisonPill,                                                          \
    "'caller', 'callee', and 'arguments' properties may not be accessed on "   \
    "strict mode functions or the arguments objects for calls to them")        \
  T(StrictReadOnlyProperty,                                                    \
    "Cannot assign to read only property '%' of % '%'")                        \
  T(StrictCannotCreateProperty, "Cannot create property '%' on % '%'")         \
  T(SymbolIteratorInvalid,                                                     \
    "Result of the Symbol.iterator method is not an object")                   \
  T(SymbolAsyncIteratorInvalid,                                                \
    "Result of the Symbol.asyncIterator method is not an object")              \
  T(SymbolKeyFor, "% is not a symbol")                                         \
  T(SymbolToNumber, "Cannot convert a Symbol value to a number")               \
  T(SymbolToString, "Cannot convert a Symbol value to a string")               \
  T(ThrowMethodMissing, "The iterator does not provide a 'throw' method.")     \
  T(UndefinedOrNullToObject, "Cannot convert undefined or null to object")     \
  T(ValueAndAccessor,                                                          \
    "Invalid property descriptor. Cannot both specify accessors and a value "  \
    "or writable attribute, %")                                                \
  T(VarRedeclaration, "Identifier '%' has already been declared")              \
  T(WrongArgs, "%: Arguments list has wrong type")                             \
  /* ReferenceError */                                                         \
  T(NotDefined, "% is not defined")                                            \
  T(SuperAlreadyCalled, "Super constructor may only be called once")           \
  T(UnsupportedSuper, "Unsupported reference to 'super'")                      \
  /* RangeError */                                                             \
  T(BigIntDivZero, "Division by zero")                                         \
  T(BigIntNegativeExponent, "Exponent must be positive")                       \
  T(BigIntTooBig, "Maximum BigInt size exceeded")                              \
  T(DateRange, "Provided date is not in valid range.")                         \
  T(ExpectedTimezoneID,                                                        \
    "Expected Area/Location(/Location)* for time zone, got %")                 \
  T(ExpectedLocation,                                                          \
    "Expected letters optionally connected with underscores or hyphens for "   \
    "a location, got %")                                                       \
  T(InvalidArrayBufferLength, "Invalid array buffer length")                   \
  T(ArrayBufferAllocationFailed, "Array buffer allocation failed")             \
  T(InvalidArrayLength, "Invalid array length")                                \
  T(InvalidAtomicAccessIndex, "Invalid atomic access index")                   \
  T(InvalidCodePoint, "Invalid code point %")                                  \
  T(InvalidCountValue, "Invalid count value")                                  \
  T(InvalidCurrencyCode, "Invalid currency code: %")                           \
  T(InvalidDataViewAccessorOffset,                                             \
    "Offset is outside the bounds of the DataView")                            \
  T(InvalidDataViewLength, "Invalid DataView length %")                        \
  T(InvalidOffset, "Start offset % is outside the bounds of the buffer")       \
  T(InvalidHint, "Invalid hint: %")                                            \
  T(InvalidIndex, "Invalid value: not (convertible to) a safe integer")        \
  T(InvalidLanguageTag, "Invalid language tag: %")                             \
  T(InvalidWeakMapKey, "Invalid value used as weak map key")                   \
  T(InvalidWeakSetValue, "Invalid value used in weak set")                     \
  T(InvalidStringLength, "Invalid string length")                              \
  T(InvalidTimeValue, "Invalid time value")                                    \
  T(InvalidTypedArrayAlignment, "% of % should be a multiple of %")            \
  T(InvalidTypedArrayIndex, "Invalid typed array index")                       \
  T(InvalidTypedArrayLength, "Invalid typed array length: %")                  \
  T(LetInLexicalBinding, "let is disallowed as a lexically bound name")        \
  T(LocaleMatcher, "Illegal value for localeMatcher:%")                        \
  T(NormalizationForm, "The normalization form should be one of %.")           \
  T(NumberFormatRange, "% argument must be between 0 and 100")                 \
  T(PropertyValueOutOfRange, "% value is out of range.")                       \
  T(StackOverflow, "Maximum call stack size exceeded")                         \
  T(ToPrecisionFormatRange,                                                    \
    "toPrecision() argument must be between 1 and 100")                        \
  T(ToRadixFormatRange, "toString() radix argument must be between 2 and 36")  \
  T(TypedArraySetOffsetOutOfBounds, "offset is out of bounds")                 \
  T(TypedArraySetSourceTooLarge, "Source is too large")                        \
  T(UnsupportedTimeZone, "Unsupported time zone specified %")                  \
  T(ValueOutOfRange, "Value % out of range for % options property %")          \
  /* SyntaxError */                                                            \
  T(AmbiguousExport,                                                           \
    "The requested module '%' contains conflicting star exports for name '%'") \
  T(BadGetterArity, "Getter must not have any formal parameters.")             \
  T(BadSetterArity, "Setter must have exactly one formal parameter.")          \
  T(BigIntInvalidString, "Invalid BigInt string")                              \
  T(ConstructorIsAccessor, "Class constructor may not be an accessor")         \
  T(ConstructorIsGenerator, "Class constructor may not be a generator")        \
  T(ConstructorIsAsync, "Class constructor may not be an async method")        \
  T(ClassConstructorReturnedNonObject,                                         \
    "Class constructors may only return object or undefined")                  \
  T(DerivedConstructorReturnedNonObject,                                       \
    "Derived constructors may only return object or undefined")                \
  T(DuplicateConstructor, "A class may only have one constructor")             \
  T(DuplicateExport, "Duplicate export of '%'")                                \
  T(DuplicateProto,                                                            \
    "Duplicate __proto__ fields are not allowed in object literals")           \
  T(ForInOfLoopInitializer,                                                    \
    "% loop variable declaration may not have an initializer.")                \
  T(ForInOfLoopMultiBindings,                                                  \
    "Invalid left-hand side in % loop: Must have a single binding.")           \
  T(GeneratorInSingleStatementContext,                                         \
    "Generators can only be declared at the top level or inside a block.")     \
  T(AsyncFunctionInSingleStatementContext,                                     \
    "Async functions can only be declared at the top level or inside a "       \
    "block.")                                                                  \
  T(IllegalBreak, "Illegal break statement")                                   \
  T(NoIterationStatement,                                                      \
    "Illegal continue statement: no surrounding iteration statement")          \
  T(IllegalContinue,                                                           \
    "Illegal continue statement: '%' does not denote an iteration statement")  \
  T(IllegalLanguageModeDirective,                                              \
    "Illegal '%' directive in function with non-simple parameter list")        \
  T(IllegalReturn, "Illegal return statement")                                 \
  T(InvalidRestBindingPattern,                                                 \
    "`...` must be followed by an identifier in declaration contexts")         \
  T(InvalidRestAssignmentPattern,                                              \
    "`...` must be followed by an assignable reference in assignment "         \
    "contexts")                                                                \
  T(InvalidEscapedReservedWord, "Keyword must not contain escaped characters") \
  T(InvalidEscapedMetaProperty, "'%' must not contain escaped characters")     \
  T(InvalidLhsInAssignment, "Invalid left-hand side in assignment")            \
  T(InvalidCoverInitializedName, "Invalid shorthand property initializer")     \
  T(InvalidDestructuringTarget, "Invalid destructuring assignment target")     \
  T(InvalidLhsInFor, "Invalid left-hand side in for-loop")                     \
  T(InvalidLhsInPostfixOp,                                                     \
    "Invalid left-hand side expression in postfix operation")                  \
  T(InvalidLhsInPrefixOp,                                                      \
    "Invalid left-hand side expression in prefix operation")                   \
  T(InvalidRegExpFlags, "Invalid flags supplied to RegExp constructor '%'")    \
  T(InvalidOrUnexpectedToken, "Invalid or unexpected token")                   \
  T(JsonParseUnexpectedEOS, "Unexpected end of JSON input")                    \
  T(JsonParseUnexpectedToken, "Unexpected token % in JSON at position %")      \
  T(JsonParseUnexpectedTokenNumber, "Unexpected number in JSON at position %") \
  T(JsonParseUnexpectedTokenString, "Unexpected string in JSON at position %") \
  T(LabelRedeclaration, "Label '%' has already been declared")                 \
  T(LabelledFunctionDeclaration,                                               \
    "Labelled function declaration not allowed as the body of a control flow " \
    "structure")                                                               \
  T(MalformedArrowFunParamList, "Malformed arrow function parameter list")     \
  T(MalformedRegExp, "Invalid regular expression: /%/: %")                     \
  T(MalformedRegExpFlags, "Invalid regular expression flags")                  \
  T(ModuleExportUndefined, "Export '%' is not defined in module")              \
  T(HtmlCommentInModule, "HTML comments are not allowed in modules")           \
  T(MultipleDefaultsInSwitch,                                                  \
    "More than one default clause in switch statement")                        \
  T(NewlineAfterThrow, "Illegal newline after throw")                          \
  T(NoCatchOrFinally, "Missing catch or finally after try")                    \
  T(NotIsvar, "builtin %%IS_VAR: not a variable")                              \
  T(ParamAfterRest, "Rest parameter must be last formal parameter")            \
  T(PushPastSafeLength,                                                        \
    "Pushing % elements on an array-like of length % "                         \
    "is disallowed, as the total surpasses 2**53-1")                           \
  T(ElementAfterRest, "Rest element must be last element")                     \
  T(BadSetterRestParameter,                                                    \
    "Setter function argument must not be a rest parameter")                   \
  T(ParamDupe, "Duplicate parameter name not allowed in this context")         \
  T(ParenthesisInArgString, "Function arg string contains parenthesis")        \
  T(ArgStringTerminatesParametersEarly,                                        \
    "Arg string terminates parameters early")                                  \
  T(UnexpectedEndOfArgString, "Unexpected end of arg string")                  \
  T(RestDefaultInitializer,                                                    \
    "Rest parameter may not have a default initializer")                       \
  T(RuntimeWrongNumArgs, "Runtime function given wrong number of arguments")   \
  T(SuperNotCalled,                                                            \
    "Must call super constructor in derived class before accessing 'this' or " \
    "returning from derived constructor")                                      \
  T(SingleFunctionLiteral, "Single function literal required")                 \
  T(SloppyFunction,                                                            \
    "In non-strict mode code, functions can only be declared at top level, "   \
    "inside a block, or as the body of an if statement.")                      \
  T(SpeciesNotConstructor,                                                     \
    "object.constructor[Symbol.species] is not a constructor")                 \
  T(StrictDelete, "Delete of an unqualified identifier in strict mode.")       \
  T(StrictEvalArguments, "Unexpected eval or arguments in strict mode")        \
  T(StrictFunction,                                                            \
    "In strict mode code, functions can only be declared at top level or "     \
    "inside a block.")                                                         \
  T(StrictOctalLiteral, "Octal literals are not allowed in strict mode.")      \
  T(StrictDecimalWithLeadingZero,                                              \
    "Decimals with leading zeros are not allowed in strict mode.")             \
  T(StrictOctalEscape,                                                         \
    "Octal escape sequences are not allowed in strict mode.")                  \
  T(StrictWith, "Strict mode code may not include a with statement")           \
  T(TemplateOctalLiteral,                                                      \
    "Octal escape sequences are not allowed in template strings.")             \
  T(ThisFormalParameter, "'this' is not a valid formal parameter name")        \
  T(AwaitBindingIdentifier,                                                    \
    "'await' is not a valid identifier name in an async function")             \
  T(AwaitExpressionFormalParameter,                                            \
    "Illegal await-expression in formal parameters of async function")         \
  T(TooManyArguments,                                                          \
    "Too many arguments in function call (only 65535 allowed)")                \
  T(TooManyParameters,                                                         \
    "Too many parameters in function definition (only 65535 allowed)")         \
  T(TooManySpreads,                                                            \
    "Literal containing too many nested spreads (up to 65534 allowed)")        \
  T(TooManyVariables, "Too many variables declared (only 4194303 allowed)")    \
  T(TypedArrayTooShort,                                                        \
    "Derived TypedArray constructor created an array which was too small")     \
  T(UnexpectedEOS, "Unexpected end of input")                                  \
  T(UnexpectedFunctionSent,                                                    \
    "function.sent expression is not allowed outside a generator")             \
  T(UnexpectedReserved, "Unexpected reserved word")                            \
  T(UnexpectedStrictReserved, "Unexpected strict mode reserved word")          \
  T(UnexpectedSuper, "'super' keyword unexpected here")                        \
  T(UnexpectedNewTarget, "new.target expression is not allowed here")          \
  T(UnexpectedTemplateString, "Unexpected template string")                    \
  T(UnexpectedToken, "Unexpected token %")                                     \
  T(UnexpectedTokenIdentifier, "Unexpected identifier")                        \
  T(UnexpectedTokenNumber, "Unexpected number")                                \
  T(UnexpectedTokenString, "Unexpected string")                                \
  T(UnexpectedTokenRegExp, "Unexpected regular expression")                    \
  T(UnexpectedLexicalDeclaration,                                              \
    "Lexical declaration cannot appear in a single-statement context")         \
  T(UnknownLabel, "Undefined label '%'")                                       \
  T(UnresolvableExport,                                                        \
    "The requested module '%' does not provide an export named '%'")           \
  T(UnterminatedArgList, "missing ) after argument list")                      \
  T(UnterminatedRegExp, "Invalid regular expression: missing /")               \
  T(UnterminatedTemplate, "Unterminated template literal")                     \
  T(UnterminatedTemplateExpr, "Missing } in template expression")              \
  T(FoundNonCallableHasInstance, "Found non-callable @@hasInstance")           \
  T(InvalidHexEscapeSequence, "Invalid hexadecimal escape sequence")           \
  T(InvalidUnicodeEscapeSequence, "Invalid Unicode escape sequence")           \
  T(UndefinedUnicodeCodePoint, "Undefined Unicode code-point")                 \
  T(YieldInParameter, "Yield expression not allowed in formal parameter")      \
  /* EvalError */                                                              \
  T(CodeGenFromStrings, "%")                                                   \
  T(NoSideEffectDebugEvaluate, "Possible side-effect in debug-evaluate")       \
  /* URIError */                                                               \
  T(URIMalformed, "URI malformed")                                             \
  /* Wasm errors (currently Error) */                                          \
  T(WasmTrapUnreachable, "unreachable")                                        \
  T(WasmTrapMemOutOfBounds, "memory access out of bounds")                     \
  T(WasmTrapDivByZero, "divide by zero")                                       \
  T(WasmTrapDivUnrepresentable, "divide result unrepresentable")               \
  T(WasmTrapRemByZero, "remainder by zero")                                    \
  T(WasmTrapFloatUnrepresentable, "integer result unrepresentable")            \
  T(WasmTrapFuncInvalid, "invalid function")                                   \
  T(WasmTrapFuncSigMismatch, "function signature mismatch")                    \
  T(WasmTrapInvalidIndex, "invalid index into function table")                 \
  T(WasmTrapTypeError, "invalid type")                                         \
  T(WasmExceptionError, "wasm exception")                                      \
  /* Asm.js validation related */                                              \
  T(AsmJsInvalid, "Invalid asm.js: %")                                         \
  T(AsmJsCompiled, "Converted asm.js to WebAssembly: %")                       \
  T(AsmJsInstantiated, "Instantiated asm.js: %")                               \
  T(AsmJsLinkingFailed, "Linking failure in asm.js: %")                        \
  /* DataCloneError messages */                                                \
  T(DataCloneError, "% could not be cloned.")                                  \
  T(DataCloneErrorOutOfMemory, "Data cannot be cloned, out of memory.")        \
  T(DataCloneErrorNeuteredArrayBuffer,                                         \
    "An ArrayBuffer is neutered and could not be cloned.")                     \
  T(DataCloneErrorSharedArrayBufferTransferred,                                \
    "A SharedArrayBuffer could not be cloned. SharedArrayBuffer must not be "  \
    "transferred.")                                                            \
  T(DataCloneDeserializationError, "Unable to deserialize cloned data.")       \
  T(DataCloneDeserializationVersionError,                                      \
    "Unable to deserialize cloned data due to invalid or unsupported "         \
    "version.")

class MessageTemplate {
 public:
  enum Template {
#define TEMPLATE(NAME, STRING) k##NAME,
    MESSAGE_TEMPLATES(TEMPLATE)
#undef TEMPLATE
        kLastMessage
  };

  static const char* TemplateString(int template_index);

  static MaybeHandle<String> FormatMessage(int template_index,
                                           Handle<String> arg0,
                                           Handle<String> arg1,
                                           Handle<String> arg2);

  static Handle<String> FormatMessage(Isolate* isolate, int template_index,
                                      Handle<Object> arg);
};


// A message handler is a convenience interface for accessing the list
// of message listeners registered in an environment
class MessageHandler {
 public:
  // Returns a message object for the API to use.
  static Handle<JSMessageObject> MakeMessageObject(
      Isolate* isolate, MessageTemplate::Template type,
      const MessageLocation* location, Handle<Object> argument,
      Handle<FixedArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  static void ReportMessage(Isolate* isolate, const MessageLocation* loc,
                            Handle<JSMessageObject> message);

  static void DefaultMessageReport(Isolate* isolate, const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static std::unique_ptr<char[]> GetLocalizedMessage(Isolate* isolate,
                                                     Handle<Object> data);

 private:
  static void ReportMessageNoExceptions(Isolate* isolate,
                                        const MessageLocation* loc,
                                        Handle<Object> message_obj,
                                        v8::Local<v8::Value> api_exception_obj);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_MESSAGES_H_
