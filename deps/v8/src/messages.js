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


// -------------------------------------------------------------------
//
// Matches Script::Type from objects.h
var TYPE_NATIVE = 0;
var TYPE_EXTENSION = 1;
var TYPE_NORMAL = 2;

// Matches Script::CompilationType from objects.h
var COMPILATION_TYPE_HOST = 0;
var COMPILATION_TYPE_EVAL = 1;
var COMPILATION_TYPE_JSON = 2;

// Lazily initialized.
var kVowelSounds = 0;
var kCapitalVowelSounds = 0;

// Matches Messages::kNoLineNumberInfo from v8.h
var kNoLineNumberInfo = 0;

// If this object gets passed to an error constructor the error will
// get an accessor for .message that constructs a descriptive error
// message on access.
var kAddMessageAccessorsMarker = { };


function GetInstanceName(cons) {
  if (cons.length == 0) {
    return "";
  }
  var first = %StringToLowerCase(StringCharAt.call(cons, 0));
  if (kVowelSounds === 0) {
    kVowelSounds = {a: true, e: true, i: true, o: true, u: true, y: true};
    kCapitalVowelSounds = {a: true, e: true, i: true, o: true, u: true, h: true,
        f: true, l: true, m: true, n: true, r: true, s: true, x: true, y: true};
  }
  var vowel_mapping = kVowelSounds;
  if (cons.length > 1 && (StringCharAt.call(cons, 0) != first)) {
    // First char is upper case
    var second = %StringToLowerCase(StringCharAt.call(cons, 1));
    // Second char is upper case
    if (StringCharAt.call(cons, 1) != second) {
      vowel_mapping = kCapitalVowelSounds;
    }
  }
  var s = vowel_mapping[first] ? "an " : "a ";
  return s + cons;
}


var kMessages = 0;


function FormatString(format, args) {
  var result = format;
  for (var i = 0; i < args.length; i++) {
    var str;
    try { str = ToDetailString(args[i]); }
    catch (e) { str = "#<error>"; }
    result = ArrayJoin.call(StringSplit.call(result, "%" + i), str);
  }
  return result;
}


function ToDetailString(obj) {
  if (obj != null && IS_OBJECT(obj) && obj.toString === $Object.prototype.toString) {
    var constructor = obj.constructor;
    if (!constructor) return ToString(obj);
    var constructorName = constructor.name;
    if (!constructorName) return ToString(obj);
    return "#<" + GetInstanceName(constructorName) + ">";
  } else if (obj instanceof $Error) {
    // When formatting internally created error messages, do not
    // invoke overwritten error toString methods but explicitly use
    // the error to string method. This is to avoid leaking error
    // objects between script tags in a browser setting.
    return %_CallFunction(obj, errorToString);
  } else {
    return ToString(obj);
  }
}


function MakeGenericError(constructor, type, args) {
  if (IS_UNDEFINED(args)) {
    args = [];
  }
  var e = new constructor(kAddMessageAccessorsMarker);
  e.type = type;
  e.arguments = args;
  return e;
}


/**
 * Setup the Script function and constructor.
 */
%FunctionSetInstanceClassName(Script, 'Script');
%SetProperty(Script.prototype, 'constructor', Script, DONT_ENUM);
%SetCode(Script, function(x) {
  // Script objects can only be created by the VM.
  throw new $Error("Not supported");
});


