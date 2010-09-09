// Copyright 2006-2009 the V8 project authors. All rights reserved.
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

// Expect $Object = global.Object;
// Expect $Array = global.Array;

const $RegExp = global.RegExp;

// A recursive descent parser for Patterns according to the grammar of
// ECMA-262 15.10.1, with deviations noted below.
function DoConstructRegExp(object, pattern, flags, isConstructorCall) {
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
    var c = StringCharAt.call(flags, i);
    switch (c) {
      case 'g':
        // Allow duplicate flags to be consistent with JSC and others.
        global = true;
        break;
      case 'i':
        ignoreCase = true;
        break;
      case 'm':
        multiline = true;
        break;
      default:
        // Ignore flags that have no meaning to be consistent with
        // JSC.
        break;
    }
  }

  if (!isConstructorCall) {
    regExpCache.type = 'none';
  }
  %RegExpInitializeObject(object, pattern, global, ignoreCase, multiline);

  // Call internal function to compile the pattern.
  %RegExpCompile(object, pattern, flags);
}


function RegExpConstructor(pattern, flags) {
  if (%_IsConstructCall()) {
    DoConstructRegExp(this, pattern, flags, true);
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
function CompileRegExp(pattern, flags) {
  // Both JSC and SpiderMonkey treat a missing pattern argument as the
  // empty subject string, and an actual undefined value passed as the
  // pattern as the string 'undefined'.  Note that JSC is inconsistent
  // here, treating undefined values differently in
  // RegExp.prototype.compile and in the constructor, where they are
  // the empty string.  For compatibility with JSC, we match their
  // behavior.
  if (IS_UNDEFINED(pattern) && %_ArgumentsLength() != 0) {
    DoConstructRegExp(this, 'undefined', flags, false);
  } else {
    DoConstructRegExp(this, pattern, flags, false);
  }
}


function DoRegExpExec(regexp, string, index) {
  var result = %_RegExpExec(regexp, string, index, lastMatchInfo);
  if (result !== null) lastMatchInfoOverride = null;
  return result;
}


function RegExpCache() {
  this.type = 'none';
  this.regExp = 0;
  this.subject = 0;
  this.replaceString = 0;
  this.lastIndex = 0;  // Also used for splitLimit when type is "split"
  this.answer = 0;
  // answerSaved marks whether the contents of answer is valid for a cache
  // hit in RegExpExec, StringMatch and StringSplit.
  this.answerSaved = false;
}


var regExpCache = new RegExpCache();


function BuildResultFromMatchInfo(lastMatchInfo, s) {
  var numResults = NUMBER_OF_CAPTURES(lastMatchInfo) >> 1;
  var result = %_RegExpConstructResult(numResults, lastMatchInfo[CAPTURE0], s);
  if (numResults === 1) {
    var matchStart = lastMatchInfo[CAPTURE(0)];
    var matchEnd = lastMatchInfo[CAPTURE(1)];
    result[0] = SubString(s, matchStart, matchEnd);
  } else {
    for (var i = 0; i < numResults; i++) {
      var matchStart = lastMatchInfo[CAPTURE(i << 1)];
      var matchEnd = lastMatchInfo[CAPTURE((i << 1) + 1)];
      if (matchStart != -1 && matchEnd != -1) {
        result[i] = SubString(s, matchStart, matchEnd);
      } else {
        // Make sure the element is present. Avoid reading the undefined
        // property from the global object since this may change.
        result[i] = void 0;
      }
    }
  }
  return result;
}


function RegExpExecNoTests(regexp, string, start) {
  // Must be called with RegExp, string and positive integer as arguments.
  var matchInfo = DoRegExpExec(regexp, string, start);
  var result = null;
  if (matchInfo !== null) {
    result = BuildResultFromMatchInfo(matchInfo, string);
  }
  return result;
}


function RegExpExec(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.exec', this]);
  }

  var cache = regExpCache;
  var saveAnswer = false;

  if (%_ObjectEquals(cache.type, 'exec') &&
      %_ObjectEquals(cache.lastIndex, this.lastIndex) &&
      %_IsRegExpEquivalent(cache.regExp, this) &&
      %_ObjectEquals(cache.subject, string)) {
    if (cache.answerSaved) {
      return %_RegExpCloneResult(cache.answer);
    } else {
      saveAnswer = true;
    }
  }

  if (%_ArgumentsLength() == 0) {
    var regExpInput = LAST_INPUT(lastMatchInfo);
    if (IS_UNDEFINED(regExpInput)) {
      throw MakeError('no_input_to_regexp', [this]);
    }
    string = regExpInput;
  }
  var s;
  if (IS_STRING(string)) {
    s = string;
  } else {
    s = ToString(string);
  }
  var lastIndex = this.lastIndex;

  var i = this.global ? TO_INTEGER(lastIndex) : 0;

  if (i < 0 || i > s.length) {
    this.lastIndex = 0;
    return null;
  }

  %_Log('regexp', 'regexp-exec,%0r,%1S,%2i', [this, s, lastIndex]);
  // matchIndices is either null or the lastMatchInfo array.
  var matchIndices = %_RegExpExec(this, s, i, lastMatchInfo);

  if (matchIndices == null) {
    if (this.global) {
      this.lastIndex = 0;
      if (lastIndex != 0) return matchIndices;
    }
    cache.lastIndex = lastIndex;
    cache.regExp = this;
    cache.subject = s;
    cache.answer = matchIndices;  // Null.
    cache.answerSaved = true;     // Safe since no cloning is needed.
    cache.type = 'exec';
    return matchIndices;        // No match.
  }
  lastMatchInfoOverride = null;
  var result = BuildResultFromMatchInfo(matchIndices, s);

  if (this.global) {
    this.lastIndex = lastMatchInfo[CAPTURE1];
  } else {
    cache.regExp = this;
    cache.subject = s;
    cache.lastIndex = lastIndex;
    if (saveAnswer) cache.answer = %_RegExpCloneResult(result);
    cache.answerSaved = saveAnswer;
    cache.type = 'exec';
  }
  return result;

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
  if (%_ArgumentsLength() == 0) {
    var regExpInput = LAST_INPUT(lastMatchInfo);
    if (IS_UNDEFINED(regExpInput)) {
      throw MakeError('no_input_to_regexp', [this]);
    }
    string = regExpInput;
  }
  var s;
  if (IS_STRING(string)) {
    s = string;
  } else {
    s = ToString(string);
  }

  var lastIndex = this.lastIndex;
  var cache = regExpCache;
  if (%_ObjectEquals(cache.type, 'test') &&
      %_IsRegExpEquivalent(cache.regExp, this) &&
      %_ObjectEquals(cache.subject, string) &&
      %_ObjectEquals(cache.lastIndex, lastIndex)) {
    return cache.answer;
  }

  // Remove irrelevant preceeding '.*' in a test regexp. The expression
  // checks whether this.source starts with '.*' and that the third
  // char is not a '?'
  if (%_StringCharCodeAt(this.source,0) == 46 && // '.'
      %_StringCharCodeAt(this.source,1) == 42 && // '*'
      %_StringCharCodeAt(this.source,2) != 63) { // '?'
    if (!%_ObjectEquals(regexp_key, this)) {
      regexp_key = this;
      regexp_val = new $RegExp(this.source.substring(2, this.source.length),
                               (this.global ? 'g' : '')
                               + (this.ignoreCase ? 'i' : '')
                               + (this.multiline ? 'm' : ''));
    }
    if (!regexp_val.test(s)) return false;
  }

  var length = s.length;
  var i = this.global ? TO_INTEGER(lastIndex) : 0;

  cache.type = 'test';
  cache.regExp = this;
  cache.subject = s;
  cache.lastIndex = i;

  if (i < 0 || i > length) {
    this.lastIndex = 0;
    cache.answer = false;
    return false;
  }

  %_Log('regexp', 'regexp-exec,%0r,%1S,%2i', [this, s, lastIndex]);
  // matchIndices is either null or the lastMatchInfo array.
  var matchIndices = %_RegExpExec(this, s, i, lastMatchInfo);

  if (matchIndices == null) {
    if (this.global) this.lastIndex = 0;
    cache.answer = false;
    return false;
  }
  lastMatchInfoOverride = null;
  if (this.global) this.lastIndex = lastMatchInfo[CAPTURE1];
  cache.answer = true;
  return true;
}


function RegExpToString() {
  // If this.source is an empty string, output /(?:)/.
  // http://bugzilla.mozilla.org/show_bug.cgi?id=225550
  // ecma_2/RegExp/properties-001.js.
  var src = this.source ? this.source : '(?:)';
  var result = '/' + src + '/';
  if (this.global)
    result += 'g';
  if (this.ignoreCase)
    result += 'i';
  if (this.multiline)
    result += 'm';
  return result;
}


// Getters for the static properties lastMatch, lastParen, leftContext, and
// rightContext of the RegExp constructor.  The properties are computed based
// on the captures array of the last successful match and the subject string
// of the last successful match.
function RegExpGetLastMatch() {
  if (lastMatchInfoOverride !== null) {
    return lastMatchInfoOverride[0];
  }
  var regExpSubject = LAST_SUBJECT(lastMatchInfo);
  return SubString(regExpSubject,
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
    return SubString(regExpSubject, start, end);
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
    start_index = override[override.length - 2];
    subject = override[override.length - 1];
  }
  return SubString(subject, 0, start_index);
}


function RegExpGetRightContext() {
  var start_index;
  var subject;
  if (!lastMatchInfoOverride) {
    start_index = lastMatchInfo[CAPTURE1];
    subject = LAST_SUBJECT(lastMatchInfo);
  } else {
    var override = lastMatchInfoOverride;
    subject = override[override.length - 1];
    start_index = override[override.length - 2] + subject.length;
  }
  return SubString(subject, start_index, subject.length);
}


// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
function RegExpMakeCaptureGetter(n) {
  return function() {
    if (lastMatchInfoOverride) {
      if (n < lastMatchInfoOverride.length - 2) return lastMatchInfoOverride[n];
      return '';
    }
    var index = n * 2;
    if (index >= NUMBER_OF_CAPTURES(lastMatchInfo)) return '';
    var matchStart = lastMatchInfo[CAPTURE(index)];
    var matchEnd = lastMatchInfo[CAPTURE(index + 1)];
    if (matchStart == -1 || matchEnd == -1) return '';
    return SubString(LAST_SUBJECT(lastMatchInfo), matchStart, matchEnd);
  };
}


// Property of the builtins object for recording the result of the last
// regexp match.  The property lastMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indeces.  The array also contains
// the subject string for the last successful match.
var lastMatchInfo = [
    2,                 // REGEXP_NUMBER_OF_CAPTURES
    "",                // Last subject.
    void 0,            // Last input - settable with RegExpSetInput.
    0,                 // REGEXP_FIRST_CAPTURE + 0
    0,                 // REGEXP_FIRST_CAPTURE + 1
];

// Override last match info with an array of actual substrings.
// Used internally by replace regexp with function.
// The array has the format of an "apply" argument for a replacement
// function.
var lastMatchInfoOverride = null;

// -------------------------------------------------------------------

function SetupRegExp() {
  %FunctionSetInstanceClassName($RegExp, 'RegExp');
  %FunctionSetPrototype($RegExp, new $Object());
  %SetProperty($RegExp.prototype, 'constructor', $RegExp, DONT_ENUM);
  %SetCode($RegExp, RegExpConstructor);

  InstallFunctions($RegExp.prototype, DONT_ENUM, $Array(
    "exec", RegExpExec,
    "test", RegExpTest,
    "toString", RegExpToString,
    "compile", CompileRegExp
  ));

  // The length of compile is 1 in SpiderMonkey.
  %FunctionSetLength($RegExp.prototype.compile, 1);

  // The properties input, $input, and $_ are aliases for each other.  When this
  // value is set the value it is set to is coerced to a string.
  // Getter and setter for the input.
  function RegExpGetInput() {
    var regExpInput = LAST_INPUT(lastMatchInfo);
    return IS_UNDEFINED(regExpInput) ? "" : regExpInput;
  }
  function RegExpSetInput(string) {
    regExpCache.type = 'none';
    LAST_INPUT(lastMatchInfo) = ToString(string);
  };

  %DefineAccessor($RegExp, 'input', GETTER, RegExpGetInput, DONT_DELETE);
  %DefineAccessor($RegExp, 'input', SETTER, RegExpSetInput, DONT_DELETE);
  %DefineAccessor($RegExp, '$_', GETTER, RegExpGetInput, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$_', SETTER, RegExpSetInput, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$input', GETTER, RegExpGetInput, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$input', SETTER, RegExpSetInput, DONT_ENUM | DONT_DELETE);

  // The properties multiline and $* are aliases for each other.  When this
  // value is set in SpiderMonkey, the value it is set to is coerced to a
  // boolean.  We mimic that behavior with a slight difference: in SpiderMonkey
  // the value of the expression 'RegExp.multiline = null' (for instance) is the
  // boolean false (ie, the value after coercion), while in V8 it is the value
  // null (ie, the value before coercion).

  // Getter and setter for multiline.
  var multiline = false;
  function RegExpGetMultiline() { return multiline; };
  function RegExpSetMultiline(flag) { multiline = flag ? true : false; };

  %DefineAccessor($RegExp, 'multiline', GETTER, RegExpGetMultiline, DONT_DELETE);
  %DefineAccessor($RegExp, 'multiline', SETTER, RegExpSetMultiline, DONT_DELETE);
  %DefineAccessor($RegExp, '$*', GETTER, RegExpGetMultiline, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$*', SETTER, RegExpSetMultiline, DONT_ENUM | DONT_DELETE);


  function NoOpSetter(ignored) {}


  // Static properties set by a successful match.
  %DefineAccessor($RegExp, 'lastMatch', GETTER, RegExpGetLastMatch, DONT_DELETE);
  %DefineAccessor($RegExp, 'lastMatch', SETTER, NoOpSetter, DONT_DELETE);
  %DefineAccessor($RegExp, '$&', GETTER, RegExpGetLastMatch, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$&', SETTER, NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, 'lastParen', GETTER, RegExpGetLastParen, DONT_DELETE);
  %DefineAccessor($RegExp, 'lastParen', SETTER, NoOpSetter, DONT_DELETE);
  %DefineAccessor($RegExp, '$+', GETTER, RegExpGetLastParen, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$+', SETTER, NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, 'leftContext', GETTER, RegExpGetLeftContext, DONT_DELETE);
  %DefineAccessor($RegExp, 'leftContext', SETTER, NoOpSetter, DONT_DELETE);
  %DefineAccessor($RegExp, '$`', GETTER, RegExpGetLeftContext, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, '$`', SETTER, NoOpSetter, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, 'rightContext', GETTER, RegExpGetRightContext, DONT_DELETE);
  %DefineAccessor($RegExp, 'rightContext', SETTER, NoOpSetter, DONT_DELETE);
  %DefineAccessor($RegExp, "$'", GETTER, RegExpGetRightContext, DONT_ENUM | DONT_DELETE);
  %DefineAccessor($RegExp, "$'", SETTER, NoOpSetter, DONT_ENUM | DONT_DELETE);

  for (var i = 1; i < 10; ++i) {
    %DefineAccessor($RegExp, '$' + i, GETTER, RegExpMakeCaptureGetter(i), DONT_DELETE);
    %DefineAccessor($RegExp, '$' + i, SETTER, NoOpSetter, DONT_DELETE);
  }
}


SetupRegExp();
