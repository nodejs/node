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
  T(None, "")                                                                  \
  T(CyclicProto, "Cyclic __proto__ value")                                     \
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
  T(ArrayFunctionsOnFrozen, "Cannot modify frozen array elements")             \
  T(ArrayFunctionsOnSealed, "Cannot add/remove sealed array elements")         \
  T(ArrayNotSubclassable, "Subclassing Arrays is not currently supported.")    \
  T(CalledNonCallable, "% is not a function")                                  \
  T(CalledOnNonObject, "% called on non-object")                               \
  T(CalledOnNullOrUndefined, "% called on null or undefined")                  \
  T(CannotConvertToPrimitive, "Cannot convert object to primitive value")      \
  T(CannotPreventExtExternalArray,                                             \
    "Cannot prevent extension of an object with external array elements")      \
  T(CircularStructure, "Converting circular structure to JSON")                \
  T(ConstAssign, "Assignment to constant variable.")                           \
  T(ConstructorNonCallable,                                                    \
    "Class constructors cannot be invoked without 'new'")                      \
  T(ConstructorNotFunction, "Constructor % requires 'new'")                    \
  T(CurrencyCode, "Currency code is required with currency style.")            \
  T(DataViewNotArrayBuffer,                                                    \
    "First argument to DataView constructor must be an ArrayBuffer")           \
  T(DateType, "this is not a Date object.")                                    \
  T(DefineDisallowed, "Cannot define property:%, object is not extensible.")   \
  T(DuplicateTemplateProperty, "Object template has duplicate property '%'")   \
  T(ExtendsValueGenerator,                                                     \
    "Class extends value % may not be a generator function")                   \
  T(ExtendsValueNotFunction,                                                   \
    "Class extends value % is not a function or null")                         \
  T(FirstArgumentNotRegExp,                                                    \
    "First argument to % must not be a regular expression")                    \
  T(FlagsGetterNonObject,                                                      \
    "RegExp.prototype.flags getter called on non-object %")                    \
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
  T(NotTypedArray, "this is not a typed array.")                               \
  T(NotSharedTypedArray, "% is not a shared typed array.")                     \
  T(NotIntegerSharedTypedArray, "% is not an integer shared typed array.")     \
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
  T(PropertyDescObject, "Property description must be an object: %")           \
  T(PropertyNotFunction, "Property '%' of object % is not a function")         \
  T(ProtoObjectOrNull, "Object prototype may only be an Object or null: %")    \
  T(PrototypeParentNotAnObject,                                                \
    "Class extends value does not have valid prototype property %")            \
  T(ProxyHandlerDeleteFailed,                                                  \
    "Proxy handler % did not return a boolean value from 'delete' trap")       \
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
  T(RedefineExternalArray,                                                     \
    "Cannot redefine a property of an object with external array elements")    \
  T(ReduceNoInitial, "Reduce of empty array with no initial value")            \
  T(RegExpFlags,                                                               \
    "Cannot supply flags when constructing one RegExp from another")           \
  T(ReinitializeIntl, "Trying to re-initialize % object.")                     \
  T(ResolvedOptionsCalledOnNonObject,                                          \
    "resolvedOptions method called on a non-object or on a object that is "    \
    "not Intl.%.")                                                             \
  T(ResolverNotAFunction, "Promise resolver % is not a function")              \
  T(RestrictedFunctionProperties,                                              \
    "'caller' and 'arguments' are restricted function properties and cannot "  \
    "be accessed in this context.")                                            \
  T(StaticPrototype, "Classes may not have static property named prototype")   \
  T(StrictCannotAssign, "Cannot assign to read only '% in strict mode")        \
  T(StrictDeleteProperty, "Cannot delete property '%' of %")                   \
  T(StrictPoisonPill,                                                          \
    "'caller', 'callee', and 'arguments' properties may not be accessed on "   \
    "strict mode functions or the arguments objects for calls to them")        \
  T(StrictReadOnlyProperty, "Cannot assign to read only property '%' of %")    \
  T(StrongArity,                                                               \
    "In strong mode, calling a function with too few arguments is deprecated") \
  T(StrongDeleteProperty,                                                      \
    "Deleting property '%' of strong object '%' is deprecated")                \
  T(StrongImplicitConversion,                                                  \
    "In strong mode, implicit conversions are deprecated")                     \
  T(StrongRedefineDisallowed,                                                  \
    "On strong object %, redefining writable, non-configurable property '%' "  \
    "to be non-writable is deprecated")                                        \
  T(StrongSetProto,                                                            \
    "On strong object %, redefining the internal prototype is deprecated")     \
  T(SymbolKeyFor, "% is not a symbol")                                         \
  T(SymbolToPrimitive,                                                         \
    "Cannot convert a Symbol wrapper object to a primitive value")             \
  T(SymbolToNumber, "Cannot convert a Symbol value to a number")               \
  T(SymbolToString, "Cannot convert a Symbol value to a string")               \
  T(UndefinedOrNullToObject, "Cannot convert undefined or null to object")     \
  T(ValueAndAccessor,                                                          \
    "Invalid property.  A property cannot both have accessors and be "         \
    "writable or have a value, %")                                             \
  T(VarRedeclaration, "Identifier '%' has already been declared")              \
  T(WithExpression, "% has no properties")                                     \
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
  T(InvalidWeakMapKey, "Invalid value used as weak map key")                   \
  T(InvalidWeakSetValue, "Invalid value used in weak set")                     \
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
  T(BadGetterArity, "Getter must not have any formal parameters.")             \
  T(BadSetterArity, "Setter must have exactly one formal parameter.")          \
  T(ConstructorIsAccessor, "Class constructor may not be an accessor")         \
  T(ConstructorIsGenerator, "Class constructor may not be a generator")        \
  T(DerivedConstructorReturn,                                                  \
    "Derived constructors may only return object or undefined")                \
  T(DuplicateArrawFunFormalParam,                                              \
    "Arrow function may not have duplicate parameter names")                   \
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
  T(IllegalReturn, "Illegal return statement")                                 \
  T(InvalidLhsInAssignment, "Invalid left-hand side in assignment")            \
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
  T(MissingArrow,                                                              \
    "Expected () to start arrow function, but got '%' instead of '=>'")        \
  T(ModuleExportUndefined, "Export '%' is not defined in module")              \
  T(MultipleDefaultsInSwitch,                                                  \
    "More than one default clause in switch statement")                        \
  T(NewlineAfterThrow, "Illegal newline after throw")                          \
  T(NoCatchOrFinally, "Missing catch or finally after try")                    \
  T(NotIsvar, "builtin %%IS_VAR: not a variable")                              \
  T(ParamAfterRest, "Rest parameter must be last formal parameter")            \
  T(BadSetterRestParameter,                                                    \
    "Setter function argument must not be a rest parameter")                   \
  T(ParenthesisInArgString, "Function arg string contains parenthesis")        \
  T(SingleFunctionLiteral, "Single function literal required")                 \
  T(SloppyLexical,                                                             \
    "Block-scoped declarations (let, const, function, class) not yet "         \
    "supported outside strict mode")                                           \
  T(StrictDelete, "Delete of an unqualified identifier in strict mode.")       \
  T(StrictEvalArguments, "Unexpected eval or arguments in strict mode")        \
  T(StrictFunction,                                                            \
    "In strict mode code, functions can only be declared at top level or "     \
    "immediately within another function.")                                    \
  T(StrictOctalLiteral, "Octal literals are not allowed in strict mode.")      \
  T(StrictParamDupe,                                                           \
    "Strict mode function may not have duplicate parameter names")             \
  T(StrictWith, "Strict mode code may not include a with statement")           \
  T(StrongArguments,                                                           \
    "In strong mode, 'arguments' is deprecated, use '...args' instead")        \
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
      Isolate* isolate, MessageTemplate::Template type, MessageLocation* loc,
      Handle<Object> argument, Handle<JSArray> stack_frames);

  // Report a formatted message (needs JS allocation).
  static void ReportMessage(Isolate* isolate, MessageLocation* loc,
                            Handle<JSMessageObject> message);

  static void DefaultMessageReport(Isolate* isolate, const MessageLocation* loc,
                                   Handle<Object> message_obj);
  static Handle<String> GetMessage(Isolate* isolate, Handle<Object> data);
  static SmartArrayPointer<char> GetLocalizedMessage(Isolate* isolate,
                                                     Handle<Object> data);
};
} }  // namespace v8::internal

#endif  // V8_MESSAGES_H_