// Helper functions; called from the runtime system.
function FormatMessage(message) {
  if (kMessages === 0) {
    kMessages = {
      // Error
      cyclic_proto:                 "Cyclic __proto__ value",
      // TypeError
      unexpected_token:             "Unexpected token %0",
      unexpected_token_number:      "Unexpected number",
      unexpected_token_string:      "Unexpected string",
      unexpected_token_identifier:  "Unexpected identifier",
      unexpected_eos:               "Unexpected end of input",
      malformed_regexp:             "Invalid regular expression: /%0/: %1",
      unterminated_regexp:          "Invalid regular expression: missing /",
      regexp_flags:                 "Cannot supply flags when constructing one RegExp from another",
      incompatible_method_receiver: "Method %0 called on incompatible receiver %1",
      invalid_lhs_in_assignment:    "Invalid left-hand side in assignment",
      invalid_lhs_in_for_in:        "Invalid left-hand side in for-in",
      invalid_lhs_in_postfix_op:    "Invalid left-hand side expression in postfix operation",
      invalid_lhs_in_prefix_op:     "Invalid left-hand side expression in prefix operation",
      multiple_defaults_in_switch:  "More than one default clause in switch statement",
      newline_after_throw:          "Illegal newline after throw",
      redeclaration:                "%0 '%1' has already been declared",
      no_catch_or_finally:          "Missing catch or finally after try",
      unknown_label:                "Undefined label '%0'",
      uncaught_exception:           "Uncaught %0",
      stack_trace:                  "Stack Trace:\n%0",
      called_non_callable:          "%0 is not a function",
      undefined_method:             "Object %1 has no method '%0'",
      property_not_function:        "Property '%0' of object %1 is not a function",
      cannot_convert_to_primitive:  "Cannot convert object to primitive value",
      not_constructor:              "%0 is not a constructor",
      not_defined:                  "%0 is not defined",
      non_object_property_load:     "Cannot read property '%0' of %1",
      non_object_property_store:    "Cannot set property '%0' of %1",
      non_object_property_call:     "Cannot call method '%0' of %1",
      with_expression:              "%0 has no properties",
      illegal_invocation:           "Illegal invocation",
      no_setter_in_callback:        "Cannot set property %0 of %1 which has only a getter",
      apply_non_function:           "Function.prototype.apply was called on %0, which is a %1 and not a function",
      apply_wrong_args:             "Function.prototype.apply: Arguments list has wrong type",
      invalid_in_operator_use:      "Cannot use 'in' operator to search for '%0' in %1",
      instanceof_function_expected: "Expecting a function in instanceof check, but got %0",
      instanceof_nonobject_proto:   "Function has non-object prototype '%0' in instanceof check",
      null_to_object:               "Cannot convert null to object",
      reduce_no_initial:            "Reduce of empty array with no initial value",
      getter_must_be_callable:      "Getter must be a function: %0",
      setter_must_be_callable:      "Setter must be a function: %0",
      value_and_accessor:           "Invalid property.  A property cannot both have accessors and be writable or have a value: %0",
      proto_object_or_null:         "Object prototype may only be an Object or null",
      property_desc_object:         "Property description must be an object: %0",
      redefine_disallowed:          "Cannot redefine property: %0",
      define_disallowed:            "Cannot define property, object is not extensible: %0",
      // RangeError
      invalid_array_length:         "Invalid array length",
      stack_overflow:               "Maximum call stack size exceeded",
      // SyntaxError
      unable_to_parse:              "Parse error",
      duplicate_regexp_flag:        "Duplicate RegExp flag %0",
      invalid_regexp:               "Invalid RegExp pattern /%0/",
      illegal_break:                "Illegal break statement",
      illegal_continue:             "Illegal continue statement",
      illegal_return:               "Illegal return statement",
      error_loading_debugger:       "Error loading debugger",
      no_input_to_regexp:           "No input to %0",
      invalid_json:                 "String '%0' is not valid JSON",
      circular_structure:           "Converting circular structure to JSON",
      obj_ctor_property_non_object: "Object.%0 called on non-object",
      array_indexof_not_defined:    "Array.getIndexOf: Argument undefined",
      object_not_extensible:        "Can't add property %0, object is not extensible",
      illegal_access:               "Illegal access",
      invalid_preparser_data:       "Invalid preparser data for function %0"
    };
  }
  var format = kMessages[message.type];
  if (!format) return "<unknown message " + message.type + ">";
  return FormatString(format, message.args);
}


function GetLineNumber(message) {
  if (message.startPos == -1) return kNoLineNumberInfo;
  var location = message.script.locationFromPosition(message.startPos, true);
  if (location == null) return kNoLineNumberInfo;
  return location.line + 1;
}


