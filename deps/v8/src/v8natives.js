// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file relies on the fact that the following declarations have been made
// in runtime.js:
// var $Object = global.Object;
// var $Boolean = global.Boolean;
// var $Number = global.Number;
// var $Function = global.Function;
// var $Array = global.Array;
//
// in math.js:
// var $floor = MathFloor

var $isNaN = GlobalIsNaN;
var $isFinite = GlobalIsFinite;

// ----------------------------------------------------------------------------

// Helper function used to install functions on objects.
function InstallFunctions(object, attributes, functions) {
  if (functions.length >= 8) {
    %OptimizeObjectForAddingMultipleProperties(object, functions.length >> 1);
  }
  for (var i = 0; i < functions.length; i += 2) {
    var key = functions[i];
    var f = functions[i + 1];
    %FunctionSetName(f, key);
    %FunctionRemovePrototype(f);
    %SetProperty(object, key, f, attributes);
    %SetNativeFlag(f);
  }
  %ToFastProperties(object);
}


// Helper function to install a getter-only accessor property.
function InstallGetter(object, name, getter) {
  %FunctionSetName(getter, name);
  %FunctionRemovePrototype(getter);
  %DefineOrRedefineAccessorProperty(object, name, getter, null, DONT_ENUM);
  %SetNativeFlag(getter);
}


// Helper function to install a getter/setter accessor property.
function InstallGetterSetter(object, name, getter, setter) {
  %FunctionSetName(getter, name);
  %FunctionSetName(setter, name);
  %FunctionRemovePrototype(getter);
  %FunctionRemovePrototype(setter);
  %DefineOrRedefineAccessorProperty(object, name, getter, setter, DONT_ENUM);
  %SetNativeFlag(getter);
  %SetNativeFlag(setter);
}


// Helper function for installing constant properties on objects.
function InstallConstants(object, constants) {
  if (constants.length >= 4) {
    %OptimizeObjectForAddingMultipleProperties(object, constants.length >> 1);
  }
  var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;
  for (var i = 0; i < constants.length; i += 2) {
    var name = constants[i];
    var k = constants[i + 1];
    %SetProperty(object, name, k, attributes);
  }
  %ToFastProperties(object);
}


// Prevents changes to the prototype of a built-in function.
// The "prototype" property of the function object is made non-configurable,
// and the prototype object is made non-extensible. The latter prevents
// changing the __proto__ property.
function SetUpLockedPrototype(constructor, fields, methods) {
  %CheckIsBootstrapping();
  var prototype = constructor.prototype;
  // Install functions first, because this function is used to initialize
  // PropertyDescriptor itself.
  var property_count = (methods.length >> 1) + (fields ? fields.length : 0);
  if (property_count >= 4) {
    %OptimizeObjectForAddingMultipleProperties(prototype, property_count);
  }
  if (fields) {
    for (var i = 0; i < fields.length; i++) {
      %SetProperty(prototype, fields[i], UNDEFINED, DONT_ENUM | DONT_DELETE);
    }
  }
  for (var i = 0; i < methods.length; i += 2) {
    var key = methods[i];
    var f = methods[i + 1];
    %SetProperty(prototype, key, f, DONT_ENUM | DONT_DELETE | READ_ONLY);
    %SetNativeFlag(f);
  }
  %SetPrototype(prototype, null);
  %ToFastProperties(prototype);
}


// ----------------------------------------------------------------------------


// ECMA 262 - 15.1.4
function GlobalIsNaN(number) {
  if (!IS_NUMBER(number)) number = NonNumberToNumber(number);
  return NUMBER_IS_NAN(number);
}


// ECMA 262 - 15.1.5
function GlobalIsFinite(number) {
  if (!IS_NUMBER(number)) number = NonNumberToNumber(number);
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

  // For consistency with JSC we require the global object passed to
  // eval to be the global object from which 'eval' originated. This
  // is not mandated by the spec.
  // We only throw if the global has been detached, since we need the
  // receiver as this-value for the call.
  if (!%IsAttachedGlobal(global)) {
    throw new $EvalError('The "this" value passed to eval must ' +
                         'be the global object from which eval originated');
  }

  var global_receiver = %GlobalReceiver(global);

  var f = %CompileString(x, false);
  if (!IS_FUNCTION(f)) return f;

  return %_CallFunction(global_receiver, f);
}


// ----------------------------------------------------------------------------

// Set up global object.
function SetUpGlobal() {
  %CheckIsBootstrapping();

  var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;

  // ECMA 262 - 15.1.1.1.
  %SetProperty(global, "NaN", NAN, attributes);

  // ECMA-262 - 15.1.1.2.
  %SetProperty(global, "Infinity", INFINITY, attributes);

  // ECMA-262 - 15.1.1.3.
  %SetProperty(global, "undefined", UNDEFINED, attributes);

  // Set up non-enumerable function on the global object.
  InstallFunctions(global, DONT_ENUM, $Array(
    "isNaN", GlobalIsNaN,
    "isFinite", GlobalIsFinite,
    "parseInt", GlobalParseInt,
    "parseFloat", GlobalParseFloat,
    "eval", GlobalEval
  ));
}

SetUpGlobal();


// ----------------------------------------------------------------------------
// Object

