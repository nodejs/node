// Copyright 2006-2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {
"use strict";

// ----------------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var IsNaN = global.isNaN;
var JSONStringify = global.JSON.stringify;
var MapEntries = global.Map.prototype.entries;
var MapIteratorNext = (new global.Map).entries().next;
var SetIteratorNext = (new global.Set).values().next;
var SetValues = global.Set.prototype.values;

// ----------------------------------------------------------------------------

// Mirror hierarchy:
// - Mirror
//   - ValueMirror
//     - UndefinedMirror
//     - NullMirror
//     - BooleanMirror
//     - NumberMirror
//     - StringMirror
//     - SymbolMirror
//     - ObjectMirror
//       - FunctionMirror
//         - UnresolvedFunctionMirror
//       - ArrayMirror
//       - DateMirror
//       - RegExpMirror
//       - ErrorMirror
//       - PromiseMirror
//       - MapMirror
//       - SetMirror
//       - IteratorMirror
//       - GeneratorMirror
//   - PropertyMirror
//   - InternalPropertyMirror
//   - FrameMirror
//   - ScriptMirror
//   - ScopeMirror

macro IS_BOOLEAN(arg)
(typeof(arg) === 'boolean')
endmacro

macro IS_DATE(arg)
(%IsDate(arg))
endmacro

macro IS_ERROR(arg)
(%_ClassOf(arg) === 'Error')
endmacro

macro IS_GENERATOR(arg)
(%_ClassOf(arg) === 'Generator')
endmacro

macro IS_MAP(arg)
(%_IsJSMap(arg))
endmacro

macro IS_MAP_ITERATOR(arg)
(%_ClassOf(arg) === 'Map Iterator')
endmacro

macro IS_SCRIPT(arg)
(%_ClassOf(arg) === 'Script')
endmacro

macro IS_SET(arg)
(%_IsJSSet(arg))
endmacro

macro IS_SET_ITERATOR(arg)
(%_ClassOf(arg) === 'Set Iterator')
endmacro

// Must match PropertyFilter in property-details.h
define PROPERTY_FILTER_NONE = 0;

// Type names of the different mirrors.
var MirrorType = {
  UNDEFINED_TYPE : 'undefined',
  NULL_TYPE : 'null',
  BOOLEAN_TYPE : 'boolean',
  NUMBER_TYPE : 'number',
  STRING_TYPE : 'string',
  SYMBOL_TYPE : 'symbol',
  OBJECT_TYPE : 'object',
  FUNCTION_TYPE : 'function',
  REGEXP_TYPE : 'regexp',
  ERROR_TYPE : 'error',
  PROPERTY_TYPE : 'property',
  INTERNAL_PROPERTY_TYPE : 'internalProperty',
  FRAME_TYPE : 'frame',
  SCRIPT_TYPE : 'script',
  CONTEXT_TYPE : 'context',
  SCOPE_TYPE : 'scope',
  PROMISE_TYPE : 'promise',
  MAP_TYPE : 'map',
  SET_TYPE : 'set',
  ITERATOR_TYPE : 'iterator',
  GENERATOR_TYPE : 'generator',
}

/**
 * Returns the mirror for a specified value or object.
 *
 * @param {value or Object} value the value or object to retrieve the mirror for
 * @returns {Mirror} the mirror reflects the passed value or object
 */
function MakeMirror(value) {
  var mirror;

  if (IS_UNDEFINED(value)) {
    mirror = new UndefinedMirror();
  } else if (IS_NULL(value)) {
    mirror = new NullMirror();
  } else if (IS_BOOLEAN(value)) {
    mirror = new BooleanMirror(value);
  } else if (IS_NUMBER(value)) {
    mirror = new NumberMirror(value);
  } else if (IS_STRING(value)) {
    mirror = new StringMirror(value);
  } else if (IS_SYMBOL(value)) {
    mirror = new SymbolMirror(value);
  } else if (IS_ARRAY(value)) {
    mirror = new ArrayMirror(value);
  } else if (IS_DATE(value)) {
    mirror = new DateMirror(value);
  } else if (IS_FUNCTION(value)) {
    mirror = new FunctionMirror(value);
  } else if (%IsRegExp(value)) {
    mirror = new RegExpMirror(value);
  } else if (IS_ERROR(value)) {
    mirror = new ErrorMirror(value);
  } else if (IS_SCRIPT(value)) {
    mirror = new ScriptMirror(value);
  } else if (IS_MAP(value) || IS_WEAKMAP(value)) {
    mirror = new MapMirror(value);
  } else if (IS_SET(value) || IS_WEAKSET(value)) {
    mirror = new SetMirror(value);
  } else if (IS_MAP_ITERATOR(value) || IS_SET_ITERATOR(value)) {
    mirror = new IteratorMirror(value);
  } else if (%is_promise(value)) {
    mirror = new PromiseMirror(value);
  } else if (IS_GENERATOR(value)) {
    mirror = new GeneratorMirror(value);
  } else {
    mirror = new ObjectMirror(value, MirrorType.OBJECT_TYPE);
  }

  return mirror;
}


/**
 * Returns the mirror for the undefined value.
 *
 * @returns {Mirror} the mirror reflects the undefined value
 */
function GetUndefinedMirror() {
  return MakeMirror(UNDEFINED);
}


/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be revritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype
 * @param {function} superCtor Constructor function to inherit prototype from
 */
function inherits(ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
}

// Maximum length when sending strings through the JSON protocol.
var kMaxProtocolStringLength = 80;


// A copy of the PropertyKind enum from property-details.h
var PropertyType = {};
PropertyType.Data     = 0;
PropertyType.Accessor = 1;


// Different attributes for a property.
var PropertyAttribute = {};
PropertyAttribute.None       = NONE;
PropertyAttribute.ReadOnly   = READ_ONLY;
PropertyAttribute.DontEnum   = DONT_ENUM;
PropertyAttribute.DontDelete = DONT_DELETE;


// A copy of the scope types from runtime-debug.cc.
// NOTE: these constants should be backward-compatible, so
// add new ones to the end of this list.
var ScopeType = { Global:  0,
                  Local:   1,
                  With:    2,
                  Closure: 3,
                  Catch:   4,
                  Block:   5,
                  Script:  6,
                  Eval:    7,
                  Module:  8,
                };

/**
 * Base class for all mirror objects.
 * @param {string} type The type of the mirror
 * @constructor
 */
function Mirror(type) {
  this.type_ = type;
}


Mirror.prototype.type = function() {
  return this.type_;
};


/**
 * Check whether the mirror reflects a value.
 * @returns {boolean} True if the mirror reflects a value.
 */
Mirror.prototype.isValue = function() {
  return this instanceof ValueMirror;
};


/**
 * Check whether the mirror reflects the undefined value.
 * @returns {boolean} True if the mirror reflects the undefined value.
 */
Mirror.prototype.isUndefined = function() {
  return this instanceof UndefinedMirror;
};


/**
 * Check whether the mirror reflects the null value.
 * @returns {boolean} True if the mirror reflects the null value
 */
Mirror.prototype.isNull = function() {
  return this instanceof NullMirror;
};


/**
 * Check whether the mirror reflects a boolean value.
 * @returns {boolean} True if the mirror reflects a boolean value
 */
Mirror.prototype.isBoolean = function() {
  return this instanceof BooleanMirror;
};


/**
 * Check whether the mirror reflects a number value.
 * @returns {boolean} True if the mirror reflects a number value
 */