// Returns the source code line containing the given source
// position, or the empty string if the position is invalid.
function GetSourceLine(message) {
  var location = message.script.locationFromPosition(message.startPos, true);
  if (location == null) return "";
  location.restrict();
  return location.sourceText();
}


function MakeTypeError(type, args) {
  return MakeGenericError($TypeError, type, args);
}


function MakeRangeError(type, args) {
  return MakeGenericError($RangeError, type, args);
}


function MakeSyntaxError(type, args) {
  return MakeGenericError($SyntaxError, type, args);
}


function MakeReferenceError(type, args) {
  return MakeGenericError($ReferenceError, type, args);
}


function MakeEvalError(type, args) {
  return MakeGenericError($EvalError, type, args);
}


function MakeError(type, args) {
  return MakeGenericError($Error, type, args);
}

/**
 * Find a line number given a specific source position.
 * @param {number} position The source position.
 * @return {number} 0 if input too small, -1 if input too large,
       else the line number.
 */
Script.prototype.lineFromPosition = function(position) {
  var lower = 0;
  var upper = this.lineCount() - 1;
  var line_ends = this.line_ends;

  // We'll never find invalid positions so bail right away.
  if (position > line_ends[upper]) {
    return -1;
  }

  // This means we don't have to safe-guard indexing line_ends[i - 1].
  if (position <= line_ends[0]) {
    return 0;
  }

  // Binary search to find line # from position range.
  while (upper >= 1) {
    var i = (lower + upper) >> 1;

    if (position > line_ends[i]) {
      lower = i + 1;
    } else if (position <= line_ends[i - 1]) {
      upper = i - 1;
    } else {
      return i;
    }
  }
  return -1;
}

/**
 * Get information on a specific source position.
 * @param {number} position The source position
 * @param {boolean} include_resource_offset Set to true to have the resource
 *     offset added to the location
 * @return {SourceLocation}
 *     If line is negative or not in the source null is returned.
 */
Script.prototype.locationFromPosition = function (position,
                                                  include_resource_offset) {
  var line = this.lineFromPosition(position);
  if (line == -1) return null;

  // Determine start, end and column.
  var line_ends = this.line_ends;
  var start = line == 0 ? 0 : line_ends[line - 1] + 1;
  var end = line_ends[line];
  if (end > 0 && StringCharAt.call(this.source, end - 1) == '\r') end--;
  var column = position - start;

  // Adjust according to the offset within the resource.
  if (include_resource_offset) {
    line += this.line_offset;
    if (line == this.line_offset) {
      column += this.column_offset;
    }
  }

  return new SourceLocation(this, position, line, column, start, end);
};


/**
 * Get information on a specific source line and column possibly offset by a
 * fixed source position. This function is used to find a source position from
 * a line and column position. The fixed source position offset is typically
 * used to find a source position in a function based on a line and column in
 * the source for the function alone. The offset passed will then be the
 * start position of the source for the function within the full script source.
 * @param {number} opt_line The line within the source. Default value is 0
 * @param {number} opt_column The column in within the line. Default value is 0
 * @param {number} opt_offset_position The offset from the begining of the
 *     source from where the line and column calculation starts. Default value is 0
 * @return {SourceLocation}
 *     If line is negative or not in the source null is returned.
 */
Script.prototype.locationFromLine = function (opt_line, opt_column, opt_offset_position) {
  // Default is the first line in the script. Lines in the script is relative
  // to the offset within the resource.
  var line = 0;
  if (!IS_UNDEFINED(opt_line)) {
    line = opt_line - this.line_offset;
  }

  // Default is first column. If on the first line add the offset within the
  // resource.
  var column = opt_column || 0;
  if (line == 0) {
    column -= this.column_offset
  }

  var offset_position = opt_offset_position || 0;
  if (line < 0 || column < 0 || offset_position < 0) return null;
  if (line == 0) {
    return this.locationFromPosition(offset_position + column, false);
  } else {
    // Find the line where the offset position is located.
    var offset_line = this.lineFromPosition(offset_position);

    if (offset_line == -1 || offset_line + line >= this.lineCount()) {
      return null;
    }

    return this.locationFromPosition(this.line_ends[offset_line + line - 1] + 1 + column);  // line > 0 here.
  }
}


