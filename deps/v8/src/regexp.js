// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $Object = global.Object;
// var $Array = global.Array;

var $RegExp = global.RegExp;

// -------------------------------------------------------------------

// A recursive descent parser for Patterns according to the grammar of
// ECMA-262 15.10.1, with deviations noted below.
function DoConstructRegExp(object, pattern, flags) {
  // RegExp : Called as constructor; see ECMA-262, section 15.10.4.
  if (IS_REGEXP(pattern)) {
    if (!IS_UNDEFINED(flags)) {
      throw MakeTypeError('regexp_flags', []);
    }
    flags = (pattern.global ? 'g' : '')
        + (pattern.ignoreCase ? 'i' : '')
        + (pattern.multiline ? 'm' : '');
    pattern = pattern.source;
  }

  pattern = IS_UNDEFINED(pattern) ? '' : ToString(pattern);
  flags = IS_UNDEFINED(flags) ? '' : ToString(flags);

  var global = false;
  var ignoreCase = false;
  var multiline = false;
  for (var i = 0; i < flags.length; i++) {
    var c = %_CallFunction(flags, i, StringCharAt);
    switch (c) {
      case 'g':
        if (global) {
          throw MakeSyntaxError("invalid_regexp_flags", [flags]);
        }
        global = true;
        break;
      case 'i':
        if (ignoreCase) {
          throw MakeSyntaxError("invalid_regexp_flags", [flags]);
        }
        ignoreCase = true;
        break;
      case 'm':
        if (multiline) {
          throw MakeSyntaxError("invalid_regexp_flags", [flags]);
        }
        multiline = true;
        break;
      default:
        throw MakeSyntaxError("invalid_regexp_flags", [flags]);
    }
  }

  %RegExpInitializeObject(object, pattern, global, ignoreCase, multiline);

  // Call internal function to compile the pattern.
  %RegExpCompile(object, pattern, flags);
}


function RegExpConstructor(pattern, flags) {
  if (%_IsConstructCall()) {
    DoConstructRegExp(this, pattern, flags);
  } else {
    // RegExp : Called as function; see ECMA-262, section 15.10.3.1.
    if (IS_REGEXP(pattern) && IS_UNDEFINED(flags)) {
      return pattern;
    }
    return new $RegExp(pattern, flags);
  }
}

// Deprecated RegExp.prototype.compile method.  We behave like the constructor
// were called again.  In SpiderMonkey, this method returns the regexp object.
// In JSC, it returns undefined.  For compatibility with JSC, we match their
// behavior.
function RegExpCompileJS(pattern, flags) {
  // Both JSC and SpiderMonkey treat a missing pattern argument as the
  // empty subject string, and an actual undefined value passed as the
  // pattern as the string 'undefined'.  Note that JSC is inconsistent
  // here, treating undefined values differently in
  // RegExp.prototype.compile and in the constructor, where they are
  // the empty string.  For compatibility with JSC, we match their
  // behavior.
  if (this == $RegExp.prototype) {
    // We don't allow recompiling RegExp.prototype.
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.compile', this]);
  }
  if (IS_UNDEFINED(pattern) && %_ArgumentsLength() != 0) {
    DoConstructRegExp(this, 'undefined', flags);
  } else {
    DoConstructRegExp(this, pattern, flags);
  }
}


function DoRegExpExec(regexp, string, index) {
  var result = %_RegExpExec(regexp, string, index, lastMatchInfo);
  if (result !== null) lastMatchInfoOverride = null;
  return result;
}


