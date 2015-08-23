// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -------------------------------------------------------------------

var $errorToString;
var $getStackTraceLine;
var $messageGetPositionInLine;
var $messageGetLineNumber;
var $messageGetSourceLine;
var $noSideEffectToString;
var $stackOverflowBoilerplate;
var $stackTraceSymbol;
var $toDetailString;
var $Error;
var $EvalError;
var $RangeError;
var $ReferenceError;
var $SyntaxError;
var $TypeError;
var $URIError;
var MakeError;
var MakeEvalError;
var MakeRangeError;
var MakeReferenceError;
var MakeSyntaxError;
var MakeTypeError;
var MakeURIError;

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalObject = global.Object;
var InternalArray = utils.InternalArray;
var ObjectDefineProperty = utils.ObjectDefineProperty;

var ArrayJoin;
var ObjectToString;
var StringCharAt;
var StringIndexOf;
var StringSubstring;

utils.Import(function(from) {
  ArrayJoin = from.ArrayJoin;
  ObjectToString = from.ObjectToString;
  StringCharAt = from.StringCharAt;
  StringIndexOf = from.StringIndexOf;
  StringSubstring = from.StringSubstring;
});

// -------------------------------------------------------------------

var GlobalError;
var GlobalTypeError;
var GlobalRangeError;
var GlobalURIError;
var GlobalSyntaxError;
var GlobalReferenceError;
var GlobalEvalError;


function NoSideEffectsObjectToString() {
  if (IS_UNDEFINED(this) && !IS_UNDETECTABLE(this)) return "[object Undefined]";
  if (IS_NULL(this)) return "[object Null]";
  return "[object " + %_ClassOf(TO_OBJECT_INLINE(this)) + "]";
}


function NoSideEffectToString(obj) {
  if (IS_STRING(obj)) return obj;
  if (IS_NUMBER(obj)) return %_NumberToString(obj);
  if (IS_BOOLEAN(obj)) return obj ? 'true' : 'false';
  if (IS_UNDEFINED(obj)) return 'undefined';
  if (IS_NULL(obj)) return 'null';
  if (IS_FUNCTION(obj)) {
    var str = %_CallFunction(obj, obj, $functionSourceString);
    if (str.length > 128) {
      str = %_SubString(str, 0, 111) + "...<omitted>..." +
            %_SubString(str, str.length - 2, str.length);
    }
    return str;
  }
  if (IS_SYMBOL(obj)) return %_CallFunction(obj, $symbolToString);
  if (IS_OBJECT(obj)
      && %GetDataProperty(obj, "toString") === ObjectToString) {
    var constructor = %GetDataProperty(obj, "constructor");
    if (typeof constructor == "function") {
      var constructorName = constructor.name;
      if (IS_STRING(constructorName) && constructorName !== "") {
        return "#<" + constructorName + ">";
      }
    }
  }
  if (CanBeSafelyTreatedAsAnErrorObject(obj)) {
    return %_CallFunction(obj, ErrorToString);
  }

  return %_CallFunction(obj, NoSideEffectsObjectToString);
}

// To determine whether we can safely stringify an object using ErrorToString
// without the risk of side-effects, we need to check whether the object is
// either an instance of a native error type (via '%_ClassOf'), or has Error
// in its prototype chain and hasn't overwritten 'toString' with something
// strange and unusual.
function CanBeSafelyTreatedAsAnErrorObject(obj) {
  switch (%_ClassOf(obj)) {
    case 'Error':
    case 'EvalError':
    case 'RangeError':
    case 'ReferenceError':
    case 'SyntaxError':
    case 'TypeError':
    case 'URIError':
      return true;
  }

  var objToString = %GetDataProperty(obj, "toString");
  return obj instanceof GlobalError && objToString === ErrorToString;
}


