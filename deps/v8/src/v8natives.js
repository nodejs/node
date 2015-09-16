// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $functionSourceString;
var $globalEval;
var $objectDefineOwnProperty;
var $objectGetOwnPropertyDescriptor;
var $toCompletePropertyDescriptor;

(function(global, utils) {

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalFunction = global.Function;
var GlobalNumber = global.Number;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;

var MathAbs;
var ProxyDelegateCallAndConstruct;
var ProxyDerivedHasOwnTrap;
var ProxyDerivedKeysTrap;
var StringIndexOf;

utils.Import(function(from) {
  MathAbs = from.MathAbs;
  StringIndexOf = from.StringIndexOf;
});

utils.ImportFromExperimental(function(from) {
  ProxyDelegateCallAndConstruct = from.ProxyDelegateCallAndConstruct;
  ProxyDerivedHasOwnTrap = from.ProxyDerivedHasOwnTrap;
  ProxyDerivedKeysTrap = from.ProxyDerivedKeysTrap;
});

// ----------------------------------------------------------------------------


// ECMA 262 - 15.1.4
function GlobalIsNaN(number) {
  number = TO_NUMBER_INLINE(number);
  return NUMBER_IS_NAN(number);
}


// ECMA 262 - 15.1.5
function GlobalIsFinite(number) {
  number = TO_NUMBER_INLINE(number);
  return NUMBER_IS_FINITE(number);
}


// ECMA-262 - 15.1.2.2
function GlobalParseInt(string, radix) {
  if (IS_UNDEFINED(radix) || radix === 10 || radix === 0) {
    // Some people use parseInt instead of Math.floor.  This
    // optimization makes parseInt on a Smi 12 times faster (60ns
    // vs 800ns).  The following optimization makes parseInt on a
    // non-Smi number 9 times faster (230ns vs 2070ns).  Together
    // they make parseInt on a string 1.4% slower (274ns vs 270ns).
    if (%_IsSmi(string)) return string;
    if (IS_NUMBER(string) &&
        ((0.01 < string && string < 1e9) ||
            (-1e9 < string && string < -0.01))) {
      // Truncate number.
      return string | 0;
    }
    string = TO_STRING_INLINE(string);
    radix = radix | 0;
  } else {
    // The spec says ToString should be evaluated before ToInt32.
    string = TO_STRING_INLINE(string);
    radix = TO_INT32(radix);
    if (!(radix == 0 || (2 <= radix && radix <= 36))) {
      return NAN;
    }
  }

  if (%_HasCachedArrayIndex(string) &&
      (radix == 0 || radix == 10)) {
    return %_GetCachedArrayIndex(string);
  }
  return %StringParseInt(string, radix);
}


// ECMA-262 - 15.1.2.3
function GlobalParseFloat(string) {
  string = TO_STRING_INLINE(string);
  if (%_HasCachedArrayIndex(string)) return %_GetCachedArrayIndex(string);
  return %StringParseFloat(string);
}


function GlobalEval(x) {
  if (!IS_STRING(x)) return x;

  var global_proxy = %GlobalProxy(GlobalEval);

  var f = %CompileString(x, false);
  if (!IS_FUNCTION(f)) return f;

  return %_CallFunction(global_proxy, f);
}


// ----------------------------------------------------------------------------

// Set up global object.
var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;

utils.InstallConstants(global, [
  // ECMA 262 - 15.1.1.1.
  "NaN", NAN,
  // ECMA-262 - 15.1.1.2.
  "Infinity", INFINITY,
  // ECMA-262 - 15.1.1.2.
  "undefined", UNDEFINED,
]);

// Set up non-enumerable function on the global object.
utils.InstallFunctions(global, DONT_ENUM, [
  "isNaN", GlobalIsNaN,
  "isFinite", GlobalIsFinite,
  "parseInt", GlobalParseInt,
  "parseFloat", GlobalParseFloat,
  "eval", GlobalEval
]);


// ----------------------------------------------------------------------------
// Object

// ECMA-262 - 15.2.4.2
function ObjectToString() {
  if (IS_UNDEFINED(this) && !IS_UNDETECTABLE(this)) return "[object Undefined]";
  if (IS_NULL(this)) return "[object Null]";
  var O = TO_OBJECT_INLINE(this);
  var builtinTag = %_ClassOf(O);
  var tag;

  // TODO(caitp): cannot wait to get rid of this flag :>
  if (harmony_tostring) {
    tag = O[symbolToStringTag];
    if (!IS_STRING(tag)) {
      tag = builtinTag;
    }
  } else {
    tag = builtinTag;
  }

  return `[object ${tag}]`;
}


// ECMA-262 - 15.2.4.3
function ObjectToLocaleString() {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.toLocaleString");
  return this.toString();
}


// ECMA-262 - 15.2.4.4
function ObjectValueOf() {
  return TO_OBJECT_INLINE(this);
}


// ECMA-262 - 15.2.4.5
function ObjectHasOwnProperty(value) {
  var name = $toName(value);
  var object = TO_OBJECT_INLINE(this);

  if (%_IsJSProxy(object)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(value)) return false;

    var handler = %GetHandler(object);
    return CallTrap1(handler, "hasOwn", ProxyDerivedHasOwnTrap, name);
  }
  return %HasOwnProperty(object, name);
}


// ECMA-262 - 15.2.4.6
function ObjectIsPrototypeOf(V) {
  if (!IS_SPEC_OBJECT(V)) return false;
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.isPrototypeOf");
  return %IsInPrototypeChain(this, V);
}


// ECMA-262 - 15.2.4.6
function ObjectPropertyIsEnumerable(V) {
  var P = $toName(V);
  if (%_IsJSProxy(this)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(V)) return false;

    var desc = GetOwnPropertyJS(this, P);
    return IS_UNDEFINED(desc) ? false : desc.isEnumerable();
  }
  return %IsPropertyEnumerable(TO_OBJECT_INLINE(this), P);
}


// Extensions for providing property getters and setters.
function ObjectDefineGetter(name, fun) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalProxy(ObjectDefineGetter);
  }
  if (!IS_SPEC_FUNCTION(fun)) {
    throw MakeTypeError(kObjectGetterExpectingFunction);
  }
  var desc = new PropertyDescriptor();
  desc.setGet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(TO_OBJECT_INLINE(receiver), $toName(name), desc, false);
}


function ObjectLookupGetter(name) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalProxy(ObjectLookupGetter);
  }
  return %LookupAccessor(TO_OBJECT_INLINE(receiver), $toName(name), GETTER);
}


function ObjectDefineSetter(name, fun) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalProxy(ObjectDefineSetter);
  }
  if (!IS_SPEC_FUNCTION(fun)) {
    throw MakeTypeError(kObjectSetterExpectingFunction);
  }
  var desc = new PropertyDescriptor();
  desc.setSet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(TO_OBJECT_INLINE(receiver), $toName(name), desc, false);
}


function ObjectLookupSetter(name) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalProxy(ObjectLookupSetter);
  }
  return %LookupAccessor(TO_OBJECT_INLINE(receiver), $toName(name), SETTER);
}


