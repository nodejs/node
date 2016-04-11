// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The infrastructure used for (localized) message reporting in V8.
//
// Note: there's a big unresolved issue about ownership of the data
// structures used by this framework.

#ifndef V8_MESSAGES_H_
#define V8_MESSAGES_H_

#include "src/base/smart-pointers.h"
#include "src/handles.h"
#include "src/list.h"

namespace v8 {
namespace internal {

// Forward declarations.
class JSMessageObject;
class LookupIterator;
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


class CallSite {
 public:
  CallSite(Isolate* isolate, Handle<JSObject> call_site_obj);

  Handle<Object> GetFileName();
  Handle<Object> GetFunctionName();
  Handle<Object> GetScriptNameOrSourceUrl();
  Handle<Object> GetMethodName();
  // Return 1-based line number, including line offset.
  int GetLineNumber();
  // Return 1-based column number, including column offset if first line.
  int GetColumnNumber();
  bool IsNative();
  bool IsToplevel();
  bool IsEval();
  bool IsConstructor();

  bool IsValid() { return !fun_.is_null(); }

 private:
  Isolate* isolate_;
  Handle<Object> receiver_;
  Handle<JSFunction> fun_;
  int32_t pos_;
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
  T(ArrayBufferTooShort,                                                       \
    "Derived ArrayBuffer constructor created a buffer which was too small")    \
  T(ArrayBufferSpeciesThis,                                                    \
    "ArrayBuffer subclass returned this from species constructor")             \
  T(ArrayFunctionsOnFrozen, "Cannot modify frozen array elements")             \
  T(ArrayFunctionsOnSealed, "Cannot add/remove sealed array elements")         \
  T(ArrayNotSubclassable, "Subclassing Arrays is not currently supported.")    \
  T(CalledNonCallable, "% is not a function")                                  \
  T(CalledOnNonObject, "% called on non-object")                               \
  T(CalledOnNullOrUndefined, "% called on null or undefined")                  \
  T(CallSiteExpectsFunction,                                                   \
    "CallSite expects function as second argument, got %")                     \
  T(CannotConvertToPrimitive, "Cannot convert object to primitive value")      \
  T(CannotPreventExt, "Cannot prevent extensions")                             \
  T(CannotFreezeArrayBufferView,                                               \
    "Cannot freeze array buffer views with elements")                          \
  T(CircularStructure, "Converting circular structure to JSON")                \
  T(ConstructAbstractClass, "Abstract class % not directly constructable")     \
  T(ConstAssign, "Assignment to constant variable.")                           \
  T(ConstructorNonCallable,                                                    \
    "Class constructor % cannot be invoked without 'new'")                     \
  T(ConstructorNotFunction, "Constructor % requires 'new'")                    \
  T(ConstructorNotReceiver, "The .constructor property is not an object")      \
  T(CurrencyCode, "Currency code is required with currency style.")            \
  T(DataViewNotArrayBuffer,                                                    \
    "First argument to DataView constructor must be an ArrayBuffer")           \
  T(DateType, "this is not a Date object.")                                    \
  T(DebuggerFrame, "Debugger: Invalid frame index.")                           \
  T(DebuggerType, "Debugger: Parameters have wrong types.")                    \
  T(DeclarationMissingInitializer, "Missing initializer in % declaration")     \
  T(DefineDisallowed, "Cannot define property:%, object is not extensible.")   \
  T(DuplicateTemplateProperty, "Object template has duplicate property '%'")   \
  T(ExtendsValueGenerator,                                                     \
    "Class extends value % may not be a generator function")                   \
  T(ExtendsValueNotFunction,                                                   \
    "Class extends value % is not a function or null")                         \
  T(FirstArgumentNotRegExp,                                                    \
    "First argument to % must not be a regular expression")                    \
  T(FunctionBind, "Bind must be called on a function")                         \
  T(GeneratorRunning, "Generator is already running")                          \
  T(IllegalInvocation, "Illegal invocation")                                   \
  T(IncompatibleMethodReceiver, "Method % called on incompatible receiver %")  \
  T(InstanceofFunctionExpected,                                                \
    "Expecting a function in instanceof check, but got %")                     \
  T(InstanceofNonobjectProto,                                                  \
    "Function has non-object prototype '%' in instanceof check")               \
  T(InvalidArgument, "invalid_argument")                                       \
  T(InvalidInOperatorUse, "Cannot use 'in' operator to search for '%' in %")   \
  T(InvalidSimdOperation, "% is not a valid type for this SIMD operation.")    \
  T(IteratorResultNotAnObject, "Iterator result % is not an object")           \
  T(IteratorValueNotAnObject, "Iterator value % is not an entry object")       \
  T(LanguageID, "Language ID should be string or object.")                     \
  T(MethodCalledOnWrongObject,                                                 \
    "Method % called on a non-object or on a wrong type of object.")           \
  T(MethodInvokedOnNullOrUndefined,                                            \
    "Method invoked on undefined or null value.")                              \
  T(MethodInvokedOnWrongType, "Method invoked on an object that is not %.")    \
  T(NoAccess, "no access")                                                     \
  T(NonCoercible, "Cannot match against 'undefined' or 'null'.")               \
  T(NonExtensibleProto, "% is not extensible")                                 \
  T(NonObjectPropertyLoad, "Cannot read property '%' of %")                    \
  T(NonObjectPropertyStore, "Cannot set property '%' of %")                    \
  T(NoSetterInCallback, "Cannot set property % of % which has only a getter")  \
  T(NotAnIterator, "% is not an iterator")                                     \
  T(NotAPromise, "% is not a promise")                                         \
  T(NotConstructor, "% is not a constructor")                                  \
  T(NotDateObject, "this is not a Date object.")                               \
  T(NotIntlObject, "% is not an i18n object.")                                 \
  T(NotGeneric, "% is not generic")                                            \
  T(NotIterable, "% is not iterable")                                          \
  T(NotPropertyName, "% is not a valid property name")                         \
  T(NotTypedArray, "this is not a typed array.")                               \
  T(NotSharedTypedArray, "% is not a shared typed array.")                     \
  T(NotIntegerSharedTypedArray, "% is not an integer shared typed array.")     \
  T(NotInt32SharedTypedArray, "% is not an int32 shared typed array.")         \
  T(ObjectGetterExpectingFunction,                                             \
    "Object.prototype.__defineGetter__: Expecting function")                   \
  T(ObjectGetterCallable, "Getter must be a function: %")                      \
  T(ObjectNotExtensible, "Can't add property %, object is not extensible")     \
  T(ObjectSetterExpectingFunction,                                             \
    "Object.prototype.__defineSetter__: Expecting function")                   \
  T(ObjectSetterCallable, "Setter must be a function: %")                      \
  T(ObserveCallbackFrozen,                                                     \
    "Object.observe cannot deliver to a frozen function object")               \
  T(ObserveGlobalProxy, "% cannot be called on the global proxy object")       \
  T(ObserveAccessChecked, "% cannot be called on access-checked objects")      \
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
  T(PromiseExecutorAlreadyInvoked,                                             \
    "Promise executor has already been invoked with non-undefined arguments")  \
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
  T(ProxyEnumerateNonObject, "'enumerate' on proxy: trap returned non-object") \
  T(ProxyEnumerateNonString,                                                   \
    "'enumerate' on proxy: trap result includes non-string")                   \
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
  T(ReadGlobalReferenceThroughProxy, "Trying to access '%' through proxy")     \
  T(RedefineDisallowed, "Cannot redefine property: %")                         \
  T(RedefineExternalArray,                                                     \
    "Cannot redefine a property of an object with external array elements")    \
  T(ReduceNoInitial, "Reduce of empty array with no initial value")            \
  T(RegExpFlags,                                                               \
    "Cannot supply flags when constructing one RegExp from another")           \
  T(RegExpNonObject, "% getter called on non-object %")                        \
  T(RegExpNonRegExp, "% getter called on non-RegExp object")                   \
  T(ReinitializeIntl, "Trying to re-initialize % object.")                     \
  T(ResolvedOptionsCalledOnNonObject,                                          \
    "resolvedOptions method called on a non-object or on a object that is "    \
    "not Intl.%.")                                                             \
  T(ResolverNotAFunction, "Promise resolver % is not a function")              \
  T(RestrictedFunctionProperties,                                              \
    "'caller' and 'arguments' are restricted function properties and cannot "  \
    "be accessed in this context.")                                            \
  T(StaticPrototype, "Classes may not have static property named prototype")   \
  T(StrictCannotAssign, "Cannot assign to read only '%' in strict mode")       \
  T(StrictDeleteProperty, "Cannot delete property '%' of %")                   \
  T(StrictPoisonPill,                                                          \
    "'caller', 'callee', and 'arguments' properties may not be accessed on "   \
    "strict mode functions or the arguments objects for calls to them")        \
  T(StrictReadOnlyProperty,                                                    \
    "Cannot assign to read only property '%' of % '%'")                        \
  T(StrictCannotCreateProperty, "Cannot create property '%' on % '%'")         \
  T(StrongArity,                                                               \
    "In strong mode, calling a function with too few arguments is deprecated") \
  T(StrongDeleteProperty,                                                      \
    "Deleting property '%' of strong object '%' is deprecated")                \
  T(StrongExtendNull, "In strong mode, classes extending null are deprecated") \
  T(StrongImplicitConversion,                                                  \
    "In strong mode, implicit conversions are deprecated")                     \
  T(StrongRedefineDisallowed,                                                  \
    "On strong object %, redefining writable, non-configurable property '%' "  \
    "to be non-writable is deprecated")                                        \
  T(StrongSetProto,                                                            \
    "On strong object %, redefining the internal prototype is deprecated")     \
  T(SymbolKeyFor, "% is not a symbol")                                         \
  T(SymbolToNumber, "Cannot convert a Symbol value to a number")               \
  T(SymbolToString, "Cannot convert a Symbol value to a string")               \
  T(SimdToNumber, "Cannot convert a SIMD value to a number")                   \
  T(UndefinedOrNullToObject, "Cannot convert undefined or null to object")     \
  T(ValueAndAccessor,                                                          \
    "Invalid property descriptor. Cannot both specify accessors and a value "  \
    "or writable attribute, %")                                                \
  T(VarRedeclaration, "Identifier '%' has already been declared")              \
  T(WrongArgs, "%: Arguments list has wrong type")                             \
  /* ReferenceError */                                                         \
  T(NonMethod, "'super' is referenced from non-method")                        \
  T(NotDefined, "% is not defined")                                            \
  T(StrongSuperCallMissing,                                                    \
    "In strong mode, invoking the super constructor in a subclass is "         \
    "required")                                                                \
  T(StrongUnboundGlobal,                                                       \
    "In strong mode, using an undeclared global variable '%' is not allowed")  \
  T(UnsupportedSuper, "Unsupported reference to 'super'")                      \
  /* RangeError */                                                             \
  T(DateRange, "Provided date is not in valid range.")                         \
  T(ExpectedTimezoneID,                                                        \
    "Expected Area/Location(/Location)* for time zone, got %")                 \
  T(ExpectedLocation,                                                          \
    "Expected letters optionally connected with underscores or hyphens for "   \
    "a location, got %")                                                       \
  T(InvalidArrayBufferLength, "Invalid array buffer length")                   \
  T(ArrayBufferAllocationFailed, "Array buffer allocation failed")             \
  T(InvalidArrayLength, "Invalid array length")                                \
  T(InvalidCodePoint, "Invalid code point %")                                  \
  T(InvalidCountValue, "Invalid count value")                                  \
  T(InvalidCurrencyCode, "Invalid currency code: %")                           \
  T(InvalidDataViewAccessorOffset,                                             \
    "Offset is outside the bounds of the DataView")                            \
  T(InvalidDataViewLength, "Invalid data view length")                         \
  T(InvalidDataViewOffset, "Start offset is outside the bounds of the buffer") \
  T(InvalidHint, "Invalid hint: %")                                            \
  T(InvalidLanguageTag, "Invalid language tag: %")                             \
  T(InvalidWeakMapKey, "Invalid value used as weak map key")                   \
  T(InvalidWeakSetValue, "Invalid value used in weak set")                     \
  T(InvalidStringLength, "Invalid string length")                              \
  T(InvalidTimeValue, "Invalid time value")                                    \
  T(InvalidTypedArrayAlignment, "% of % should be a multiple of %")            \
  T(InvalidTypedArrayLength, "Invalid typed array length")                     \
  T(InvalidTypedArrayOffset, "Start offset is too large:")                     \
  T(LetInLexicalBinding, "let is disallowed as a lexically bound name")        \
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
  T(BadGetterArity, "Getter must not have any formal parameters.")             \
  T(BadSetterArity, "Setter must have exactly one formal parameter.")          \
  T(ConstructorIsAccessor, "Class constructor may not be an accessor")         \
  T(ConstructorIsGenerator, "Class constructor may not be a generator")        \
  T(DerivedConstructorReturn,                                                  \
    "Derived constructors may only return object or undefined")                \
  T(DuplicateConstructor, "A class may only have one constructor")             \
  T(DuplicateExport, "Duplicate export of '%'")                                \
  T(DuplicateProto,                                                            \
    "Duplicate __proto__ fields are not allowed in object literals")           \
  T(ForInLoopInitializer,                                                      \
    "for-in loop variable declaration may not have an initializer.")           \
  T(ForInOfLoopMultiBindings,                                                  \
    "Invalid left-hand side in % loop: Must have a single binding.")           \
  T(ForOfLoopInitializer,                                                      \
    "for-of loop variable declaration may not have an initializer.")           \
  T(IllegalAccess, "Illegal access")                                           \
  T(IllegalBreak, "Illegal break statement")                                   \
  T(IllegalContinue, "Illegal continue statement")                             \
  T(IllegalLanguageModeDirective,                                              \
    "Illegal '%' directive in function with non-simple parameter list")        \
  T(IllegalReturn, "Illegal return statement")                                 \
  T(InvalidEscapedReservedWord, "Keyword must not contain escaped characters") \
  T(InvalidLhsInAssignment, "Invalid left-hand side in assignment")            \
  T(InvalidCoverInitializedName, "Invalid shorthand property initializer")     \
  T(InvalidDestructuringTarget, "Invalid destructuring assignment target")     \
  T(InvalidLhsInFor, "Invalid left-hand side in for-loop")                     \
  T(InvalidLhsInPostfixOp,                                                     \
    "Invalid left-hand side expression in postfix operation")                  \
  T(InvalidLhsInPrefixOp,                                                      \
    "Invalid left-hand side expression in prefix operation")                   \
  T(InvalidRegExpFlags, "Invalid flags supplied to RegExp constructor '%'")    \
  T(LabelRedeclaration, "Label '%' has already been declared")                 \
  T(MalformedArrowFunParamList, "Malformed arrow function parameter list")     \
  T(MalformedRegExp, "Invalid regular expression: /%/: %")                     \
  T(MalformedRegExpFlags, "Invalid regular expression flags")                  \
  T(ModuleExportUndefined, "Export '%' is not defined in module")              \
  T(MultipleDefaultsInSwitch,                                                  \
    "More than one default clause in switch statement")                        \
  T(NewlineAfterThrow, "Illegal newline after throw")                          \
  T(NoCatchOrFinally, "Missing catch or finally after try")                    \
  T(NotIsvar, "builtin %%IS_VAR: not a variable")                              \
  T(ParamAfterRest, "Rest parameter must be last formal parameter")            \
  T(PushPastSafeLength,                                                        \
    "Pushing % elements on an array-like of length % "                         \
    "is disallowed, as the total surpasses 2**53-1")                           \
  T(ElementAfterRest, "Rest element must be last element in array")            \
  T(BadSetterRestParameter,                                                    \
    "Setter function argument must not be a rest parameter")                   \
  T(ParamDupe, "Duplicate parameter name not allowed in this context")         \
  T(ParenthesisInArgString, "Function arg string contains parenthesis")        \
  T(SingleFunctionLiteral, "Single function literal required")                 \
  T(SloppyLexical,                                                             \
    "Block-scoped declarations (let, const, function, class) not yet "         \
    "supported outside strict mode")                                           \
  T(SpeciesNotConstructor,                                                     \
    "object.constructor[Symbol.species] is not a constructor")                 \
  T(StrictDelete, "Delete of an unqualified identifier in strict mode.")       \
  T(StrictEvalArguments, "Unexpected eval or arguments in strict mode")        \
  T(StrictFunction,                                                            \
    "In strict mode code, functions can only be declared at top level or "     \
    "immediately within another function.")                                    \
  T(StrictOctalLiteral, "Octal literals are not allowed in strict mode.")      \
  T(StrictWith, "Strict mode code may not include a with statement")           \
  T(StrongArguments,                                                           \
    "In strong mode, 'arguments' is deprecated, use '...args' instead")        \
  T(StrongConstructorDirective,                                                \
    "\"use strong\" directive is disallowed in class constructor body")        \
  T(StrongConstructorReturnMisplaced,                                          \
    "In strong mode, returning from a constructor before its super "           \
    "constructor invocation or all assignments to 'this' is deprecated")       \
  T(StrongConstructorReturnValue,                                              \
    "In strong mode, returning a value from a constructor is deprecated")      \
  T(StrongConstructorSuper,                                                    \
    "In strong mode, 'super' can only be used to invoke the super "            \
    "constructor, and cannot be nested inside another statement or "           \
    "expression")                                                              \
  T(StrongConstructorThis,                                                     \
    "In strong mode, 'this' can only be used to initialize properties, and "   \
    "cannot be nested inside another statement or expression")                 \
  T(StrongDelete,                                                              \
    "In strong mode, 'delete' is deprecated, use maps or sets instead")        \
  T(StrongDirectEval, "In strong mode, direct calls to eval are deprecated")   \
  T(StrongEllision,                                                            \
    "In strong mode, arrays with holes are deprecated, use maps instead")      \
  T(StrongEmpty,                                                               \
    "In strong mode, empty sub-statements are deprecated, make them explicit " \
    "with '{}' instead")                                                       \
  T(StrongEqual,                                                               \
    "In strong mode, '==' and '!=' are deprecated, use '===' and '!==' "       \
    "instead")                                                                 \
  T(StrongForIn,                                                               \
    "In strong mode, 'for'-'in' loops are deprecated, use 'for'-'of' instead") \
  T(StrongPropertyAccess,                                                      \
    "In strong mode, accessing missing property '%' of % is deprecated")       \
  T(StrongSuperCallDuplicate,                                                  \
    "In strong mode, invoking the super constructor multiple times is "        \
    "deprecated")                                                              \
  T(StrongSuperCallMisplaced,                                                  \
    "In strong mode, the super constructor must be invoked before any "        \
    "assignment to 'this'")                                                    \
  T(StrongSwitchFallthrough,                                                   \
    "In strong mode, switch fall-through is deprecated, terminate each case "  \
    "with 'break', 'continue', 'return' or 'throw'")                           \
  T(StrongUndefined,                                                           \
    "In strong mode, binding or assigning to 'undefined' is deprecated")       \
  T(StrongUseBeforeDeclaration,                                                \
    "In strong mode, declaring variable '%' before its use is required")       \
  T(StrongVar,                                                                 \
    "In strong mode, 'var' is deprecated, use 'let' or 'const' instead")       \
  T(TemplateOctalLiteral,                                                      \
    "Octal literals are not allowed in template strings.")                     \
  T(ThisFormalParameter, "'this' is not a valid formal parameter name")        \
  T(TooManyArguments,                                                          \
    "Too many arguments in function call (only 65535 allowed)")                \
  T(TooManyParameters,                                                         \
    "Too many parameters in function definition (only 65535 allowed)")         \
  T(TooManyVariables, "Too many variables declared (only 4194303 allowed)")    \
  T(TypedArrayTooShort,                                                        \
    "Derived TypedArray constructor created an array which was too small")     \
  T(UnexpectedEOS, "Unexpected end of input")                                  \
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
  T(UnknownLabel, "Undefined label '%'")                                       \
  T(UnterminatedArgList, "missing ) after argument list")                      \
  T(UnterminatedRegExp, "Invalid regular expression: missing /")               \
  T(UnterminatedTemplate, "Unterminated template literal")                     \
  T(UnterminatedTemplateExpr, "Missing } in template expression")              \
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
      MessageLocation* location, Handle<Object> argument,
      Handle<JSArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  static void ReportMessage(Isolate* isolate, MessageLocation* loc,
                            Handle<JSMessageObject> message);

  static void DefaultMessageReport(Isolate* isolate, const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static base::SmartArrayPointer<char> GetLocalizedMessage(Isolate* isolate,
                                                           Handle<Object> data);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_MESSAGES_H_