// When formatting internally created error messages, do not
// invoke overwritten error toString methods but explicitly use
// the error to string method. This is to avoid leaking error
// objects between script tags in a browser setting.
function ToStringCheckErrorObject(obj) {
  if (CanBeSafelyTreatedAsAnErrorObject(obj)) {
    return %_CallFunction(obj, ErrorToString);
  } else {
    return $toString(obj);
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


function MakeGenericError(constructor, type, arg0, arg1, arg2) {
  if (IS_UNDEFINED(arg0) && IS_STRING(type)) arg0 = [];
  return new constructor(FormatMessage(type, arg0, arg1, arg2));
}


/**
 * Set up the Script function and constructor.
 */
%FunctionSetInstanceClassName(Script, 'Script');
%AddNamedProperty(Script.prototype, 'constructor', Script,
                  DONT_ENUM | DONT_DELETE | READ_ONLY);
%SetCode(Script, function(x) {
  // Script objects can only be created by the VM.
  throw MakeError(kUnsupported);
});


// Helper functions; called from the runtime system.
function FormatMessage(type, arg0, arg1, arg2) {
  var arg0 = NoSideEffectToString(arg0);
  var arg1 = NoSideEffectToString(arg1);
  var arg2 = NoSideEffectToString(arg2);
  try {
    return %FormatMessageString(type, arg0, arg1, arg2);
  } catch (e) {
    return "<error>";
  }
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
  return location.sourceText();
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
}


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
    column -= this.column_offset;
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

    return this.locationFromPosition(
        this.line_ends[offset_line + line - 1] + 1 + column);  // line > 0 here.
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
  var from_line = IS_UNDEFINED(opt_from_line) ? this.line_offset
                                              : opt_from_line;
  var to_line = IS_UNDEFINED(opt_to_line) ? this.line_offset + this.lineCount()
                                          : opt_to_line;

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
  return new SourceSlice(this,
                         from_line + this.line_offset,
                         to_line + this.line_offset,
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
}


/**
 * Returns the position of the nth line end.
 * @return {number}
 *     Zero-based position of the nth line end in the script.
 */
function ScriptLineEnd(n) {
  return this.line_ends[n];
}


/**
 * If sourceURL comment is available returns sourceURL comment contents.
 * Otherwise, script name is returned. See
 * http://fbug.googlecode.com/svn/branches/firebug1.1/docs/ReleaseNotes_1.1.txt
 * and Source Map Revision 3 proposal for details on using //# sourceURL and
 * deprecated //@ sourceURL comment to identify scripts that don't have name.
 *
 * @return {?string} script name if present, value for //# sourceURL or
 * deprecated //@ sourceURL comment otherwise.
 */
function ScriptNameOrSourceURL() {
  if (this.source_url) return this.source_url;
  return this.name;
}


utils.SetUpLockedPrototype(Script, [
    "source",
    "name",
    "source_url",
    "source_mapping_url",
    "line_ends",
    "line_offset",
    "column_offset"
  ], [
    "lineFromPosition", ScriptLineFromPosition,
    "locationFromPosition", ScriptLocationFromPosition,
    "locationFromLine", ScriptLocationFromLine,
    "sourceSlice", ScriptSourceSlice,
    "sourceLine", ScriptSourceLine,
    "lineCount", ScriptLineCount,
    "nameOrSourceURL", ScriptNameOrSourceURL,
    "lineEnd", ScriptLineEnd
  ]
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
 * Source text for the source context is the character interval
 * [start, end[. In most cases end will point to a newline character.
 * It might point just past the final position of the source if the last
 * source line does not end with a newline character.
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


/**
 * Get the source text for a SourceLocation
 * @return {String}
 *     Source text for this location.
 */
function SourceLocationSourceText() {
  return %_CallFunction(this.script.source,
                        this.start,
                        this.end,
                        StringSubstring);
}


utils.SetUpLockedPrototype(SourceLocation,
  ["script", "position", "line", "column", "start", "end"],
  ["sourceText", SourceLocationSourceText]
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
}

utils.SetUpLockedPrototype(SourceSlice,
  ["script", "from_line", "to_line", "from_position", "to_position"],
  ["sourceText", SourceSliceSourceText]
);


// Returns the offset of the given position within the containing
// line.
function GetPositionInLine(message) {
  var script = %MessageGetScript(message);
  var start_position = %MessageGetStartPosition(message);
  var location = script.locationFromPosition(start_position, false);
  if (location == null) return -1;
  return start_position - location.start;
}


function GetStackTraceLine(recv, fun, pos, isGlobal) {
  return new CallSite(recv, fun, pos, false).toString();
}

// ----------------------------------------------------------------------------
// Error implementation

var CallSiteReceiverKey = NEW_PRIVATE("CallSite#receiver");
var CallSiteFunctionKey = NEW_PRIVATE("CallSite#function");
var CallSitePositionKey = NEW_PRIVATE("CallSite#position");
var CallSiteStrictModeKey = NEW_PRIVATE("CallSite#strict_mode");

function CallSite(receiver, fun, pos, strict_mode) {
  SET_PRIVATE(this, CallSiteReceiverKey, receiver);
  SET_PRIVATE(this, CallSiteFunctionKey, fun);
  SET_PRIVATE(this, CallSitePositionKey, pos);
  SET_PRIVATE(this, CallSiteStrictModeKey, strict_mode);
}

function CallSiteGetThis() {
  return GET_PRIVATE(this, CallSiteStrictModeKey)
      ? UNDEFINED : GET_PRIVATE(this, CallSiteReceiverKey);
}

function CallSiteGetFunction() {
  return GET_PRIVATE(this, CallSiteStrictModeKey)
      ? UNDEFINED : GET_PRIVATE(this, CallSiteFunctionKey);
}

function CallSiteGetPosition() {
  return GET_PRIVATE(this, CallSitePositionKey);
}

function CallSiteGetTypeName() {
  return GetTypeName(GET_PRIVATE(this, CallSiteReceiverKey), false);
}

function CallSiteIsToplevel() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteIsToplevelRT(receiver, fun, pos);
}

function CallSiteIsEval() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteIsEvalRT(receiver, fun, pos);
}

function CallSiteGetEvalOrigin() {
  var script = %FunctionGetScript(GET_PRIVATE(this, CallSiteFunctionKey));
  return FormatEvalOrigin(script);
}

function CallSiteGetScriptNameOrSourceURL() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetScriptNameOrSourceUrlRT(receiver, fun, pos);
}

function CallSiteGetFunctionName() {
  // See if the function knows its own name
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetFunctionNameRT(receiver, fun, pos);
}

function CallSiteGetMethodName() {
  // See if we can find a unique property on the receiver that holds
  // this function.
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetMethodNameRT(receiver, fun, pos);
}

function CallSiteGetFileName() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetFileNameRT(receiver, fun, pos);
}

function CallSiteGetLineNumber() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetLineNumberRT(receiver, fun, pos);
}