// This is kind of performance sensitive, so we want to avoid unnecessary
// type checks on inputs. But we also don't want to inline it several times
// manually, so we use a macro :-)
macro RETURN_NEW_RESULT_FROM_MATCH_INFO(MATCHINFO, STRING)
  var numResults = NUMBER_OF_CAPTURES(MATCHINFO) >> 1;
  var start = MATCHINFO[CAPTURE0];
  var end = MATCHINFO[CAPTURE1];
  // Calculate the substring of the first match before creating the result array
  // to avoid an unnecessary write barrier storing the first result.
  var first = %_SubString(STRING, start, end);
  var result = %_RegExpConstructResult(numResults, start, STRING);
  result[0] = first;
  if (numResults == 1) return result;
  var j = REGEXP_FIRST_CAPTURE + 2;
  for (var i = 1; i < numResults; i++) {
    start = MATCHINFO[j++];
    if (start != -1) {
      end = MATCHINFO[j];
      result[i] = %_SubString(STRING, start, end);
    }
    j++;
  }
  return result;
endmacro


function RegExpExecNoTests(regexp, string, start) {
  // Must be called with RegExp, string and positive integer as arguments.
  var matchInfo = %_RegExpExec(regexp, string, start, lastMatchInfo);
  if (matchInfo !== null) {
    lastMatchInfoOverride = null;
    RETURN_NEW_RESULT_FROM_MATCH_INFO(matchInfo, string);
  }
  regexp.lastIndex = 0;
  return null;
}


function RegExpExec(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.exec', this]);
  }

  string = TO_STRING_INLINE(string);
  var lastIndex = this.lastIndex;

  // Conversion is required by the ES5 specification (RegExp.prototype.exec
  // algorithm, step 5) even if the value is discarded for non-global RegExps.
  var i = TO_INTEGER(lastIndex);

  var global = this.global;
  if (global) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return null;
    }
  } else {
    i = 0;
  }

  // matchIndices is either null or the lastMatchInfo array.
  var matchIndices = %_RegExpExec(this, string, i, lastMatchInfo);

  if (IS_NULL(matchIndices)) {
    this.lastIndex = 0;
    return null;
  }

  // Successful match.
  lastMatchInfoOverride = null;
  if (global) {
    this.lastIndex = lastMatchInfo[CAPTURE1];
  }
  RETURN_NEW_RESULT_FROM_MATCH_INFO(matchIndices, string);
}


// One-element cache for the simplified test regexp.
var regexp_key;
var regexp_val;

// Section 15.10.6.3 doesn't actually make sense, but the intention seems to be
// that test is defined in terms of String.prototype.exec. However, it probably
// means the original value of String.prototype.exec, which is what everybody
// else implements.
function RegExpTest(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.test', this]);
  }
  string = TO_STRING_INLINE(string);

  var lastIndex = this.lastIndex;

  // Conversion is required by the ES5 specification (RegExp.prototype.exec
  // algorithm, step 5) even if the value is discarded for non-global RegExps.
  var i = TO_INTEGER(lastIndex);

  if (this.global) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return false;
    }
    // matchIndices is either null or the lastMatchInfo array.
    var matchIndices = %_RegExpExec(this, string, i, lastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    lastMatchInfoOverride = null;
    this.lastIndex = lastMatchInfo[CAPTURE1];
    return true;
  } else {
    // Non-global regexp.
    // Remove irrelevant preceeding '.*' in a non-global test regexp.
    // The expression checks whether this.source starts with '.*' and
    // that the third char is not a '?'.
    var regexp = this;
    if (%_StringCharCodeAt(regexp.source, 0) == 46 &&  // '.'
        %_StringCharCodeAt(regexp.source, 1) == 42 &&  // '*'
        %_StringCharCodeAt(regexp.source, 2) != 63) {  // '?'
      regexp = TrimRegExp(regexp);
    }
    // matchIndices is either null or the lastMatchInfo array.
    var matchIndices = %_RegExpExec(regexp, string, 0, lastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    lastMatchInfoOverride = null;
    return true;
  }
}

function TrimRegExp(regexp) {
  if (!%_ObjectEquals(regexp_key, regexp)) {
    regexp_key = regexp;
    regexp_val =
      new $RegExp(%_SubString(regexp.source, 2, regexp.source.length),
                  (regexp.ignoreCase ? regexp.multiline ? "im" : "i"
                                     : regexp.multiline ? "m" : ""));
  }
  return regexp_val;
}


