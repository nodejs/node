// Copyright 2011 the V8 project authors. All rights reserved.
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
// If this object gets passed to an error constructor the error will
// get an accessor for .message that constructs a descriptive error
// message on access.
const kAddMessageAccessorsMarker = { };

// This will be lazily initialized when first needed (and forcibly
// overwritten even though it's const).
const kMessages = 0;

function FormatString(format, message) {
  var args = %MessageGetArguments(message);
  var result = "";
  var arg_num = 0;
  for (var i = 0; i < format.length; i++) {
    var str = format[i];
    if (str.length == 2 && %_StringCharCodeAt(str, 0) == 0x25) {
      // Two-char string starts with "%".
      var arg_num = (%_StringCharCodeAt(str, 1) - 0x30) >>> 0;
      if (arg_num < 4) {
        // str is one of %0, %1, %2 or %3.
        try {
          str = ToDetailString(args[arg_num]);
        } catch (e) {
          str = "#<error>";
        }
      }
    }
    result += str;
  }
  return result;
}


// To check if something is a native error we need to check the
// concrete native error types. It is not enough to check "obj
// instanceof $Error" because user code can replace
// NativeError.prototype.__proto__. User code cannot replace
// NativeError.prototype though and therefore this is a safe test.
function IsNativeErrorObject(obj) {
  return (obj instanceof $Error) ||
      (obj instanceof $EvalError) ||
      (obj instanceof $RangeError) ||
      (obj instanceof $ReferenceError) ||
      (obj instanceof $SyntaxError) ||
      (obj instanceof $TypeError) ||
      (obj instanceof $URIError);
}


// When formatting internally created error messages, do not
// invoke overwritten error toString methods but explicitly use
// the error to string method. This is to avoid leaking error
// objects between script tags in a browser setting.
function ToStringCheckErrorObject(obj) {
  if (IsNativeErrorObject(obj)) {
    return %_CallFunction(obj, errorToString);
  } else {
    return ToString(obj);
  }
}