function ObjectKeys(obj) {
  obj = TO_OBJECT_INLINE(obj);
  if (%_IsJSProxy(obj)) {
    var handler = %GetHandler(obj);
    var names = CallTrap0(handler, "keys", ProxyDerivedKeysTrap);
    return ToNameArray(names, "keys", false);
  }
  return %OwnKeys(obj);
}


// ES5 8.10.1.
function IsAccessorDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return desc.hasGetter() || desc.hasSetter();
}


// ES5 8.10.2.
function IsDataDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return desc.hasValue() || desc.hasWritable();
}


// ES5 8.10.3.
function IsGenericDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return !(IsAccessorDescriptor(desc) || IsDataDescriptor(desc));
}


function IsInconsistentDescriptor(desc) {
  return IsAccessorDescriptor(desc) && IsDataDescriptor(desc);
}


// ES5 8.10.4
function FromPropertyDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return desc;

  if (IsDataDescriptor(desc)) {
    return { value: desc.getValue(),
             writable: desc.isWritable(),
             enumerable: desc.isEnumerable(),
             configurable: desc.isConfigurable() };
  }
  // Must be an AccessorDescriptor then. We never return a generic descriptor.
  return { get: desc.getGet(),
           set: desc.getSet(),
           enumerable: desc.isEnumerable(),
           configurable: desc.isConfigurable() };
}


// Harmony Proxies
function FromGenericPropertyDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return desc;
  var obj = new GlobalObject();

  if (desc.hasValue()) {
    %AddNamedProperty(obj, "value", desc.getValue(), NONE);
  }
  if (desc.hasWritable()) {
    %AddNamedProperty(obj, "writable", desc.isWritable(), NONE);
  }
  if (desc.hasGetter()) {
    %AddNamedProperty(obj, "get", desc.getGet(), NONE);
  }
  if (desc.hasSetter()) {
    %AddNamedProperty(obj, "set", desc.getSet(), NONE);
  }
  if (desc.hasEnumerable()) {
    %AddNamedProperty(obj, "enumerable", desc.isEnumerable(), NONE);
  }
  if (desc.hasConfigurable()) {
    %AddNamedProperty(obj, "configurable", desc.isConfigurable(), NONE);
  }
  return obj;
}


// ES5 8.10.5.
function ToPropertyDescriptor(obj) {
  if (!IS_SPEC_OBJECT(obj)) throw MakeTypeError(kPropertyDescObject, obj);

  var desc = new PropertyDescriptor();

  if ("enumerable" in obj) {
    desc.setEnumerable($toBoolean(obj.enumerable));
  }

  if ("configurable" in obj) {
    desc.setConfigurable($toBoolean(obj.configurable));
  }

  if ("value" in obj) {
    desc.setValue(obj.value);
  }

  if ("writable" in obj) {
    desc.setWritable($toBoolean(obj.writable));
  }

  if ("get" in obj) {
    var get = obj.get;
    if (!IS_UNDEFINED(get) && !IS_SPEC_FUNCTION(get)) {
      throw MakeTypeError(kObjectGetterCallable, get);
    }
    desc.setGet(get);
  }

  if ("set" in obj) {
    var set = obj.set;
    if (!IS_UNDEFINED(set) && !IS_SPEC_FUNCTION(set)) {
      throw MakeTypeError(kObjectSetterCallable, set);
    }
    desc.setSet(set);
  }

  if (IsInconsistentDescriptor(desc)) {
    throw MakeTypeError(kValueAndAccessor, obj);
  }
  return desc;
}


// For Harmony proxies.
function ToCompletePropertyDescriptor(obj) {
  var desc = ToPropertyDescriptor(obj);
  if (IsGenericDescriptor(desc) || IsDataDescriptor(desc)) {
    if (!desc.hasValue()) desc.setValue(UNDEFINED);
    if (!desc.hasWritable()) desc.setWritable(false);
  } else {
    // Is accessor descriptor.
    if (!desc.hasGetter()) desc.setGet(UNDEFINED);
    if (!desc.hasSetter()) desc.setSet(UNDEFINED);
  }
  if (!desc.hasEnumerable()) desc.setEnumerable(false);
  if (!desc.hasConfigurable()) desc.setConfigurable(false);
  return desc;
}


function PropertyDescriptor() {
  // Initialize here so they are all in-object and have the same map.
  // Default values from ES5 8.6.1.
  this.value_ = UNDEFINED;
  this.hasValue_ = false;
  this.writable_ = false;
  this.hasWritable_ = false;
  this.enumerable_ = false;
  this.hasEnumerable_ = false;
  this.configurable_ = false;
  this.hasConfigurable_ = false;
  this.get_ = UNDEFINED;
  this.hasGetter_ = false;
  this.set_ = UNDEFINED;
  this.hasSetter_ = false;
}

utils.SetUpLockedPrototype(PropertyDescriptor, [
  "value_",
  "hasValue_",
  "writable_",
  "hasWritable_",
  "enumerable_",
  "hasEnumerable_",
  "configurable_",
  "hasConfigurable_",
  "get_",
  "hasGetter_",
  "set_",
  "hasSetter_"
], [
  "toString", function PropertyDescriptor_ToString() {
    return "[object PropertyDescriptor]";
  },
  "setValue", function PropertyDescriptor_SetValue(value) {
    this.value_ = value;
    this.hasValue_ = true;
  },
  "getValue", function PropertyDescriptor_GetValue() {
    return this.value_;
  },
  "hasValue", function PropertyDescriptor_HasValue() {
    return this.hasValue_;
  },
  "setEnumerable", function PropertyDescriptor_SetEnumerable(enumerable) {
    this.enumerable_ = enumerable;
      this.hasEnumerable_ = true;
  },
  "isEnumerable", function PropertyDescriptor_IsEnumerable() {
    return this.enumerable_;
  },
  "hasEnumerable", function PropertyDescriptor_HasEnumerable() {
    return this.hasEnumerable_;
  },
  "setWritable", function PropertyDescriptor_SetWritable(writable) {
    this.writable_ = writable;
    this.hasWritable_ = true;
  },
  "isWritable", function PropertyDescriptor_IsWritable() {
    return this.writable_;
  },
  "hasWritable", function PropertyDescriptor_HasWritable() {
    return this.hasWritable_;
  },
  "setConfigurable",
  function PropertyDescriptor_SetConfigurable(configurable) {
    this.configurable_ = configurable;
    this.hasConfigurable_ = true;
  },
  "hasConfigurable", function PropertyDescriptor_HasConfigurable() {
    return this.hasConfigurable_;
  },
  "isConfigurable", function PropertyDescriptor_IsConfigurable() {
    return this.configurable_;
  },
  "setGet", function PropertyDescriptor_SetGetter(get) {
    this.get_ = get;
    this.hasGetter_ = true;
  },
  "getGet", function PropertyDescriptor_GetGetter() {
    return this.get_;
  },
  "hasGetter", function PropertyDescriptor_HasGetter() {
    return this.hasGetter_;
  },
  "setSet", function PropertyDescriptor_SetSetter(set) {
    this.set_ = set;
    this.hasSetter_ = true;
  },
  "getSet", function PropertyDescriptor_GetSetter() {
    return this.set_;
  },
  "hasSetter", function PropertyDescriptor_HasSetter() {
    return this.hasSetter_;
  }
]);