// ECMA-262 - 15.2.4.2
function ObjectToString() {
  if (IS_UNDEFINED(this) && !IS_UNDETECTABLE(this)) return "[object Undefined]";
  if (IS_NULL(this)) return "[object Null]";
  return "[object " + %_ClassOf(ToObject(this)) + "]";
}


// ECMA-262 - 15.2.4.3
function ObjectToLocaleString() {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.toLocaleString");
  return this.toString();
}


// ECMA-262 - 15.2.4.4
function ObjectValueOf() {
  return ToObject(this);
}


// ECMA-262 - 15.2.4.5
function ObjectHasOwnProperty(V) {
  if (%IsJSProxy(this)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(V)) return false;

    var handler = %GetHandler(this);
    return CallTrap1(handler, "hasOwn", DerivedHasOwnTrap, ToName(V));
  }
  return %HasLocalProperty(TO_OBJECT_INLINE(this), ToName(V));
}


// ECMA-262 - 15.2.4.6
function ObjectIsPrototypeOf(V) {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.isPrototypeOf");
  if (!IS_SPEC_OBJECT(V)) return false;
  return %IsInPrototypeChain(this, V);
}


// ECMA-262 - 15.2.4.6
function ObjectPropertyIsEnumerable(V) {
  var P = ToName(V);
  if (%IsJSProxy(this)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(V)) return false;

    var desc = GetOwnProperty(this, P);
    return IS_UNDEFINED(desc) ? false : desc.isEnumerable();
  }
  return %IsPropertyEnumerable(ToObject(this), P);
}


// Extensions for providing property getters and setters.
function ObjectDefineGetter(name, fun) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalReceiver(global);
  }
  if (!IS_SPEC_FUNCTION(fun)) {
    throw new $TypeError(
        'Object.prototype.__defineGetter__: Expecting function');
  }
  var desc = new PropertyDescriptor();
  desc.setGet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(ToObject(receiver), ToName(name), desc, false);
}


function ObjectLookupGetter(name) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalReceiver(global);
  }
  return %LookupAccessor(ToObject(receiver), ToName(name), GETTER);
}


function ObjectDefineSetter(name, fun) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalReceiver(global);
  }
  if (!IS_SPEC_FUNCTION(fun)) {
    throw new $TypeError(
        'Object.prototype.__defineSetter__: Expecting function');
  }
  var desc = new PropertyDescriptor();
  desc.setSet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(ToObject(receiver), ToName(name), desc, false);
}


function ObjectLookupSetter(name) {
  var receiver = this;
  if (receiver == null && !IS_UNDETECTABLE(receiver)) {
    receiver = %GlobalReceiver(global);
  }
  return %LookupAccessor(ToObject(receiver), ToName(name), SETTER);
}


function ObjectKeys(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.keys"]);
  }
  if (%IsJSProxy(obj)) {
    var handler = %GetHandler(obj);
    var names = CallTrap0(handler, "keys", DerivedKeysTrap);
    return ToNameArray(names, "keys", false);
  }
  return %LocalKeys(obj);
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
  var obj = new $Object();

  if (desc.hasValue()) {
    %IgnoreAttributesAndSetProperty(obj, "value", desc.getValue(), NONE);
  }
  if (desc.hasWritable()) {
    %IgnoreAttributesAndSetProperty(obj, "writable", desc.isWritable(), NONE);
  }
  if (desc.hasGetter()) {
    %IgnoreAttributesAndSetProperty(obj, "get", desc.getGet(), NONE);
  }
  if (desc.hasSetter()) {
    %IgnoreAttributesAndSetProperty(obj, "set", desc.getSet(), NONE);
  }
  if (desc.hasEnumerable()) {
    %IgnoreAttributesAndSetProperty(obj, "enumerable",
                                    desc.isEnumerable(), NONE);
  }
  if (desc.hasConfigurable()) {
    %IgnoreAttributesAndSetProperty(obj, "configurable",
                                    desc.isConfigurable(), NONE);
  }
  return obj;
}


