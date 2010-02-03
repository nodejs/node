// Copyright 2006-2008 the V8 project authors. All rights reserved.
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

// Touch the RegExp and Date functions to make sure that date-delay.js and
// regexp-delay.js has been loaded. This is required as the mirrors use
// functions within these files through the builtins object.
RegExp;
Date;


// Handle id counters.
var next_handle_ = 0;
var next_transient_handle_ = -1;

// Mirror cache.
var mirror_cache_ = [];


/**
 * Clear the mirror handle cache.
 */
function ClearMirrorCache() {
  next_handle_ = 0;
  mirror_cache_ = [];
}


/**
 * Returns the mirror for a specified value or object.
 *
 * @param {value or Object} value the value or object to retreive the mirror for
 * @param {boolean} transient indicate whether this object is transient and
 *    should not be added to the mirror cache. The default is not transient.
 * @returns {Mirror} the mirror reflects the passed value or object
 */
function MakeMirror(value, opt_transient) {
  var mirror;

  // Look for non transient mirrors in the mirror cache.
  if (!opt_transient) {
    for (id in mirror_cache_) {
      mirror = mirror_cache_[id];
      if (mirror.value() === value) {
        return mirror;
      }
      // Special check for NaN as NaN == NaN is false.
      if (mirror.isNumber() && isNaN(mirror.value()) &&
          typeof value == 'number' && isNaN(value)) {
        return mirror;
      }
    }
  }
  
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
  } else if (IS_ARRAY(value)) {
    mirror = new ArrayMirror(value);
  } else if (IS_DATE(value)) {
    mirror = new DateMirror(value);
  } else if (IS_FUNCTION(value)) {
    mirror = new FunctionMirror(value);
  } else if (IS_REGEXP(value)) {
    mirror = new RegExpMirror(value);
  } else if (IS_ERROR(value)) {
    mirror = new ErrorMirror(value);
  } else if (IS_SCRIPT(value)) {
    mirror = new ScriptMirror(value);
  } else {
    mirror = new ObjectMirror(value, OBJECT_TYPE, opt_transient);
  }

  mirror_cache_[mirror.handle()] = mirror;
  return mirror;
}


/**
 * Returns the mirror for a specified mirror handle.
 *
 * @param {number} handle the handle to find the mirror for
 * @returns {Mirror or undefiend} the mirror with the requested handle or
 *     undefined if no mirror with the requested handle was found
 */
function LookupMirror(handle) {
  return mirror_cache_[handle];
}

  
/**
 * Returns the mirror for the undefined value.
 *
 * @returns {Mirror} the mirror reflects the undefined value
 */