// Converts an array returned from Runtime_GetOwnProperty to an actual
// property descriptor. For a description of the array layout please
// see the runtime.cc file.
function ConvertDescriptorArrayToDescriptor(desc_array) {
  if (IS_UNDEFINED(desc_array)) {
    return UNDEFINED;
  }

  var desc = new PropertyDescriptor();
  // This is an accessor.
  if (desc_array[IS_ACCESSOR_INDEX]) {
    desc.setGet(desc_array[GETTER_INDEX]);
    desc.setSet(desc_array[SETTER_INDEX]);
  } else {
    desc.setValue(desc_array[VALUE_INDEX]);
    desc.setWritable(desc_array[WRITABLE_INDEX]);
  }
  desc.setEnumerable(desc_array[ENUMERABLE_INDEX]);
  desc.setConfigurable(desc_array[CONFIGURABLE_INDEX]);

  return desc;
}


// For Harmony proxies.
function GetTrap(handler, name, defaultTrap) {
  var trap = handler[name];
  if (IS_UNDEFINED(trap)) {
    if (IS_UNDEFINED(defaultTrap)) {
      throw MakeTypeError(kProxyHandlerTrapMissing, handler, name);
    }
    trap = defaultTrap;
  } else if (!IS_SPEC_FUNCTION(trap)) {
    throw MakeTypeError(kProxyHandlerTrapMustBeCallable, handler, name);
  }
  return trap;
}


function CallTrap0(handler, name, defaultTrap) {
  return %_CallFunction(handler, GetTrap(handler, name, defaultTrap));
}


function CallTrap1(handler, name, defaultTrap, x) {
  return %_CallFunction(handler, x, GetTrap(handler, name, defaultTrap));
}


function CallTrap2(handler, name, defaultTrap, x, y) {
  return %_CallFunction(handler, x, y, GetTrap(handler, name, defaultTrap));
}


// ES5 section 8.12.1.
function GetOwnPropertyJS(obj, v) {
  var p = $toName(v);
  if (%_IsJSProxy(obj)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(v)) return UNDEFINED;

    var handler = %GetHandler(obj);
    var descriptor = CallTrap1(
                         handler, "getOwnPropertyDescriptor", UNDEFINED, p);
    if (IS_UNDEFINED(descriptor)) return descriptor;
    var desc = ToCompletePropertyDescriptor(descriptor);
    if (!desc.isConfigurable()) {
      throw MakeTypeError(kProxyPropNotConfigurable,
                          handler, p, "getOwnPropertyDescriptor");
    }
    return desc;
  }

  // GetOwnProperty returns an array indexed by the constants
  // defined in macros.py.
  // If p is not a property on obj undefined is returned.
  var props = %GetOwnProperty(TO_OBJECT_INLINE(obj), p);

  return ConvertDescriptorArrayToDescriptor(props);
}


// ES5 section 8.12.7.
function Delete(obj, p, should_throw) {
  var desc = GetOwnPropertyJS(obj, p);
  if (IS_UNDEFINED(desc)) return true;
  if (desc.isConfigurable()) {
    %DeleteProperty(obj, p, 0);
    return true;
  } else if (should_throw) {
    throw MakeTypeError(kDefineDisallowed, p);
  } else {
    return;
  }
}


// ES6, draft 12-24-14, section 7.3.8
function GetMethod(obj, p) {
  var func = obj[p];
  if (IS_NULL_OR_UNDEFINED(func)) return UNDEFINED;
  if (IS_SPEC_FUNCTION(func)) return func;
  throw MakeTypeError(kCalledNonCallable, typeof func);
}


// Harmony proxies.
function DefineProxyProperty(obj, p, attributes, should_throw) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(p)) return false;

  var handler = %GetHandler(obj);
  var result = CallTrap2(handler, "defineProperty", UNDEFINED, p, attributes);
  if (!$toBoolean(result)) {
    if (should_throw) {
      throw MakeTypeError(kProxyHandlerReturned,
                          handler, "false", "defineProperty");
    } else {
      return false;
    }
  }
  return true;
}