// ES5 8.10.5.
function ToPropertyDescriptor(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("property_desc_object", [obj]);
  }
  var desc = new PropertyDescriptor();

  if ("enumerable" in obj) {
    desc.setEnumerable(ToBoolean(obj.enumerable));
  }

  if ("configurable" in obj) {
    desc.setConfigurable(ToBoolean(obj.configurable));
  }

  if ("value" in obj) {
    desc.setValue(obj.value);
  }

  if ("writable" in obj) {
    desc.setWritable(ToBoolean(obj.writable));
  }

  if ("get" in obj) {
    var get = obj.get;
    if (!IS_UNDEFINED(get) && !IS_SPEC_FUNCTION(get)) {
      throw MakeTypeError("getter_must_be_callable", [get]);
    }
    desc.setGet(get);
  }

  if ("set" in obj) {
    var set = obj.set;
    if (!IS_UNDEFINED(set) && !IS_SPEC_FUNCTION(set)) {
      throw MakeTypeError("setter_must_be_callable", [set]);
    }
    desc.setSet(set);
  }

  if (IsInconsistentDescriptor(desc)) {
    throw MakeTypeError("value_and_accessor", [obj]);
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

SetUpLockedPrototype(PropertyDescriptor, $Array(
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
  ), $Array(
    "toString", function() {
      return "[object PropertyDescriptor]";
    },
    "setValue", function(value) {
      this.value_ = value;
      this.hasValue_ = true;
    },
    "getValue", function() {
      return this.value_;
    },
    "hasValue", function() {
      return this.hasValue_;
    },
    "setEnumerable", function(enumerable) {
      this.enumerable_ = enumerable;
        this.hasEnumerable_ = true;
    },
    "isEnumerable", function () {
      return this.enumerable_;
    },
    "hasEnumerable", function() {
      return this.hasEnumerable_;
    },
    "setWritable", function(writable) {
      this.writable_ = writable;
      this.hasWritable_ = true;
    },
    "isWritable", function() {
      return this.writable_;
    },
    "hasWritable", function() {
      return this.hasWritable_;
    },
    "setConfigurable", function(configurable) {
      this.configurable_ = configurable;
      this.hasConfigurable_ = true;
    },
    "hasConfigurable", function() {
      return this.hasConfigurable_;
    },
    "isConfigurable", function() {
      return this.configurable_;
    },
    "setGet", function(get) {
      this.get_ = get;
        this.hasGetter_ = true;
    },
    "getGet", function() {
      return this.get_;
    },
    "hasGetter", function() {
      return this.hasGetter_;
    },
    "setSet", function(set) {
      this.set_ = set;
      this.hasSetter_ = true;
    },
    "getSet", function() {
      return this.set_;
    },
    "hasSetter", function() {
      return this.hasSetter_;
  }));


// Converts an array returned from Runtime_GetOwnProperty to an actual
// property descriptor. For a description of the array layout please
// see the runtime.cc file.
function ConvertDescriptorArrayToDescriptor(desc_array) {
  if (desc_array === false) {
    throw 'Internal error: invalid desc_array';
  }

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
      throw MakeTypeError("handler_trap_missing", [handler, name]);
    }
    trap = defaultTrap;
  } else if (!IS_SPEC_FUNCTION(trap)) {
    throw MakeTypeError("handler_trap_must_be_callable", [handler, name]);
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
function GetOwnProperty(obj, v) {
  var p = ToName(v);
  if (%IsJSProxy(obj)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(v)) return UNDEFINED;

    var handler = %GetHandler(obj);
    var descriptor = CallTrap1(
                         handler, "getOwnPropertyDescriptor", UNDEFINED, p);
    if (IS_UNDEFINED(descriptor)) return descriptor;
    var desc = ToCompletePropertyDescriptor(descriptor);
    if (!desc.isConfigurable()) {
      throw MakeTypeError("proxy_prop_not_configurable",
                          [handler, "getOwnPropertyDescriptor", p, descriptor]);
    }
    return desc;
  }

  // GetOwnProperty returns an array indexed by the constants
  // defined in macros.py.
  // If p is not a property on obj undefined is returned.
  var props = %GetOwnProperty(ToObject(obj), p);

  // A false value here means that access checks failed.
  if (props === false) return UNDEFINED;

  return ConvertDescriptorArrayToDescriptor(props);
}


// ES5 section 8.12.7.
function Delete(obj, p, should_throw) {
  var desc = GetOwnProperty(obj, p);
  if (IS_UNDEFINED(desc)) return true;
  if (desc.isConfigurable()) {
    %DeleteProperty(obj, p, 0);
    return true;
  } else if (should_throw) {
    throw MakeTypeError("define_disallowed", [p]);
  } else {
    return;
  }
}


// Harmony proxies.
function DefineProxyProperty(obj, p, attributes, should_throw) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(p)) return false;

  var handler = %GetHandler(obj);
  var result = CallTrap2(handler, "defineProperty", UNDEFINED, p, attributes);
  if (!ToBoolean(result)) {
    if (should_throw) {
      throw MakeTypeError("handler_returned_false",
                          [handler, "defineProperty"]);
    } else {
      return false;
    }
  }
  return true;
}


