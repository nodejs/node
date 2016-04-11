// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// ----------------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var GlobalBoolean = global.Boolean;
var GlobalNumber = global.Number;
var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var iteratorSymbol = utils.ImportNow("iterator_symbol");
var MakeRangeError;
var MakeSyntaxError;
var MakeTypeError;
var MathAbs;
var NaN = %GetRootNaN();
var ObjectToString = utils.ImportNow("object_to_string");
var ObserveBeginPerformSplice;
var ObserveEndPerformSplice;
var ObserveEnqueueSpliceRecord;
var SameValue = utils.ImportNow("SameValue");
var toStringTagSymbol = utils.ImportNow("to_string_tag_symbol");

utils.Import(function(from) {
  MakeRangeError = from.MakeRangeError;
  MakeSyntaxError = from.MakeSyntaxError;
  MakeTypeError = from.MakeTypeError;
  MathAbs = from.MathAbs;
  ObserveBeginPerformSplice = from.ObserveBeginPerformSplice;
  ObserveEndPerformSplice = from.ObserveEndPerformSplice;
  ObserveEnqueueSpliceRecord = from.ObserveEnqueueSpliceRecord;
});

// ----------------------------------------------------------------------------


// ES6 18.2.3 isNaN(number)
function GlobalIsNaN(number) {
  number = TO_NUMBER(number);
  return NUMBER_IS_NAN(number);
}


// ES6 18.2.2 isFinite(number)
function GlobalIsFinite(number) {
  number = TO_NUMBER(number);
  return NUMBER_IS_FINITE(number);
}


// ES6 18.2.5 parseInt(string, radix)
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
    string = TO_STRING(string);
    radix = radix | 0;
  } else {
    // The spec says ToString should be evaluated before ToInt32.
    string = TO_STRING(string);
    radix = TO_INT32(radix);
    if (!(radix == 0 || (2 <= radix && radix <= 36))) {
      return NaN;
    }
  }

  if (%_HasCachedArrayIndex(string) &&
      (radix == 0 || radix == 10)) {
    return %_GetCachedArrayIndex(string);
  }
  return %StringParseInt(string, radix);
}


// ES6 18.2.4 parseFloat(string)
function GlobalParseFloat(string) {
  // 1. Let inputString be ? ToString(string).
  string = TO_STRING(string);
  if (%_HasCachedArrayIndex(string)) return %_GetCachedArrayIndex(string);
  return %StringParseFloat(string);
}


// ----------------------------------------------------------------------------

// Set up global object.
var attributes = DONT_ENUM | DONT_DELETE | READ_ONLY;

utils.InstallConstants(global, [
  // ES6 18.1.1
  "Infinity", INFINITY,
  // ES6 18.1.2
  "NaN", NaN,
  // ES6 18.1.3
  "undefined", UNDEFINED,
]);

// Set up non-enumerable function on the global object.
utils.InstallFunctions(global, DONT_ENUM, [
  "isNaN", GlobalIsNaN,
  "isFinite", GlobalIsFinite,
  "parseInt", GlobalParseInt,
  "parseFloat", GlobalParseFloat,
]);


// ----------------------------------------------------------------------------
// Object

// ES6 19.1.3.5 Object.prototype.toLocaleString([reserved1 [,reserved2]])
function ObjectToLocaleString() {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.toLocaleString");
  return this.toString();
}


// ES6 19.1.3.7 Object.prototype.valueOf()
function ObjectValueOf() {
  return TO_OBJECT(this);
}


// ES6 7.3.11
function ObjectHasOwnProperty(value) {
  var name = TO_NAME(value);
  var object = TO_OBJECT(this);
  return %HasOwnProperty(object, name);
}


// ES6 19.1.3.3 Object.prototype.isPrototypeOf(V)
function ObjectIsPrototypeOf(V) {
  if (!IS_RECEIVER(V)) return false;
  var O = TO_OBJECT(this);
  return %HasInPrototypeChain(V, O);
}


// ES6 19.1.3.4
function ObjectPropertyIsEnumerable(V) {
  var P = TO_NAME(V);
  return %PropertyIsEnumerable(TO_OBJECT(this), P);
}


// Extensions for providing property getters and setters.
function ObjectDefineGetter(name, fun) {
  var receiver = this;
  if (IS_NULL(receiver) || IS_UNDEFINED(receiver)) {
    receiver = %GlobalProxy(ObjectDefineGetter);
  }
  if (!IS_CALLABLE(fun)) {
    throw MakeTypeError(kObjectGetterExpectingFunction);
  }
  var desc = new PropertyDescriptor();
  desc.setGet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(TO_OBJECT(receiver), TO_NAME(name), desc, false);
}