function RegExpToString() {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.toString', this]);
  }
  var result = '/' + this.source + '/';
  if (this.global) result += 'g';
  if (this.ignoreCase) result += 'i';
  if (this.multiline) result += 'm';
  return result;
}


// Getters for the static properties lastMatch, lastParen, leftContext, and
// rightContext of the RegExp constructor.  The properties are computed based
// on the captures array of the last successful match and the subject string
// of the last successful match.
function RegExpGetLastMatch() {
  if (lastMatchInfoOverride !== null) {
    return OVERRIDE_MATCH(lastMatchInfoOverride);
  }
  var regExpSubject = LAST_SUBJECT(lastMatchInfo);
  return %_SubString(regExpSubject,
                     lastMatchInfo[CAPTURE0],
                     lastMatchInfo[CAPTURE1]);
}


function RegExpGetLastParen() {
  if (lastMatchInfoOverride) {
    var override = lastMatchInfoOverride;
    if (override.length <= 3) return '';
    return override[override.length - 3];
  }
  var length = NUMBER_OF_CAPTURES(lastMatchInfo);
  if (length <= 2) return '';  // There were no captures.
  // We match the SpiderMonkey behavior: return the substring defined by the
  // last pair (after the first pair) of elements of the capture array even if
  // it is empty.
  var regExpSubject = LAST_SUBJECT(lastMatchInfo);
  var start = lastMatchInfo[CAPTURE(length - 2)];
  var end = lastMatchInfo[CAPTURE(length - 1)];
  if (start != -1 && end != -1) {
    return %_SubString(regExpSubject, start, end);
  }
  return "";
}


function RegExpGetLeftContext() {
  var start_index;
  var subject;
  if (!lastMatchInfoOverride) {
    start_index = lastMatchInfo[CAPTURE0];
    subject = LAST_SUBJECT(lastMatchInfo);
  } else {
    var override = lastMatchInfoOverride;
    start_index = OVERRIDE_POS(override);
    subject = OVERRIDE_SUBJECT(override);
  }
  return %_SubString(subject, 0, start_index);
}


function RegExpGetRightContext() {
  var start_index;
  var subject;
  if (!lastMatchInfoOverride) {
    start_index = lastMatchInfo[CAPTURE1];
    subject = LAST_SUBJECT(lastMatchInfo);
  } else {
    var override = lastMatchInfoOverride;
    subject = OVERRIDE_SUBJECT(override);
    var match = OVERRIDE_MATCH(override);
    start_index = OVERRIDE_POS(override) + match.length;
  }
  return %_SubString(subject, start_index, subject.length);
}


// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
function RegExpMakeCaptureGetter(n) {
  return function() {
    if (lastMatchInfoOverride) {
      if (n < lastMatchInfoOverride.length - 2) {
        return OVERRIDE_CAPTURE(lastMatchInfoOverride, n);
      }
      return '';
    }
    var index = n * 2;
    if (index >= NUMBER_OF_CAPTURES(lastMatchInfo)) return '';
    var matchStart = lastMatchInfo[CAPTURE(index)];
    var matchEnd = lastMatchInfo[CAPTURE(index + 1)];
    if (matchStart == -1 || matchEnd == -1) return '';
    return %_SubString(LAST_SUBJECT(lastMatchInfo), matchStart, matchEnd);
  };
}


// Property of the builtins object for recording the result of the last
// regexp match.  The property lastMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indeces.  The array also contains
// the subject string for the last successful match.
var lastMatchInfo = new InternalPackedArray(
    2,                 // REGEXP_NUMBER_OF_CAPTURES
    "",                // Last subject.
    UNDEFINED,         // Last input - settable with RegExpSetInput.
    0,                 // REGEXP_FIRST_CAPTURE + 0
    0                  // REGEXP_FIRST_CAPTURE + 1
);