// ES5 8.12.9.
function DefineObjectProperty(obj, p, desc, should_throw) {
  var current_or_access = %GetOwnProperty(ToObject(obj), ToName(p));
  // A false value here means that access checks failed.
  if (current_or_access === false) return UNDEFINED;

  var current = ConvertDescriptorArrayToDescriptor(current_or_access);
  var extensible = %IsExtensible(ToObject(obj));

  // Error handling according to spec.
  // Step 3
  if (IS_UNDEFINED(current) && !extensible) {
    if (should_throw) {
      throw MakeTypeError("define_disallowed", [p]);
    } else {
      return false;
    }
  }

  if (!IS_UNDEFINED(current)) {
    // Step 5 and 6
    if ((IsGenericDescriptor(desc) ||
         IsDataDescriptor(desc) == IsDataDescriptor(current)) &&
        (!desc.hasEnumerable() ||
         SameValue(desc.isEnumerable(), current.isEnumerable())) &&
        (!desc.hasConfigurable() ||
         SameValue(desc.isConfigurable(), current.isConfigurable())) &&
        (!desc.hasWritable() ||
         SameValue(desc.isWritable(), current.isWritable())) &&
        (!desc.hasValue() ||
         SameValue(desc.getValue(), current.getValue())) &&
        (!desc.hasGetter() ||
         SameValue(desc.getGet(), current.getGet())) &&
        (!desc.hasSetter() ||
         SameValue(desc.getSet(), current.getSet()))) {
      return true;
    }
    if (!current.isConfigurable()) {
      // Step 7
      if (desc.isConfigurable() ||
          (desc.hasEnumerable() &&
           desc.isEnumerable() != current.isEnumerable())) {
        if (should_throw) {
          throw MakeTypeError("redefine_disallowed", [p]);
        } else {
          return false;
        }
      }
      // Step 8
      if (!IsGenericDescriptor(desc)) {
        // Step 9a
        if (IsDataDescriptor(current) != IsDataDescriptor(desc)) {
          if (should_throw) {
            throw MakeTypeError("redefine_disallowed", [p]);
          } else {
            return false;
          }
        }
        // Step 10a
        if (IsDataDescriptor(current) && IsDataDescriptor(desc)) {
          if (!current.isWritable() && desc.isWritable()) {
            if (should_throw) {
              throw MakeTypeError("redefine_disallowed", [p]);
            } else {
              return false;
            }
          }
          if (!current.isWritable() && desc.hasValue() &&
              !SameValue(desc.getValue(), current.getValue())) {
            if (should_throw) {
              throw MakeTypeError("redefine_disallowed", [p]);
            } else {
              return false;
            }
          }
        }
        // Step 11
        if (IsAccessorDescriptor(desc) && IsAccessorDescriptor(current)) {
          if (desc.hasSetter() && !SameValue(desc.getSet(), current.getSet())) {
            if (should_throw) {
              throw MakeTypeError("redefine_disallowed", [p]);
            } else {
              return false;
            }
          }
          if (desc.hasGetter() && !SameValue(desc.getGet(),current.getGet())) {
            if (should_throw) {
              throw MakeTypeError("redefine_disallowed", [p]);
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

    %DefineOrRedefineDataProperty(obj, p, value, flag);
  } else {
    // There are 3 cases that lead here:
    // Step 4b - defining a new accessor property.
    // Steps 9c & 12 - replacing an existing data property with an accessor
    //                 property.
    // Step 12 - updating an existing accessor property with an accessor
    //           descriptor.
    var getter = desc.hasGetter() ? desc.getGet() : null;
    var setter = desc.hasSetter() ? desc.getSet() : null;
    %DefineOrRedefineAccessorProperty(obj, p, getter, setter, flag);
  }
  return true;
}


// ES5 section 15.4.5.1.
function DefineArrayProperty(obj, p, desc, should_throw) {
  // Note that the length of an array is not actually stored as part of the
  // property, hence we use generated code throughout this function instead of
  // DefineObjectProperty() to modify its value.

  // Step 3 - Special handling for length property.
  if (p === "length") {
    var length = obj.length;
    var old_length = length;
    if (!desc.hasValue()) {
      return DefineObjectProperty(obj, "length", desc, should_throw);
    }
    var new_length = ToUint32(desc.getValue());
    if (new_length != ToNumber(desc.getValue())) {
      throw new $RangeError('defineProperty() array length out of range');
    }
    var length_desc = GetOwnProperty(obj, "length");
    if (new_length != length && !length_desc.isWritable()) {
      if (should_throw) {
        throw MakeTypeError("redefine_disallowed", [p]);
      } else {
        return false;
      }
    }
    var threw = false;

    var emit_splice = %IsObserved(obj) && new_length !== old_length;
    var removed;
    if (emit_splice) {
      BeginPerformSplice(obj);
      removed = [];
      if (new_length < old_length)
        removed.length = old_length - new_length;
    }

    while (new_length < length--) {
      var index = ToString(length);
      if (emit_splice) {
        var deletedDesc = GetOwnProperty(obj, index);
        if (deletedDesc && deletedDesc.hasValue())
          removed[length - new_length] = deletedDesc.getValue();
      }
      if (!Delete(obj, index, false)) {
        new_length = length + 1;
        threw = true;
        break;
      }
    }
    // Make sure the below call to DefineObjectProperty() doesn't overwrite
    // any magic "length" property by removing the value.
    // TODO(mstarzinger): This hack should be removed once we have addressed the
    // respective TODO in Runtime_DefineOrRedefineDataProperty.
    // For the time being, we need a hack to prevent Object.observe from
    // generating two change records.
    obj.length = new_length;
    desc.value_ = UNDEFINED;
    desc.hasValue_ = false;
    threw = !DefineObjectProperty(obj, "length", desc, should_throw) || threw;
    if (emit_splice) {
      EndPerformSplice(obj);
      EnqueueSpliceRecord(obj,
          new_length < old_length ? new_length : old_length,
          removed,
          new_length > old_length ? new_length - old_length : 0);
    }
    if (threw) {
      if (should_throw) {
        throw MakeTypeError("redefine_disallowed", [p]);
      } else {
        return false;
      }
    }
    return true;
  }

  // Step 4 - Special handling for array index.
  var index = ToUint32(p);
  var emit_splice = false;
  if (ToString(index) == p && index != 4294967295) {
    var length = obj.length;
    if (index >= length && %IsObserved(obj)) {
      emit_splice = true;
      BeginPerformSplice(obj);
    }

    var length_desc = GetOwnProperty(obj, "length");
    if ((index >= length && !length_desc.isWritable()) ||
        !DefineObjectProperty(obj, p, desc, true)) {
      if (emit_splice)
        EndPerformSplice(obj);
      if (should_throw) {
        throw MakeTypeError("define_disallowed", [p]);
      } else {
        return false;
      }
    }
    if (index >= length) {
      obj.length = index + 1;
    }
    if (emit_splice) {
      EndPerformSplice(obj);
      EnqueueSpliceRecord(obj, length, [], index + 1 - length);
    }
    return true;
  }

  // Step 5 - Fallback to default implementation.
  return DefineObjectProperty(obj, p, desc, should_throw);
}


// ES5 section 8.12.9, ES5 section 15.4.5.1 and Harmony proxies.
function DefineOwnProperty(obj, p, desc, should_throw) {
  if (%IsJSProxy(obj)) {
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


// ES5 section 15.2.3.2.
function ObjectGetPrototypeOf(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.getPrototypeOf"]);
  }
  return %GetPrototype(obj);
}

// ES6 section 19.1.2.19.
function ObjectSetPrototypeOf(obj, proto) {
  CHECK_OBJECT_COERCIBLE(obj, "Object.setPrototypeOf");

  if (proto !== null && !IS_SPEC_OBJECT(proto)) {
    throw MakeTypeError("proto_object_or_null", [proto]);
  }

  if (IS_SPEC_OBJECT(obj)) {
    %SetPrototype(obj, proto);
  }

  return obj;
}


// ES5 section 15.2.3.3
function ObjectGetOwnPropertyDescriptor(obj, p) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object",
                        ["Object.getOwnPropertyDescriptor"]);
  }
  var desc = GetOwnProperty(obj, p);
  return FromPropertyDescriptor(desc);
}


// For Harmony proxies
function ToNameArray(obj, trap, includeSymbols) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("proxy_non_object_prop_names", [obj, trap]);
  }
  var n = ToUint32(obj.length);
  var array = new $Array(n);
  var realLength = 0;
  var names = { __proto__: null };  // TODO(rossberg): use sets once ready.
  for (var index = 0; index < n; index++) {
    var s = ToName(obj[index]);
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(s) && !includeSymbols) continue;
    if (%HasLocalProperty(names, s)) {
      throw MakeTypeError("proxy_repeated_prop_name", [obj, trap, s]);
    }
    array[index] = s;
    ++realLength;
    names[s] = 0;
  }
  array.length = realLength;
  return array;
}