function ObjectLookupGetter(name) {
  var receiver = this;
  if (IS_NULL(receiver) || IS_UNDEFINED(receiver)) {
    receiver = %GlobalProxy(ObjectLookupGetter);
  }
  return %LookupAccessor(TO_OBJECT(receiver), TO_NAME(name), GETTER);
}


function ObjectDefineSetter(name, fun) {
  var receiver = this;
  if (IS_NULL(receiver) || IS_UNDEFINED(receiver)) {
    receiver = %GlobalProxy(ObjectDefineSetter);
  }
  if (!IS_CALLABLE(fun)) {
    throw MakeTypeError(kObjectSetterExpectingFunction);
  }
  var desc = new PropertyDescriptor();
  desc.setSet(fun);
  desc.setEnumerable(true);
  desc.setConfigurable(true);
  DefineOwnProperty(TO_OBJECT(receiver), TO_NAME(name), desc, false);
}


function ObjectLookupSetter(name) {
  var receiver = this;
  if (IS_NULL(receiver) || IS_UNDEFINED(receiver)) {
    receiver = %GlobalProxy(ObjectLookupSetter);
  }
  return %LookupAccessor(TO_OBJECT(receiver), TO_NAME(name), SETTER);
}


// ES6 6.2.4.1
function IsAccessorDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return desc.hasGetter() || desc.hasSetter();
}


// ES6 6.2.4.2
function IsDataDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return desc.hasValue() || desc.hasWritable();
}


// ES6 6.2.4.3
function IsGenericDescriptor(desc) {
  if (IS_UNDEFINED(desc)) return false;
  return !(IsAccessorDescriptor(desc) || IsDataDescriptor(desc));
}


function IsInconsistentDescriptor(desc) {
  return IsAccessorDescriptor(desc) && IsDataDescriptor(desc);
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


// ES6 6.2.4.5
function ToPropertyDescriptor(obj) {
  if (!IS_RECEIVER(obj)) throw MakeTypeError(kPropertyDescObject, obj);

  var desc = new PropertyDescriptor();

  if ("enumerable" in obj) {
    desc.setEnumerable(TO_BOOLEAN(obj.enumerable));
  }

  if ("configurable" in obj) {
    desc.setConfigurable(TO_BOOLEAN(obj.configurable));
  }

  if ("value" in obj) {
    desc.setValue(obj.value);
  }

  if ("writable" in obj) {
    desc.setWritable(TO_BOOLEAN(obj.writable));
  }

  if ("get" in obj) {
    var get = obj.get;
    if (!IS_UNDEFINED(get) && !IS_CALLABLE(get)) {
      throw MakeTypeError(kObjectGetterCallable, get);
    }
    desc.setGet(get);
  }

  if ("set" in obj) {
    var set = obj.set;
    if (!IS_UNDEFINED(set) && !IS_CALLABLE(set)) {
      throw MakeTypeError(kObjectSetterCallable, set);
    }
    desc.setSet(set);
  }

  if (IsInconsistentDescriptor(desc)) {
    throw MakeTypeError(kValueAndAccessor, obj);
  }
  return desc;
}

// TODO(cbruni): remove once callers have been removed
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
      throw MakeTypeError(kIllegalInvocation);
    }
    trap = defaultTrap;
  } else if (!IS_CALLABLE(trap)) {
    throw MakeTypeError(kIllegalInvocation);
  }
  return trap;
}


function CallTrap1(handler, name, defaultTrap, x) {
  return %_Call(GetTrap(handler, name, defaultTrap), handler, x);
}


function CallTrap2(handler, name, defaultTrap, x, y) {
  return %_Call(GetTrap(handler, name, defaultTrap), handler, x, y);
}


// ES5 section 8.12.1.
// TODO(jkummerow): Deprecated. Migrate all callers to
// ObjectGetOwnPropertyDescriptor and delete this.
function GetOwnPropertyJS(obj, v) {
  var p = TO_NAME(v);
  if (IS_PROXY(obj)) {
    // TODO(rossberg): adjust once there is a story for symbols vs proxies.
    if (IS_SYMBOL(v)) return UNDEFINED;

    var handler = %JSProxyGetHandler(obj);
    var descriptor = CallTrap1(
                         handler, "getOwnPropertyDescriptor", UNDEFINED, p);
    if (IS_UNDEFINED(descriptor)) return descriptor;
    var desc = ToCompletePropertyDescriptor(descriptor);
    if (!desc.isConfigurable()) {
      throw MakeTypeError(kIllegalInvocation);
    }
    return desc;
  }

  // GetOwnProperty returns an array indexed by the constants
  // defined in macros.py.
  // If p is not a property on obj undefined is returned.
  var props = %GetOwnProperty_Legacy(TO_OBJECT(obj), p);

  return ConvertDescriptorArrayToDescriptor(props);
}