Mirror.prototype.isNumber = function() {
  return this instanceof NumberMirror;
};


/**
 * Check whether the mirror reflects a string value.
 * @returns {boolean} True if the mirror reflects a string value
 */
Mirror.prototype.isString = function() {
  return this instanceof StringMirror;
};


/**
 * Check whether the mirror reflects a symbol.
 * @returns {boolean} True if the mirror reflects a symbol
 */
Mirror.prototype.isSymbol = function() {
  return this instanceof SymbolMirror;
};


/**
 * Check whether the mirror reflects an object.
 * @returns {boolean} True if the mirror reflects an object
 */
Mirror.prototype.isObject = function() {
  return this instanceof ObjectMirror;
};


/**
 * Check whether the mirror reflects a function.
 * @returns {boolean} True if the mirror reflects a function
 */
Mirror.prototype.isFunction = function() {
  return this instanceof FunctionMirror;
};


/**
 * Check whether the mirror reflects an unresolved function.
 * @returns {boolean} True if the mirror reflects an unresolved function
 */
Mirror.prototype.isUnresolvedFunction = function() {
  return this instanceof UnresolvedFunctionMirror;
};


/**
 * Check whether the mirror reflects an array.
 * @returns {boolean} True if the mirror reflects an array
 */
Mirror.prototype.isArray = function() {
  return this instanceof ArrayMirror;
};


/**
 * Check whether the mirror reflects a date.
 * @returns {boolean} True if the mirror reflects a date
 */
Mirror.prototype.isDate = function() {
  return this instanceof DateMirror;
};


/**
 * Check whether the mirror reflects a regular expression.
 * @returns {boolean} True if the mirror reflects a regular expression
 */
Mirror.prototype.isRegExp = function() {
  return this instanceof RegExpMirror;
};


/**
 * Check whether the mirror reflects an error.
 * @returns {boolean} True if the mirror reflects an error
 */
Mirror.prototype.isError = function() {
  return this instanceof ErrorMirror;
};


/**
 * Check whether the mirror reflects a promise.
 * @returns {boolean} True if the mirror reflects a promise
 */
Mirror.prototype.isPromise = function() {
  return this instanceof PromiseMirror;
};


/**
 * Check whether the mirror reflects a generator object.
 * @returns {boolean} True if the mirror reflects a generator object
 */
Mirror.prototype.isGenerator = function() {
  return this instanceof GeneratorMirror;
};


/**
 * Check whether the mirror reflects a property.
 * @returns {boolean} True if the mirror reflects a property
 */
Mirror.prototype.isProperty = function() {
  return this instanceof PropertyMirror;
};


/**
 * Check whether the mirror reflects an internal property.
 * @returns {boolean} True if the mirror reflects an internal property
 */
Mirror.prototype.isInternalProperty = function() {
  return this instanceof InternalPropertyMirror;
};


/**
 * Check whether the mirror reflects a stack frame.
 * @returns {boolean} True if the mirror reflects a stack frame
 */
Mirror.prototype.isFrame = function() {
  return this instanceof FrameMirror;
};


/**
 * Check whether the mirror reflects a script.
 * @returns {boolean} True if the mirror reflects a script
 */
Mirror.prototype.isScript = function() {
  return this instanceof ScriptMirror;
};


/**
 * Check whether the mirror reflects a context.
 * @returns {boolean} True if the mirror reflects a context
 */
Mirror.prototype.isContext = function() {
  return this instanceof ContextMirror;
};


/**
 * Check whether the mirror reflects a scope.
 * @returns {boolean} True if the mirror reflects a scope
 */
Mirror.prototype.isScope = function() {
  return this instanceof ScopeMirror;
};


/**
 * Check whether the mirror reflects a map.
 * @returns {boolean} True if the mirror reflects a map
 */
Mirror.prototype.isMap = function() {
  return this instanceof MapMirror;
};


/**
 * Check whether the mirror reflects a set.
 * @returns {boolean} True if the mirror reflects a set
 */
Mirror.prototype.isSet = function() {
  return this instanceof SetMirror;
};


/**
 * Check whether the mirror reflects an iterator.
 * @returns {boolean} True if the mirror reflects an iterator
 */
Mirror.prototype.isIterator = function() {
  return this instanceof IteratorMirror;
};


Mirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "#<" + this.constructor.name + ">";
};


/**
 * Base class for all value mirror objects.
 * @param {string} type The type of the mirror
 * @param {value} value The value reflected by this mirror
 * @constructor
 * @extends Mirror
 */
function ValueMirror(type, value) {
  %_Call(Mirror, this, type);
  this.value_ = value;
}
inherits(ValueMirror, Mirror);


/**
 * Check whether this is a primitive value.
 * @return {boolean} True if the mirror reflects a primitive value
 */
ValueMirror.prototype.isPrimitive = function() {
  var type = this.type();
  return type === 'undefined' ||
         type === 'null' ||
         type === 'boolean' ||
         type === 'number' ||
         type === 'string' ||
         type === 'symbol';
};


/**
 * Get the actual value reflected by this mirror.
 * @return {value} The value reflected by this mirror
 */
ValueMirror.prototype.value = function() {
  return this.value_;
};


/**
 * Mirror object for Undefined.
 * @constructor
 * @extends ValueMirror
 */
function UndefinedMirror() {
  %_Call(ValueMirror, this, MirrorType.UNDEFINED_TYPE, UNDEFINED);
}
inherits(UndefinedMirror, ValueMirror);


UndefinedMirror.prototype.toText = function() {
  return 'undefined';
};


/**
 * Mirror object for null.
 * @constructor
 * @extends ValueMirror
 */
function NullMirror() {
  %_Call(ValueMirror, this, MirrorType.NULL_TYPE, null);
}
inherits(NullMirror, ValueMirror);


NullMirror.prototype.toText = function() {
  return 'null';
};


/**
 * Mirror object for boolean values.
 * @param {boolean} value The boolean value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function BooleanMirror(value) {
  %_Call(ValueMirror, this, MirrorType.BOOLEAN_TYPE, value);
}
inherits(BooleanMirror, ValueMirror);


BooleanMirror.prototype.toText = function() {
  return this.value_ ? 'true' : 'false';
};


/**
 * Mirror object for number values.
 * @param {number} value The number value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function NumberMirror(value) {
  %_Call(ValueMirror, this, MirrorType.NUMBER_TYPE, value);
}
inherits(NumberMirror, ValueMirror);


NumberMirror.prototype.toText = function() {
  return '' + this.value_;
};


/**
 * Mirror object for string values.
 * @param {string} value The string value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function StringMirror(value) {
  %_Call(ValueMirror, this, MirrorType.STRING_TYPE, value);
}
inherits(StringMirror, ValueMirror);


StringMirror.prototype.length = function() {
  return this.value_.length;
};

StringMirror.prototype.getTruncatedValue = function(maxLength) {
  if (maxLength != -1 && this.length() > maxLength) {
    return this.value_.substring(0, maxLength) +
           '... (length: ' + this.length() + ')';
  }
  return this.value_;
};

StringMirror.prototype.toText = function() {
  return this.getTruncatedValue(kMaxProtocolStringLength);
};


/**
 * Mirror object for a Symbol
 * @param {Object} value The Symbol
 * @constructor
 * @extends Mirror
 */