function CallSiteGetColumnNumber() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteGetColumnNumberRT(receiver, fun, pos);
}

function CallSiteIsNative() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteIsNativeRT(receiver, fun, pos);
}

function CallSiteIsConstructor() {
  var receiver = GET_PRIVATE(this, CallSiteReceiverKey);
  var fun = GET_PRIVATE(this, CallSiteFunctionKey);
  var pos = GET_PRIVATE(this, CallSitePositionKey);
  return %CallSiteIsConstructorRT(receiver, fun, pos);
}

function CallSiteToString() {
  var fileName;
  var fileLocation = "";
  if (this.isNative()) {
    fileLocation = "native";
  } else {
    fileName = this.getScriptNameOrSourceURL();
    if (!fileName && this.isEval()) {
      fileLocation = this.getEvalOrigin();
      fileLocation += ", ";  // Expecting source position to follow.
    }

    if (fileName) {
      fileLocation += fileName;
    } else {
      // Source code does not originate from a file and is not native, but we
      // can still get the source position inside the source string, e.g. in
      // an eval string.
      fileLocation += "<anonymous>";
    }
    var lineNumber = this.getLineNumber();
    if (lineNumber != null) {
      fileLocation += ":" + lineNumber;
      var columnNumber = this.getColumnNumber();
      if (columnNumber) {
        fileLocation += ":" + columnNumber;
      }
    }
  }

  var line = "";
  var functionName = this.getFunctionName();
  var addSuffix = true;
  var isConstructor = this.isConstructor();
  var isMethodCall = !(this.isToplevel() || isConstructor);
  if (isMethodCall) {
    var typeName = GetTypeName(GET_PRIVATE(this, CallSiteReceiverKey), true);
    var methodName = this.getMethodName();
    if (functionName) {
      if (typeName &&
          %_CallFunction(functionName, typeName, StringIndexOf) != 0) {
        line += typeName + ".";
      }
      line += functionName;
      if (methodName &&
          (%_CallFunction(functionName, "." + methodName, StringIndexOf) !=
           functionName.length - methodName.length - 1)) {
        line += " [as " + methodName + "]";
      }
    } else {
      line += typeName + "." + (methodName || "<anonymous>");
    }
  } else if (isConstructor) {
    line += "new " + (functionName || "<anonymous>");
  } else if (functionName) {
    line += functionName;
  } else {
    line += fileLocation;
    addSuffix = false;
  }
  if (addSuffix) {
    line += " (" + fileLocation + ")";
  }
  return line;
}