// ES5 8.12.9.
function DefineObjectProperty(obj, p, desc, should_throw) {
  var current_array = %GetOwnProperty(obj, $toName(p));
  var current = ConvertDescriptorArrayToDescriptor(current_array);
  var extensible = %IsExtensible(obj);

  // Error handling according to spec.
  // Step 3
  if (IS_UNDEFINED(current) && !extensible) {
    if (should_throw) {
      throw MakeTypeError(kDefineDisallowed, p);
    } else {
      return false;
    }
  }

  if (!IS_UNDEFINED(current)) {
    // Step 5 and 6
    if ((IsGenericDescriptor(desc) ||
         IsDataDescriptor(desc) == IsDataDescriptor(current)) &&
        (!desc.hasEnumerable() ||
         $sameValue(desc.isEnumerable(), current.isEnumerable())) &&
        (!desc.hasConfigurable() ||
         $sameValue(desc.isConfigurable(), current.isConfigurable())) &&
        (!desc.hasWritable() ||
         $sameValue(desc.isWritable(), current.isWritable())) &&
        (!desc.hasValue() ||
         $sameValue(desc.getValue(), current.getValue())) &&
        (!desc.hasGetter() ||
         $sameValue(desc.getGet(), current.getGet())) &&
        (!desc.hasSetter() ||
         $sameValue(desc.getSet(), current.getSet()))) {
      return true;
    }
    if (!current.isConfigurable()) {
      // Step 7
      if (desc.isConfigurable() ||
          (desc.hasEnumerable() &&
           desc.isEnumerable() != current.isEnumerable())) {
        if (should_throw) {
          throw MakeTypeError(kRedefineDisallowed, p);
        } else {
          return false;
        }
      }
      // Step 8
      if (!IsGenericDescriptor(desc)) {
        // Step 9a
        if (IsDataDescriptor(current) != IsDataDescriptor(desc)) {
          if (should_throw) {
            throw MakeTypeError(kRedefineDisallowed, p);
          } else {
            return false;
          }
        }
        // Step 10a
        if (IsDataDescriptor(current) && IsDataDescriptor(desc)) {
          var currentIsWritable = current.isWritable();
          if (currentIsWritable != desc.isWritable()) {
            if (!currentIsWritable || IS_STRONG(obj)) {
              if (should_throw) {
                throw currentIsWritable
                    ? MakeTypeError(kStrongRedefineDisallowed, obj, p)
                    : MakeTypeError(kRedefineDisallowed, p);
              } else {
                return false;
              }
            }
          }
          if (!currentIsWritable && desc.hasValue() &&
              !$sameValue(desc.getValue(), current.getValue())) {
            if (should_throw) {
              throw MakeTypeError(kRedefineDisallowed, p);
            } else {
              return false;
            }
          }
        }
        // Step 11
        if (IsAccessorDescriptor(desc) && IsAccessorDescriptor(current)) {
          if (desc.hasSetter() &&
              !$sameValue(desc.getSet(), current.getSet())) {
            if (should_throw) {
              throw MakeTypeError(kRedefineDisallowed, p);
            } else {
              return false;
            }
          }
          if (desc.hasGetter() && !$sameValue(desc.getGet(),current.getGet())) {
            if (should_throw) {
              throw MakeTypeError(kRedefineDisallowed, p);
            } else {
              return false;
            }
          }
        }
      }
    }
  }

  // Send flags - enumerable and configurable are common - writable is
  // only send to the data descriptor.
  // Take special care if enumerable and configurable is not defined on
  // desc (we need to preserve the existing values from current).
  var flag = NONE;
  if (desc.hasEnumerable()) {
    flag |= desc.isEnumerable() ? 0 : DONT_ENUM;
  } else if (!IS_UNDEFINED(current)) {
    flag |= current.isEnumerable() ? 0 : DONT_ENUM;
  } else {
    flag |= DONT_ENUM;
  }

  if (desc.hasConfigurable()) {
    flag |= desc.isConfigurable() ? 0 : DONT_DELETE;
  } else if (!IS_UNDEFINED(current)) {
    flag |= current.isConfigurable() ? 0 : DONT_DELETE;
  } else
    flag |= DONT_DELETE;

  if (IsDataDescriptor(desc) ||
      (IsGenericDescriptor(desc) &&
       (IS_UNDEFINED(current) || IsDataDescriptor(current)))) {
    // There are 3 cases that lead here:
    // Step 4a - defining a new data property.
    // Steps 9b & 12 - replacing an existing accessor property with a data
    //                 property.
    // Step 12 - updating an existing data property with a data or generic
    //           descriptor.

    if (desc.hasWritable()) {
      flag |= desc.isWritable() ? 0 : READ_ONLY;
    } else if (!IS_UNDEFINED(current)) {
      flag |= current.isWritable() ? 0 : READ_ONLY;
    } else {
      flag |= READ_ONLY;
    }

    var value = UNDEFINED;  // Default value is undefined.
    if (desc.hasValue()) {
      value = desc.getValue();
    } else if (!IS_UNDEFINED(current) && IsDataDescriptor(current)) {
      value = current.getValue();
    }

    %DefineDataPropertyUnchecked(obj, p, value, flag);
  } else {
    // There are 3 cases that lead here:
    // Step 4b - defining a new accessor property.
    // Steps 9c & 12 - replacing an existing data property with an accessor
    //                 property.
    // Step 12 - updating an existing accessor property with an accessor
    //           descriptor.
    var getter = null;
    if (desc.hasGetter()) {
      getter = desc.getGet();
    } else if (IsAccessorDescriptor(current) && current.hasGetter()) {
      getter = current.getGet();
    }
    var setter = null;
    if (desc.hasSetter()) {
      setter = desc.getSet();
    } else if (IsAccessorDescriptor(current) && current.hasSetter()) {
      setter = current.getSet();
    }
    %DefineAccessorPropertyUnchecked(obj, p, getter, setter, flag);
  }
  return true;
}


// ES5 section 15.4.5.1.
function DefineArrayProperty(obj, p, desc, should_throw) {
  // Step 3 - Special handling for array index.
  if (!IS_SYMBOL(p)) {
    var index = $toUint32(p);
    var emit_splice = false;
    if ($toString(index) == p && index != 4294967295) {
      var length = obj.length;
      if (index >= length && %IsObserved(obj)) {
        emit_splice = true;
        $observeBeginPerformSplice(obj);
      }

      var length_desc = GetOwnPropertyJS(obj, "length");
      if ((index >= length && !length_desc.isWritable()) ||
          !DefineObjectProperty(obj, p, desc, true)) {
        if (emit_splice)
          $observeEndPerformSplice(obj);
        if (should_throw) {
          throw MakeTypeError(kDefineDisallowed, p);
        } else {
          return false;
        }
      }
      if (index >= length) {
        obj.length = index + 1;
      }
      if (emit_splice) {
        $observeEndPerformSplice(obj);
        $observeEnqueueSpliceRecord(obj, length, [], index + 1 - length);
      }
      return true;
    }
  }

  // Step 5 - Fallback to default implementation.
  return DefineObjectProperty(obj, p, desc, should_throw);
}


// ES5 section 8.12.9, ES5 section 15.4.5.1 and Harmony proxies.
function DefineOwnProperty(obj, p, desc, should_throw) {
  if (%_IsJSProxy(obj)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(p)) return false;

    var attributes = FromGenericPropertyDescriptor(desc);
    return DefineProxyProperty(obj, p, attributes, should_throw);
  } else if (IS_ARRAY(obj)) {
    return DefineArrayProperty(obj, p, desc, should_throw);
  } else {
    return DefineObjectProperty(obj, p, desc, should_throw);
  }
}


function DefineOwnPropertyFromAPI(obj, p, value, desc) {
  return DefineOwnProperty(obj, p, ToPropertyDescriptor({
                             value: value,
                             writable: desc[0],
                             enumerable: desc[1],
                             configurable: desc[2]
                           }),
                           false);
}


// ES6 section 19.1.2.9
function ObjectGetPrototypeOf(obj) {
  return %_GetPrototype(TO_OBJECT_INLINE(obj));
}

// ES6 section 19.1.2.19.
function ObjectSetPrototypeOf(obj, proto) {
  CHECK_OBJECT_COERCIBLE(obj, "Object.setPrototypeOf");

  if (proto !== null && !IS_SPEC_OBJECT(proto)) {
    throw MakeTypeError(kProtoObjectOrNull, proto);
  }

  if (IS_SPEC_OBJECT(obj)) {
    %SetPrototype(obj, proto);
  }

  return obj;
}


// ES6 section 19.1.2.6
function ObjectGetOwnPropertyDescriptor(obj, p) {
  var desc = GetOwnPropertyJS(TO_OBJECT_INLINE(obj), p);
  return FromPropertyDescriptor(desc);
}


// For Harmony proxies
function ToNameArray(obj, trap, includeSymbols) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError(kProxyNonObjectPropNames, trap, obj);
  }
  var n = $toUint32(obj.length);
  var array = new GlobalArray(n);
  var realLength = 0;
  var names = { __proto__: null };  // TODO(rossberg): use sets once ready.
  for (var index = 0; index < n; index++) {
    var s = $toName(obj[index]);
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(s) && !includeSymbols) continue;
    if (%HasOwnProperty(names, s)) {
      throw MakeTypeError(kProxyRepeatedPropName, trap, s);
    }
    array[realLength] = s;
    ++realLength;
    names[s] = 0;
  }
  array.length = realLength;
  return array;
}