function SymbolMirror(value) {
  %_Call(ValueMirror, this, MirrorType.SYMBOL_TYPE, value);
}
inherits(SymbolMirror, ValueMirror);


SymbolMirror.prototype.description = function() {
  return %SymbolDescription(%ValueOf(this.value_));
}


SymbolMirror.prototype.toText = function() {
  return %SymbolDescriptiveString(%ValueOf(this.value_));
}


/**
 * Mirror object for objects.
 * @param {object} value The object reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function ObjectMirror(value, type) {
  type = type || MirrorType.OBJECT_TYPE;
  %_Call(ValueMirror, this, type, value);
}
inherits(ObjectMirror, ValueMirror);


ObjectMirror.prototype.className = function() {
  return %_ClassOf(this.value_);
};


ObjectMirror.prototype.constructorFunction = function() {
  return MakeMirror(%DebugGetProperty(this.value_, 'constructor'));
};


ObjectMirror.prototype.prototypeObject = function() {
  return MakeMirror(%DebugGetProperty(this.value_, 'prototype'));
};


ObjectMirror.prototype.protoObject = function() {
  return MakeMirror(%DebugGetPrototype(this.value_));
};


ObjectMirror.prototype.hasNamedInterceptor = function() {
  // Get information on interceptors for this object.
  var x = %GetInterceptorInfo(this.value_);
  return (x & 2) != 0;
};


ObjectMirror.prototype.hasIndexedInterceptor = function() {
  // Get information on interceptors for this object.
  var x = %GetInterceptorInfo(this.value_);
  return (x & 1) != 0;
};


/**
 * Return the property names for this object.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     properties are requested
 * @param {number} limit Limit the number of names returend to the specified
       value
 * @return {Array} Property names for this object
 */
ObjectMirror.prototype.propertyNames = function() {
  return %GetOwnPropertyKeys(this.value_, PROPERTY_FILTER_NONE);
};


/**
 * Return the properties for this object as an array of PropertyMirror objects.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     properties are requested
 * @param {number} limit Limit the number of properties returned to the
       specified value
 * @return {Array} Property mirrors for this object
 */
ObjectMirror.prototype.properties = function() {
  var names = this.propertyNames();
  var properties = new GlobalArray(names.length);
  for (var i = 0; i < names.length; i++) {
    properties[i] = this.property(names[i]);
  }

  return properties;
};


/**
 * Return the internal properties for this object as an array of
 * InternalPropertyMirror objects.
 * @return {Array} Property mirrors for this object
 */
ObjectMirror.prototype.internalProperties = function() {
  return ObjectMirror.GetInternalProperties(this.value_);
}


ObjectMirror.prototype.property = function(name) {
  var details = %DebugGetPropertyDetails(this.value_, name);
  if (details) {
    return new PropertyMirror(this, name, details);
  }

  // Nothing found.
  return GetUndefinedMirror();
};



/**
 * Try to find a property from its value.
 * @param {Mirror} value The property value to look for
 * @return {PropertyMirror} The property with the specified value. If no
 *     property was found with the specified value UndefinedMirror is returned
 */
ObjectMirror.prototype.lookupProperty = function(value) {
  var properties = this.properties();

  // Look for property value in properties.
  for (var i = 0; i < properties.length; i++) {

    // Skip properties which are defined through accessors.
    var property = properties[i];
    if (property.propertyType() == PropertyType.Data) {
      if (property.value_ === value.value_) {
        return property;
      }
    }
  }

  // Nothing found.
  return GetUndefinedMirror();
};


/**
 * Returns objects which has direct references to this object
 * @param {number} opt_max_objects Optional parameter specifying the maximum
 *     number of referencing objects to return.
 * @return {Array} The objects which has direct references to this object.
 */
ObjectMirror.prototype.referencedBy = function(opt_max_objects) {
  // Find all objects with direct references to this object.
  var result = %DebugReferencedBy(this.value_,
                                  Mirror.prototype, opt_max_objects || 0);

  // Make mirrors for all the references found.
  for (var i = 0; i < result.length; i++) {
    result[i] = MakeMirror(result[i]);
  }

  return result;
};


ObjectMirror.prototype.toText = function() {
  var name;
  var ctor = this.constructorFunction();
  if (!ctor.isFunction()) {
    name = this.className();
  } else {
    name = ctor.name();
    if (!name) {
      name = this.className();
    }
  }
  return '#<' + name + '>';
};


/**
 * Return the internal properties of the value, such as [[PrimitiveValue]] of
 * scalar wrapper objects, properties of the bound function and properties of
 * the promise.
 * This method is done static to be accessible from Debug API with the bare
 * values without mirrors.
 * @return {Array} array (possibly empty) of InternalProperty instances
 */
ObjectMirror.GetInternalProperties = function(value) {
  var properties = %DebugGetInternalProperties(value);
  var result = [];
  for (var i = 0; i < properties.length; i += 2) {
    result.push(new InternalPropertyMirror(properties[i], properties[i + 1]));
  }
  return result;
}


/**
 * Mirror object for functions.
 * @param {function} value The function object reflected by this mirror.
 * @constructor
 * @extends ObjectMirror
 */
function FunctionMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.FUNCTION_TYPE);
  this.resolved_ = true;
}
inherits(FunctionMirror, ObjectMirror);


/**
 * Returns whether the function is resolved.
 * @return {boolean} True if the function is resolved. Unresolved functions can
 *     only originate as functions from stack frames
 */
FunctionMirror.prototype.resolved = function() {
  return this.resolved_;
};


/**
 * Returns the name of the function.
 * @return {string} Name of the function
 */
FunctionMirror.prototype.name = function() {
  return %FunctionGetName(this.value_);
};


/**
 * Returns the displayName if it is set, otherwise name, otherwise inferred
 * name.
 * @return {string} Name of the function
 */
FunctionMirror.prototype.debugName = function() {
  return %FunctionGetDebugName(this.value_);
}


/**
 * Returns the inferred name of the function.
 * @return {string} Name of the function
 */
FunctionMirror.prototype.inferredName = function() {
  return %FunctionGetInferredName(this.value_);
};


/**
 * Returns the source code for the function.
 * @return {string or undefined} The source code for the function. If the
 *     function is not resolved undefined will be returned.
 */
FunctionMirror.prototype.source = function() {
  // Return source if function is resolved. Otherwise just fall through to
  // return undefined.
  if (this.resolved()) {
    return %FunctionToString(this.value_);
  }
};


/**
 * Returns the script object for the function.
 * @return {ScriptMirror or undefined} Script object for the function or
 *     undefined if the function has no script
 */
FunctionMirror.prototype.script = function() {
  // Return script if function is resolved. Otherwise just fall through
  // to return undefined.
  if (this.resolved()) {
    if (this.script_) {
      return this.script_;
    }
    var script = %FunctionGetScript(this.value_);
    if (script) {
      return this.script_ = MakeMirror(script);
    }
  }
};


/**
 * Returns the script source position for the function. Only makes sense
 * for functions which has a script defined.
 * @return {Number or undefined} in-script position for the function
 */
FunctionMirror.prototype.sourcePosition_ = function() {
  // Return position if function is resolved. Otherwise just fall
  // through to return undefined.
  if (this.resolved()) {
    return %FunctionGetScriptSourcePosition(this.value_);
  }
};


/**
 * Returns the script source location object for the function. Only makes sense
 * for functions which has a script defined.
 * @return {Location or undefined} in-script location for the function begin
 */