function ObjectGetOwnPropertyKeys(obj, symbolsOnly) {
  var nameArrays = new InternalArray();
  var filter = symbolsOnly ?
      PROPERTY_ATTRIBUTES_STRING | PROPERTY_ATTRIBUTES_PRIVATE_SYMBOL :
      PROPERTY_ATTRIBUTES_SYMBOLIC;

  // Find all the indexed properties.

  // Only get the local element names if we want to include string keys.
  if (!symbolsOnly) {
    var localElementNames = %GetLocalElementNames(obj);
    for (var i = 0; i < localElementNames.length; ++i) {
      localElementNames[i] = %_NumberToString(localElementNames[i]);
    }
    nameArrays.push(localElementNames);

    // Get names for indexed interceptor properties.
    var interceptorInfo = %GetInterceptorInfo(obj);
    if ((interceptorInfo & 1) != 0) {
      var indexedInterceptorNames = %GetIndexedInterceptorElementNames(obj);
      if (!IS_UNDEFINED(indexedInterceptorNames)) {
        nameArrays.push(indexedInterceptorNames);
      }
    }
  }

  // Find all the named properties.

  // Get the local property names.
  nameArrays.push(%GetLocalPropertyNames(obj, filter));

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
      if (symbolsOnly) {
        if (!IS_SYMBOL(name) || IS_PRIVATE(name)) continue;
      } else {
        if (IS_SYMBOL(name)) continue;
        name = ToString(name);
      }
      if (seenKeys[name]) continue;
      seenKeys[name] = true;
      propertyNames[j++] = name;
    }
    propertyNames.length = j;
  }

  return propertyNames;
}


// ES5 section 15.2.3.4.
function ObjectGetOwnPropertyNames(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.getOwnPropertyNames"]);
  }
  // Special handling for proxies.
  if (%IsJSProxy(obj)) {
    var handler = %GetHandler(obj);
    var names = CallTrap0(handler, "getOwnPropertyNames", UNDEFINED);
    return ToNameArray(names, "getOwnPropertyNames", false);
  }

  return ObjectGetOwnPropertyKeys(obj, false);
}


// ES5 section 15.2.3.5.
function ObjectCreate(proto, properties) {
  if (!IS_SPEC_OBJECT(proto) && proto !== null) {
    throw MakeTypeError("proto_object_or_null", [proto]);
  }
  var obj = { __proto__: proto };
  if (!IS_UNDEFINED(properties)) ObjectDefineProperties(obj, properties);
  return obj;
}