function ObjectGetOwnPropertyKeys(obj, filter) {
  var nameArrays = new InternalArray();
  filter |= PROPERTY_ATTRIBUTES_PRIVATE_SYMBOL;
  var interceptorInfo = %GetInterceptorInfo(obj);

  // Find all the indexed properties.

  // Only get own element names if we want to include string keys.
  if ((filter & PROPERTY_ATTRIBUTES_STRING) === 0) {
    var ownElementNames = %GetOwnElementNames(obj);
    for (var i = 0; i < ownElementNames.length; ++i) {
      ownElementNames[i] = %_NumberToString(ownElementNames[i]);
    }
    nameArrays.push(ownElementNames);
    // Get names for indexed interceptor properties.
    if ((interceptorInfo & 1) != 0) {
      var indexedInterceptorNames = %GetIndexedInterceptorElementNames(obj);
      if (!IS_UNDEFINED(indexedInterceptorNames)) {
        nameArrays.push(indexedInterceptorNames);
      }
    }
  }

  // Find all the named properties.

  // Get own property names.
  nameArrays.push(%GetOwnPropertyNames(obj, filter));

  // Get names for named interceptor properties if any.
  if ((interceptorInfo & 2) != 0) {
    var namedInterceptorNames =
        %GetNamedInterceptorPropertyNames(obj);
    if (!IS_UNDEFINED(namedInterceptorNames)) {
      nameArrays.push(namedInterceptorNames);
    }
  }

  var propertyNames =
      %Apply(InternalArray.prototype.concat,
             nameArrays[0], nameArrays, 1, nameArrays.length - 1);

  // Property names are expected to be unique strings,
  // but interceptors can interfere with that assumption.
  if (interceptorInfo != 0) {
    var seenKeys = { __proto__: null };
    var j = 0;
    for (var i = 0; i < propertyNames.length; ++i) {
      var name = propertyNames[i];
      if (IS_SYMBOL(name)) {
        if ((filter & PROPERTY_ATTRIBUTES_SYMBOLIC) || IS_PRIVATE(name)) {
          continue;
        }
      } else {
        if (filter & PROPERTY_ATTRIBUTES_STRING) continue;
        name = $toString(name);
      }
      if (seenKeys[name]) continue;
      seenKeys[name] = true;
      propertyNames[j++] = name;
    }
    propertyNames.length = j;
  }

  return propertyNames;
}


// ES6 section 9.1.12 / 9.5.12
function OwnPropertyKeys(obj) {
  if (%_IsJSProxy(obj)) {
    var handler = %GetHandler(obj);
    // TODO(caitp): Proxy.[[OwnPropertyKeys]] can not be implemented to spec
    // without an implementation of Direct Proxies.
    var names = CallTrap0(handler, "ownKeys", UNDEFINED);
    return ToNameArray(names, "getOwnPropertyNames", false);
  }
  return ObjectGetOwnPropertyKeys(obj, PROPERTY_ATTRIBUTES_PRIVATE_SYMBOL);
}


// ES5 section 15.2.3.4.
function ObjectGetOwnPropertyNames(obj) {
  obj = TO_OBJECT_INLINE(obj);
  // Special handling for proxies.
  if (%_IsJSProxy(obj)) {
    var handler = %GetHandler(obj);
    var names = CallTrap0(handler, "getOwnPropertyNames", UNDEFINED);
    return ToNameArray(names, "getOwnPropertyNames", false);
  }

  return ObjectGetOwnPropertyKeys(obj, PROPERTY_ATTRIBUTES_SYMBOLIC);
}


// ES5 section 15.2.3.5.
function ObjectCreate(proto, properties) {
  if (!IS_SPEC_OBJECT(proto) && proto !== null) {
    throw MakeTypeError(kProtoObjectOrNull, proto);
  }
  var obj = {};
  %InternalSetPrototype(obj, proto);
  if (!IS_UNDEFINED(properties)) ObjectDefineProperties(obj, properties);
  return obj;
}


// ES5 section 15.2.3.6.
function ObjectDefineProperty(obj, p, attributes) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError(kCalledOnNonObject, "Object.defineProperty");
  }
  var name = $toName(p);
  if (%_IsJSProxy(obj)) {
    // Clone the attributes object for protection.
    // TODO(rossberg): not spec'ed yet, so not sure if this should involve
    // non-own properties as it does (or non-enumerable ones, as it doesn't?).
    var attributesClone = { __proto__: null };
    for (var a in attributes) {
      attributesClone[a] = attributes[a];
    }
    DefineProxyProperty(obj, name, attributesClone, true);
    // The following would implement the spec as in the current proposal,
    // but after recent comments on es-discuss, is most likely obsolete.
    /*
    var defineObj = FromGenericPropertyDescriptor(desc);
    var names = ObjectGetOwnPropertyNames(attributes);
    var standardNames =
      {value: 0, writable: 0, get: 0, set: 0, enumerable: 0, configurable: 0};
    for (var i = 0; i < names.length; i++) {
      var N = names[i];
      if (!(%HasOwnProperty(standardNames, N))) {
        var attr = GetOwnPropertyJS(attributes, N);
        DefineOwnProperty(descObj, N, attr, true);
      }
    }
    // This is really confusing the types, but it is what the proxies spec
    // currently requires:
    desc = descObj;
    */
  } else {
    var desc = ToPropertyDescriptor(attributes);
    DefineOwnProperty(obj, name, desc, true);
  }
  return obj;
}


function GetOwnEnumerablePropertyNames(object) {
  var names = new InternalArray();
  for (var key in object) {
    if (%HasOwnProperty(object, key)) {
      names.push(key);
    }
  }

  var filter = PROPERTY_ATTRIBUTES_STRING | PROPERTY_ATTRIBUTES_PRIVATE_SYMBOL;
  var symbols = %GetOwnPropertyNames(object, filter);
  for (var i = 0; i < symbols.length; ++i) {
    var symbol = symbols[i];
    if (IS_SYMBOL(symbol)) {
      var desc = ObjectGetOwnPropertyDescriptor(object, symbol);
      if (desc.enumerable) names.push(symbol);
    }
  }

  return names;
}


// ES5 section 15.2.3.7.
function ObjectDefineProperties(obj, properties) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError(kCalledOnNonObject, "Object.defineProperties");
  }
  var props = TO_OBJECT_INLINE(properties);
  var names = GetOwnEnumerablePropertyNames(props);
  var descriptors = new InternalArray();
  for (var i = 0; i < names.length; i++) {
    descriptors.push(ToPropertyDescriptor(props[names[i]]));
  }
  for (var i = 0; i < names.length; i++) {
    DefineOwnProperty(obj, names[i], descriptors[i], true);
  }
  return obj;
}