// Override last match info with an array of actual substrings.
// Used internally by replace regexp with function.
// The array has the format of an "apply" argument for a replacement
// function.
var lastMatchInfoOverride = null;

// -------------------------------------------------------------------

function SetUpRegExp() {
  %CheckIsBootstrapping();
  %FunctionSetInstanceClassName($RegExp, 'RegExp');
  %AddNamedProperty($RegExp.prototype, 'constructor', $RegExp, DONT_ENUM);
  %SetCode($RegExp, RegExpConstructor);

  InstallFunctions($RegExp.prototype, DONT_ENUM, $Array(
    "exec", RegExpExec,
    "test", RegExpTest,
    "toString", RegExpToString,
    "compile", RegExpCompileJS
  ));

  // The length of compile is 1 in SpiderMonkey.
  %FunctionSetLength($RegExp.prototype.compile, 1);

  // The properties input, $input, and $_ are aliases for each other.  When this
  // value is set the value it is set to is coerced to a string.
  // Getter and setter for the input.
  var RegExpGetInput = function() {
    var regExpInput = LAST_INPUT(lastMatchInfo);
    return IS_UNDEFINED(regExpInput) ? "" : regExpInput;
  };
  var RegExpSetInput = function(string) {
    LAST_INPUT(lastMatchInfo) = ToString(string);
  };

  %OptimizeObjectForAddingMultipleProperties($RegExp, 22);
  %DefineAccessorPropertyUnchecked($RegExp, 'input', RegExpGetInput,
                                   RegExpSetInput, DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$_', RegExpGetInput,
                                   RegExpSetInput, DONT_ENUM | DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$input', RegExpGetInput,
                                   RegExpSetInput, DONT_ENUM | DONT_DELETE);

  // The properties multiline and $* are aliases for each other.  When this
  // value is set in SpiderMonkey, the value it is set to is coerced to a
  // boolean.  We mimic that behavior with a slight difference: in SpiderMonkey
  // the value of the expression 'RegExp.multiline = null' (for instance) is the
  // boolean false (i.e., the value after coercion), while in V8 it is the value
  // null (i.e., the value before coercion).

  // Getter and setter for multiline.
  var multiline = false;
  var RegExpGetMultiline = function() { return multiline; };
  var RegExpSetMultiline = function(flag) { multiline = flag ? true : false; };

  %DefineAccessorPropertyUnchecked($RegExp, 'multiline', RegExpGetMultiline,
                                   RegExpSetMultiline, DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$*', RegExpGetMultiline,
                                   RegExpSetMultiline,
                                   DONT_ENUM | DONT_DELETE);


  var NoOpSetter = function(ignored) {};


  // Static properties set by a successful match.
  %DefineAccessorPropertyUnchecked($RegExp, 'lastMatch', RegExpGetLastMatch,
                                   NoOpSetter, DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$&', RegExpGetLastMatch,
                                   NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, 'lastParen', RegExpGetLastParen,
                                   NoOpSetter, DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$+', RegExpGetLastParen,
                                   NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, 'leftContext',
                                   RegExpGetLeftContext, NoOpSetter,
                                   DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, '$`', RegExpGetLeftContext,
                                   NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, 'rightContext',
                                   RegExpGetRightContext, NoOpSetter,
                                   DONT_DELETE);
  %DefineAccessorPropertyUnchecked($RegExp, "$'", RegExpGetRightContext,
                                   NoOpSetter, DONT_ENUM | DONT_DELETE);

  for (var i = 1; i < 10; ++i) {
    %DefineAccessorPropertyUnchecked($RegExp, '$' + i,
                                     RegExpMakeCaptureGetter(i), NoOpSetter,
                                     DONT_DELETE);
  }
  %ToFastProperties($RegExp);
}

SetUpRegExp();