FunctionMirror.prototype.sourceLocation = function() {
  if (this.resolved()) {
    var script = this.script();
    if (script) {
      return script.locationFromPosition(this.sourcePosition_(), true);
    }
  }
};


/**
 * Returns objects constructed by this function.
 * @param {number} opt_max_instances Optional parameter specifying the maximum
 *     number of instances to return.
 * @return {Array or undefined} The objects constructed by this function.
 */
FunctionMirror.prototype.constructedBy = function(opt_max_instances) {
  if (this.resolved()) {
    // Find all objects constructed from this function.
    var result = %DebugConstructedBy(this.value_, opt_max_instances || 0);

    // Make mirrors for all the instances found.
    for (var i = 0; i < result.length; i++) {
      result[i] = MakeMirror(result[i]);
    }

    return result;
  } else {
    return [];
  }
};


FunctionMirror.prototype.scopeCount = function() {
  if (this.resolved()) {
    if (IS_UNDEFINED(this.scopeCount_)) {
      this.scopeCount_ = %GetFunctionScopeCount(this.value());
    }
    return this.scopeCount_;
  } else {
    return 0;
  }
};


FunctionMirror.prototype.scope = function(index) {
  if (this.resolved()) {
    return new ScopeMirror(UNDEFINED, this, UNDEFINED, index);
  }
};


FunctionMirror.prototype.toText = function() {
  return this.source();
};


FunctionMirror.prototype.context = function() {
  if (this.resolved()) {
    if (!this._context)
      this._context = new ContextMirror(%FunctionGetContextData(this.value_));
    return this._context;
  }
};


/**
 * Mirror object for unresolved functions.
 * @param {string} value The name for the unresolved function reflected by this
 *     mirror.
 * @constructor
 * @extends ObjectMirror
 */
function UnresolvedFunctionMirror(value) {
  // Construct this using the ValueMirror as an unresolved function is not a
  // real object but just a string.
  %_Call(ValueMirror, this, MirrorType.FUNCTION_TYPE, value);
  this.propertyCount_ = 0;
  this.elementCount_ = 0;
  this.resolved_ = false;
}
inherits(UnresolvedFunctionMirror, FunctionMirror);


UnresolvedFunctionMirror.prototype.className = function() {
  return 'Function';
};


UnresolvedFunctionMirror.prototype.constructorFunction = function() {
  return GetUndefinedMirror();
};


UnresolvedFunctionMirror.prototype.prototypeObject = function() {
  return GetUndefinedMirror();
};


UnresolvedFunctionMirror.prototype.protoObject = function() {
  return GetUndefinedMirror();
};


UnresolvedFunctionMirror.prototype.name = function() {
  return this.value_;
};


UnresolvedFunctionMirror.prototype.debugName = function() {
  return this.value_;
};


UnresolvedFunctionMirror.prototype.inferredName = function() {
  return UNDEFINED;
};


UnresolvedFunctionMirror.prototype.propertyNames = function(kind, limit) {
  return [];
};


/**
 * Mirror object for arrays.
 * @param {Array} value The Array object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ArrayMirror(value) {
  %_Call(ObjectMirror, this, value);
}
inherits(ArrayMirror, ObjectMirror);


ArrayMirror.prototype.length = function() {
  return this.value_.length;
};


ArrayMirror.prototype.indexedPropertiesFromRange = function(opt_from_index,
                                                            opt_to_index) {
  var from_index = opt_from_index || 0;
  var to_index = opt_to_index || this.length() - 1;
  if (from_index > to_index) return new GlobalArray();
  var values = new GlobalArray(to_index - from_index + 1);
  for (var i = from_index; i <= to_index; i++) {
    var details = %DebugGetPropertyDetails(this.value_, TO_STRING(i));
    var value;
    if (details) {
      value = new PropertyMirror(this, i, details);
    } else {
      value = GetUndefinedMirror();
    }
    values[i - from_index] = value;
  }
  return values;
};


/**
 * Mirror object for dates.
 * @param {Date} value The Date object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function DateMirror(value) {
  %_Call(ObjectMirror, this, value);
}
inherits(DateMirror, ObjectMirror);


DateMirror.prototype.toText = function() {
  var s = JSONStringify(this.value_);
  return s.substring(1, s.length - 1);  // cut quotes
};


/**
 * Mirror object for regular expressions.
 * @param {RegExp} value The RegExp object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function RegExpMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.REGEXP_TYPE);
}
inherits(RegExpMirror, ObjectMirror);


/**
 * Returns the source to the regular expression.
 * @return {string or undefined} The source to the regular expression
 */
RegExpMirror.prototype.source = function() {
  return this.value_.source;
};


/**
 * Returns whether this regular expression has the global (g) flag set.
 * @return {boolean} Value of the global flag
 */
RegExpMirror.prototype.global = function() {
  return this.value_.global;
};


/**
 * Returns whether this regular expression has the ignore case (i) flag set.
 * @return {boolean} Value of the ignore case flag
 */
RegExpMirror.prototype.ignoreCase = function() {
  return this.value_.ignoreCase;
};


/**
 * Returns whether this regular expression has the multiline (m) flag set.
 * @return {boolean} Value of the multiline flag
 */
RegExpMirror.prototype.multiline = function() {
  return this.value_.multiline;
};


/**
 * Returns whether this regular expression has the sticky (y) flag set.
 * @return {boolean} Value of the sticky flag
 */
RegExpMirror.prototype.sticky = function() {
  return this.value_.sticky;
};


/**
 * Returns whether this regular expression has the unicode (u) flag set.
 * @return {boolean} Value of the unicode flag
 */
RegExpMirror.prototype.unicode = function() {
  return this.value_.unicode;
};


RegExpMirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "/" + this.source() + "/";
};


/**
 * Mirror object for error objects.
 * @param {Error} value The error object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ErrorMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.ERROR_TYPE);
}
inherits(ErrorMirror, ObjectMirror);


/**
 * Returns the message for this eror object.
 * @return {string or undefined} The message for this eror object
 */
ErrorMirror.prototype.message = function() {
  return this.value_.message;
};


ErrorMirror.prototype.toText = function() {
  // Use the same text representation as in messages.js.
  var text;
  try {
    text = %ErrorToString(this.value_);
  } catch (e) {
    text = '#<Error>';
  }
  return text;
};


/**
 * Mirror object for a Promise object.
 * @param {Object} value The Promise object
 * @constructor
 * @extends ObjectMirror
 */
function PromiseMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.PROMISE_TYPE);
}
inherits(PromiseMirror, ObjectMirror);


function PromiseGetStatus_(value) {
  var status = %PromiseStatus(value);
  if (status == 0) return "pending";
  if (status == 1) return "resolved";
  return "rejected";
}


function PromiseGetValue_(value) {
  return %PromiseResult(value);
}


PromiseMirror.prototype.status = function() {
  return PromiseGetStatus_(this.value_);
};


PromiseMirror.prototype.promiseValue = function() {
  return MakeMirror(PromiseGetValue_(this.value_));
};


function MapMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.MAP_TYPE);
}
inherits(MapMirror, ObjectMirror);


/**
 * Returns an array of key/value pairs of a map.
 * This will keep keys alive for WeakMaps.
 *
 * @param {number=} opt_limit Max elements to return.
 * @returns {Array.<Object>} Array of key/value pairs of a map.
 */