utils.SetUpLockedPrototype(CallSite, ["receiver", "fun", "pos"], [
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
  "isConstructor", CallSiteIsConstructor,
  "toString", CallSiteToString
]);


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
        var location = eval_from_script.locationFromPosition(
            script.eval_from_script_position, true);
        if (location) {
          eval_origin += ":" + (location.line + 1);
          eval_origin += ":" + (location.column + 1);
        }
        eval_origin += ")";
      } else {
        eval_origin += " (unknown source)";
      }
    }
  }

  return eval_origin;
}


function FormatErrorString(error) {
  try {
    return %_CallFunction(error, ErrorToString);
  } catch (e) {
    try {
      return "<error: " + e + ">";
    } catch (ee) {
      return "<error>";
    }
  }
}


function GetStackFrames(raw_stack) {
  var frames = new InternalArray();
  var sloppy_frames = raw_stack[0];
  for (var i = 1; i < raw_stack.length; i += 4) {
    var recv = raw_stack[i];
    var fun = raw_stack[i + 1];
    var code = raw_stack[i + 2];
    var pc = raw_stack[i + 3];
    var pos = %_IsSmi(code) ? code : %FunctionGetPositionForOffset(code, pc);
    sloppy_frames--;
    frames.push(new CallSite(recv, fun, pos, (sloppy_frames < 0)));
  }
  return frames;
}


// Flag to prevent recursive call of Error.prepareStackTrace.
var formatting_custom_stack_trace = false;