// ES5 section 15.2.3.6.
function ObjectDefineProperty(obj, p, attributes) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.defineProperty"]);
  }
  var name = ToName(p);
  if (%IsJSProxy(obj)) {
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
      if (!(%HasLocalProperty(standardNames, N))) {
        var attr = GetOwnProperty(attributes, N);
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


function GetOwnEnumerablePropertyNames(properties) {
  var names = new InternalArray();
  for (var key in properties) {
    if (%HasLocalProperty(properties, key)) {
      names.push(key);
    }
  }
  return names;
}


// ES5 section 15.2.3.7.
function ObjectDefineProperties(obj, properties) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.defineProperties"]);
  }
  var props = ToObject(properties);
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
    throw MakeTypeError("handler_returned_undefined", [handler, "fix"]);
  }

  if (%IsJSFunctionProxy(obj)) {
    var callTrap = %GetCallTrap(obj);
    var constructTrap = %GetConstructTrap(obj);
    var code = DelegateCallAndConstruct(callTrap, constructTrap);
    %Fix(obj);  // becomes a regular function
    %SetCode(obj, code);
    // TODO(rossberg): What about length and other properties? Not specified.
    // We just put in some half-reasonable defaults for now.
    var prototype = new $Object();
    $Object.defineProperty(prototype, "constructor",
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
function ObjectSeal(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.seal"]);
  }
  if (%IsJSProxy(obj)) {
    ProxyFix(obj);
  }
  var names = ObjectGetOwnPropertyNames(obj);
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var desc = GetOwnProperty(obj, name);
    if (desc.isConfigurable()) {
      desc.setConfigurable(false);
      DefineOwnProperty(obj, name, desc, true);
    }
  }
  %PreventExtensions(obj);
  return obj;
}


// ES5 section 15.2.3.9.
function ObjectFreeze(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.freeze"]);
  }
  var isProxy = %IsJSProxy(obj);
  if (isProxy || %HasSloppyArgumentsElements(obj) || %IsObserved(obj)) {
    if (isProxy) {
      ProxyFix(obj);
    }
    var names = ObjectGetOwnPropertyNames(obj);
    for (var i = 0; i < names.length; i++) {
      var name = names[i];
      var desc = GetOwnProperty(obj, name);
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
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.preventExtension"]);
  }
  if (%IsJSProxy(obj)) {
    ProxyFix(obj);
  }
  %PreventExtensions(obj);
  return obj;
}


// ES5 section 15.2.3.11
function ObjectIsSealed(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.isSealed"]);
  }
  if (%IsJSProxy(obj)) {
    return false;
  }
  if (%IsExtensible(obj)) {
    return false;
  }
  var names = ObjectGetOwnPropertyNames(obj);
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var desc = GetOwnProperty(obj, name);
    if (desc.isConfigurable()) return false;
  }
  return true;
}


// ES5 section 15.2.3.12
function ObjectIsFrozen(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.isFrozen"]);
  }
  if (%IsJSProxy(obj)) {
    return false;
  }
  if (%IsExtensible(obj)) {
    return false;
  }
  var names = ObjectGetOwnPropertyNames(obj);
  for (var i = 0; i < names.length; i++) {
    var name = names[i];
    var desc = GetOwnProperty(obj, name);
    if (IsDataDescriptor(desc) && desc.isWritable()) return false;
    if (desc.isConfigurable()) return false;
  }
  return true;
}


// ES5 section 15.2.3.13
function ObjectIsExtensible(obj) {
  if (!IS_SPEC_OBJECT(obj)) {
    throw MakeTypeError("called_on_non_object", ["Object.isExtensible"]);
  }
  if (%IsJSProxy(obj)) {
    return true;
  }
  return %IsExtensible(obj);
}


// Harmony egal.
function ObjectIs(obj1, obj2) {
  if (obj1 === obj2) {
    return (obj1 !== 0) || (1 / obj1 === 1 / obj2);
  } else {
    return (obj1 !== obj1) && (obj2 !== obj2);
  }
}


// ECMA-262, Edition 6, section B.2.2.1.1
function ObjectGetProto() {
  return %GetPrototype(ToObject(this));
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
    return ToObject(x);
  } else {
    if (x == null) return { };
    return ToObject(x);
  }
}


// ----------------------------------------------------------------------------
// Object

function SetUpObject() {
  %CheckIsBootstrapping();

  %SetNativeFlag($Object);
  %SetCode($Object, ObjectConstructor);
  %SetExpectedNumberOfProperties($Object, 4);

  %SetProperty($Object.prototype, "constructor", $Object, DONT_ENUM);

  // Set up non-enumerable functions on the Object.prototype object.
  InstallFunctions($Object.prototype, DONT_ENUM, $Array(
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
  ));
  InstallGetterSetter($Object.prototype, "__proto__",
                      ObjectGetProto, ObjectSetProto);

  // Set up non-enumerable functions in the Object object.
  InstallFunctions($Object, DONT_ENUM, $Array(
    "keys", ObjectKeys,
    "create", ObjectCreate,
    "defineProperty", ObjectDefineProperty,
    "defineProperties", ObjectDefineProperties,
    "freeze", ObjectFreeze,
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
    "seal", ObjectSeal
    // deliverChangeRecords, getNotifier, observe and unobserve are added
    // in object-observe.js.
  ));
}