function GetUndefinedMirror() {
  return MakeMirror(void 0);
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


// Type names of the different mirrors.
const UNDEFINED_TYPE = 'undefined';
const NULL_TYPE = 'null';
const BOOLEAN_TYPE = 'boolean';
const NUMBER_TYPE = 'number';
const STRING_TYPE = 'string';
const OBJECT_TYPE = 'object';
const FUNCTION_TYPE = 'function';
const REGEXP_TYPE = 'regexp';
const ERROR_TYPE = 'error';
const PROPERTY_TYPE = 'property';
const FRAME_TYPE = 'frame';
const SCRIPT_TYPE = 'script';
const CONTEXT_TYPE = 'context';
const SCOPE_TYPE = 'scope';

// Maximum length when sending strings through the JSON protocol.
const kMaxProtocolStringLength = 80;

// Different kind of properties.
PropertyKind = {};
PropertyKind.Named   = 1;
PropertyKind.Indexed = 2;


// A copy of the PropertyType enum from global.h
PropertyType = {};
PropertyType.Normal             = 0;
PropertyType.Field              = 1;
PropertyType.ConstantFunction   = 2;
PropertyType.Callbacks          = 3;
PropertyType.Interceptor        = 4;
PropertyType.MapTransition      = 5;
PropertyType.ConstantTransition = 6;
PropertyType.NullDescriptor     = 7;


// Different attributes for a property.
PropertyAttribute = {};
PropertyAttribute.None       = NONE;
PropertyAttribute.ReadOnly   = READ_ONLY;
PropertyAttribute.DontEnum   = DONT_ENUM;
PropertyAttribute.DontDelete = DONT_DELETE;


// A copy of the scope types from runtime.cc.
ScopeType = { Global: 0,
              Local: 1,
              With: 2,
              Closure: 3,
              Catch: 4 };


// Mirror hierarchy:
//   - Mirror
//     - ValueMirror
//       - UndefinedMirror
//       - NullMirror
//       - NumberMirror
//       - StringMirror
//       - ObjectMirror
//         - FunctionMirror
//           - UnresolvedFunctionMirror
//         - ArrayMirror
//         - DateMirror
//         - RegExpMirror
//         - ErrorMirror
//     - PropertyMirror
//     - FrameMirror
//     - ScriptMirror


/**
 * Base class for all mirror objects.
 * @param {string} type The type of the mirror
 * @constructor
 */
function Mirror(type) {
  this.type_ = type;
};


Mirror.prototype.type = function() {
  return this.type_;
};


/**
 * Check whether the mirror reflects a value.
 * @returns {boolean} True if the mirror reflects a value.
 */
Mirror.prototype.isValue = function() {
  return this instanceof ValueMirror;
}


/**
 * Check whether the mirror reflects the undefined value.
 * @returns {boolean} True if the mirror reflects the undefined value.
 */
Mirror.prototype.isUndefined = function() {
  return this instanceof UndefinedMirror;
}


/**
 * Check whether the mirror reflects the null value.
 * @returns {boolean} True if the mirror reflects the null value
 */
Mirror.prototype.isNull = function() {
  return this instanceof NullMirror;
}


/**
 * Check whether the mirror reflects a boolean value.
 * @returns {boolean} True if the mirror reflects a boolean value
 */
Mirror.prototype.isBoolean = function() {
  return this instanceof BooleanMirror;
}


/**
 * Check whether the mirror reflects a number value.
 * @returns {boolean} True if the mirror reflects a number value
 */
Mirror.prototype.isNumber = function() {
  return this instanceof NumberMirror;
}


/**
 * Check whether the mirror reflects a string value.
 * @returns {boolean} True if the mirror reflects a string value
 */
Mirror.prototype.isString = function() {
  return this instanceof StringMirror;
}


/**
 * Check whether the mirror reflects an object.
 * @returns {boolean} True if the mirror reflects an object
 */
Mirror.prototype.isObject = function() {
  return this instanceof ObjectMirror;
}


/**
 * Check whether the mirror reflects a function.
 * @returns {boolean} True if the mirror reflects a function
 */
Mirror.prototype.isFunction = function() {
  return this instanceof FunctionMirror;
}


/**
 * Check whether the mirror reflects an unresolved function.
 * @returns {boolean} True if the mirror reflects an unresolved function
 */
Mirror.prototype.isUnresolvedFunction = function() {
  return this instanceof UnresolvedFunctionMirror;
}


/**
 * Check whether the mirror reflects an array.
 * @returns {boolean} True if the mirror reflects an array
 */
Mirror.prototype.isArray = function() {
  return this instanceof ArrayMirror;
}


/**
 * Check whether the mirror reflects a date.
 * @returns {boolean} True if the mirror reflects a date
 */
Mirror.prototype.isDate = function() {
  return this instanceof DateMirror;
}


/**
 * Check whether the mirror reflects a regular expression.
 * @returns {boolean} True if the mirror reflects a regular expression
 */
Mirror.prototype.isRegExp = function() {
  return this instanceof RegExpMirror;
}


/**
 * Check whether the mirror reflects an error.
 * @returns {boolean} True if the mirror reflects an error
 */
Mirror.prototype.isError = function() {
  return this instanceof ErrorMirror;
}


/**
 * Check whether the mirror reflects a property.
 * @returns {boolean} True if the mirror reflects a property
 */
Mirror.prototype.isProperty = function() {
  return this instanceof PropertyMirror;
}


/**
 * Check whether the mirror reflects a stack frame.
 * @returns {boolean} True if the mirror reflects a stack frame
 */
Mirror.prototype.isFrame = function() {
  return this instanceof FrameMirror;
}


/**
 * Check whether the mirror reflects a script.
 * @returns {boolean} True if the mirror reflects a script
 */
Mirror.prototype.isScript = function() {
  return this instanceof ScriptMirror;
}


/**
 * Check whether the mirror reflects a context.
 * @returns {boolean} True if the mirror reflects a context
 */
Mirror.prototype.isContext = function() {
  return this instanceof ContextMirror;
}


/**
 * Check whether the mirror reflects a scope.
 * @returns {boolean} True if the mirror reflects a scope
 */
Mirror.prototype.isScope = function() {
  return this instanceof ScopeMirror;
}


/**
 * Allocate a handle id for this object.
 */
Mirror.prototype.allocateHandle_ = function() {
  this.handle_ = next_handle_++;
}


/**
 * Allocate a transient handle id for this object. Transient handles are
 * negative.
 */
Mirror.prototype.allocateTransientHandle_ = function() {
  this.handle_ = next_transient_handle_--;
}


Mirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "#<" + builtins.GetInstanceName(this.constructor.name) + ">";
}


/**
 * Base class for all value mirror objects.
 * @param {string} type The type of the mirror
 * @param {value} value The value reflected by this mirror
 * @param {boolean} transient indicate whether this object is transient with a
 *    transient handle
 * @constructor
 * @extends Mirror
 */
function ValueMirror(type, value, transient) {
  Mirror.call(this, type);
  this.value_ = value;
  if (!transient) {
    this.allocateHandle_();
  } else {
    this.allocateTransientHandle_();
  }
}
inherits(ValueMirror, Mirror);


Mirror.prototype.handle = function() {
  return this.handle_;
};


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
         type === 'string';
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
  ValueMirror.call(this, UNDEFINED_TYPE, void 0);
}
inherits(UndefinedMirror, ValueMirror);


UndefinedMirror.prototype.toText = function() {
  return 'undefined';
}


/**
 * Mirror object for null.
 * @constructor
 * @extends ValueMirror
 */
function NullMirror() {
  ValueMirror.call(this, NULL_TYPE, null);
}
inherits(NullMirror, ValueMirror);


NullMirror.prototype.toText = function() {
  return 'null';
}