// ES6 7.3.9
function GetMethod(obj, p) {
  var func = obj[p];
  if (IS_NULL_OR_UNDEFINED(func)) return UNDEFINED;
  if (IS_CALLABLE(func)) return func;
  throw MakeTypeError(kCalledNonCallable, typeof func);
}


// Harmony proxies.
function DefineProxyProperty(obj, p, attributes, should_throw) {
  // TODO(rossberg): adjust once there is a story for symbols vs proxies.
  if (IS_SYMBOL(p)) return false;

  var handler = %JSProxyGetHandler(obj);
  var result = CallTrap2(handler, "defineProperty", UNDEFINED, p, attributes);
  if (!result) {
    if (should_throw) {
      throw MakeTypeError(kIllegalInvocation);
    } else {
      return false;
    }
  }
  return true;
}


// ES6 9.1.6 [[DefineOwnProperty]](P, Desc)
function DefineObjectProperty(obj, p, desc, should_throw) {
  var current_array = %GetOwnProperty_Legacy(obj, TO_NAME(p));
  var current = ConvertDescriptorArrayToDescriptor(current_array);
  var extensible = %object_is_extensible(obj);

  if (IS_UNDEFINED(current) && !extensible) {
    if (should_throw) {
      throw MakeTypeError(kDefineDisallowed, p);
    } else {
      return false;
    }
  }

  if (!IS_UNDEFINED(current)) {
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
              !SameValue(desc.getValue(), current.getValue())) {
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
              !SameValue(desc.getSet(), current.getSet())) {
            if (should_throw) {
              throw MakeTypeError(kRedefineDisallowed, p);
            } else {
              return false;
            }
          }
          if (desc.hasGetter() && !SameValue(desc.getGet(),current.getGet())) {
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
    var index = TO_UINT32(p);
    var emit_splice = false;
    if (TO_STRING(index) == p && index != 4294967295) {
      var length = obj.length;
      if (index >= length && %IsObserved(obj)) {
        emit_splice = true;
        ObserveBeginPerformSplice(obj);
      }

      var length_desc = GetOwnPropertyJS(obj, "length");
      if ((index >= length && !length_desc.isWritable()) ||
          !DefineObjectProperty(obj, p, desc, true)) {
        if (emit_splice)
          ObserveEndPerformSplice(obj);
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
        ObserveEndPerformSplice(obj);
        ObserveEnqueueSpliceRecord(obj, length, [], index + 1 - length);
      }
      return true;
    }
  }

  // Step 5 - Fallback to default implementation.
  return DefineObjectProperty(obj, p, desc, should_throw);
}