SetUpObject();


// ----------------------------------------------------------------------------
// Boolean

function BooleanConstructor(x) {
  if (%_IsConstructCall()) {
    %_SetValueOf(this, ToBoolean(x));
  } else {
    return ToBoolean(x);
  }
}


function BooleanToString() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var b = this;
  if (!IS_BOOLEAN(b)) {
    if (!IS_BOOLEAN_WRAPPER(b)) {
      throw new $TypeError('Boolean.prototype.toString is not generic');
    }
    b = %_ValueOf(b);
  }
  return b ? 'true' : 'false';
}


function BooleanValueOf() {
  // NOTE: Both Boolean objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_BOOLEAN(this) && !IS_BOOLEAN_WRAPPER(this)) {
    throw new $TypeError('Boolean.prototype.valueOf is not generic');
  }
  return %_ValueOf(this);
}


// ----------------------------------------------------------------------------

function SetUpBoolean () {
  %CheckIsBootstrapping();

  %SetCode($Boolean, BooleanConstructor);
  %FunctionSetPrototype($Boolean, new $Boolean(false));
  %SetProperty($Boolean.prototype, "constructor", $Boolean, DONT_ENUM);

  InstallFunctions($Boolean.prototype, DONT_ENUM, $Array(
    "toString", BooleanToString,
    "valueOf", BooleanValueOf
  ));
}

SetUpBoolean();


// ----------------------------------------------------------------------------
// Number

function NumberConstructor(x) {
  var value = %_ArgumentsLength() == 0 ? 0 : ToNumber(x);
  if (%_IsConstructCall()) {
    %_SetValueOf(this, value);
  } else {
    return value;
  }
}


// ECMA-262 section 15.7.4.2.
function NumberToString(radix) {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  var number = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw new $TypeError('Number.prototype.toString is not generic');
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
  if (radix < 2 || radix > 36) {
    throw new $RangeError('toString() radix argument must be between 2 and 36');
  }
  // Convert the number to a string in the given radix.
  return %NumberToRadixString(number, radix);
}


// ECMA-262 section 15.7.4.3
function NumberToLocaleString() {
  return %_CallFunction(this, NumberToString);
}


// ECMA-262 section 15.7.4.4
function NumberValueOf() {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_NUMBER(this) && !IS_NUMBER_WRAPPER(this)) {
    throw new $TypeError('Number.prototype.valueOf is not generic');
  }
  return %_ValueOf(this);
}


// ECMA-262 section 15.7.4.5
function NumberToFixed(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError("incompatible_method_receiver",
                          ["Number.prototype.toFixed", this]);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  var f = TO_INTEGER(fractionDigits);

  if (f < 0 || f > 20) {
    throw new $RangeError("toFixed() digits argument must be between 0 and 20");
  }

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  return %NumberToFixed(x, f);
}


// ECMA-262 section 15.7.4.6
function NumberToExponential(fractionDigits) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError("incompatible_method_receiver",
                          ["Number.prototype.toExponential", this]);
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
    throw new $RangeError("toExponential() argument must be between 0 and 20");
  }
  return %NumberToExponential(x, f);
}


// ECMA-262 section 15.7.4.7
function NumberToPrecision(precision) {
  var x = this;
  if (!IS_NUMBER(this)) {
    if (!IS_NUMBER_WRAPPER(this)) {
      throw MakeTypeError("incompatible_method_receiver",
                          ["Number.prototype.toPrecision", this]);
    }
    // Get the value of this number in case it's an object.
    x = %_ValueOf(this);
  }
  if (IS_UNDEFINED(precision)) return ToString(%_ValueOf(this));
  var p = TO_INTEGER(precision);

  if (NUMBER_IS_NAN(x)) return "NaN";
  if (x == INFINITY) return "Infinity";
  if (x == -INFINITY) return "-Infinity";

  if (p < 1 || p > 21) {
    throw new $RangeError("toPrecision() argument must be between 1 and 21");
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
    if (integral == number)
      return MathAbs(integral) <= $Number.MAX_SAFE_INTEGER;
  }
  return false;
}


// ----------------------------------------------------------------------------

function SetUpNumber() {
  %CheckIsBootstrapping();

  %SetCode($Number, NumberConstructor);
  %FunctionSetPrototype($Number, new $Number(0));

  %OptimizeObjectForAddingMultipleProperties($Number.prototype, 8);
  // Set up the constructor property on the Number prototype object.
  %SetProperty($Number.prototype, "constructor", $Number, DONT_ENUM);

  InstallConstants($Number, $Array(
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
  ));

  // Set up non-enumerable functions on the Number prototype object.
  InstallFunctions($Number.prototype, DONT_ENUM, $Array(
    "toString", NumberToString,
    "toLocaleString", NumberToLocaleString,
    "valueOf", NumberValueOf,
    "toFixed", NumberToFixed,
    "toExponential", NumberToExponential,
    "toPrecision", NumberToPrecision
  ));

  // Harmony Number constructor additions
  InstallFunctions($Number, DONT_ENUM, $Array(
    "isFinite", NumberIsFinite,
    "isInteger", NumberIsInteger,
    "isNaN", NumberIsNaN,
    "isSafeInteger", NumberIsSafeInteger,
    "parseInt", GlobalParseInt,
    "parseFloat", GlobalParseFloat
  ));
}