/**
 * Mirror object for boolean values.
 * @param {boolean} value The boolean value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function BooleanMirror(value) {
  ValueMirror.call(this, BOOLEAN_TYPE, value);
}
inherits(BooleanMirror, ValueMirror);


BooleanMirror.prototype.toText = function() {
  return this.value_ ? 'true' : 'false';
}


/**
 * Mirror object for number values.
 * @param {number} value The number value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function NumberMirror(value) {
  ValueMirror.call(this, NUMBER_TYPE, value);
}
inherits(NumberMirror, ValueMirror);


NumberMirror.prototype.toText = function() {
  return %NumberToString(this.value_);
}


/**
 * Mirror object for string values.
 * @param {string} value The string value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function StringMirror(value) {
  ValueMirror.call(this, STRING_TYPE, value);
}
inherits(StringMirror, ValueMirror);


StringMirror.prototype.length = function() {
  return this.value_.length;
};


StringMirror.prototype.toText = function() {
  if (this.length() > kMaxProtocolStringLength) {
    return this.value_.substring(0, kMaxProtocolStringLength) +
           '... (length: ' + this.length() + ')';
  } else {
    return this.value_;
  }
}


/**
 * Mirror object for objects.
 * @param {object} value The object reflected by this mirror
 * @param {boolean} transient indicate whether this object is transient with a
 *    transient handle
 * @constructor
 * @extends ValueMirror
 */