function FormatStackTrace(obj, raw_stack) {
  var frames = GetStackFrames(raw_stack);
  if (IS_FUNCTION(GlobalError.prepareStackTrace) &&
      !formatting_custom_stack_trace) {
    var array = [];
    %MoveArrayContents(frames, array);
    formatting_custom_stack_trace = true;
    var stack_trace = UNDEFINED;
    try {
      stack_trace = GlobalError.prepareStackTrace(obj, array);
    } catch (e) {
      throw e;  // The custom formatting function threw.  Rethrow.
    } finally {
      formatting_custom_stack_trace = false;
    }
    return stack_trace;
  }

  var lines = new InternalArray();
  lines.push(FormatErrorString(obj));
  for (var i = 0; i < frames.length; i++) {
    var frame = frames[i];
    var line;
    try {
      line = frame.toString();
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
  return %_CallFunction(lines, "\n", ArrayJoin);
}


function GetTypeName(receiver, requireConstructor) {
  if (IS_NULL_OR_UNDEFINED(receiver)) return null;
  var constructor = receiver.constructor;
  if (!constructor) {
    return requireConstructor ? null :
        %_CallFunction(receiver, NoSideEffectsObjectToString);
  }
  var constructorName = constructor.name;
  if (!constructorName) {
    return requireConstructor ? null :
        %_CallFunction(receiver, NoSideEffectsObjectToString);
  }
  return constructorName;
}

var formatted_stack_trace_symbol = NEW_PRIVATE("formatted stack trace");


// Format the stack trace if not yet done, and return it.
// Cache the formatted stack trace on the holder.
var StackTraceGetter = function() {
  var formatted_stack_trace = UNDEFINED;
  var holder = this;
  while (holder) {
    var formatted_stack_trace =
      GET_PRIVATE(holder, formatted_stack_trace_symbol);
    if (IS_UNDEFINED(formatted_stack_trace)) {
      // No formatted stack trace available.
      var stack_trace = GET_PRIVATE(holder, $stackTraceSymbol);
      if (IS_UNDEFINED(stack_trace)) {
        // Neither formatted nor structured stack trace available.
        // Look further up the prototype chain.
        holder = %_GetPrototype(holder);
        continue;
      }
      formatted_stack_trace = FormatStackTrace(holder, stack_trace);
      SET_PRIVATE(holder, $stackTraceSymbol, UNDEFINED);
      SET_PRIVATE(holder, formatted_stack_trace_symbol, formatted_stack_trace);
    }
    return formatted_stack_trace;
  }
  return UNDEFINED;
};


// If the receiver equals the holder, set the formatted stack trace that the
// getter returns.
var StackTraceSetter = function(v) {
  if (HAS_PRIVATE(this, $stackTraceSymbol)) {
    SET_PRIVATE(this, $stackTraceSymbol, UNDEFINED);
    SET_PRIVATE(this, formatted_stack_trace_symbol, v);
  }
};


// Use a dummy function since we do not actually want to capture a stack trace
// when constructing the initial Error prototytpes.
var captureStackTrace = function() {};


// Define special error type constructors.
function DefineError(global, f) {
  // Store the error function in both the global object
  // and the runtime object. The function is fetched
  // from the runtime object when throwing errors from
  // within the runtime system to avoid strange side
  // effects when overwriting the error functions from
  // user code.
  var name = f.name;
  %AddNamedProperty(global, name, f, DONT_ENUM);
  // Configure the error function.
  if (name == 'Error') {
    // The prototype of the Error object must itself be an error.
    // However, it can't be an instance of the Error object because
    // it hasn't been properly configured yet.  Instead we create a
    // special not-a-true-error-but-close-enough object.
    var ErrorPrototype = function() {};
    %FunctionSetPrototype(ErrorPrototype, GlobalObject.prototype);
    %FunctionSetInstanceClassName(ErrorPrototype, 'Error');
    %FunctionSetPrototype(f, new ErrorPrototype());
  } else {
    %FunctionSetPrototype(f, new GlobalError());
    %InternalSetPrototype(f, GlobalError);
  }
  %FunctionSetInstanceClassName(f, 'Error');
  %AddNamedProperty(f.prototype, 'constructor', f, DONT_ENUM);
  %AddNamedProperty(f.prototype, 'name', name, DONT_ENUM);
  %SetCode(f, function(m) {
    if (%_IsConstructCall()) {
      try { captureStackTrace(this, f); } catch (e) { }
      // Define all the expected properties directly on the error
      // object. This avoids going through getters and setters defined
      // on prototype objects.
      if (!IS_UNDEFINED(m)) {
        %AddNamedProperty(this, 'message', $toString(m), DONT_ENUM);
      }
    } else {
      return new f(m);
    }
  });
  %SetNativeFlag(f);
  return f;
};

GlobalError = DefineError(global, function Error() { });
GlobalEvalError = DefineError(global, function EvalError() { });
GlobalRangeError = DefineError(global, function RangeError() { });
GlobalReferenceError = DefineError(global, function ReferenceError() { });
GlobalSyntaxError = DefineError(global, function SyntaxError() { });
GlobalTypeError = DefineError(global, function TypeError() { });
GlobalURIError = DefineError(global, function URIError() { });

%AddNamedProperty(GlobalError.prototype, 'message', '', DONT_ENUM);

// Global list of error objects visited during ErrorToString. This is
// used to detect cycles in error toString formatting.
var visited_errors = new InternalArray();
var cyclic_error_marker = new GlobalObject();

function GetPropertyWithoutInvokingMonkeyGetters(error, name) {
  var current = error;
  // Climb the prototype chain until we find the holder.
  while (current && !%HasOwnProperty(current, name)) {
    current = %_GetPrototype(current);
  }
  if (IS_NULL(current)) return UNDEFINED;
  if (!IS_OBJECT(current)) return error[name];
  // If the property is an accessor on one of the predefined errors that can be
  // generated statically by the compiler, don't touch it. This is to address
  // http://code.google.com/p/chromium/issues/detail?id=69187
  var desc = %GetOwnProperty(current, name);
  if (desc && desc[IS_ACCESSOR_INDEX]) {
    var isName = name === "name";
    if (current === GlobalReferenceError.prototype)
      return isName ? "ReferenceError" : UNDEFINED;
    if (current === GlobalSyntaxError.prototype)
      return isName ? "SyntaxError" : UNDEFINED;
    if (current === GlobalTypeError.prototype)
      return isName ? "TypeError" : UNDEFINED;
  }
  // Otherwise, read normally.
  return error[name];
}

function ErrorToStringDetectCycle(error) {
  if (!%PushIfAbsent(visited_errors, error)) throw cyclic_error_marker;
  try {
    var name = GetPropertyWithoutInvokingMonkeyGetters(error, "name");
    name = IS_UNDEFINED(name) ? "Error" : TO_STRING_INLINE(name);
    var message = GetPropertyWithoutInvokingMonkeyGetters(error, "message");
    message = IS_UNDEFINED(message) ? "" : TO_STRING_INLINE(message);
    if (name === "") return message;
    if (message === "") return name;
    return name + ": " + message;
  } finally {
    visited_errors.length = visited_errors.length - 1;
  }
}

function ErrorToString() {
  if (!IS_SPEC_OBJECT(this)) {
    throw MakeTypeError(kCalledOnNonObject, "Error.prototype.toString");
  }

  try {
    return ErrorToStringDetectCycle(this);
  } catch(e) {
    // If this error message was encountered already return the empty
    // string for it instead of recursively formatting it.
    if (e === cyclic_error_marker) {
      return '';
    }
    throw e;
  }
}

utils.InstallFunctions(GlobalError.prototype, DONT_ENUM,
                       ['toString', ErrorToString]);

$errorToString = ErrorToString;
$getStackTraceLine = GetStackTraceLine;
$messageGetPositionInLine = GetPositionInLine;
$messageGetLineNumber = GetLineNumber;
$messageGetSourceLine = GetSourceLine;
$noSideEffectToString = NoSideEffectToString;
$toDetailString = ToDetailString;

$Error = GlobalError;
$EvalError = GlobalEvalError;
$RangeError = GlobalRangeError;
$ReferenceError = GlobalReferenceError;
$SyntaxError = GlobalSyntaxError;
$TypeError = GlobalTypeError;
$URIError = GlobalURIError;

MakeError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalError, type, arg0, arg1, arg2);
}