MapMirror.prototype.entries = function(opt_limit) {
  var result = [];

  if (IS_WEAKMAP(this.value_)) {
    var entries = %GetWeakMapEntries(this.value_, opt_limit || 0);
    for (var i = 0; i < entries.length; i += 2) {
      result.push({
        key: entries[i],
        value: entries[i + 1]
      });
    }
    return result;
  }

  var iter = %_Call(MapEntries, this.value_);
  var next;
  while ((!opt_limit || result.length < opt_limit) &&
         !(next = iter.next()).done) {
    result.push({
      key: next.value[0],
      value: next.value[1]
    });
  }
  return result;
};


function SetMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.SET_TYPE);
}
inherits(SetMirror, ObjectMirror);


function IteratorGetValues_(iter, next_function, opt_limit) {
  var result = [];
  var next;
  while ((!opt_limit || result.length < opt_limit) &&
         !(next = %_Call(next_function, iter)).done) {
    result.push(next.value);
  }
  return result;
}


/**
 * Returns an array of elements of a set.
 * This will keep elements alive for WeakSets.
 *
 * @param {number=} opt_limit Max elements to return.
 * @returns {Array.<Object>} Array of elements of a set.
 */
SetMirror.prototype.values = function(opt_limit) {
  if (IS_WEAKSET(this.value_)) {
    return %GetWeakSetValues(this.value_, opt_limit || 0);
  }

  var iter = %_Call(SetValues, this.value_);
  return IteratorGetValues_(iter, SetIteratorNext, opt_limit);
};


function IteratorMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.ITERATOR_TYPE);
}
inherits(IteratorMirror, ObjectMirror);


/**
 * Returns a preview of elements of an iterator.
 * Does not change the backing iterator state.
 *
 * @param {number=} opt_limit Max elements to return.
 * @returns {Array.<Object>} Array of elements of an iterator.
 */
IteratorMirror.prototype.preview = function(opt_limit) {
  if (IS_MAP_ITERATOR(this.value_)) {
    return IteratorGetValues_(%MapIteratorClone(this.value_),
                              MapIteratorNext,
                              opt_limit);
  } else if (IS_SET_ITERATOR(this.value_)) {
    return IteratorGetValues_(%SetIteratorClone(this.value_),
                              SetIteratorNext,
                              opt_limit);
  }
};


/**
 * Mirror object for a Generator object.
 * @param {Object} data The Generator object
 * @constructor
 * @extends Mirror
 */
function GeneratorMirror(value) {
  %_Call(ObjectMirror, this, value, MirrorType.GENERATOR_TYPE);
}
inherits(GeneratorMirror, ObjectMirror);


function GeneratorGetStatus_(value) {
  var continuation = %GeneratorGetContinuation(value);
  if (continuation < -1) return "running";
  if (continuation == -1) return "closed";
  return "suspended";
}


GeneratorMirror.prototype.status = function() {
  return GeneratorGetStatus_(this.value_);
};


GeneratorMirror.prototype.sourcePosition_ = function() {
  return %GeneratorGetSourcePosition(this.value_);
};


GeneratorMirror.prototype.sourceLocation = function() {
  var pos = this.sourcePosition_();
  if (!IS_UNDEFINED(pos)) {
    var script = this.func().script();
    if (script) {
      return script.locationFromPosition(pos, true);
    }
  }
};


GeneratorMirror.prototype.func = function() {
  if (!this.func_) {
    this.func_ = MakeMirror(%GeneratorGetFunction(this.value_));
  }
  return this.func_;
};


GeneratorMirror.prototype.receiver = function() {
  if (!this.receiver_) {
    this.receiver_ = MakeMirror(%GeneratorGetReceiver(this.value_));
  }
  return this.receiver_;
};


GeneratorMirror.prototype.scopeCount = function() {
  // This value can change over time as the underlying generator is suspended
  // at different locations.
  return %GetGeneratorScopeCount(this.value());
};


GeneratorMirror.prototype.scope = function(index) {
  return new ScopeMirror(UNDEFINED, UNDEFINED, this, index);
};


GeneratorMirror.prototype.allScopes = function() {
  var scopes = [];
  for (let i = 0; i < this.scopeCount(); i++) {
    scopes.push(this.scope(i));
  }
  return scopes;
};


/**
 * Base mirror object for properties.
 * @param {ObjectMirror} mirror The mirror object having this property
 * @param {string} name The name of the property
 * @param {Array} details Details about the property
 * @constructor
 * @extends Mirror
 */
function PropertyMirror(mirror, name, details) {
  %_Call(Mirror, this, MirrorType.PROPERTY_TYPE);
  this.mirror_ = mirror;
  this.name_ = name;
  this.value_ = details[0];
  this.details_ = details[1];
  this.is_interceptor_ = details[2];
  if (details.length > 3) {
    this.exception_ = details[3];
    this.getter_ = details[4];
    this.setter_ = details[5];
  }
}
inherits(PropertyMirror, Mirror);


PropertyMirror.prototype.isReadOnly = function() {
  return (this.attributes() & PropertyAttribute.ReadOnly) != 0;
};


PropertyMirror.prototype.isEnum = function() {
  return (this.attributes() & PropertyAttribute.DontEnum) == 0;
};


PropertyMirror.prototype.canDelete = function() {
  return (this.attributes() & PropertyAttribute.DontDelete) == 0;
};


PropertyMirror.prototype.name = function() {
  return this.name_;
};


PropertyMirror.prototype.toText = function() {
  if (IS_SYMBOL(this.name_)) return %SymbolDescriptiveString(this.name_);
  return this.name_;
};


PropertyMirror.prototype.isIndexed = function() {
  for (var i = 0; i < this.name_.length; i++) {
    if (this.name_[i] < '0' || '9' < this.name_[i]) {
      return false;
    }
  }
  return true;
};


PropertyMirror.prototype.value = function() {
  return MakeMirror(this.value_, false);
};


/**
 * Returns whether this property value is an exception.
 * @return {boolean} True if this property value is an exception
 */
PropertyMirror.prototype.isException = function() {
  return this.exception_ ? true : false;
};


PropertyMirror.prototype.attributes = function() {
  return %DebugPropertyAttributesFromDetails(this.details_);
};


PropertyMirror.prototype.propertyType = function() {
  return %DebugPropertyKindFromDetails(this.details_);
};


/**
 * Returns whether this property has a getter defined through __defineGetter__.
 * @return {boolean} True if this property has a getter
 */
PropertyMirror.prototype.hasGetter = function() {
  return this.getter_ ? true : false;
};


/**
 * Returns whether this property has a setter defined through __defineSetter__.
 * @return {boolean} True if this property has a setter
 */
PropertyMirror.prototype.hasSetter = function() {
  return this.setter_ ? true : false;
};


/**
 * Returns the getter for this property defined through __defineGetter__.
 * @return {Mirror} FunctionMirror reflecting the getter function or
 *     UndefinedMirror if there is no getter for this property
 */
PropertyMirror.prototype.getter = function() {
  if (this.hasGetter()) {
    return MakeMirror(this.getter_);
  } else {
    return GetUndefinedMirror();
  }
};


/**
 * Returns the setter for this property defined through __defineSetter__.
 * @return {Mirror} FunctionMirror reflecting the setter function or
 *     UndefinedMirror if there is no setter for this property
 */
PropertyMirror.prototype.setter = function() {
  if (this.hasSetter()) {
    return MakeMirror(this.setter_);
  } else {
    return GetUndefinedMirror();
  }
};