function ObjectMirror(value, type, transient) {
  ValueMirror.call(this, type || OBJECT_TYPE, value, transient);
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
ObjectMirror.prototype.propertyNames = function(kind, limit) {
  // Find kind and limit and allocate array for the result
  kind = kind || PropertyKind.Named | PropertyKind.Indexed;

  var propertyNames;
  var elementNames;
  var total = 0;
  
  // Find all the named properties.
  if (kind & PropertyKind.Named) {
    // Get the local property names.
    propertyNames = %GetLocalPropertyNames(this.value_);
    total += propertyNames.length;

    // Get names for named interceptor properties if any.
    if (this.hasNamedInterceptor() && (kind & PropertyKind.Named)) {
      var namedInterceptorNames =
          %GetNamedInterceptorPropertyNames(this.value_);
      if (namedInterceptorNames) {
        propertyNames = propertyNames.concat(namedInterceptorNames);
        total += namedInterceptorNames.length;
      }
    }
  }

  // Find all the indexed properties.
  if (kind & PropertyKind.Indexed) {
    // Get the local element names.
    elementNames = %GetLocalElementNames(this.value_);
    total += elementNames.length;

    // Get names for indexed interceptor properties.
    if (this.hasIndexedInterceptor() && (kind & PropertyKind.Indexed)) {
      var indexedInterceptorNames =
          %GetIndexedInterceptorElementNames(this.value_);
      if (indexedInterceptorNames) {
        elementNames = elementNames.concat(indexedInterceptorNames);
        total += indexedInterceptorNames.length;
      }
    }
  }
  limit = Math.min(limit || total, total);

  var names = new Array(limit);
  var index = 0;

  // Copy names for named properties.
  if (kind & PropertyKind.Named) {
    for (var i = 0; index < limit && i < propertyNames.length; i++) {
      names[index++] = propertyNames[i];
    }
  }

  // Copy names for indexed properties.
  if (kind & PropertyKind.Indexed) {
    for (var i = 0; index < limit && i < elementNames.length; i++) {
      names[index++] = elementNames[i];
    }
  }

  return names;
};


/**
 * Return the properties for this object as an array of PropertyMirror objects.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     properties are requested
 * @param {number} limit Limit the number of properties returend to the
       specified value
 * @return {Array} Property mirrors for this object
 */
ObjectMirror.prototype.properties = function(kind, limit) {
  var names = this.propertyNames(kind, limit);
  var properties = new Array(names.length);
  for (var i = 0; i < names.length; i++) {
    properties[i] = this.property(names[i]);
  }

  return properties;
};


ObjectMirror.prototype.property = function(name) {
  var details = %DebugGetPropertyDetails(this.value_, %ToString(name));
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

    // Skip properties which are defined through assessors.
    var property = properties[i];
    if (property.propertyType() != PropertyType.Callbacks) {
      if (%_ObjectEquals(property.value_, value.value_)) {
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
  return '#<' + builtins.GetInstanceName(name) + '>';
};


/**
 * Mirror object for functions.
 * @param {function} value The function object reflected by this mirror.
 * @constructor
 * @extends ObjectMirror
 */
function FunctionMirror(value) {
  ObjectMirror.call(this, value, FUNCTION_TYPE);
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
    return builtins.FunctionSourceString(this.value_);
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
    var script = %FunctionGetScript(this.value_);
    if (script) {
      return MakeMirror(script);
    }
  }
};


/**
 * Returns the script source position for the function. Only makes sense
 * for functions which has a script defined.
 * @return {Number or undefined} in-script position for the function
 */
FunctionMirror.prototype.sourcePosition_ = function() {
  // Return script if function is resolved. Otherwise just fall through
  // to return undefined.
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
  if (this.resolved() && this.script()) {
    return this.script().locationFromPosition(this.sourcePosition_(),
                                              true);
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


FunctionMirror.prototype.toText = function() {
  return this.source();
}


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
  ValueMirror.call(this, FUNCTION_TYPE, value);
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


UnresolvedFunctionMirror.prototype.inferredName = function() {
  return undefined;
};


UnresolvedFunctionMirror.prototype.propertyNames = function(kind, limit) {
  return [];
}


/**
 * Mirror object for arrays.
 * @param {Array} value The Array object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ArrayMirror(value) {
  ObjectMirror.call(this, value);
}
inherits(ArrayMirror, ObjectMirror);


ArrayMirror.prototype.length = function() {
  return this.value_.length;
};


ArrayMirror.prototype.indexedPropertiesFromRange = function(opt_from_index, opt_to_index) {
  var from_index = opt_from_index || 0;
  var to_index = opt_to_index || this.length() - 1;
  if (from_index > to_index) return new Array();
  var values = new Array(to_index - from_index + 1);
  for (var i = from_index; i <= to_index; i++) {
    var details = %DebugGetPropertyDetails(this.value_, %ToString(i));
    var value;
    if (details) {
      value = new PropertyMirror(this, i, details);
    } else {
      value = GetUndefinedMirror();
    }
    values[i - from_index] = value;
  }
  return values;
}


/**
 * Mirror object for dates.
 * @param {Date} value The Date object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function DateMirror(value) {
  ObjectMirror.call(this, value);
}
inherits(DateMirror, ObjectMirror);


DateMirror.prototype.toText = function() {
  var s = JSON.stringify(this.value_);
  return s.substring(1, s.length - 1);  // cut quotes
}


/**
 * Mirror object for regular expressions.
 * @param {RegExp} value The RegExp object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function RegExpMirror(value) {
  ObjectMirror.call(this, value, REGEXP_TYPE);
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


RegExpMirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "/" + this.source() + "/";
}


/**
 * Mirror object for error objects.
 * @param {Error} value The error object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ErrorMirror(value) {
  ObjectMirror.call(this, value, ERROR_TYPE);
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
    str = builtins.ToDetailString(this.value_);
  } catch (e) {
    str = '#<an Error>';
  }
  return str;
}


/**
 * Base mirror object for properties.
 * @param {ObjectMirror} mirror The mirror object having this property
 * @param {string} name The name of the property
 * @param {Array} details Details about the property
 * @constructor
 * @extends Mirror
 */
function PropertyMirror(mirror, name, details) {
  Mirror.call(this, PROPERTY_TYPE);
  this.mirror_ = mirror;
  this.name_ = name;
  this.value_ = details[0];
  this.details_ = details[1];
  if (details.length > 2) {
    this.exception_ = details[2]
    this.getter_ = details[3];
    this.setter_ = details[4];
  }
}
inherits(PropertyMirror, Mirror);


PropertyMirror.prototype.isReadOnly = function() {
  return (this.attributes() & PropertyAttribute.ReadOnly) != 0;
}


PropertyMirror.prototype.isEnum = function() {
  return (this.attributes() & PropertyAttribute.DontEnum) == 0;
}


PropertyMirror.prototype.canDelete = function() {
  return (this.attributes() & PropertyAttribute.DontDelete) == 0;
}


PropertyMirror.prototype.name = function() {
  return this.name_;
}


PropertyMirror.prototype.isIndexed = function() {
  for (var i = 0; i < this.name_.length; i++) {
    if (this.name_[i] < '0' || '9' < this.name_[i]) {
      return false;
    }
  }
  return true;
}


PropertyMirror.prototype.value = function() {
  return MakeMirror(this.value_, false);
}


/**
 * Returns whether this property value is an exception.
 * @return {booolean} True if this property value is an exception
 */
PropertyMirror.prototype.isException = function() {
  return this.exception_ ? true : false;
}


PropertyMirror.prototype.attributes = function() {
  return %DebugPropertyAttributesFromDetails(this.details_);
}


PropertyMirror.prototype.propertyType = function() {
  return %DebugPropertyTypeFromDetails(this.details_);
}


PropertyMirror.prototype.insertionIndex = function() {
  return %DebugPropertyIndexFromDetails(this.details_);
}


/**
 * Returns whether this property has a getter defined through __defineGetter__.
 * @return {booolean} True if this property has a getter
 */
PropertyMirror.prototype.hasGetter = function() {
  return this.getter_ ? true : false;
}


/**
 * Returns whether this property has a setter defined through __defineSetter__.
 * @return {booolean} True if this property has a setter
 */
PropertyMirror.prototype.hasSetter = function() {
  return this.setter_ ? true : false;
}


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
}


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
}


/**
 * Returns whether this property is natively implemented by the host or a set
 * through JavaScript code.
 * @return {boolean} True if the property is 
 *     UndefinedMirror if there is no setter for this property
 */
PropertyMirror.prototype.isNative = function() {
  return (this.propertyType() == PropertyType.Interceptor) ||
         ((this.propertyType() == PropertyType.Callbacks) &&
          !this.hasGetter() && !this.hasSetter());
}


const kFrameDetailsFrameIdIndex = 0;
const kFrameDetailsReceiverIndex = 1;
const kFrameDetailsFunctionIndex = 2;
const kFrameDetailsArgumentCountIndex = 3;
const kFrameDetailsLocalCountIndex = 4;
const kFrameDetailsSourcePositionIndex = 5;
const kFrameDetailsConstructCallIndex = 6;
const kFrameDetailsDebuggerFrameIndex = 7;
const kFrameDetailsFirstDynamicIndex = 8;

const kFrameDetailsNameIndex = 0;
const kFrameDetailsValueIndex = 1;
const kFrameDetailsNameValueSize = 2;

/**
 * Wrapper for the frame details information retreived from the VM. The frame
 * details from the VM is an array with the following content. See runtime.cc
 * Runtime_GetFrameDetails.
 *     0: Id
 *     1: Receiver
 *     2: Function
 *     3: Argument count
 *     4: Local count
 *     5: Source position
 *     6: Construct call
 *     Arguments name, value
 *     Locals name, value
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
}


FrameDetails.prototype.receiver = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsReceiverIndex];
}


FrameDetails.prototype.func = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsFunctionIndex];
}


FrameDetails.prototype.isConstructCall = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsConstructCallIndex];
}


FrameDetails.prototype.isDebuggerFrame = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsDebuggerFrameIndex];
}


FrameDetails.prototype.argumentCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsArgumentCountIndex];
}


FrameDetails.prototype.argumentName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex]
  }
}


FrameDetails.prototype.argumentValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex]
  }
}


FrameDetails.prototype.localCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsLocalCountIndex];
}


FrameDetails.prototype.sourcePosition = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsSourcePositionIndex];
}


FrameDetails.prototype.localName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex + this.argumentCount() * kFrameDetailsNameValueSize
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex]
  }
}


FrameDetails.prototype.localValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex + this.argumentCount() * kFrameDetailsNameValueSize
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex]
  }
}


FrameDetails.prototype.scopeCount = function() {
  return %GetScopeCount(this.break_id_, this.frameId());
}


/**
 * Mirror object for stack frames.
 * @param {number} break_id The break id in the VM for which this frame is
       valid
 * @param {number} index The frame index (top frame is index 0)
 * @constructor
 * @extends Mirror
 */
function FrameMirror(break_id, index) {
  Mirror.call(this, FRAME_TYPE);
  this.break_id_ = break_id;
  this.index_ = index;
  this.details_ = new FrameDetails(break_id, index);
}
inherits(FrameMirror, Mirror);


FrameMirror.prototype.index = function() {
  return this.index_;
};


FrameMirror.prototype.func = function() {
  // Get the function for this frame from the VM.
  var f = this.details_.func();
  
  // Create a function mirror. NOTE: MakeMirror cannot be used here as the
  // value returned from the VM might be a string if the function for the
  // frame is unresolved.
  if (IS_FUNCTION(f)) {
    return MakeMirror(f);
  } else {
    return new UnresolvedFunctionMirror(f);
  }
};


FrameMirror.prototype.receiver = function() {
  return MakeMirror(this.details_.receiver());
};


FrameMirror.prototype.isConstructCall = function() {
  return this.details_.isConstructCall();
};


FrameMirror.prototype.isDebuggerFrame = function() {
  return this.details_.isDebuggerFrame();
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


FrameMirror.prototype.sourcePosition = function() {
  return this.details_.sourcePosition();
};


FrameMirror.prototype.sourceLocation = function() {
  if (this.func().resolved() && this.func().script()) {
    return this.func().script().locationFromPosition(this.sourcePosition(),
                                                     true);
  }
};


FrameMirror.prototype.sourceLine = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.line;
    }
  }
};