// Harmony proxies.
function ProxyFix(obj) {
  var handler = %GetHandler(obj);
  var props = CallTrap0(handler, "fix", UNDEFINED);
  if (IS_UNDEFINED(props)) {
    throw MakeTypeError(kProxyHandlerReturned, handler, "undefined", "fix");
  }

  if (%IsJSFunctionProxy(obj)) {
    var callTrap = %GetCallTrap(obj);
    var constructTrap = %GetConstructTrap(obj);
    var code = ProxyDelegateCallAndConstruct(callTrap, constructTrap);
    %Fix(obj);  // becomes a regular function
    %SetCode(obj, code);
    // TODO(rossberg): What about length and other properties? Not specified.
    // We just put in some half-reasonable defaults for now.
    var prototype = new GlobalObject();
    ObjectDefineProperty(prototype, "constructor",
      {value: obj, writable: true, enumerable: false, configurable: true});
    // TODO(v8:1530): defineProperty does not handle prototype and length.
    %FunctionSetPrototype(obj, prototype);
    obj.length = 0;
  } else {
    %Fix(obj);
  }
  ObjectDefineProperties(obj, props);
}


// ES5 section 15.2.3.8.
function ObjectSealJS(obj) {
  if (!IS_SPEC_OBJECT(obj)) return obj;
  var isProxy = %_IsJSProxy(obj);
  if (isProxy || %HasSloppyArgumentsElements(obj) || %IsObserved(obj)) {
    if (isProxy) {
      ProxyFix(obj);
    }
    var names = ObjectGetOwnPropertyNames(obj);
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      var desc = GetOwnPropertyJS(obj, name);
      if (desc.isConfigurable()) {
        desc.setConfigurable(false);
        DefineOwnProperty(obj, name, desc, true);
      }
    }
    %PreventExtensions(obj);
  } else {
    // TODO(adamk): Is it worth going to this fast path if the
    // object's properties are already in dictionary mode?
    %ObjectSeal(obj);
  }
  return obj;
}


// ES5 section 15.2.3.9.
function ObjectFreezeJS(obj) {
  if (!IS_SPEC_OBJECT(obj)) return obj;
  var isProxy = %_IsJSProxy(obj);
  // TODO(conradw): Investigate modifying the fast path to accommodate strong
  // objects.
  if (isProxy || %HasSloppyArgumentsElements(obj) || %IsObserved(obj) ||
      IS_STRONG(obj)) {
    if (isProxy) {
      ProxyFix(obj);
    }
    var names = ObjectGetOwnPropertyNames(obj);
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      var desc = GetOwnPropertyJS(obj, name);
      if (desc.isWritable() || desc.isConfigurable()) {
        if (IsDataDescriptor(desc)) desc.setWritable(false);
        desc.setConfigurable(false);
        DefineOwnProperty(obj, name, desc, true);
      }
    }
    %PreventExtensions(obj);
  } else {
    // TODO(adamk): Is it worth going to this fast path if the
    // object's properties are already in dictionary mode?
    %ObjectFreeze(obj);
  }
  return obj;
}


// ES5 section 15.2.3.10
function ObjectPreventExtension(obj) {
  if (!IS_SPEC_OBJECT(obj)) return obj;
  if (%_IsJSProxy(obj)) {
    ProxyFix(obj);
  }
  %PreventExtensions(obj);
  return obj;
}


// ES5 section 15.2.3.11
function ObjectIsSealed(obj) {
  if (!IS_SPEC_OBJECT(obj)) return true;
  if (%_IsJSProxy(obj)) {
    return false;
  }
  if (%IsExtensible(obj)) {
    return false;
  }
  var names = ObjectGetOwnPropertyNames(obj);
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var desc = GetOwnPropertyJS(obj, name);
    if (desc.isConfigurable()) {
      return false;
    }
  }
  return true;
}


// ES5 section 15.2.3.12
function ObjectIsFrozen(obj) {
  if (!IS_SPEC_OBJECT(obj)) return true;
  if (%_IsJSProxy(obj)) {
    return false;
  }
  if (%IsExtensible(obj)) {
    return false;
  }
  var names = ObjectGetOwnPropertyNames(obj);
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var desc = GetOwnPropertyJS(obj, name);
    if (IsDataDescriptor(desc) && desc.isWritable()) return false;
    if (desc.isConfigurable()) return false;
  }
  return true;
}


// ES5 section 15.2.3.13
function ObjectIsExtensible(obj) {
  if (!IS_SPEC_OBJECT(obj)) return false;
  if (%_IsJSProxy(obj)) {
    return true;
  }
  return %IsExtensible(obj);
}


// ECMA-262, Edition 6, section 19.1.2.10
function ObjectIs(obj1, obj2) {
  return $sameValue(obj1, obj2);
}


// ECMA-262, Edition 6, section B.2.2.1.1
function ObjectGetProto() {
  return %_GetPrototype(TO_OBJECT_INLINE(this));
}


// ECMA-262, Edition 6, section B.2.2.1.2
function ObjectSetProto(proto) {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.__proto__");

  if ((IS_SPEC_OBJECT(proto) || IS_NULL(proto)) && IS_SPEC_OBJECT(this)) {
    %SetPrototype(this, proto);
  }
}


function ObjectConstructor(x) {
  if (%_IsConstructCall()) {
    if (x == null) return this;
    return TO_OBJECT_INLINE(x);
  } else {
    if (x == null) return { };
    return TO_OBJECT_INLINE(x);
  }
}


// ----------------------------------------------------------------------------
// Object

%SetNativeFlag(GlobalObject);
%SetCode(GlobalObject, ObjectConstructor);

%AddNamedProperty(GlobalObject.prototype, "constructor", GlobalObject,
                  DONT_ENUM);

// Set up non-enumerable functions on the Object.prototype object.
utils.InstallFunctions(GlobalObject.prototype, DONT_ENUM, [
  "toString", ObjectToString,
  "toLocaleString", ObjectToLocaleString,
  "valueOf", ObjectValueOf,
  "hasOwnProperty", ObjectHasOwnProperty,
  "isPrototypeOf", ObjectIsPrototypeOf,
  "propertyIsEnumerable", ObjectPropertyIsEnumerable,
  "__defineGetter__", ObjectDefineGetter,
  "__lookupGetter__", ObjectLookupGetter,
  "__defineSetter__", ObjectDefineSetter,
  "__lookupSetter__", ObjectLookupSetter
]);
utils.InstallGetterSetter(GlobalObject.prototype, "__proto__", ObjectGetProto,
                    ObjectSetProto);

// Set up non-enumerable functions in the Object object.
utils.InstallFunctions(GlobalObject, DONT_ENUM, [
  "keys", ObjectKeys,
  "create", ObjectCreate,
  "defineProperty", ObjectDefineProperty,
  "defineProperties", ObjectDefineProperties,
  "freeze", ObjectFreezeJS,
  "getPrototypeOf", ObjectGetPrototypeOf,
  "setPrototypeOf", ObjectSetPrototypeOf,
  "getOwnPropertyDescriptor", ObjectGetOwnPropertyDescriptor,
  "getOwnPropertyNames", ObjectGetOwnPropertyNames,
  // getOwnPropertySymbols is added in symbol.js.
  "is", ObjectIs,
  "isExtensible", ObjectIsExtensible,
  "isFrozen", ObjectIsFrozen,
  "isSealed", ObjectIsSealed,
  "preventExtensions", ObjectPreventExtension,
  "seal", ObjectSealJS
  // deliverChangeRecords, getNotifier, observe and unobserve are added
  // in object-observe.js.
]);