/**
 * Returns whether this property is natively implemented by the host or a set
 * through JavaScript code.
 * @return {boolean} True if the property is
 *     UndefinedMirror if there is no setter for this property
 */
PropertyMirror.prototype.isNative = function() {
  return this.is_interceptor_ ||
         ((this.propertyType() == PropertyType.Accessor) &&
          !this.hasGetter() && !this.hasSetter());
};


/**
 * Mirror object for internal properties. Internal property reflects properties
 * not accessible from user code such as [[BoundThis]] in bound function.
 * Their names are merely symbolic.
 * @param {string} name The name of the property
 * @param {value} property value
 * @constructor
 * @extends Mirror
 */
function InternalPropertyMirror(name, value) {
  %_Call(Mirror, this, MirrorType.INTERNAL_PROPERTY_TYPE);
  this.name_ = name;
  this.value_ = value;
}
inherits(InternalPropertyMirror, Mirror);


InternalPropertyMirror.prototype.name = function() {
  return this.name_;
};


InternalPropertyMirror.prototype.value = function() {
  return MakeMirror(this.value_, false);
};


var kFrameDetailsFrameIdIndex = 0;
var kFrameDetailsReceiverIndex = 1;
var kFrameDetailsFunctionIndex = 2;
var kFrameDetailsScriptIndex = 3;
var kFrameDetailsArgumentCountIndex = 4;
var kFrameDetailsLocalCountIndex = 5;
var kFrameDetailsSourcePositionIndex = 6;
var kFrameDetailsConstructCallIndex = 7;
var kFrameDetailsAtReturnIndex = 8;
var kFrameDetailsFlagsIndex = 9;
var kFrameDetailsFirstDynamicIndex = 10;

var kFrameDetailsNameIndex = 0;
var kFrameDetailsValueIndex = 1;
var kFrameDetailsNameValueSize = 2;

var kFrameDetailsFlagDebuggerFrameMask = 1 << 0;
var kFrameDetailsFlagOptimizedFrameMask = 1 << 1;
var kFrameDetailsFlagInlinedFrameIndexMask = 7 << 2;

/**
 * Wrapper for the frame details information retreived from the VM. The frame
 * details from the VM is an array with the following content. See runtime.cc
 * Runtime_GetFrameDetails.
 *     0: Id
 *     1: Receiver
 *     2: Function
 *     3: Script
 *     4: Argument count
 *     5: Local count
 *     6: Source position
 *     7: Construct call
 *     8: Is at return
 *     9: Flags (debugger frame, optimized frame, inlined frame index)
 *     Arguments name, value
 *     Locals name, value
 *     Return value if any
 * @param {number} break_id Current break id
 * @param {number} index Frame number
 * @constructor
 */
function FrameDetails(break_id, index) {
  this.break_id_ = break_id;
  this.details_ = %GetFrameDetails(break_id, index);
}


FrameDetails.prototype.frameId = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsFrameIdIndex];
};


FrameDetails.prototype.receiver = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsReceiverIndex];
};


FrameDetails.prototype.func = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsFunctionIndex];
};


FrameDetails.prototype.script = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsScriptIndex];
};


FrameDetails.prototype.isConstructCall = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsConstructCallIndex];
};


FrameDetails.prototype.isAtReturn = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsAtReturnIndex];
};


FrameDetails.prototype.isDebuggerFrame = function() {
  %CheckExecutionState(this.break_id_);
  var f = kFrameDetailsFlagDebuggerFrameMask;
  return (this.details_[kFrameDetailsFlagsIndex] & f) == f;
};


FrameDetails.prototype.isOptimizedFrame = function() {
  %CheckExecutionState(this.break_id_);
  var f = kFrameDetailsFlagOptimizedFrameMask;
  return (this.details_[kFrameDetailsFlagsIndex] & f) == f;
};


FrameDetails.prototype.isInlinedFrame = function() {
  return this.inlinedFrameIndex() > 0;
};


FrameDetails.prototype.inlinedFrameIndex = function() {
  %CheckExecutionState(this.break_id_);
  var f = kFrameDetailsFlagInlinedFrameIndexMask;
  return (this.details_[kFrameDetailsFlagsIndex] & f) >> 2;
};


FrameDetails.prototype.argumentCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsArgumentCountIndex];
};


FrameDetails.prototype.argumentName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex];
  }
};


FrameDetails.prototype.argumentValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex];
  }
};


FrameDetails.prototype.localCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsLocalCountIndex];
};


FrameDetails.prototype.sourcePosition = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsSourcePositionIndex];
};


FrameDetails.prototype.localName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex +
                        this.argumentCount() * kFrameDetailsNameValueSize;
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex];
  }
};


FrameDetails.prototype.localValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex +
                        this.argumentCount() * kFrameDetailsNameValueSize;
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex];
  }
};


FrameDetails.prototype.returnValue = function() {
  %CheckExecutionState(this.break_id_);
  var return_value_offset =
      kFrameDetailsFirstDynamicIndex +
      (this.argumentCount() + this.localCount()) * kFrameDetailsNameValueSize;
  if (this.details_[kFrameDetailsAtReturnIndex]) {
    return this.details_[return_value_offset];
  }
};


FrameDetails.prototype.scopeCount = function() {
  if (IS_UNDEFINED(this.scopeCount_)) {
    this.scopeCount_ = %GetScopeCount(this.break_id_, this.frameId());
  }
  return this.scopeCount_;
};


/**
 * Mirror object for stack frames.
 * @param {number} break_id The break id in the VM for which this frame is
       valid
 * @param {number} index The frame index (top frame is index 0)
 * @constructor
 * @extends Mirror
 */
function FrameMirror(break_id, index) {
  %_Call(Mirror, this, MirrorType.FRAME_TYPE);
  this.break_id_ = break_id;
  this.index_ = index;
  this.details_ = new FrameDetails(break_id, index);
}
inherits(FrameMirror, Mirror);


FrameMirror.prototype.details = function() {
  return this.details_;
};


FrameMirror.prototype.index = function() {
  return this.index_;
};


FrameMirror.prototype.func = function() {
  if (this.func_) {
    return this.func_;
  }

  // Get the function for this frame from the VM.
  var f = this.details_.func();

  // Create a function mirror. NOTE: MakeMirror cannot be used here as the
  // value returned from the VM might be a string if the function for the
  // frame is unresolved.
  if (IS_FUNCTION(f)) {
    return this.func_ = MakeMirror(f);
  } else {
    return new UnresolvedFunctionMirror(f);
  }
};


FrameMirror.prototype.script = function() {
  if (!this.script_) {
    this.script_ = MakeMirror(this.details_.script());
  }

  return this.script_;
}


FrameMirror.prototype.receiver = function() {
  return MakeMirror(this.details_.receiver());
};


FrameMirror.prototype.isConstructCall = function() {
  return this.details_.isConstructCall();
};


FrameMirror.prototype.isAtReturn = function() {
  return this.details_.isAtReturn();
};


FrameMirror.prototype.isDebuggerFrame = function() {
  return this.details_.isDebuggerFrame();
};


FrameMirror.prototype.isOptimizedFrame = function() {
  return this.details_.isOptimizedFrame();
};


FrameMirror.prototype.isInlinedFrame = function() {
  return this.details_.isInlinedFrame();
};


FrameMirror.prototype.inlinedFrameIndex = function() {
  return this.details_.inlinedFrameIndex();
};