FrameMirror.prototype.sourceColumn = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.column;
    }
  }
};


FrameMirror.prototype.sourceLineText = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.sourceText();
    }
  }
};


FrameMirror.prototype.scopeCount = function() {
  return this.details_.scopeCount();
};


FrameMirror.prototype.scope = function(index) {
  return new ScopeMirror(this, index);
};


FrameMirror.prototype.evaluate = function(source, disable_break) {
  var result = %DebugEvaluate(this.break_id_, this.details_.frameId(),
                              source, Boolean(disable_break));
  return MakeMirror(result);
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
    var display_receiver = !receiver.className || receiver.className() != 'global';
    if (display_receiver) {
      result += receiver.toText();
    }
    // Try to find the function as a property in the receiver. Include the
    // prototype chain in the lookup.
    var property = GetUndefinedMirror();
    if (!receiver.isUndefined()) {
      for (var r = receiver; !r.isNull() && property.isUndefined(); r = r.protoObject()) {
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
        result += property.name();
      } else {
        result += '[';
        result += property.name();
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

  return result;
}


FrameMirror.prototype.sourceAndPositionText = function() {
  // Format source and position.
  var result = '';
  var func = this.func();
  if (func.resolved()) {
    if (func.script()) {
      if (func.script().name()) {
        result += func.script().name();
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
}


FrameMirror.prototype.localsText = function() {
  // Format local variables.
  var result = '';
  var locals_count = this.localCount()
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
}


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
}


const kScopeDetailsTypeIndex = 0;
const kScopeDetailsObjectIndex = 1;

function ScopeDetails(frame, index) {
  this.break_id_ = frame.break_id_;
  this.details_ = %GetScopeDetails(frame.break_id_,
                                   frame.details_.frameId(),
                                   index);
}


ScopeDetails.prototype.type = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kScopeDetailsTypeIndex];
}


ScopeDetails.prototype.object = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kScopeDetailsObjectIndex];
}


/**
 * Mirror object for scope.
 * @param {FrameMirror} frame The frame this scope is a part of
 * @param {number} index The scope index in the frame
 * @constructor
 * @extends Mirror
 */
function ScopeMirror(frame, index) {
  Mirror.call(this, SCOPE_TYPE);
  this.frame_index_ = frame.index_;
  this.scope_index_ = index;
  this.details_ = new ScopeDetails(frame, index);
}
inherits(ScopeMirror, Mirror);


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
  // For local and closure scopes create a transient mirror as these objects are
  // created on the fly materializing the local or closure scopes and
  // therefore will not preserve identity.
  var transient = this.scopeType() == ScopeType.Local ||
                  this.scopeType() == ScopeType.Closure;
  return MakeMirror(this.details_.object(), transient);
};


