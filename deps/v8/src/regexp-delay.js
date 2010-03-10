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

  if (isConstructorCall) {
    // ECMA-262, section 15.10.7.1.
    %SetProperty(object, 'source', pattern,
                 DONT_DELETE |  READ_ONLY | DONT_ENUM);

    // ECMA-262, section 15.10.7.2.
    %SetProperty(object, 'global', global, DONT_DELETE | READ_ONLY | DONT_ENUM);

    // ECMA-262, section 15.10.7.3.
    %SetProperty(object, 'ignoreCase', ignoreCase,
                 DONT_DELETE | READ_ONLY | DONT_ENUM);

    // ECMA-262, section 15.10.7.4.
    %SetProperty(object, 'multiline', multiline,
                 DONT_DELETE | READ_ONLY | DONT_ENUM);

    // ECMA-262, section 15.10.7.5.
    %SetProperty(object, 'lastIndex', 0, DONT_DELETE | DONT_ENUM);
  } else { // RegExp is being recompiled via RegExp.prototype.compile.
    %IgnoreAttributesAndSetProperty(object, 'source', pattern);
    %IgnoreAttributesAndSetProperty(object, 'global', global);
    %IgnoreAttributesAndSetProperty(object, 'ignoreCase', ignoreCase);
    %IgnoreAttributesAndSetProperty(object, 'multiline', multiline);
    %IgnoreAttributesAndSetProperty(object, 'lastIndex', 0);
  }

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
  return %_RegExpExec(regexp, string, index, lastMatchInfo);
}


function RegExpExec(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError('incompatible_method_receiver',
                        ['RegExp.prototype.exec', this]);
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
    if (this.global) this.lastIndex = 0;
    return matchIndices; // no match
  }

  var numResults = NUMBER_OF_CAPTURES(lastMatchInfo) >> 1;
  var result;
  if (numResults === 1) {
    var matchStart = lastMatchInfo[CAPTURE(0)];
    var matchEnd = lastMatchInfo[CAPTURE(1)];
    result = [SubString(s, matchStart, matchEnd)];
  } else {
    result = new $Array(numResults);
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

  if (this.global)
    this.lastIndex = lastMatchInfo[CAPTURE1];
  result.index = lastMatchInfo[CAPTURE0];
  result.input = s;
  return result;
}


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
  var s = ToString(string);
  var length = s.length;
  var lastIndex = this.lastIndex;
  var i = this.global ? TO_INTEGER(lastIndex) : 0;

  if (i < 0 || i > s.length) {
    this.lastIndex = 0;
    return false;
  }

  %_Log('regexp', 'regexp-exec,%0r,%1S,%2i', [this, s, lastIndex]);
  // matchIndices is either null or the lastMatchInfo array.
  var matchIndices = %_RegExpExec(this, s, i, lastMatchInfo);

  if (matchIndices == null) {
    if (this.global) this.lastIndex = 0;
    return false;
  }

  if (this.global) this.lastIndex = lastMatchInfo[CAPTURE1];
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
  var regExpSubject = LAST_SUBJECT(lastMatchInfo);
  return SubString(regExpSubject,
                   lastMatchInfo[CAPTURE0],
                   lastMatchInfo[CAPTURE1]);
}


function RegExpGetLastParen() {
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
  return SubString(LAST_SUBJECT(lastMatchInfo),
                   0,
                   lastMatchInfo[CAPTURE0]);
}


function RegExpGetRightContext() {
  var subject = LAST_SUBJECT(lastMatchInfo);
  return SubString(subject,
                   lastMatchInfo[CAPTURE1],
                   subject.length);
}


// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
function RegExpMakeCaptureGetter(n) {
  return function() {
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