/**
 * Get a slice of source code from the script. The boundaries for the slice is
 * specified in lines.
 * @param {number} opt_from_line The first line (zero bound) in the slice.
 *     Default is 0
 * @param {number} opt_to_column The last line (zero bound) in the slice (non
 *     inclusive). Default is the number of lines in the script
 * @return {SourceSlice} The source slice or null of the parameters where
 *     invalid
 */
Script.prototype.sourceSlice = function (opt_from_line, opt_to_line) {
  var from_line = IS_UNDEFINED(opt_from_line) ? this.line_offset : opt_from_line;
  var to_line = IS_UNDEFINED(opt_to_line) ? this.line_offset + this.lineCount() : opt_to_line

  // Adjust according to the offset within the resource.
  from_line -= this.line_offset;
  to_line -= this.line_offset;
  if (from_line < 0) from_line = 0;
  if (to_line > this.lineCount()) to_line = this.lineCount();

  // Check parameters.
  if (from_line >= this.lineCount() ||
      to_line < 0 ||
      from_line > to_line) {
    return null;
  }

  var line_ends = this.line_ends;
  var from_position = from_line == 0 ? 0 : line_ends[from_line - 1] + 1;
  var to_position = to_line == 0 ? 0 : line_ends[to_line - 1] + 1;

  // Return a source slice with line numbers re-adjusted to the resource.
  return new SourceSlice(this, from_line + this.line_offset, to_line + this.line_offset,
                         from_position, to_position);
}


Script.prototype.sourceLine = function (opt_line) {
  // Default is the first line in the script. Lines in the script are relative
  // to the offset within the resource.
  var line = 0;
  if (!IS_UNDEFINED(opt_line)) {
    line = opt_line - this.line_offset;
  }

  // Check parameter.
  if (line < 0 || this.lineCount() <= line) {
    return null;
  }

  // Return the source line.
  var line_ends = this.line_ends;
  var start = line == 0 ? 0 : line_ends[line - 1] + 1;
  var end = line_ends[line];
  return StringSubstring.call(this.source, start, end);
}


/**
 * Returns the number of source lines.
 * @return {number}
 *     Number of source lines.
 */
Script.prototype.lineCount = function() {
  // Return number of source lines.
  return this.line_ends.length;
};


/**
 * Returns the name of script if available, contents of sourceURL comment
 * otherwise. See
 * http://fbug.googlecode.com/svn/branches/firebug1.1/docs/ReleaseNotes_1.1.txt
 * for details on using //@ sourceURL comment to identify scritps that don't
 * have name.
 *
 * @return {?string} script name if present, value for //@ sourceURL comment
 * otherwise.
 */