/**
 * Mirror object for script source.
 * @param {Script} script The script object
 * @constructor
 * @extends Mirror
 */
function ScriptMirror(script) {
  Mirror.call(this, SCRIPT_TYPE);
  this.script_ = script;
  this.context_ = new ContextMirror(script.context_data);
  this.allocateHandle_();
}
inherits(ScriptMirror, Mirror);


ScriptMirror.prototype.value = function() {
  return this.script_;
};


ScriptMirror.prototype.name = function() {
  return this.script_.name;
};


ScriptMirror.prototype.id = function() {
  return this.script_.id;
};


ScriptMirror.prototype.source = function() {
  return this.script_.source;
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
  return this.script_.lineCount();
};


ScriptMirror.prototype.locationFromPosition = function(
    position, include_resource_offset) {
  return this.script_.locationFromPosition(position, include_resource_offset);
}


ScriptMirror.prototype.sourceSlice = function (opt_from_line, opt_to_line) {
  return this.script_.sourceSlice(opt_from_line, opt_to_line);
}


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
}


/**
 * Mirror object for context.
 * @param {Object} data The context data
 * @constructor
 * @extends Mirror
 */
function ContextMirror(data) {
  Mirror.call(this, CONTEXT_TYPE);
  this.data_ = data;
  this.allocateHandle_();
}
inherits(ContextMirror, Mirror);


ContextMirror.prototype.data = function() {
  return this.data_;
};


/**
 * Returns a mirror serializer
 *
 * @param {boolean} details Set to true to include details
 * @param {Object} options Options comtrolling the serialization
 *     The following options can be set:
 *       includeSource: include ths full source of scripts
 * @returns {MirrorSerializer} mirror serializer
 */
function MakeMirrorSerializer(details, options) {
  return new JSONProtocolSerializer(details, options);
}


/**
 * Object for serializing a mirror objects and its direct references.
 * @param {boolean} details Indicates whether to include details for the mirror
 *     serialized
 * @constructor
 */
function JSONProtocolSerializer(details, options) {
  this.details_ = details;
  this.options_ = options;
  this.mirrors_ = [ ];
}


/**
 * Returns a serialization of an object reference. The referenced object are
 * added to the serialization state.
 *
 * @param {Mirror} mirror The mirror to serialize
 * @returns {String} JSON serialization
 */
JSONProtocolSerializer.prototype.serializeReference = function(mirror) {
  return this.serialize_(mirror, true, true);
}


/**
 * Returns a serialization of an object value. The referenced objects are
 * added to the serialization state.
 *
 * @param {Mirror} mirror The mirror to serialize
 * @returns {String} JSON serialization
 */
JSONProtocolSerializer.prototype.serializeValue = function(mirror) {
  var json = this.serialize_(mirror, false, true);
  return json;
}


/**
 * Returns a serialization of all the objects referenced.
 *
 * @param {Mirror} mirror The mirror to serialize.
 * @returns {Array.<Object>} Array of the referenced objects converted to
 *     protcol objects.
 */
JSONProtocolSerializer.prototype.serializeReferencedObjects = function() {
  // Collect the protocol representation of the referenced objects in an array.
  var content = [];
  
  // Get the number of referenced objects.
  var count = this.mirrors_.length;
  
  for (var i = 0; i < count; i++) {
    content.push(this.serialize_(this.mirrors_[i], false, false));
  }

  return content;
}


JSONProtocolSerializer.prototype.includeSource_ = function() {
  return this.options_ && this.options_.includeSource;
}


JSONProtocolSerializer.prototype.inlineRefs_ = function() {
  return this.options_ && this.options_.inlineRefs;
}


JSONProtocolSerializer.prototype.add_ = function(mirror) {
  // If this mirror is already in the list just return.
  for (var i = 0; i < this.mirrors_.length; i++) {
    if (this.mirrors_[i] === mirror) {
      return;
    }
  }
  
  // Add the mirror to the list of mirrors to be serialized.
  this.mirrors_.push(mirror);
}


/**
 * Formats mirror object to protocol reference object with some data that can
 * be used to display the value in debugger.
 * @param {Mirror} mirror Mirror to serialize.
 * @return {Object} Protocol reference object.
 */
JSONProtocolSerializer.prototype.serializeReferenceWithDisplayData_ = 
    function(mirror) {
  var o = {};
  o.ref = mirror.handle();
  o.type = mirror.type();
  switch (mirror.type()) {
    case UNDEFINED_TYPE:
    case NULL_TYPE:
    case BOOLEAN_TYPE:
    case NUMBER_TYPE:
      o.value = mirror.value();
      break;
    case STRING_TYPE:
      // Limit string length.
      o.value = mirror.toText();
      break;
    case FUNCTION_TYPE:
      o.name = mirror.name();
      o.inferredName = mirror.inferredName();
      if (mirror.script()) {
        o.scriptId = mirror.script().id();
      }
      break;
    case ERROR_TYPE:
    case REGEXP_TYPE:
      o.value = mirror.toText();
      break;
    case OBJECT_TYPE:
      o.className = mirror.className();
      break;
  }
  return o;
};