SetUpNumber();


// ----------------------------------------------------------------------------
// Function

function FunctionSourceString(func) {
  while (%IsJSFunctionProxy(func)) {
    func = %GetCallTrap(func);
  }

  if (!IS_FUNCTION(func)) {
    throw new $TypeError('Function.prototype.toString is not generic');
  }

  var source = %FunctionGetSourceCode(func);
  if (!IS_STRING(source) || %FunctionIsBuiltin(func)) {
    var name = %FunctionGetName(func);
    if (name) {
      // Mimic what KJS does.
      return 'function ' + name + '() { [native code] }';
    } else {
      return 'function () { [native code] }';
    }
  }

  var name = %FunctionNameShouldPrintAsAnonymous(func)
      ? 'anonymous'
      : %FunctionGetName(func);
  var head = %FunctionIsGenerator(func) ? 'function* ' : 'function ';
  return head + name + source;
}


function FunctionToString() {
  return FunctionSourceString(this);
}


// ES5 15.3.4.5
function FunctionBind(this_arg) { // Length is 1.
  if (!IS_SPEC_FUNCTION(this)) {
    throw new $TypeError('Bind must be called on a function');
  }
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

  %FunctionRemovePrototype(boundFunction);
  var new_length = 0;
  if (%_ClassOf(this) == "Function") {
    // Function or FunctionProxy.
    var old_length = this.length;
    // FunctionProxies might provide a non-UInt32 value. If so, ignore it.
    if ((typeof old_length === "number") &&
        ((old_length >>> 0) === old_length)) {
      var argc = %_ArgumentsLength();
      if (argc > 0) argc--;  // Don't count the thisArg as parameter.
      new_length = old_length - argc;
      if (new_length < 0) new_length = 0;
    }
  }
  // This runtime function finds any remaining arguments on the stack,
  // so we don't pass the arguments object.
  var result = %FunctionBindArguments(boundFunction, this,
                                      this_arg, new_length);

  // We already have caller and arguments properties on functions,
  // which are non-configurable. It therefore makes no sence to
  // try to redefine these as defined by the spec. The spec says
  // that bind should make these throw a TypeError if get or set
  // is called and make them non-enumerable and non-configurable.
  // To be consistent with our normal functions we leave this as it is.
  // TODO(lrn): Do set these to be thrower.
  return result;
}


function NewFunctionString(arguments, function_token) {
  var n = arguments.length;
  var p = '';
  if (n > 1) {
    p = ToString(arguments[0]);
    for (var i = 1; i < n - 1; i++) {
      p += ',' + ToString(arguments[i]);
    }
    // If the formal parameters string include ) - an illegal
    // character - it may make the combined function expression
    // compile. We avoid this problem by checking for this early on.
    if (%_CallFunction(p, ')', StringIndexOf) != -1) {
      throw MakeSyntaxError('paren_in_arg_string', []);
    }
    // If the formal parameters include an unbalanced block comment, the
    // function must be rejected. Since JavaScript does not allow nested
    // comments we can include a trailing block comment to catch this.
    p += '\n/' + '**/';
  }
  var body = (n > 0) ? ToString(arguments[n - 1]) : '';
  return '(' + function_token + '(' + p + ') {\n' + body + '\n})';
}


function FunctionConstructor(arg1) {  // length == 1
  var source = NewFunctionString(arguments, 'function');
  var global_receiver = %GlobalReceiver(global);
  // Compile the string in the constructor and not a helper so that errors
  // appear to come from here.
  var f = %_CallFunction(global_receiver, %CompileString(source, true));
  %FunctionMarkNameShouldPrintAsAnonymous(f);
  return f;
}


// ----------------------------------------------------------------------------

function SetUpFunction() {
  %CheckIsBootstrapping();

  %SetCode($Function, FunctionConstructor);
  %SetProperty($Function.prototype, "constructor", $Function, DONT_ENUM);

  InstallFunctions($Function.prototype, DONT_ENUM, $Array(
    "bind", FunctionBind,
    "toString", FunctionToString
  ));
}

SetUpFunction();


//----------------------------------------------------------------------------

// TODO(rossberg): very simple abstraction for generic microtask queue.
// Eventually, we should move to a real event queue that allows to maintain
// relative ordering of different kinds of tasks.

function GetMicrotaskQueue() {
  var microtaskState = %GetMicrotaskState();
  if (IS_UNDEFINED(microtaskState.queue)) {
    microtaskState.queue = new InternalArray;
  }
  return microtaskState.queue;
}

function RunMicrotasks() {
  while (%SetMicrotaskPending(false)) {
    var microtaskState = %GetMicrotaskState();
    if (IS_UNDEFINED(microtaskState.queue))
      return;

    var microtasks = microtaskState.queue;
    microtaskState.queue = new InternalArray;

    for (var i = 0; i < microtasks.length; i++) {
      microtasks[i]();
    }
  }
}

function EnqueueExternalMicrotask(fn) {
  GetMicrotaskQueue().push(fn);
  %SetMicrotaskPending(true);
}