// ES5 section 8.12.9, ES5 section 15.4.5.1 and Harmony proxies.
function DefineOwnProperty(obj, p, desc, should_throw) {
  if (IS_PROXY(obj)) {
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


// ES6 section 19.1.2.9
function ObjectGetPrototypeOf(obj) {
  return %_GetPrototype(TO_OBJECT(obj));
}

// ES6 section 19.1.2.18.
function ObjectSetPrototypeOf(obj, proto) {
  CHECK_OBJECT_COERCIBLE(obj, "Object.setPrototypeOf");

  if (proto !== null && !IS_RECEIVER(proto)) {
    throw MakeTypeError(kProtoObjectOrNull, proto);
  }

  if (IS_RECEIVER(obj)) {
    %SetPrototype(obj, proto);
  }

  return obj;
}


// ES6 section 19.1.2.6
function ObjectGetOwnPropertyDescriptor(obj, p) {
  return %GetOwnProperty(obj, p);
}


// ES5 section 15.2.3.4.
function ObjectGetOwnPropertyNames(obj) {
  obj = TO_OBJECT(obj);
  return %GetOwnPropertyKeys(obj, PROPERTY_FILTER_SKIP_SYMBOLS);
}


// ES5 section 15.2.3.6.
function ObjectDefineProperty(obj, p, attributes) {
  // The new pure-C++ implementation doesn't support O.o.
  // TODO(jkummerow): Implement missing features and remove fallback path.
  if (%IsObserved(obj)) {
    if (!IS_RECEIVER(obj)) {
      throw MakeTypeError(kCalledOnNonObject, "Object.defineProperty");
    }
    var name = TO_NAME(p);
    var desc = ToPropertyDescriptor(attributes);
    DefineOwnProperty(obj, name, desc, true);
    return obj;
  }
  return %ObjectDefineProperty(obj, p, attributes);
}


function GetOwnEnumerablePropertyNames(object) {
  return %GetOwnPropertyKeys(object, PROPERTY_FILTER_ONLY_ENUMERABLE);
}


// ES5 section 15.2.3.7.
function ObjectDefineProperties(obj, properties) {
  // The new pure-C++ implementation doesn't support O.o.
  // TODO(jkummerow): Implement missing features and remove fallback path.
  if (%IsObserved(obj)) {
    if (!IS_RECEIVER(obj)) {
      throw MakeTypeError(kCalledOnNonObject, "Object.defineProperties");
    }
    var props = TO_OBJECT(properties);
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
  return %ObjectDefineProperties(obj, properties);
}


// ES6 B.2.2.1.1
function ObjectGetProto() {
  return %_GetPrototype(TO_OBJECT(this));
}


// ES6 B.2.2.1.2
function ObjectSetProto(proto) {
  CHECK_OBJECT_COERCIBLE(this, "Object.prototype.__proto__");

  if ((IS_RECEIVER(proto) || IS_NULL(proto)) && IS_RECEIVER(this)) {
    %SetPrototype(this, proto);
  }
}


// ES6 19.1.1.1
function ObjectConstructor(x) {
  if (GlobalObject != new.target && !IS_UNDEFINED(new.target)) {
    return this;
  }
  if (IS_NULL(x) || IS_UNDEFINED(x)) return {};
  return TO_OBJECT(x);
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
  // assign is added in bootstrapper.cc.
  // keys is added in bootstrapper.cc.
  "defineProperty", ObjectDefineProperty,
  "defineProperties", ObjectDefineProperties,
  "getPrototypeOf", ObjectGetPrototypeOf,
  "setPrototypeOf", ObjectSetPrototypeOf,
  "getOwnPropertyDescriptor", ObjectGetOwnPropertyDescriptor,
  "getOwnPropertyNames", ObjectGetOwnPropertyNames,
  // getOwnPropertySymbols is added in symbol.js.
  "is", SameValue,  // ECMA-262, Edition 6, section 19.1.2.10
  // deliverChangeRecords, getNotifier, observe and unobserve are added
  // in object-observe.js.
]);


// ----------------------------------------------------------------------------
// Boolean

function BooleanConstructor(x) {
  // TODO(bmeurer): Move this to toplevel.
  "use strict";
  if (!IS_UNDEFINED(new.target)) {
    %_SetValueOf(this, TO_BOOLEAN(x));
  } else {
    return TO_BOOLEAN(x);
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

// ES6 Number.prototype.toString([ radix ])
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


// ES6 20.1.3.4 Number.prototype.toLocaleString([reserved1 [, reserved2]])
function NumberToLocaleString() {
  return %_Call(NumberToStringJS, this);
}


// ES6 20.1.3.7 Number.prototype.valueOf()
function NumberValueOf() {
  // NOTE: Both Number objects and values can enter here as
  // 'this'. This is not as dictated by ECMA-262.
  if (!IS_NUMBER(this) && !IS_NUMBER_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'Number.prototype.valueOf');
  }
  return %_ValueOf(this);
}


// ES6 20.1.3.3 Number.prototype.toFixed(fractionDigits)
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


// ES6 20.1.3.2 Number.prototype.toExponential(fractionDigits)
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


// ES6 20.1.3.5 Number.prototype.toPrecision(precision)
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
  if (IS_UNDEFINED(precision)) return TO_STRING(x);
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
      return MathAbs(integral) <= kMaxSafeInteger;
    }
  }
  return false;
}


// ----------------------------------------------------------------------------

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
  "NaN", NaN,
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
// Iterator related spec functions.

// ES6 7.4.1 GetIterator(obj, method)
function GetIterator(obj, method) {
  if (IS_UNDEFINED(method)) {
    method = obj[iteratorSymbol];
  }
  if (!IS_CALLABLE(method)) {
    throw MakeTypeError(kNotIterable, obj);
  }
  var iterator = %_Call(method, obj);
  if (!IS_RECEIVER(iterator)) {
    throw MakeTypeError(kNotAnIterator, iterator);
  }
  return iterator;
}

// ----------------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetIterator = GetIterator;
  to.GetMethod = GetMethod;
  to.IsFinite = GlobalIsFinite;
  to.IsNaN = GlobalIsNaN;
  to.NumberIsNaN = NumberIsNaN;
  to.ObjectDefineProperties = ObjectDefineProperties;
  to.ObjectDefineProperty = ObjectDefineProperty;
  to.ObjectHasOwnProperty = ObjectHasOwnProperty;
});

%InstallToContext([
  "object_value_of", ObjectValueOf,
]);

})