JSONProtocolSerializer.prototype.serialize_ = function(mirror, reference,
                                                       details) {
  // If serializing a reference to a mirror just return the reference and add
  // the mirror to the referenced mirrors.
  if (reference &&
      (mirror.isValue() || mirror.isScript() || mirror.isContext())) {
    if (this.inlineRefs_() && mirror.isValue()) {
      return this.serializeReferenceWithDisplayData_(mirror);
    } else {
      this.add_(mirror);
      return {'ref' : mirror.handle()};
    }
  }
  
  // Collect the JSON property/value pairs.
  var content = {};

  // Add the mirror handle.
  if (mirror.isValue() || mirror.isScript() || mirror.isContext()) {
    content.handle = mirror.handle();
  }

  // Always add the type.
  content.type = mirror.type();

  switch (mirror.type()) {
    case UNDEFINED_TYPE:
    case NULL_TYPE:
      // Undefined and null are represented just by their type.
      break;

    case BOOLEAN_TYPE:
      // Boolean values are simply represented by their value.
      content.value = mirror.value();
      break;

    case NUMBER_TYPE:
      // Number values are simply represented by their value.
      content.value = NumberToJSON_(mirror.value());
      break;

    case STRING_TYPE:
      // String values might have their value cropped to keep down size.
      if (mirror.length() > kMaxProtocolStringLength) {
        var str = mirror.value().substring(0, kMaxProtocolStringLength);
        content.value = str;
        content.fromIndex = 0;
        content.toIndex = kMaxProtocolStringLength;
      } else {
        content.value = mirror.value();
      }
      content.length = mirror.length();
      break;

    case OBJECT_TYPE:
    case FUNCTION_TYPE:
    case ERROR_TYPE:
    case REGEXP_TYPE:
      // Add object representation.
      this.serializeObject_(mirror, content, details);
      break;

    case PROPERTY_TYPE:
      throw new Error('PropertyMirror cannot be serialized independeltly')
      break;

    case FRAME_TYPE:
      // Add object representation.
      this.serializeFrame_(mirror, content);
      break;

    case SCOPE_TYPE:
      // Add object representation.
      this.serializeScope_(mirror, content);
      break;

    case SCRIPT_TYPE:
      // Script is represented by id, name and source attributes.
      if (mirror.name()) {
        content.name = mirror.name();
      }
      content.id = mirror.id();
      content.lineOffset = mirror.lineOffset();
      content.columnOffset = mirror.columnOffset();
      content.lineCount = mirror.lineCount();
      if (mirror.data()) {
        content.data = mirror.data();
      }
      if (this.includeSource_()) {
        content.source = mirror.source();
      } else {
        var sourceStart = mirror.source().substring(0, 80);
        content.sourceStart = sourceStart;
      }
      content.sourceLength = mirror.source().length;
      content.scriptType = mirror.scriptType();
      content.compilationType = mirror.compilationType();
      // For compilation type eval emit information on the script from which
      // eval was called if a script is present.
      if (mirror.compilationType() == 1 &&
          mirror.evalFromScript()) {
        content.evalFromScript =
            this.serializeReference(mirror.evalFromScript());
        var evalFromLocation = mirror.evalFromLocation()
        if (evalFromLocation) {
          content.evalFromLocation = { line: evalFromLocation.line,
                                       column: evalFromLocation.column };
        }
        if (mirror.evalFromFunctionName()) {
          content.evalFromFunctionName = mirror.evalFromFunctionName();
        }
      }
      if (mirror.context()) {
        content.context = this.serializeReference(mirror.context());
      }
      break;

    case CONTEXT_TYPE:
      content.data = mirror.data();
      break;
  }

  // Always add the text representation.
  content.text = mirror.toText();
  
  // Create and return the JSON string.
  return content;
}


/**
 * Serialize object information to the following JSON format.
 *
 *   {"className":"<class name>",
 *    "constructorFunction":{"ref":<number>},
 *    "protoObject":{"ref":<number>},
 *    "prototypeObject":{"ref":<number>},
 *    "namedInterceptor":<boolean>,
 *    "indexedInterceptor":<boolean>,
 *    "properties":[<properties>]}
 */
JSONProtocolSerializer.prototype.serializeObject_ = function(mirror, content,
                                                             details) {
  // Add general object properties.
  content.className = mirror.className();
  content.constructorFunction =
      this.serializeReference(mirror.constructorFunction());
  content.protoObject = this.serializeReference(mirror.protoObject());
  content.prototypeObject = this.serializeReference(mirror.prototypeObject());

  // Add flags to indicate whether there are interceptors.
  if (mirror.hasNamedInterceptor()) {
    content.namedInterceptor = true;
  }
  if (mirror.hasIndexedInterceptor()) {
    content.indexedInterceptor = true;
  }
  
  // Add function specific properties.
  if (mirror.isFunction()) {
    // Add function specific properties.
    content.name = mirror.name();
    if (!IS_UNDEFINED(mirror.inferredName())) {
      content.inferredName = mirror.inferredName();
    }
    content.resolved = mirror.resolved();
    if (mirror.resolved()) {
      content.source = mirror.source();
    }
    if (mirror.script()) {
      content.script = this.serializeReference(mirror.script());
      content.scriptId = mirror.script().id();
      
      serializeLocationFields(mirror.sourceLocation(), content);
    }
  }

  // Add date specific properties.
  if (mirror.isDate()) {
    // Add date specific properties.
    content.value = mirror.value();
  }

  // Add actual properties - named properties followed by indexed properties.
  var propertyNames = mirror.propertyNames(PropertyKind.Named);
  var propertyIndexes = mirror.propertyNames(PropertyKind.Indexed);
  var p = new Array(propertyNames.length + propertyIndexes.length);
  for (var i = 0; i < propertyNames.length; i++) {
    var propertyMirror = mirror.property(propertyNames[i]);
    p[i] = this.serializeProperty_(propertyMirror);
    if (details) {
      this.add_(propertyMirror.value());
    }
  }
  for (var i = 0; i < propertyIndexes.length; i++) {
    var propertyMirror = mirror.property(propertyIndexes[i]);
    p[propertyNames.length + i] = this.serializeProperty_(propertyMirror);
    if (details) {
      this.add_(propertyMirror.value());
    }
  }
  content.properties = p;
}