FrameMirror.prototype.argumentCount = function() {
  return this.details_.argumentCount();
};


FrameMirror.prototype.argumentName = function(index) {
  return this.details_.argumentName(index);
};


FrameMirror.prototype.argumentValue = function(index) {
  return MakeMirror(this.details_.argumentValue(index));
};


FrameMirror.prototype.localCount = function() {
  return this.details_.localCount();
};


FrameMirror.prototype.localName = function(index) {
  return this.details_.localName(index);
};


FrameMirror.prototype.localValue = function(index) {
  return MakeMirror(this.details_.localValue(index));
};


FrameMirror.prototype.returnValue = function() {
  return MakeMirror(this.details_.returnValue());
};


FrameMirror.prototype.sourcePosition = function() {
  return this.details_.sourcePosition();
};


FrameMirror.prototype.sourceLocation = function() {
  var script = this.script();
  if (script) {
    return script.locationFromPosition(this.sourcePosition(), true);
  }
};


FrameMirror.prototype.sourceLine = function() {
  var location = this.sourceLocation();
  if (location) {
    return location.line;
  }
};


FrameMirror.prototype.sourceColumn = function() {
  var location = this.sourceLocation();
  if (location) {
    return location.column;
  }
};


FrameMirror.prototype.sourceLineText = function() {
  var location = this.sourceLocation();
  if (location) {
    return location.sourceText;
  }
};


FrameMirror.prototype.scopeCount = function() {
  return this.details_.scopeCount();
};


FrameMirror.prototype.scope = function(index) {
  return new ScopeMirror(this, UNDEFINED, UNDEFINED, index);
};


FrameMirror.prototype.allScopes = function(opt_ignore_nested_scopes) {
  var scopeDetails = %GetAllScopesDetails(this.break_id_,
                                          this.details_.frameId(),
                                          this.details_.inlinedFrameIndex(),
                                          !!opt_ignore_nested_scopes);
  var result = [];
  for (var i = 0; i < scopeDetails.length; ++i) {
    result.push(new ScopeMirror(this, UNDEFINED, UNDEFINED, i,
                                scopeDetails[i]));
  }
  return result;
};


FrameMirror.prototype.evaluate = function(source, throw_on_side_effect = false) {
  return MakeMirror(%DebugEvaluate(this.break_id_,
                                   this.details_.frameId(),
                                   this.details_.inlinedFrameIndex(),
                                   source,
                                   throw_on_side_effect));
};


FrameMirror.prototype.invocationText = function() {
  // Format frame invoaction (receiver, function and arguments).
  var result = '';
  var func = this.func();
  var receiver = this.receiver();
  if (this.isConstructCall()) {
    // For constructor frames display new followed by the function name.
    result += 'new ';
    result += func.name() ? func.name() : '[anonymous]';
  } else if (this.isDebuggerFrame()) {
    result += '[debugger]';
  } else {
    // If the receiver has a className which is 'global' don't display it.
    var display_receiver =
      !receiver.className || (receiver.className() != 'global');
    if (display_receiver) {
      result += receiver.toText();
    }
    // Try to find the function as a property in the receiver. Include the
    // prototype chain in the lookup.
    var property = GetUndefinedMirror();
    if (receiver.isObject()) {
      for (var r = receiver;
           !r.isNull() && property.isUndefined();
           r = r.protoObject()) {
        property = r.lookupProperty(func);
      }
    }
    if (!property.isUndefined()) {
      // The function invoked was found on the receiver. Use the property name
      // for the backtrace.
      if (!property.isIndexed()) {
        if (display_receiver) {
          result += '.';
        }
        result += property.toText();
      } else {
        result += '[';
        result += property.toText();
        result += ']';
      }
      // Also known as - if the name in the function doesn't match the name
      // under which it was looked up.
      if (func.name() && func.name() != property.name()) {
        result += '(aka ' + func.name() + ')';
      }
    } else {
      // The function invoked was not found on the receiver. Use the function
      // name if available for the backtrace.
      if (display_receiver) {
        result += '.';
      }
      result += func.name() ? func.name() : '[anonymous]';
    }
  }

  // Render arguments for normal frames.
  if (!this.isDebuggerFrame()) {
    result += '(';
    for (var i = 0; i < this.argumentCount(); i++) {
      if (i != 0) result += ', ';
      if (this.argumentName(i)) {
        result += this.argumentName(i);
        result += '=';
      }
      result += this.argumentValue(i).toText();
    }
    result += ')';
  }

  if (this.isAtReturn()) {
    result += ' returning ';
    result += this.returnValue().toText();
  }

  return result;
};


FrameMirror.prototype.sourceAndPositionText = function() {
  // Format source and position.
  var result = '';
  var func = this.func();
  if (func.resolved()) {
    var script = func.script();
    if (script) {
      if (script.name()) {
        result += script.name();
      } else {
        result += '[unnamed]';
      }
      if (!this.isDebuggerFrame()) {
        var location = this.sourceLocation();
        result += ' line ';
        result += !IS_UNDEFINED(location) ? (location.line + 1) : '?';
        result += ' column ';
        result += !IS_UNDEFINED(location) ? (location.column + 1) : '?';
        if (!IS_UNDEFINED(this.sourcePosition())) {
          result += ' (position ' + (this.sourcePosition() + 1) + ')';
        }
      }
    } else {
      result += '[no source]';
    }
  } else {
    result += '[unresolved]';
  }

  return result;
};


FrameMirror.prototype.localsText = function() {
  // Format local variables.
  var result = '';
  var locals_count = this.localCount();
  if (locals_count > 0) {
    for (var i = 0; i < locals_count; ++i) {
      result += '      var ';
      result += this.localName(i);
      result += ' = ';
      result += this.localValue(i).toText();
      if (i < locals_count - 1) result += '\n';
    }
  }

  return result;
};


FrameMirror.prototype.restart = function() {
  var result = %LiveEditRestartFrame(this.break_id_, this.index_);
  if (IS_UNDEFINED(result)) {
    result = "Failed to find requested frame";
  }
  return result;
};


FrameMirror.prototype.toText = function(opt_locals) {
  var result = '';
  result += '#' + (this.index() <= 9 ? '0' : '') + this.index();
  result += ' ';
  result += this.invocationText();
  result += ' ';
  result += this.sourceAndPositionText();
  if (opt_locals) {
    result += '\n';
    result += this.localsText();
  }
  return result;
};


// This indexes correspond definitions in debug-scopes.h.
var kScopeDetailsTypeIndex = 0;
var kScopeDetailsObjectIndex = 1;
var kScopeDetailsNameIndex = 2;
var kScopeDetailsStartPositionIndex = 3;
var kScopeDetailsEndPositionIndex = 4;
var kScopeDetailsFunctionIndex = 5;

function ScopeDetails(frame, fun, gen, index, opt_details) {
  if (frame) {
    this.break_id_ = frame.break_id_;
    this.details_ = opt_details ||
                    %GetScopeDetails(frame.break_id_,
                                     frame.details_.frameId(),
                                     frame.details_.inlinedFrameIndex(),
                                     index);
    this.frame_id_ = frame.details_.frameId();
    this.inlined_frame_id_ = frame.details_.inlinedFrameIndex();
  } else if (fun) {
    this.details_ = opt_details || %GetFunctionScopeDetails(fun.value(), index);
    this.fun_value_ = fun.value();
    this.break_id_ = UNDEFINED;
  } else {
    this.details_ =
      opt_details || %GetGeneratorScopeDetails(gen.value(), index);
    this.gen_value_ = gen.value();
    this.break_id_ = UNDEFINED;
  }
  this.index_ = index;
}