// ----------------------------------------------------------------------------
// Boolean

function BooleanConstructor(x) {
  if (%_IsConstructCall()) {
    %_SetValueOf(this, $toBoolean(x));
  } else {
    return $toBoolean(x);
  }
}


function BooleanToString() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var b = this;
  if (!IS_BOOLEAN(b)) {
    if (!IS_BOOLEAN_WRAPPER(b)) {
      throw MakeTypeError(kNotGeneric, 'Boolean.prototype.toString');
    }
    b = %_ValueOf(b);
  }
  return b ? 'true' : 'false';
}


function BooleanValueOf() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_BOOLEAN(this) && !IS_BOOLEAN_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'Boolean.prototype.valueOf');
  }
  return %_ValueOf(this);
}


// ----------------------------------------------------------------------------

%SetCode(GlobalBoolean, BooleanConstructor);
%FunctionSetPrototype(GlobalBoolean, new GlobalBoolean(false));
%AddNamedProperty(GlobalBoolean.prototype, "constructor", GlobalBoolean,
                  DONT_ENUM);

utils.InstallFunctions(GlobalBoolean.prototype, DONT_ENUM, [
  "toString", BooleanToString,
  "valueOf", BooleanValueOf
]);


// ----------------------------------------------------------------------------
// Number

function NumberConstructor(x) {
  var value = %_ArgumentsLength() == 0 ? 0 : $toNumber(x);
  if (%_IsConstructCall()) {
    %_SetValueOf(this, value);
  } else {
    return value;
  }
}


// ECMA-262 section 15.7.4.2.
function NumberToStringJS(radix) {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var number = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kNotGeneric, 'Number.prototype.toString');
    }
    // Get the value of this number in case it's an object.
    number = %_ValueOf(this);
  }
  // Fast case: Convert number in radix 10.
  if (IS_UNDEFINED(radix) || radix === 10) {
    return %_NumberToString(number);
  }

  // Convert the radix to an integer and check the range.
  radix = TO_INTEGER(radix);
  if (radix < 2 || radix > 36) throw MakeRangeError(kToRadixFormatRange);
  // Convert the number to a string in the given radix.
  return %NumberToRadixString(number, radix);
}


// ECMA-262 section 15.7.4.3
function NumberToLocaleString() {
  return %_CallFunction(this, NumberToStringJS);
}


// ECMA-262 section 15.7.4.4
function NumberValueOf() {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_NUMBER(this) && !IS_NUMBER_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'Number.prototype.valueOf');
  }
  return %_ValueOf(this);
}


// ECMA-262 section 15.7.4.5
function NumberToFixedJS(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toFixed", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  var f = TO_INTEGER(fractionDigits);

  if (f < 0 || f > 20) {
    throw MakeRangeError(kNumberFormatRange, "toFixed() digits");
  }

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  return %NumberToFixed(x, f);
}


// ECMA-262 section 15.7.4.6
function NumberToExponentialJS(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toExponential", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  var f = IS_UNDEFINED(fractionDigits) ? UNDEFINED : TO_INTEGER(fractionDigits);

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  if (IS_UNDEFINED(f)) {
    f = -1;  // Signal for runtime function that f is not defined.
  } else if (f < 0 || f > 20) {
    throw MakeRangeError(kNumberFormatRange, "toExponential()");
  }
  return %NumberToExponential(x, f);
}


// ECMA-262 section 15.7.4.7
function NumberToPrecisionJS(precision) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError(kIncompatibleMethodReceiver,
                          "Number.prototype.toPrecision", this);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  if (IS_UNDEFINED(precision)) return $toString(%_ValueOf(this));
  var p = TO_INTEGER(precision);

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  if (p < 1 || p > 21) {
    throw MakeRangeError(kToPrecisionFormatRange);
  }
  return %NumberToPrecision(x, p);
}


// Harmony isFinite.
function NumberIsFinite(number) {
  return IS_NUMBER(number) && NUMBER_IS_FINITE(number);
}


// Harmony isInteger
function NumberIsInteger(number) {
  return NumberIsFinite(number) && TO_INTEGER(number) == number;
}


// Harmony isNaN.
function NumberIsNaN(number) {
  return IS_NUMBER(number) && NUMBER_IS_NAN(number);
}


// Harmony isSafeInteger
function NumberIsSafeInteger(number) {
  if (NumberIsFinite(number)) {
    var integral = TO_INTEGER(number);
    if (integral == number) {
      return MathAbs(integral) <= GlobalNumber.MAX_SAFE_INTEGER;
    }
  }
  return false;
}


// ----------------------------------------------------------------------------

%SetCode(GlobalNumber, NumberConstructor);
%FunctionSetPrototype(GlobalNumber, new GlobalNumber(0));

%OptimizeObjectForAddingMultipleProperties(GlobalNumber.prototype, 8);
// Set up the constructor property on the Number prototype object.
%AddNamedProperty(GlobalNumber.prototype, "constructor", GlobalNumber,
                  DONT_ENUM);

utils.InstallConstants(GlobalNumber, [
  // ECMA-262 section 15.7.3.1.
  "MAX_VALUE", 1.7976931348623157e+308,
  // ECMA-262 section 15.7.3.2.
  "MIN_VALUE", 5e-324,
  // ECMA-262 section 15.7.3.3.
  "NaN", NAN,
  // ECMA-262 section 15.7.3.4.
  "NEGATIVE_INFINITY", -INFINITY,
  // ECMA-262 section 15.7.3.5.
  "POSITIVE_INFINITY", INFINITY,

  // --- Harmony constants (no spec refs until settled.)

  "MAX_SAFE_INTEGER", %_MathPow(2, 53) - 1,
  "MIN_SAFE_INTEGER", -%_MathPow(2, 53) + 1,
  "EPSILON", %_MathPow(2, -52)
]);

// Set up non-enumerable functions on the Number prototype object.
utils.InstallFunctions(GlobalNumber.prototype, DONT_ENUM, [
  "toString", NumberToStringJS,
  "toLocaleString", NumberToLocaleString,
  "valueOf", NumberValueOf,
  "toFixed", NumberToFixedJS,
  "toExponential", NumberToExponentialJS,
  "toPrecision", NumberToPrecisionJS
]);

// Harmony Number constructor additions
utils.InstallFunctions(GlobalNumber, DONT_ENUM, [
  "isFinite", NumberIsFinite,
  "isInteger", NumberIsInteger,
  "isNaN", NumberIsNaN,
  "isSafeInteger", NumberIsSafeInteger,
  "parseInt", GlobalParseInt,
  "parseFloat", GlobalParseFloat
]);

%SetForceInlineFlag(NumberIsNaN);


// ----------------------------------------------------------------------------
// Function

function NativeCodeFunctionSourceString(func) {
  var name = %FunctionGetName(func);
  if (name) {
    // Mimic what KJS does.
    return 'function ' + name + '() { [native code] }';
  }

  return 'function () { [native code] }';
}