/**
 * Serialize location information to the following JSON format:
 *
 *   "position":"<position>",
 *   "line":"<line>",
 *   "column":"<column>",
 * 
 * @param {SourceLocation} location The location to serialize, may be undefined.
 */
function serializeLocationFields (location, content) {
  if (!location) {
    return;
  }                                                                     
  content.position = location.position;
  var line = location.line;
  if (!IS_UNDEFINED(line)) {
    content.line = line;
  }
  var column = location.column;
  if (!IS_UNDEFINED(column)) {
    content.column = column;
  }
}


/**
 * Serialize property information to the following JSON format for building the
 * array of properties.
 *
 *   {"name":"<property name>",
 *    "attributes":<number>,
 *    "propertyType":<number>,
 *    "ref":<number>}
 *
 * If the attribute for the property is PropertyAttribute.None it is not added.
 * If the propertyType for the property is PropertyType.Normal it is not added.
 * Here are a couple of examples.
 *
 *   {"name":"hello","ref":1}
 *   {"name":"length","attributes":7,"propertyType":3,"ref":2}
 *
 * @param {PropertyMirror} propertyMirror The property to serialize.
 * @returns {Object} Protocol object representing the property.
 */
JSONProtocolSerializer.prototype.serializeProperty_ = function(propertyMirror) {
  var result = {};
  
  result.name = propertyMirror.name();
  var propertyValue = propertyMirror.value();
  if (this.inlineRefs_() && propertyValue.isValue()) {
    result.value = this.serializeReferenceWithDisplayData_(propertyValue);
  } else {
    if (propertyMirror.attributes() != PropertyAttribute.None) {
      result.attributes = propertyMirror.attributes();
    }
    if (propertyMirror.propertyType() != PropertyType.Normal) {
      result.propertyType = propertyMirror.propertyType();
    }
    result.ref = propertyValue.handle();
  }
  return result;
}


JSONProtocolSerializer.prototype.serializeFrame_ = function(mirror, content) {
  content.index = mirror.index();
  content.receiver = this.serializeReference(mirror.receiver());
  var func = mirror.func();
  content.func = this.serializeReference(func);
  if (func.script()) {
    content.script = this.serializeReference(func.script());
  }
  content.constructCall = mirror.isConstructCall();
  content.debuggerFrame = mirror.isDebuggerFrame();
  var x = new Array(mirror.argumentCount());
  for (var i = 0; i < mirror.argumentCount(); i++) {
    var arg = {};
    var argument_name = mirror.argumentName(i)
    if (argument_name) {
      arg.name = argument_name;
    }
    arg.value = this.serializeReference(mirror.argumentValue(i));
    x[i] = arg;
  }
  content.arguments = x;
  var x = new Array(mirror.localCount());
  for (var i = 0; i < mirror.localCount(); i++) {
    var local = {};
    local.name = mirror.localName(i);
    local.value = this.serializeReference(mirror.localValue(i));
    x[i] = local;
  }
  content.locals = x;
  serializeLocationFields(mirror.sourceLocation(), content);
  var source_line_text = mirror.sourceLineText();
  if (!IS_UNDEFINED(source_line_text)) {
    content.sourceLineText = source_line_text;
  }
  
  content.scopes = [];
  for (var i = 0; i < mirror.scopeCount(); i++) {
    var scope = mirror.scope(i);
    content.scopes.push({
      type: scope.scopeType(),
      index: i
    });
  }
}


JSONProtocolSerializer.prototype.serializeScope_ = function(mirror, content) {
  content.index = mirror.scopeIndex();
  content.frameIndex = mirror.frameIndex();
  content.type = mirror.scopeType();
  content.object = this.inlineRefs_() ?
                   this.serializeValue(mirror.scopeObject()) :
                   this.serializeReference(mirror.scopeObject());
}


/**
 * Convert a number to a protocol value. For all finite numbers the number
 * itself is returned. For non finite numbers NaN, Infinite and
 * -Infinite the string representation "NaN", "Infinite" or "-Infinite"
 * (not including the quotes) is returned.
 *
 * @param {number} value The number value to convert to a protocol value.
 * @returns {number|string} Protocol value.
 */
function NumberToJSON_(value) {
  if (isNaN(value)) {
    return 'NaN';
  }
  if (!isFinite(value)) {
    if (value > 0) {
      return 'Infinity';
    } else {
      return '-Infinity';
    }
  }
  return value; 
}