MakeEvalError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalEvalError, type, arg0, arg1, arg2);
}

MakeRangeError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalRangeError, type, arg0, arg1, arg2);
}

MakeReferenceError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalReferenceError, type, arg0, arg1, arg2);
}

MakeSyntaxError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalSyntaxError, type, arg0, arg1, arg2);
}

MakeTypeError = function(type, arg0, arg1, arg2) {
  return MakeGenericError(GlobalTypeError, type, arg0, arg1, arg2);
}

MakeURIError = function() {
  return MakeGenericError(GlobalURIError, kURIMalformed);
}

// Boilerplate for exceptions for stack overflows. Used from
// Isolate::StackOverflow().
$stackOverflowBoilerplate = MakeRangeError(kStackOverflow);
%DefineAccessorPropertyUnchecked($stackOverflowBoilerplate, 'stack',
                                 StackTraceGetter, StackTraceSetter,
                                 DONT_ENUM);

// Define actual captureStackTrace function after everything has been set up.
captureStackTrace = function captureStackTrace(obj, cons_opt) {
  // Define accessors first, as this may fail and throw.
  ObjectDefineProperty(obj, 'stack', { get: StackTraceGetter,
                                       set: StackTraceSetter,
                                       configurable: true });
  %CollectStackTrace(obj, cons_opt ? cons_opt : captureStackTrace);
};

GlobalError.captureStackTrace = captureStackTrace;

});