ScopeDetails.prototype.type = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsTypeIndex];
};


ScopeDetails.prototype.object = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsObjectIndex];
};


ScopeDetails.prototype.name = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsNameIndex];
};


ScopeDetails.prototype.startPosition = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsStartPositionIndex];
}


ScopeDetails.prototype.endPosition = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsEndPositionIndex];
}

ScopeDetails.prototype.func = function() {
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
  }
  return this.details_[kScopeDetailsFunctionIndex];
}


ScopeDetails.prototype.setVariableValueImpl = function(name, new_value) {
  var raw_res;
  if (!IS_UNDEFINED(this.break_id_)) {
    %CheckExecutionState(this.break_id_);
    raw_res = %SetScopeVariableValue(this.break_id_, this.frame_id_,
        this.inlined_frame_id_, this.index_, name, new_value);
  } else if (!IS_UNDEFINED(this.fun_value_)) {
    raw_res = %SetScopeVariableValue(this.fun_value_, null, null, this.index_,
        name, new_value);
  } else {
    raw_res = %SetScopeVariableValue(this.gen_value_, null, null, this.index_,
        name, new_value);
  }
  if (!raw_res) throw %make_error(kDebugger, "Failed to set variable value");
};


/**
 * Mirror object for scope of frame or function. Either frame or function must
 * be specified.
 * @param {FrameMirror} frame The frame this scope is a part of
 * @param {FunctionMirror} function The function this scope is a part of
 * @param {GeneratorMirror} gen The generator this scope is a part of
 * @param {number} index The scope index in the frame
 * @param {Array=} opt_details Raw scope details data
 * @constructor
 * @extends Mirror
 */
function ScopeMirror(frame, fun, gen, index, opt_details) {
  %_Call(Mirror, this, MirrorType.SCOPE_TYPE);
  if (frame) {
    this.frame_index_ = frame.index_;
  } else {
    this.frame_index_ = UNDEFINED;
  }
  this.scope_index_ = index;
  this.details_ = new ScopeDetails(frame, fun, gen, index, opt_details);
}
inherits(ScopeMirror, Mirror);


ScopeMirror.prototype.details = function() {
  return this.details_;
};


ScopeMirror.prototype.frameIndex = function() {
  return this.frame_index_;
};


ScopeMirror.prototype.scopeIndex = function() {
  return this.scope_index_;
};


ScopeMirror.prototype.scopeType = function() {
  return this.details_.type();
};


ScopeMirror.prototype.scopeObject = function() {
  // For local, closure and script scopes create a mirror
  // as these objects are created on the fly materializing the local
  // or closure scopes and therefore will not preserve identity.
  return MakeMirror(this.details_.object());
};


ScopeMirror.prototype.setVariableValue = function(name, new_value) {
  this.details_.setVariableValueImpl(name, new_value);
};


/**
 * Mirror object for script source.
 * @param {Script} script The script object
 * @constructor
 * @extends Mirror
 */
function ScriptMirror(script) {
  %_Call(Mirror, this, MirrorType.SCRIPT_TYPE);
  this.script_ = script;
  this.context_ = new ContextMirror(script.context_data);
}
inherits(ScriptMirror, Mirror);


ScriptMirror.prototype.value = function() {
  return this.script_;
};


ScriptMirror.prototype.name = function() {
  return this.script_.name || this.script_.nameOrSourceURL();
};


ScriptMirror.prototype.id = function() {
  return this.script_.id;
};


ScriptMirror.prototype.source = function() {
  return this.script_.source;
};


ScriptMirror.prototype.setSource = function(source) {
  if (!IS_STRING(source)) throw %make_error(kDebugger, "Source is not a string");
  %DebugSetScriptSource(this.script_, source);
};


ScriptMirror.prototype.lineOffset = function() {
  return this.script_.line_offset;
};


ScriptMirror.prototype.columnOffset = function() {
  return this.script_.column_offset;
};


ScriptMirror.prototype.data = function() {
  return this.script_.data;
};


ScriptMirror.prototype.scriptType = function() {
  return this.script_.type;
};


ScriptMirror.prototype.compilationType = function() {
  return this.script_.compilation_type;
};


ScriptMirror.prototype.lineCount = function() {
  return %ScriptLineCount(this.script_);
};


ScriptMirror.prototype.locationFromPosition = function(
    position, include_resource_offset) {
  return this.script_.locationFromPosition(position, include_resource_offset);
};


ScriptMirror.prototype.context = function() {
  return this.context_;
};


ScriptMirror.prototype.evalFromScript = function() {
  return MakeMirror(this.script_.eval_from_script);
};


ScriptMirror.prototype.evalFromFunctionName = function() {
  return MakeMirror(this.script_.eval_from_function_name);
};


ScriptMirror.prototype.evalFromLocation = function() {
  var eval_from_script = this.evalFromScript();
  if (!eval_from_script.isUndefined()) {
    var position = this.script_.eval_from_script_position;
    return eval_from_script.locationFromPosition(position, true);
  }
};


ScriptMirror.prototype.toText = function() {
  var result = '';
  result += this.name();
  result += ' (lines: ';
  if (this.lineOffset() > 0) {
    result += this.lineOffset();
    result += '-';
    result += this.lineOffset() + this.lineCount() - 1;
  } else {
    result += this.lineCount();
  }
  result += ')';
  return result;
};


/**
 * Mirror object for context.
 * @param {Object} data The context data
 * @constructor
 * @extends Mirror
 */
function ContextMirror(data) {
  %_Call(Mirror, this, MirrorType.CONTEXT_TYPE);
  this.data_ = data;
}
inherits(ContextMirror, Mirror);


ContextMirror.prototype.data = function() {
  return this.data_;
};

// ----------------------------------------------------------------------------
// Exports

utils.InstallConstants(global, [
  "MakeMirror", MakeMirror,
  "ScopeType", ScopeType,
  "PropertyType", PropertyType,
  "PropertyAttribute", PropertyAttribute,
  "Mirror", Mirror,
  "ValueMirror", ValueMirror,
  "UndefinedMirror", UndefinedMirror,
  "NullMirror", NullMirror,
  "BooleanMirror", BooleanMirror,
  "NumberMirror", NumberMirror,
  "StringMirror", StringMirror,
  "SymbolMirror", SymbolMirror,
  "ObjectMirror", ObjectMirror,
  "FunctionMirror", FunctionMirror,
  "UnresolvedFunctionMirror", UnresolvedFunctionMirror,
  "ArrayMirror", ArrayMirror,
  "DateMirror", DateMirror,
  "RegExpMirror", RegExpMirror,
  "ErrorMirror", ErrorMirror,
  "PromiseMirror", PromiseMirror,
  "MapMirror", MapMirror,
  "SetMirror", SetMirror,
  "IteratorMirror", IteratorMirror,
  "GeneratorMirror", GeneratorMirror,
  "PropertyMirror", PropertyMirror,
  "InternalPropertyMirror", InternalPropertyMirror,
  "FrameMirror", FrameMirror,
  "ScriptMirror", ScriptMirror,
  "ScopeMirror", ScopeMirror,
  "FrameDetails", FrameDetails,
]);

})