function FunctionSourceString(func) {
  while (%IsJSFunctionProxy(func)) {
    func = %GetCallTrap(func);
  }

  if (!IS_FUNCTION(func)) {
    throw MakeTypeError(kNotGeneric, 'Function.prototype.toString');
  }

  if (%FunctionIsBuiltin(func)) {
    return NativeCodeFunctionSourceString(func);
  }

  var classSource = %ClassGetSourceCode(func);
  if (IS_STRING(classSource)) {
    return classSource;
  }

  var source = %FunctionGetSourceCode(func);
  if (!IS_STRING(source)) {
    return NativeCodeFunctionSourceString(func);
  }

  if (%FunctionIsArrow(func)) {
    return source;
  }

  var name = %FunctionNameShouldPrintAsAnonymous(func)
      ? 'anonymous'
      : %FunctionGetName(func);

  var isGenerator = %FunctionIsGenerator(func);
  var head = %FunctionIsConciseMethod(func)
      ? (isGenerator ? '*' : '')
      : (isGenerator ? 'function* ' : 'function ');
  return head + name + source;
}


function FunctionToString() {
  return FunctionSourceString(this);
}


// ES5 15.3.4.5
function FunctionBind(this_arg) { // Length is 1.
  if (!IS_SPEC_FUNCTION(this)) throw MakeTypeError(kFunctionBind);

  var boundFunction = function () {
    // Poison .arguments and .caller, but is otherwise not detectable.
    "use strict";
    // This function must not use any object literals (Object, Array, RegExp),
    // since the literals-array is being used to store the bound data.
    if (%_IsConstructCall()) {
      return %NewObjectFromBound(boundFunction);
    }
    var bindings = %BoundFunctionGetBindings(boundFunction);

    var argc = %_ArgumentsLength();
    if (argc == 0) {
      return %Apply(bindings[0], bindings[1], bindings, 2, bindings.length - 2);
    }
    if (bindings.length === 2) {
      return %Apply(bindings[0], bindings[1], arguments, 0, argc);
    }
    var bound_argc = bindings.length - 2;
    var argv = new InternalArray(bound_argc + argc);
    for (var i = 0; i < bound_argc; i++) {
      argv[i] = bindings[i + 2];
    }
    for (var j = 0; j < argc; j++) {
      argv[i++] = %_Arguments(j);
    }
    return %Apply(bindings[0], bindings[1], argv, 0, bound_argc + argc);
  };

  var new_length = 0;
  var old_length = this.length;
  // FunctionProxies might provide a non-UInt32 value. If so, ignore it.
  if ((typeof old_length === "number") &&
      ((old_length >>> 0) === old_length)) {
    var argc = %_ArgumentsLength();
    if (argc > 0) argc--;  // Don't count the thisArg as parameter.
    new_length = old_length - argc;
    if (new_length < 0) new_length = 0;
  }
  // This runtime function finds any remaining arguments on the stack,
  // so we don't pass the arguments object.
  var result = %FunctionBindArguments(boundFunction, this,
                                      this_arg, new_length);

  var name = this.name;
  var bound_name = IS_STRING(name) ? name : "";
  %DefineDataPropertyUnchecked(result, "name", "bound " + bound_name,
                               DONT_ENUM | READ_ONLY);

  // We already have caller and arguments properties on functions,
  // which are non-configurable. It therefore makes no sence to
  // try to redefine these as defined by the spec. The spec says
  // that bind should make these throw a TypeError if get or set
  // is called and make them non-enumerable and non-configurable.
  // To be consistent with our normal functions we leave this as it is.
  // TODO(lrn): Do set these to be thrower.
  return result;
}


function NewFunctionString(args, function_token) {
  var n = args.length;
  var p = '';
  if (n > 1) {
    p = $toString(args[0]);
    for (var i = 1; i < n - 1; i++) {
      p += ',' + $toString(args[i]);
    }
    // If the formal parameters string include ) - an illegal
    // character - it may make the combined function expression
    // compile. We avoid this problem by checking for this early on.
    if (%_CallFunction(p, ')', StringIndexOf) != -1) {
      throw MakeSyntaxError(kParenthesisInArgString);
    }
    // If the formal parameters include an unbalanced block comment, the
    // function must be rejected. Since JavaScript does not allow nested
    // comments we can include a trailing block comment to catch this.
    p += '\n/' + '**/';
  }
  var body = (n > 0) ? $toString(args[n - 1]) : '';
  return '(' + function_token + '(' + p + ') {\n' + body + '\n})';
}


function FunctionConstructor(arg1) {  // length == 1
  var source = NewFunctionString(arguments, 'function');
  var global_proxy = %GlobalProxy(FunctionConstructor);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %_CallFunction(global_proxy, %CompileString(source, true));
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
}


// ----------------------------------------------------------------------------

%SetCode(GlobalFunction, FunctionConstructor);
%AddNamedProperty(GlobalFunction.prototype, "constructor", GlobalFunction,
                  DONT_ENUM);

utils.InstallFunctions(GlobalFunction.prototype, DONT_ENUM, [
  "bind", FunctionBind,
  "toString", FunctionToString
]);

// ----------------------------------------------------------------------------
// Iterator related spec functions.

// ES6 rev 33, 2015-02-12
// 7.4.1 GetIterator ( obj, method )
function GetIterator(obj, method) {
  if (IS_UNDEFINED(method)) {
    method = obj[symbolIterator];
  }
  if (!IS_SPEC_FUNCTION(method)) {
    throw MakeTypeError(kNotIterable, obj);
  }
  var iterator = %_CallFunction(obj, method);
  if (!IS_SPEC_OBJECT(iterator)) {
    throw MakeTypeError(kNotAnIterator, iterator);
  }
  return iterator;
}

// ----------------------------------------------------------------------------
// Exports

$functionSourceString = FunctionSourceString;
$globalEval = GlobalEval;
$objectDefineOwnProperty = DefineOwnPropertyFromAPI;
$objectGetOwnPropertyDescriptor = ObjectGetOwnPropertyDescriptor;
$toCompletePropertyDescriptor = ToCompletePropertyDescriptor;

utils.ObjectDefineProperties = ObjectDefineProperties;
utils.ObjectDefineProperty = ObjectDefineProperty;

utils.Export(function(to) {
  to.Delete = Delete;
  to.GetIterator = GetIterator;
  to.GetMethod = GetMethod;
  to.IsFinite = GlobalIsFinite;
  to.IsNaN = GlobalIsNaN;
  to.NewFunctionString = NewFunctionString;
  to.NumberIsNaN = NumberIsNaN;
  to.ObjectDefineProperty = ObjectDefineProperty;
  to.ObjectFreeze = ObjectFreezeJS;
  to.ObjectGetOwnPropertyKeys = ObjectGetOwnPropertyKeys;
  to.ObjectHasOwnProperty = ObjectHasOwnProperty;
  to.ObjectIsFrozen = ObjectIsFrozen;
  to.ObjectIsSealed = ObjectIsSealed;
  to.ObjectToString = ObjectToString;
  to.OwnPropertyKeys = OwnPropertyKeys;
  to.ToNameArray = ToNameArray;
});

})