Script.prototype.nameOrSourceURL = function() {
  if (this.name)
    return this.name;
  // TODO(608): the spaces in a regexp below had to be escaped as \040
  // because this file is being processed by js2c whose handling of spaces
  // in regexps is broken. Also, ['"] are excluded from allowed URLs to
  // avoid matches against sources that invoke evals with sourceURL.
  var sourceUrlPattern =
    /\/\/@[\040\t]sourceURL=[\040\t]*([^\s'"]*)[\040\t]*$/m;
  var match = sourceUrlPattern.exec(this.source);
  return match ? match[1] : this.name;
}


/**
 * Class for source location. A source location is a position within some
 * source with the following properties:
 *   script   : script object for the source
 *   line     : source line number
 *   column   : source column within the line
 *   position : position within the source
 *   start    : position of start of source context (inclusive)
 *   end      : position of end of source context (not inclusive)
 * Source text for the source context is the character interval [start, end[. In
 * most cases end will point to a newline character. It might point just past
 * the final position of the source if the last source line does not end with a
 * newline character.
 * @param {Script} script The Script object for which this is a location
 * @param {number} position Source position for the location
 * @param {number} line The line number for the location
 * @param {number} column The column within the line for the location
 * @param {number} start Source position for start of source context
 * @param {number} end Source position for end of source context
 * @constructor
 */
function SourceLocation(script, position, line, column, start, end) {
  this.script = script;
  this.position = position;
  this.line = line;
  this.column = column;
  this.start = start;
  this.end = end;
}


const kLineLengthLimit = 78;

/**
 * Restrict source location start and end positions to make the source slice
 * no more that a certain number of characters wide.
 * @param {number} opt_limit The with limit of the source text with a default
 *     of 78
 * @param {number} opt_before The number of characters to prefer before the
 *     position with a default value of 10 less that the limit
 */
SourceLocation.prototype.restrict = function (opt_limit, opt_before) {
  // Find the actual limit to use.
  var limit;
  var before;
  if (!IS_UNDEFINED(opt_limit)) {
    limit = opt_limit;
  } else {
    limit = kLineLengthLimit;
  }
  if (!IS_UNDEFINED(opt_before)) {
    before = opt_before;
  } else {
    // If no before is specified center for small limits and perfer more source
    // before the the position that after for longer limits.
    if (limit <= 20) {
      before = $floor(limit / 2);
    } else {
      before = limit - 10;
    }
  }
  if (before >= limit) {
    before = limit - 1;
  }

  // If the [start, end[ interval is too big we restrict
  // it in one or both ends. We make sure to always produce
  // restricted intervals of maximum allowed size.
  if (this.end - this.start > limit) {
    var start_limit = this.position - before;
    var end_limit = this.position + limit - before;
    if (this.start < start_limit && end_limit < this.end) {
      this.start = start_limit;
      this.end = end_limit;
    } else if (this.start < start_limit) {
      this.start = this.end - limit;
    } else {
      this.end = this.start + limit;
    }
  }
};


/**
 * Get the source text for a SourceLocation
 * @return {String}
 *     Source text for this location.
 */
SourceLocation.prototype.sourceText = function () {
  return StringSubstring.call(this.script.source, this.start, this.end);
};


/**
 * Class for a source slice. A source slice is a part of a script source with
 * the following properties:
 *   script        : script object for the source
 *   from_line     : line number for the first line in the slice
 *   to_line       : source line number for the last line in the slice
 *   from_position : position of the first character in the slice
 *   to_position   : position of the last character in the slice
 * The to_line and to_position are not included in the slice, that is the lines
 * in the slice are [from_line, to_line[. Likewise the characters in the slice
 * are [from_position, to_position[.
 * @param {Script} script The Script object for the source slice
 * @param {number} from_line
 * @param {number} to_line
 * @param {number} from_position
 * @param {number} to_position
 * @constructor
 */
function SourceSlice(script, from_line, to_line, from_position, to_position) {
  this.script = script;
  this.from_line = from_line;
  this.to_line = to_line;
  this.from_position = from_position;
  this.to_position = to_position;
}


/**
 * Get the source text for a SourceSlice
 * @return {String} Source text for this slice. The last line will include
 *     the line terminating characters (if any)
 */
SourceSlice.prototype.sourceText = function () {
  return StringSubstring.call(this.script.source, this.from_position, this.to_position);
};


// Returns the offset of the given position within the containing
// line.
function GetPositionInLine(message) {
  var location = message.script.locationFromPosition(message.startPos, false);
  if (location == null) return -1;
  location.restrict();
  return message.startPos - location.start;
}


function ErrorMessage(type, args, startPos, endPos, script, stackTrace,
                      stackFrames) {
  this.startPos = startPos;
  this.endPos = endPos;
  this.type = type;
  this.args = args;
  this.script = script;
  this.stackTrace = stackTrace;
  this.stackFrames = stackFrames;
}


function MakeMessage(type, args, startPos, endPos, script, stackTrace,
                     stackFrames) {
  return new ErrorMessage(type, args, startPos, endPos, script, stackTrace,
                          stackFrames);
}


function GetStackTraceLine(recv, fun, pos, isGlobal) {
  return FormatSourcePosition(new CallSite(recv, fun, pos));
}

// ----------------------------------------------------------------------------
// Error implementation

// Defines accessors for a property that is calculated the first time
// the property is read.
function DefineOneShotAccessor(obj, name, fun) {
  // Note that the accessors consistently operate on 'obj', not 'this'.
  // Since the object may occur in someone else's prototype chain we
  // can't rely on 'this' being the same as 'obj'.
  var hasBeenSet = false;
  var value;
  obj.__defineGetter__(name, function () {
    if (hasBeenSet) {
      return value;
    }
    hasBeenSet = true;
    value = fun(obj);
    return value;
  });
  obj.__defineSetter__(name, function (v) {
    hasBeenSet = true;
    value = v;
  });
}

function CallSite(receiver, fun, pos) {
  this.receiver = receiver;
  this.fun = fun;
  this.pos = pos;
}

CallSite.prototype.getThis = function () {
  return this.receiver;
};

CallSite.prototype.getTypeName = function () {
  var constructor = this.receiver.constructor;
  if (!constructor)
    return $Object.prototype.toString.call(this.receiver);
  var constructorName = constructor.name;
  if (!constructorName)
    return $Object.prototype.toString.call(this.receiver);
  return constructorName;
};

CallSite.prototype.isToplevel = function () {
  if (this.receiver == null)
    return true;
  return IS_GLOBAL(this.receiver);
};

CallSite.prototype.isEval = function () {
  var script = %FunctionGetScript(this.fun);
  return script && script.compilation_type == COMPILATION_TYPE_EVAL;
};

CallSite.prototype.getEvalOrigin = function () {
  var script = %FunctionGetScript(this.fun);
  return FormatEvalOrigin(script);
};

CallSite.prototype.getScriptNameOrSourceURL = function () {
  var script = %FunctionGetScript(this.fun);
  return script ? script.nameOrSourceURL() : null;
};

CallSite.prototype.getFunction = function () {
  return this.fun;
};

CallSite.prototype.getFunctionName = function () {
  // See if the function knows its own name
  var name = this.fun.name;
  if (name) {
    return name;
  } else {
    return %FunctionGetInferredName(this.fun);
  }
  // Maybe this is an evaluation?
  var script = %FunctionGetScript(this.fun);
  if (script && script.compilation_type == COMPILATION_TYPE_EVAL)
    return "eval";
  return null;
};

CallSite.prototype.getMethodName = function () {
  // See if we can find a unique property on the receiver that holds
  // this function.
  var ownName = this.fun.name;
  if (ownName && this.receiver &&
      (ObjectLookupGetter.call(this.receiver, ownName) === this.fun ||
       ObjectLookupSetter.call(this.receiver, ownName) === this.fun ||
       this.receiver[ownName] === this.fun)) {
    // To handle DontEnum properties we guess that the method has
    // the same name as the function.
    return ownName;
  }
  var name = null;
  for (var prop in this.receiver) {
    if (this.receiver.__lookupGetter__(prop) === this.fun ||
        this.receiver.__lookupSetter__(prop) === this.fun ||
        (!this.receiver.__lookupGetter__(prop) && this.receiver[prop] === this.fun)) {
      // If we find more than one match bail out to avoid confusion.
      if (name)
        return null;
      name = prop;
    }
  }
  if (name)
    return name;
  return null;
};

CallSite.prototype.getFileName = function () {
  var script = %FunctionGetScript(this.fun);
  return script ? script.name : null;
};

CallSite.prototype.getLineNumber = function () {
  if (this.pos == -1)
    return null;
  var script = %FunctionGetScript(this.fun);
  var location = null;
  if (script) {
    location = script.locationFromPosition(this.pos, true);
  }
  return location ? location.line + 1 : null;
};

CallSite.prototype.getColumnNumber = function () {
  if (this.pos == -1)
    return null;
  var script = %FunctionGetScript(this.fun);
  var location = null;
  if (script) {
    location = script.locationFromPosition(this.pos, true);
  }
  return location ? location.column + 1: null;
};

CallSite.prototype.isNative = function () {
  var script = %FunctionGetScript(this.fun);
  return script ? (script.type == TYPE_NATIVE) : false;
};

CallSite.prototype.getPosition = function () {
  return this.pos;
};

CallSite.prototype.isConstructor = function () {
  var constructor = this.receiver ? this.receiver.constructor : null;
  if (!constructor)
    return false;
  return this.fun === constructor;
};

function FormatEvalOrigin(script) {
  var sourceURL = script.nameOrSourceURL();
  if (sourceURL)
    return sourceURL;

  var eval_origin = "eval at ";
  if (script.eval_from_function_name) {
    eval_origin += script.eval_from_function_name;
  } else {
    eval_origin +=  "<anonymous>";
  }

  var eval_from_script = script.eval_from_script;
  if (eval_from_script) {
    if (eval_from_script.compilation_type == COMPILATION_TYPE_EVAL) {
      // eval script originated from another eval.
      eval_origin += " (" + FormatEvalOrigin(eval_from_script) + ")";
    } else {
      // eval script originated from "real" source.
      if (eval_from_script.name) {
        eval_origin += " (" + eval_from_script.name;
        var location = eval_from_script.locationFromPosition(script.eval_from_script_position, true);
        if (location) {
          eval_origin += ":" + (location.line + 1);
          eval_origin += ":" + (location.column + 1);
        }
        eval_origin += ")"
      } else {
        eval_origin += " (unknown source)";
      }
    }
  }

  return eval_origin;
};

function FormatSourcePosition(frame) {
  var fileName;
  var fileLocation = "";
  if (frame.isNative()) {
    fileLocation = "native";
  } else if (frame.isEval()) {
    fileName = frame.getScriptNameOrSourceURL();
    if (!fileName)
      fileLocation = frame.getEvalOrigin();
  } else {
    fileName = frame.getFileName();
  }

  if (fileName) {
    fileLocation += fileName;
    var lineNumber = frame.getLineNumber();
    if (lineNumber != null) {
      fileLocation += ":" + lineNumber;
      var columnNumber = frame.getColumnNumber();
      if (columnNumber) {
        fileLocation += ":" + columnNumber;
      }
    }
  }

  if (!fileLocation) {
    fileLocation = "unknown source";
  }
  var line = "";
  var functionName = frame.getFunction().name;
  var addPrefix = true;
  var isConstructor = frame.isConstructor();
  var isMethodCall = !(frame.isToplevel() || isConstructor);
  if (isMethodCall) {
    var methodName = frame.getMethodName();
    line += frame.getTypeName() + ".";
    if (functionName) {
      line += functionName;
      if (methodName && (methodName != functionName)) {
        line += " [as " + methodName + "]";
      }
    } else {
      line += methodName || "<anonymous>";
    }
  } else if (isConstructor) {
    line += "new " + (functionName || "<anonymous>");
  } else if (functionName) {
    line += functionName;
  } else {
    line += fileLocation;
    addPrefix = false;
  }
  if (addPrefix) {
    line += " (" + fileLocation + ")";
  }
  return line;
}

function FormatStackTrace(error, frames) {
  var lines = [];
  try {
    lines.push(error.toString());
  } catch (e) {
    try {
      lines.push("<error: " + e + ">");
    } catch (ee) {
      lines.push("<error>");
    }
  }
  for (var i = 0; i < frames.length; i++) {
    var frame = frames[i];
    var line;
    try {
      line = FormatSourcePosition(frame);
    } catch (e) {
      try {
        line = "<error: " + e + ">";
      } catch (ee) {
        // Any code that reaches this point is seriously nasty!
        line = "<error>";
      }
    }
    lines.push("    at " + line);
  }
  return lines.join("\n");
}

function FormatRawStackTrace(error, raw_stack) {
  var frames = [ ];
  for (var i = 0; i < raw_stack.length; i += 4) {
    var recv = raw_stack[i];
    var fun = raw_stack[i + 1];
    var code = raw_stack[i + 2];
    var pc = raw_stack[i + 3];
    var pos = %FunctionGetPositionForOffset(code, pc);
    frames.push(new CallSite(recv, fun, pos));
  }
  if (IS_FUNCTION($Error.prepareStackTrace)) {
    return $Error.prepareStackTrace(error, frames);
  } else {
    return FormatStackTrace(error, frames);
  }
}

function DefineError(f) {
  // Store the error function in both the global object
  // and the runtime object. The function is fetched
  // from the runtime object when throwing errors from
  // within the runtime system to avoid strange side
  // effects when overwriting the error functions from
  // user code.
  var name = f.name;
  %SetProperty(global, name, f, DONT_ENUM);
  this['$' + name] = f;
  // Configure the error function.
  if (name == 'Error') {
    // The prototype of the Error object must itself be an error.
    // However, it can't be an instance of the Error object because
    // it hasn't been properly configured yet.  Instead we create a
    // special not-a-true-error-but-close-enough object.
    function ErrorPrototype() {}
    %FunctionSetPrototype(ErrorPrototype, $Object.prototype);
    %FunctionSetInstanceClassName(ErrorPrototype, 'Error');
    %FunctionSetPrototype(f, new ErrorPrototype());
  } else {
    %FunctionSetPrototype(f, new $Error());
  }
  %FunctionSetInstanceClassName(f, 'Error');
  %SetProperty(f.prototype, 'constructor', f, DONT_ENUM);
  // The name property on the prototype of error objects is not
  // specified as being read-one and dont-delete. However, allowing
  // overwriting allows leaks of error objects between script blocks
  // in the same context in a browser setting. Therefore we fix the
  // name.
  %SetProperty(f.prototype, "name", name, READ_ONLY | DONT_DELETE);
  %SetCode(f, function(m) {
    if (%_IsConstructCall()) {
      // Define all the expected properties directly on the error
      // object. This avoids going through getters and setters defined
      // on prototype objects.
      %IgnoreAttributesAndSetProperty(this, 'stack', void 0);
      %IgnoreAttributesAndSetProperty(this, 'arguments', void 0);
      %IgnoreAttributesAndSetProperty(this, 'type', void 0);
      if (m === kAddMessageAccessorsMarker) {
        // DefineOneShotAccessor always inserts a message property and
        // ignores setters.
        DefineOneShotAccessor(this, 'message', function (obj) {
          return FormatMessage({type: obj.type, args: obj.arguments});
        });
      } else if (!IS_UNDEFINED(m)) {
        %IgnoreAttributesAndSetProperty(this, 'message', ToString(m));
      }
      captureStackTrace(this, f);
    } else {
      return new f(m);
    }
  });
}

function captureStackTrace(obj, cons_opt) {
  var stackTraceLimit = $Error.stackTraceLimit;
  if (!stackTraceLimit) return;
  if (stackTraceLimit < 0 || stackTraceLimit > 10000)
    stackTraceLimit = 10000;
  var raw_stack = %CollectStackTrace(cons_opt ? cons_opt : captureStackTrace,
      stackTraceLimit);
  DefineOneShotAccessor(obj, 'stack', function (obj) {
    return FormatRawStackTrace(obj, raw_stack);
  });
};

$Math.__proto__ = global.Object.prototype;

DefineError(function Error() { });
DefineError(function TypeError() { });
DefineError(function RangeError() { });
DefineError(function SyntaxError() { });
DefineError(function ReferenceError() { });
DefineError(function EvalError() { });
DefineError(function URIError() { });

$Error.captureStackTrace = captureStackTrace;

// Setup extra properties of the Error.prototype object.
$Error.prototype.message = '';

function errorToString() {
  var type = this.type;
  if (type && !this.hasOwnProperty("message")) {
    return this.name + ": " + FormatMessage({ type: type, args: this.arguments });
  }
  var message = this.hasOwnProperty("message") ? (": " + this.message) : "";
  return this.name + message;
}

%FunctionSetName(errorToString, 'toString');
%SetProperty($Error.prototype, 'toString', errorToString, DONT_ENUM);


// Boilerplate for exceptions for stack overflows. Used from
// Top::StackOverflow().
const kStackOverflowBoilerplate = MakeRangeError('stack_overflow', []);