function ToDetailString(obj) {
  if (obj != null && IS_OBJECT(obj) && obj.toString === ObjectToString) {
    var constructor = obj.constructor;
    if (typeof constructor == "function") {
      var constructorName = constructor.name;
      if (IS_STRING(constructorName) && constructorName !== "") {
        return "#<" + constructorName + ">";
      }
    }
  }
  return ToStringCheckErrorObject(obj);
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
 * Set up the Script function and constructor.
 */
%FunctionSetInstanceClassName(Script, 'Script');
%SetProperty(Script.prototype, 'constructor', Script,
             DONT_ENUM | DONT_DELETE | READ_ONLY);
%SetCode(Script, function(x) {
  // Script objects can only be created by the VM.
  throw new $Error("Not supported");
});


// Helper functions; called from the runtime system.
function FormatMessage(message) {
  if (kMessages === 0) {
    var messagesDictionary = [
      // Error
      "cyclic_proto",                 ["Cyclic __proto__ value"],
      "code_gen_from_strings",        ["Code generation from strings disallowed for this context"],
      // TypeError
      "unexpected_token",             ["Unexpected token ", "%0"],
      "unexpected_token_number",      ["Unexpected number"],
      "unexpected_token_string",      ["Unexpected string"],
      "unexpected_token_identifier",  ["Unexpected identifier"],
      "unexpected_reserved",          ["Unexpected reserved word"],
      "unexpected_strict_reserved",   ["Unexpected strict mode reserved word"],
      "unexpected_eos",               ["Unexpected end of input"],
      "malformed_regexp",             ["Invalid regular expression: /", "%0", "/: ", "%1"],
      "unterminated_regexp",          ["Invalid regular expression: missing /"],
      "regexp_flags",                 ["Cannot supply flags when constructing one RegExp from another"],
      "incompatible_method_receiver", ["Method ", "%0", " called on incompatible receiver ", "%1"],
      "invalid_lhs_in_assignment",    ["Invalid left-hand side in assignment"],
      "invalid_lhs_in_for_in",        ["Invalid left-hand side in for-in"],
      "invalid_lhs_in_postfix_op",    ["Invalid left-hand side expression in postfix operation"],
      "invalid_lhs_in_prefix_op",     ["Invalid left-hand side expression in prefix operation"],
      "multiple_defaults_in_switch",  ["More than one default clause in switch statement"],
      "newline_after_throw",          ["Illegal newline after throw"],
      "redeclaration",                ["%0", " '", "%1", "' has already been declared"],
      "no_catch_or_finally",          ["Missing catch or finally after try"],
      "unknown_label",                ["Undefined label '", "%0", "'"],
      "uncaught_exception",           ["Uncaught ", "%0"],
      "stack_trace",                  ["Stack Trace:\n", "%0"],
      "called_non_callable",          ["%0", " is not a function"],
      "undefined_method",             ["Object ", "%1", " has no method '", "%0", "'"],
      "property_not_function",        ["Property '", "%0", "' of object ", "%1", " is not a function"],
      "cannot_convert_to_primitive",  ["Cannot convert object to primitive value"],
      "not_constructor",              ["%0", " is not a constructor"],
      "not_defined",                  ["%0", " is not defined"],
      "non_object_property_load",     ["Cannot read property '", "%0", "' of ", "%1"],
      "non_object_property_store",    ["Cannot set property '", "%0", "' of ", "%1"],
      "non_object_property_call",     ["Cannot call method '", "%0", "' of ", "%1"],
      "with_expression",              ["%0", " has no properties"],
      "illegal_invocation",           ["Illegal invocation"],
      "no_setter_in_callback",        ["Cannot set property ", "%0", " of ", "%1", " which has only a getter"],
      "apply_non_function",           ["Function.prototype.apply was called on ", "%0", ", which is a ", "%1", " and not a function"],
      "apply_wrong_args",             ["Function.prototype.apply: Arguments list has wrong type"],
      "invalid_in_operator_use",      ["Cannot use 'in' operator to search for '", "%0", "' in ", "%1"],
      "instanceof_function_expected", ["Expecting a function in instanceof check, but got ", "%0"],
      "instanceof_nonobject_proto",   ["Function has non-object prototype '", "%0", "' in instanceof check"],
      "null_to_object",               ["Cannot convert null to object"],
      "reduce_no_initial",            ["Reduce of empty array with no initial value"],
      "getter_must_be_callable",      ["Getter must be a function: ", "%0"],
      "setter_must_be_callable",      ["Setter must be a function: ", "%0"],
      "value_and_accessor",           ["Invalid property.  A property cannot both have accessors and be writable or have a value, ", "%0"],
      "proto_object_or_null",         ["Object prototype may only be an Object or null"],
      "property_desc_object",         ["Property description must be an object: ", "%0"],
      "redefine_disallowed",          ["Cannot redefine property: ", "%0"],
      "define_disallowed",            ["Cannot define property:", "%0", ", object is not extensible."],
      "non_extensible_proto",         ["%0", " is not extensible"],
      "handler_non_object",           ["Proxy.", "%0", " called with non-object as handler"],
      "trap_function_expected",       ["Proxy.", "%0", " called with non-function for ", "%1", " trap"],
      "handler_trap_missing",         ["Proxy handler ", "%0", " has no '", "%1", "' trap"],
      "handler_trap_must_be_callable", ["Proxy handler ", "%0", " has non-callable '", "%1", "' trap"],
      "handler_returned_false",       ["Proxy handler ", "%0", " returned false for '", "%1", "' trap"],
      "handler_returned_undefined",   ["Proxy handler ", "%0", " returned undefined for '", "%1", "' trap"],
      "proxy_prop_not_configurable",  ["Trap ", "%1", " of proxy handler ", "%0", " returned non-configurable descriptor for property ", "%2"],
      "proxy_non_object_prop_names",  ["Trap ", "%1", " returned non-object ", "%0"],
      "proxy_repeated_prop_name",     ["Trap ", "%1", " returned repeated property name ", "%2"],
      "invalid_weakmap_key",          ["Invalid value used as weak map key"],
      // RangeError
      "invalid_array_length",         ["Invalid array length"],
      "stack_overflow",               ["Maximum call stack size exceeded"],
      // SyntaxError
      "unable_to_parse",              ["Parse error"],
      "invalid_regexp_flags",         ["Invalid flags supplied to RegExp constructor '", "%0", "'"],
      "invalid_regexp",               ["Invalid RegExp pattern /", "%0", "/"],
      "illegal_break",                ["Illegal break statement"],
      "illegal_continue",             ["Illegal continue statement"],
      "illegal_return",               ["Illegal return statement"],
      "error_loading_debugger",       ["Error loading debugger"],
      "no_input_to_regexp",           ["No input to ", "%0"],
      "invalid_json",                 ["String '", "%0", "' is not valid JSON"],
      "circular_structure",           ["Converting circular structure to JSON"],
      "obj_ctor_property_non_object", ["Object.", "%0", " called on non-object"],
      "called_on_null_or_undefined",  ["%0", " called on null or undefined"],
      "array_indexof_not_defined",    ["Array.getIndexOf: Argument undefined"],
      "object_not_extensible",        ["Can't add property ", "%0", ", object is not extensible"],
      "illegal_access",               ["Illegal access"],
      "invalid_preparser_data",       ["Invalid preparser data for function ", "%0"],
      "strict_mode_with",             ["Strict mode code may not include a with statement"],
      "strict_catch_variable",        ["Catch variable may not be eval or arguments in strict mode"],
      "too_many_arguments",           ["Too many arguments in function call (only 32766 allowed)"],
      "too_many_parameters",          ["Too many parameters in function definition (only 32766 allowed)"],
      "too_many_variables",           ["Too many variables declared (only 32767 allowed)"],
      "strict_param_name",            ["Parameter name eval or arguments is not allowed in strict mode"],
      "strict_param_dupe",            ["Strict mode function may not have duplicate parameter names"],
      "strict_var_name",              ["Variable name may not be eval or arguments in strict mode"],
      "strict_function_name",         ["Function name may not be eval or arguments in strict mode"],
      "strict_octal_literal",         ["Octal literals are not allowed in strict mode."],
      "strict_duplicate_property",    ["Duplicate data property in object literal not allowed in strict mode"],
      "accessor_data_property",       ["Object literal may not have data and accessor property with the same name"],
      "accessor_get_set",             ["Object literal may not have multiple get/set accessors with the same name"],
      "strict_lhs_assignment",        ["Assignment to eval or arguments is not allowed in strict mode"],
      "strict_lhs_postfix",           ["Postfix increment/decrement may not have eval or arguments operand in strict mode"],
      "strict_lhs_prefix",            ["Prefix increment/decrement may not have eval or arguments operand in strict mode"],
      "strict_reserved_word",         ["Use of future reserved word in strict mode"],
      "strict_delete",                ["Delete of an unqualified identifier in strict mode."],
      "strict_delete_property",       ["Cannot delete property '", "%0", "' of ", "%1"],
      "strict_const",                 ["Use of const in strict mode."],
      "strict_function",              ["In strict mode code, functions can only be declared at top level or immediately within another function." ],
      "strict_read_only_property",    ["Cannot assign to read only property '", "%0", "' of ", "%1"],
      "strict_cannot_assign",         ["Cannot assign to read only '", "%0", "' in strict mode"],
      "strict_poison_pill",           ["'caller', 'callee', and 'arguments' properties may not be accessed on strict mode functions or the arguments objects for calls to them"],
      "strict_caller",                ["Illegal access to a strict mode caller function."],
      "unprotected_let",              ["Illegal let declaration in unprotected statement context."],
      "cant_prevent_ext_external_array_elements", ["Cannot prevent extension of an object with external array elements"],
      "redef_external_array_element", ["Cannot redefine a property of an object with external array elements"],
    ];
    var messages = { __proto__ : null };
    var desc = new PropertyDescriptor();
    desc.setConfigurable(false);
    desc.setEnumerable(false);
    desc.setWritable(false);
    for (var i = 0; i < messagesDictionary.length; i += 2) {
      var key = messagesDictionary[i];
      var format = messagesDictionary[i + 1];
      ObjectFreeze(format);
      desc.setValue(format);
      DefineOwnProperty(messages, key, desc);
    }
    %PreventExtensions(messages);
    %IgnoreAttributesAndSetProperty(builtins, "kMessages",
                                    messages,
                                    DONT_DELETE | DONT_ENUM | READ_ONLY);
  }
  var message_type = %MessageGetType(message);
  var format = kMessages[message_type];
  if (!format) return "<unknown message " + message_type + ">";
  return FormatString(format, message);
}


function GetLineNumber(message) {
  var start_position = %MessageGetStartPosition(message);
  if (start_position == -1) return kNoLineNumberInfo;
  var script = %MessageGetScript(message);
  var location = script.locationFromPosition(start_position, true);
  if (location == null) return kNoLineNumberInfo;
  return location.line + 1;
}


// Returns the source code line containing the given source
// position, or the empty string if the position is invalid.
function GetSourceLine(message) {
  var script = %MessageGetScript(message);
  var start_position = %MessageGetStartPosition(message);
  var location = script.locationFromPosition(start_position, true);
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
function ScriptLineFromPosition(position) {
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
function ScriptLocationFromPosition(position,
                                    include_resource_offset) {
  var line = this.lineFromPosition(position);
  if (line == -1) return null;

  // Determine start, end and column.
  var line_ends = this.line_ends;
  var start = line == 0 ? 0 : line_ends[line - 1] + 1;
  var end = line_ends[line];
  if (end > 0 && %_CallFunction(this.source, end - 1, StringCharAt) == '\r') {
    end--;
  }
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
 *     source from where the line and column calculation starts.
 *     Default value is 0
 * @return {SourceLocation}
 *     If line is negative or not in the source null is returned.
 */
function ScriptLocationFromLine(opt_line, opt_column, opt_offset_position) {
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
function ScriptSourceSlice(opt_from_line, opt_to_line) {
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


function ScriptSourceLine(opt_line) {
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
  return %_CallFunction(this.source, start, end, StringSubstring);
}


/**
 * Returns the number of source lines.
 * @return {number}
 *     Number of source lines.
 */
function ScriptLineCount() {
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
function ScriptNameOrSourceURL() {
  if (this.name) {
    return this.name;
  }
  // TODO(608): the spaces in a regexp below had to be escaped as \040
  // because this file is being processed by js2c whose handling of spaces
  // in regexps is broken. Also, ['"] are excluded from allowed URLs to
  // avoid matches against sources that invoke evals with sourceURL.
  // A better solution would be to detect these special comments in
  // the scanner/parser.
  var source = ToString(this.source);
  var sourceUrlPos = %StringIndexOf(source, "sourceURL=", 0);
  if (sourceUrlPos > 4) {
    var sourceUrlPattern =
        /\/\/@[\040\t]sourceURL=[\040\t]*([^\s\'\"]*)[\040\t]*$/gm;
    // Don't reuse lastMatchInfo here, so we create a new array with room
    // for four captures (array with length one longer than the index
    // of the fourth capture, where the numbering is zero-based).
    var matchInfo = new InternalArray(CAPTURE(3) + 1);
    var match =
        %_RegExpExec(sourceUrlPattern, source, sourceUrlPos - 4, matchInfo);
    if (match) {
      return SubString(source, matchInfo[CAPTURE(2)], matchInfo[CAPTURE(3)]);
    }
  }
  return this.name;
}


SetUpLockedPrototype(Script,
  $Array("source", "name", "line_ends", "line_offset", "column_offset"),
  $Array(
    "lineFromPosition", ScriptLineFromPosition,
    "locationFromPosition", ScriptLocationFromPosition,
    "locationFromLine", ScriptLocationFromLine,
    "sourceSlice", ScriptSourceSlice,
    "sourceLine", ScriptSourceLine,
    "lineCount", ScriptLineCount,
    "nameOrSourceURL", ScriptNameOrSourceURL
  )
);


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
function SourceLocationRestrict(opt_limit, opt_before) {
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
function SourceLocationSourceText() {
  return %_CallFunction(this.script.source, this.start, this.end, StringSubstring);
};


SetUpLockedPrototype(SourceLocation,
  $Array("script", "position", "line", "column", "start", "end"),
  $Array(
    "restrict", SourceLocationRestrict,
    "sourceText", SourceLocationSourceText
  )
);


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
function SourceSliceSourceText() {
  return %_CallFunction(this.script.source,
                        this.from_position,
                        this.to_position,
                        StringSubstring);
};

SetUpLockedPrototype(SourceSlice,
  $Array("script", "from_line", "to_line", "from_position", "to_position"),
  $Array("sourceText", SourceSliceSourceText)
);


// Returns the offset of the given position within the containing
// line.
function GetPositionInLine(message) {
  var script = %MessageGetScript(message);
  var start_position = %MessageGetStartPosition(message);
  var location = script.locationFromPosition(start_position, false);
  if (location == null) return -1;
  location.restrict();
  return start_position - location.start;
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
  function getter() {
    if (hasBeenSet) {
      return value;
    }
    hasBeenSet = true;
    value = fun(obj);
    return value;
  }
  function setter(v) {
    hasBeenSet = true;
    value = v;
  }
  var desc = { get: getter,
               set: setter,
               enumerable: false,
               configurable: true };
  desc = ToPropertyDescriptor(desc);
  DefineOwnProperty(obj, name, desc, true);
}

function CallSite(receiver, fun, pos) {
  this.receiver = receiver;
  this.fun = fun;
  this.pos = pos;
}

function CallSiteGetThis() {
  return this.receiver;
};

function CallSiteGetTypeName() {
  var constructor = this.receiver.constructor;
  if (!constructor) {
    return %_CallFunction(this.receiver, ObjectToString);
  }
  var constructorName = constructor.name;
  if (!constructorName) {
    return %_CallFunction(this.receiver, ObjectToString);
  }
  return constructorName;
};

function CallSiteIsToplevel() {
  if (this.receiver == null) {
    return true;
  }
  return IS_GLOBAL(this.receiver);
};

function CallSiteIsEval() {
  var script = %FunctionGetScript(this.fun);
  return script && script.compilation_type == COMPILATION_TYPE_EVAL;
};

function CallSiteGetEvalOrigin() {
  var script = %FunctionGetScript(this.fun);
  return FormatEvalOrigin(script);
};

function CallSiteGetScriptNameOrSourceURL() {
  var script = %FunctionGetScript(this.fun);
  return script ? script.nameOrSourceURL() : null;
};

function CallSiteGetFunction() {
  return this.fun;
};

function CallSiteGetFunctionName() {
  // See if the function knows its own name
  var name = this.fun.name;
  if (name) {
    return name;
  } else {
    return %FunctionGetInferredName(this.fun);
  }
  // Maybe this is an evaluation?
  var script = %FunctionGetScript(this.fun);
  if (script && script.compilation_type == COMPILATION_TYPE_EVAL) {
    return "eval";
  }
  return null;
};

function CallSiteGetMethodName() {
  // See if we can find a unique property on the receiver that holds
  // this function.
  var ownName = this.fun.name;
  if (ownName && this.receiver &&
      (%_CallFunction(this.receiver, ownName, ObjectLookupGetter) === this.fun ||
       %_CallFunction(this.receiver, ownName, ObjectLookupSetter) === this.fun ||
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
      if (name) {
        return null;
      }
      name = prop;
    }
  }
  if (name) {
    return name;
  }
  return null;
};

function CallSiteGetFileName() {
  var script = %FunctionGetScript(this.fun);
  return script ? script.name : null;
};

function CallSiteGetLineNumber() {
  if (this.pos == -1) {
    return null;
  }
  var script = %FunctionGetScript(this.fun);
  var location = null;
  if (script) {
    location = script.locationFromPosition(this.pos, true);
  }
  return location ? location.line + 1 : null;
};

function CallSiteGetColumnNumber() {
  if (this.pos == -1) {
    return null;
  }
  var script = %FunctionGetScript(this.fun);
  var location = null;
  if (script) {
    location = script.locationFromPosition(this.pos, true);
  }
  return location ? location.column + 1: null;
};

function CallSiteIsNative() {
  var script = %FunctionGetScript(this.fun);
  return script ? (script.type == TYPE_NATIVE) : false;
};

function CallSiteGetPosition() {
  return this.pos;
};

function CallSiteIsConstructor() {
  var constructor = this.receiver ? this.receiver.constructor : null;
  if (!constructor) {
    return false;
  }
  return this.fun === constructor;
};

SetUpLockedPrototype(CallSite, $Array("receiver", "fun", "pos"), $Array(
  "getThis", CallSiteGetThis,
  "getTypeName", CallSiteGetTypeName,
  "isToplevel", CallSiteIsToplevel,
  "isEval", CallSiteIsEval,
  "getEvalOrigin", CallSiteGetEvalOrigin,
  "getScriptNameOrSourceURL", CallSiteGetScriptNameOrSourceURL,
  "getFunction", CallSiteGetFunction,
  "getFunctionName", CallSiteGetFunctionName,
  "getMethodName", CallSiteGetMethodName,
  "getFileName", CallSiteGetFileName,
  "getLineNumber", CallSiteGetLineNumber,
  "getColumnNumber", CallSiteGetColumnNumber,
  "isNative", CallSiteIsNative,
  "getPosition", CallSiteGetPosition,
  "isConstructor", CallSiteIsConstructor
));


function FormatEvalOrigin(script) {
  var sourceURL = script.nameOrSourceURL();
  if (sourceURL) {
    return sourceURL;
  }

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


function captureStackTrace(obj, cons_opt) {
  var stackTraceLimit = $Error.stackTraceLimit;
  if (!stackTraceLimit || !IS_NUMBER(stackTraceLimit)) return;
  if (stackTraceLimit < 0 || stackTraceLimit > 10000) {
    stackTraceLimit = 10000;
  }
  var raw_stack = %CollectStackTrace(cons_opt
                                     ? cons_opt
                                     : captureStackTrace, stackTraceLimit);
  DefineOneShotAccessor(obj, 'stack', function (obj) {
    return FormatRawStackTrace(obj, raw_stack);
  });
};


function SetUpError() {
  // Define special error type constructors.

  function DefineError(f) {
    // Store the error function in both the global object
    // and the runtime object. The function is fetched
    // from the runtime object when throwing errors from
    // within the runtime system to avoid strange side
    // effects when overwriting the error functions from
    // user code.
    var name = f.name;
    %SetProperty(global, name, f, DONT_ENUM);
    %SetProperty(builtins, '$' + name, f, DONT_ENUM | DONT_DELETE | READ_ONLY);
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
    %SetProperty(f.prototype, "name", name,
                 DONT_ENUM | DONT_DELETE | READ_ONLY)  ;
    %SetCode(f, function(m) {
      if (%_IsConstructCall()) {
        // Define all the expected properties directly on the error
        // object. This avoids going through getters and setters defined
        // on prototype objects.
        %IgnoreAttributesAndSetProperty(this, 'stack', void 0, DONT_ENUM);
        %IgnoreAttributesAndSetProperty(this, 'arguments', void 0, DONT_ENUM);
        %IgnoreAttributesAndSetProperty(this, 'type', void 0, DONT_ENUM);
        if (m === kAddMessageAccessorsMarker) {
          // DefineOneShotAccessor always inserts a message property and
          // ignores setters.
          DefineOneShotAccessor(this, 'message', function (obj) {
              return FormatMessage(%NewMessageObject(obj.type, obj.arguments));
          });
        } else if (!IS_UNDEFINED(m)) {
          %IgnoreAttributesAndSetProperty(this,
                                          'message',
                                          ToString(m),
                                          DONT_ENUM);
        }
        captureStackTrace(this, f);
      } else {
        return new f(m);
      }
    });
  }

  DefineError(function Error() { });
  DefineError(function TypeError() { });
  DefineError(function RangeError() { });
  DefineError(function SyntaxError() { });
  DefineError(function ReferenceError() { });
  DefineError(function EvalError() { });
  DefineError(function URIError() { });
}

SetUpError();

$Error.captureStackTrace = captureStackTrace;

%SetProperty($Error.prototype, 'message', '', DONT_ENUM);

// Global list of error objects visited during errorToString. This is
// used to detect cycles in error toString formatting.
const visited_errors = new InternalArray();
const cyclic_error_marker = new $Object();

function errorToStringDetectCycle(error) {
  if (!%PushIfAbsent(visited_errors, error)) throw cyclic_error_marker;
  try {
    var type = error.type;
    var hasMessage = %_CallFunction(error, "message", ObjectHasOwnProperty);
    if (type && !hasMessage) {
      var formatted = FormatMessage(%NewMessageObject(type, error.arguments));
      return error.name + ": " + formatted;
    }
    var message = hasMessage ? (": " + error.message) : "";
    return error.name + message;
  } finally {
    visited_errors.length = visited_errors.length - 1;
  }
}

function errorToString() {
  if (IS_NULL_OR_UNDEFINED(this) && !IS_UNDETECTABLE(this)) {
    throw MakeTypeError("called_on_null_or_undefined",
                        ["Error.prototype.toString"]);
  }
  // This helper function is needed because access to properties on
  // the builtins object do not work inside of a catch clause.
  function isCyclicErrorMarker(o) { return o === cyclic_error_marker; }

  try {
    return errorToStringDetectCycle(this);
  } catch(e) {
    // If this error message was encountered already return the empty
    // string for it instead of recursively formatting it.
    if (isCyclicErrorMarker(e)) {
      return '';
    }
    throw e;
  }
}


InstallFunctions($Error.prototype, DONT_ENUM, ['toString', errorToString]);

// Boilerplate for exceptions for stack overflows. Used from
// Isolate::StackOverflow().
const kStackOverflowBoilerplate = MakeRangeError('stack_overflow', []);
