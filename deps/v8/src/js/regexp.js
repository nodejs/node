// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var ExpandReplacement;
var GlobalObject = global.Object;
var GlobalRegExp = global.RegExp;
var GlobalRegExpPrototype;
var InternalArray = utils.InternalArray;
var InternalPackedArray = utils.InternalPackedArray;
var MakeTypeError;
var matchSymbol = utils.ImportNow("match_symbol");
var replaceSymbol = utils.ImportNow("replace_symbol");
var searchSymbol = utils.ImportNow("search_symbol");
var splitSymbol = utils.ImportNow("split_symbol");

utils.Import(function(from) {
  ExpandReplacement = from.ExpandReplacement;
  MakeTypeError = from.MakeTypeError;
});

// -------------------------------------------------------------------

// Property of the builtins object for recording the result of the last
// regexp match.  The property RegExpLastMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indeces.  The array also contains
// the subject string for the last successful match.
var RegExpLastMatchInfo = new InternalPackedArray(
 2,                 // REGEXP_NUMBER_OF_CAPTURES
 "",                // Last subject.
 UNDEFINED,         // Last input - settable with RegExpSetInput.
 0,                 // REGEXP_FIRST_CAPTURE + 0
 0                  // REGEXP_FIRST_CAPTURE + 1
);

// -------------------------------------------------------------------

function IsRegExp(o) {
  if (!IS_RECEIVER(o)) return false;
  var is_regexp = o[matchSymbol];
  if (!IS_UNDEFINED(is_regexp)) return TO_BOOLEAN(is_regexp);
  return IS_REGEXP(o);
}


// ES6 section 21.2.3.2.2
function RegExpInitialize(object, pattern, flags) {
  pattern = IS_UNDEFINED(pattern) ? '' : TO_STRING(pattern);
  flags = IS_UNDEFINED(flags) ? '' : TO_STRING(flags);
  %RegExpInitializeAndCompile(object, pattern, flags);
  return object;
}


function PatternFlags(pattern) {
  return (REGEXP_GLOBAL(pattern) ? 'g' : '') +
         (REGEXP_IGNORE_CASE(pattern) ? 'i' : '') +
         (REGEXP_MULTILINE(pattern) ? 'm' : '') +
         (REGEXP_UNICODE(pattern) ? 'u' : '') +
         (REGEXP_STICKY(pattern) ? 'y' : '');
}


function RegExpConstructor(pattern, flags) {
  var newtarget = new.target;
  var pattern_is_regexp = IsRegExp(pattern);

  if (IS_UNDEFINED(newtarget)) {
    newtarget = GlobalRegExp;

    // ES6 section 21.2.3.1 step 3.b
    if (pattern_is_regexp && IS_UNDEFINED(flags) &&
        pattern.constructor === newtarget) {
      return pattern;
    }
  }

  if (IS_REGEXP(pattern)) {
    if (IS_UNDEFINED(flags)) flags = PatternFlags(pattern);
    pattern = REGEXP_SOURCE(pattern);

  } else if (pattern_is_regexp) {
    var input_pattern = pattern;
    pattern = pattern.source;
    if (IS_UNDEFINED(flags)) flags = input_pattern.flags;
  }

  var object = %NewObject(GlobalRegExp, newtarget);
  return RegExpInitialize(object, pattern, flags);
}


function RegExpCompileJS(pattern, flags) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "RegExp.prototype.compile", this);
  }

  if (IS_REGEXP(pattern)) {
    if (!IS_UNDEFINED(flags)) throw MakeTypeError(kRegExpFlags);

    flags = PatternFlags(pattern);
    pattern = REGEXP_SOURCE(pattern);
  }

  RegExpInitialize(this, pattern, flags);

  // Return undefined for compatibility with JSC.
  // See http://crbug.com/585775 for web compat details.
}


function DoRegExpExec(regexp, string, index) {
  return %_RegExpExec(regexp, string, index, RegExpLastMatchInfo);
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
  var matchInfo = %_RegExpExec(regexp, string, start, RegExpLastMatchInfo);
  if (matchInfo !== null) {
    // ES6 21.2.5.2.2 step 18.
    if (REGEXP_STICKY(regexp)) regexp.lastIndex = matchInfo[CAPTURE1];
    RETURN_NEW_RESULT_FROM_MATCH_INFO(matchInfo, string);
  }
  regexp.lastIndex = 0;
  return null;
}


function RegExpExecJS(string) {
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.exec', this);
  }

  string = TO_STRING(string);
  var lastIndex = this.lastIndex;

  // Conversion is required by the ES2015 specification (RegExpBuiltinExec
  // algorithm, step 4) even if the value is discarded for non-global RegExps.
  var i = TO_LENGTH(lastIndex);

  var updateLastIndex = REGEXP_GLOBAL(this) || REGEXP_STICKY(this);
  if (updateLastIndex) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return null;
    }
  } else {
    i = 0;
  }

  // matchIndices is either null or the RegExpLastMatchInfo array.
  var matchIndices = %_RegExpExec(this, string, i, RegExpLastMatchInfo);

  if (IS_NULL(matchIndices)) {
    this.lastIndex = 0;
    return null;
  }

  // Successful match.
  if (updateLastIndex) {
    this.lastIndex = RegExpLastMatchInfo[CAPTURE1];
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
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        'RegExp.prototype.test', this);
  }
  string = TO_STRING(string);

  var lastIndex = this.lastIndex;

  // Conversion is required by the ES2015 specification (RegExpBuiltinExec
  // algorithm, step 4) even if the value is discarded for non-global RegExps.
  var i = TO_LENGTH(lastIndex);

  if (REGEXP_GLOBAL(this) || REGEXP_STICKY(this)) {
    if (i < 0 || i > string.length) {
      this.lastIndex = 0;
      return false;
    }
    // matchIndices is either null or the RegExpLastMatchInfo array.
    var matchIndices = %_RegExpExec(this, string, i, RegExpLastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    this.lastIndex = RegExpLastMatchInfo[CAPTURE1];
    return true;
  } else {
    // Non-global, non-sticky regexp.
    // Remove irrelevant preceeding '.*' in a test regexp.  The expression
    // checks whether this.source starts with '.*' and that the third char is
    // not a '?'.  But see https://code.google.com/p/v8/issues/detail?id=3560
    var regexp = this;
    var source = REGEXP_SOURCE(regexp);
    if (regexp.length >= 3 &&
        %_StringCharCodeAt(regexp, 0) == 46 &&  // '.'
        %_StringCharCodeAt(regexp, 1) == 42 &&  // '*'
        %_StringCharCodeAt(regexp, 2) != 63) {  // '?'
      regexp = TrimRegExp(regexp);
    }
    // matchIndices is either null or the RegExpLastMatchInfo array.
    var matchIndices = %_RegExpExec(regexp, string, 0, RegExpLastMatchInfo);
    if (IS_NULL(matchIndices)) {
      this.lastIndex = 0;
      return false;
    }
    return true;
  }
}

function TrimRegExp(regexp) {
  if (regexp_key !== regexp) {
    regexp_key = regexp;
    regexp_val =
      new GlobalRegExp(
          %_SubString(REGEXP_SOURCE(regexp), 2, REGEXP_SOURCE(regexp).length),
          (REGEXP_IGNORE_CASE(regexp) ? REGEXP_MULTILINE(regexp) ? "im" : "i"
                                      : REGEXP_MULTILINE(regexp) ? "m" : ""));
  }
  return regexp_val;
}


function RegExpToString() {
  if (!IS_REGEXP(this)) {
    // RegExp.prototype.toString() returns '/(?:)/' as a compatibility fix;
    // a UseCounter is incremented to track it.
    // TODO(littledan): Remove this workaround or standardize it
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeToString);
      return '/(?:)/';
    }
    if (!IS_RECEIVER(this)) {
      throw MakeTypeError(
          kIncompatibleMethodReceiver, 'RegExp.prototype.toString', this);
    }
    return '/' + TO_STRING(this.source) + '/' + TO_STRING(this.flags);
  }
  var result = '/' + REGEXP_SOURCE(this) + '/';
  if (REGEXP_GLOBAL(this)) result += 'g';
  if (REGEXP_IGNORE_CASE(this)) result += 'i';
  if (REGEXP_MULTILINE(this)) result += 'm';
  if (REGEXP_UNICODE(this)) result += 'u';
  if (REGEXP_STICKY(this)) result += 'y';
  return result;
}


function AtSurrogatePair(subject, index) {
  if (index + 1 >= subject.length) return false;
  var first = %_StringCharCodeAt(subject, index);
  if (first < 0xD800 || first > 0xDBFF) return false;
  var second = %_StringCharCodeAt(subject, index + 1);
  return second >= 0xDC00 || second <= 0xDFFF;
}


// ES6 21.2.5.11.
function RegExpSplit(string, limit) {
  // TODO(yangguo): allow non-regexp receivers.
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@split", this);
  }
  var separator = this;
  var subject = TO_STRING(string);

  limit = (IS_UNDEFINED(limit)) ? kMaxUint32 : TO_UINT32(limit);
  var length = subject.length;

  if (limit === 0) return [];

  if (length === 0) {
    if (DoRegExpExec(separator, subject, 0, 0) !== null) return [];
    return [subject];
  }

  var currentIndex = 0;
  var startIndex = 0;
  var startMatch = 0;
  var result = new InternalArray();

  outer_loop:
  while (true) {
    if (startIndex === length) {
      result[result.length] = %_SubString(subject, currentIndex, length);
      break;
    }

    var matchInfo = DoRegExpExec(separator, subject, startIndex);
    if (matchInfo === null || length === (startMatch = matchInfo[CAPTURE0])) {
      result[result.length] = %_SubString(subject, currentIndex, length);
      break;
    }
    var endIndex = matchInfo[CAPTURE1];

    // We ignore a zero-length match at the currentIndex.
    if (startIndex === endIndex && endIndex === currentIndex) {
      if (REGEXP_UNICODE(this) && AtSurrogatePair(subject, startIndex)) {
        startIndex += 2;
      } else {
        startIndex++;
      }
      continue;
    }

    result[result.length] = %_SubString(subject, currentIndex, startMatch);

    if (result.length === limit) break;

    var matchinfo_len = NUMBER_OF_CAPTURES(matchInfo) + REGEXP_FIRST_CAPTURE;
    for (var i = REGEXP_FIRST_CAPTURE + 2; i < matchinfo_len; ) {
      var start = matchInfo[i++];
      var end = matchInfo[i++];
      if (end != -1) {
        result[result.length] = %_SubString(subject, start, end);
      } else {
        result[result.length] = UNDEFINED;
      }
      if (result.length === limit) break outer_loop;
    }

    startIndex = currentIndex = endIndex;
  }

  var array_result = [];
  %MoveArrayContents(result, array_result);
  return array_result;
}


// ES6 21.2.5.6.
function RegExpMatch(string) {
  // TODO(yangguo): allow non-regexp receivers.
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@match", this);
  }
  var subject = TO_STRING(string);

  if (!REGEXP_GLOBAL(this)) return RegExpExecNoTests(this, subject, 0);
  this.lastIndex = 0;
  var result = %StringMatch(subject, this, RegExpLastMatchInfo);
  return result;
}


// ES6 21.2.5.8.

// TODO(lrn): This array will survive indefinitely if replace is never
// called again. However, it will be empty, since the contents are cleared
// in the finally block.
var reusableReplaceArray = new InternalArray(4);

// Helper function for replacing regular expressions with the result of a
// function application in String.prototype.replace.
function StringReplaceGlobalRegExpWithFunction(subject, regexp, replace) {
  var resultArray = reusableReplaceArray;
  if (resultArray) {
    reusableReplaceArray = null;
  } else {
    // Inside a nested replace (replace called from the replacement function
    // of another replace) or we have failed to set the reusable array
    // back due to an exception in a replacement function. Create a new
    // array to use in the future, or until the original is written back.
    resultArray = new InternalArray(16);
  }
  var res = %RegExpExecMultiple(regexp,
                                subject,
                                RegExpLastMatchInfo,
                                resultArray);
  regexp.lastIndex = 0;
  if (IS_NULL(res)) {
    // No matches at all.
    reusableReplaceArray = resultArray;
    return subject;
  }
  var len = res.length;
  if (NUMBER_OF_CAPTURES(RegExpLastMatchInfo) == 2) {
    // If the number of captures is two then there are no explicit captures in
    // the regexp, just the implicit capture that captures the whole match.  In
    // this case we can simplify quite a bit and end up with something faster.
    // The builder will consist of some integers that indicate slices of the
    // input string and some replacements that were returned from the replace
    // function.
    var match_start = 0;
    for (var i = 0; i < len; i++) {
      var elem = res[i];
      if (%_IsSmi(elem)) {
        // Integers represent slices of the original string.
        if (elem > 0) {
          match_start = (elem >> 11) + (elem & 0x7ff);
        } else {
          match_start = res[++i] - elem;
        }
      } else {
        var func_result = replace(elem, match_start, subject);
        // Overwrite the i'th element in the results with the string we got
        // back from the callback function.
        res[i] = TO_STRING(func_result);
        match_start += elem.length;
      }
    }
  } else {
    for (var i = 0; i < len; i++) {
      var elem = res[i];
      if (!%_IsSmi(elem)) {
        // elem must be an Array.
        // Use the apply argument as backing for global RegExp properties.
        var func_result = %Apply(replace, UNDEFINED, elem, 0, elem.length);
        // Overwrite the i'th element in the results with the string we got
        // back from the callback function.
        res[i] = TO_STRING(func_result);
      }
    }
  }
  var result = %StringBuilderConcat(res, len, subject);
  resultArray.length = 0;
  reusableReplaceArray = resultArray;
  return result;
}


// Compute the string of a given regular expression capture.
function CaptureString(string, lastCaptureInfo, index) {
  // Scale the index.
  var scaled = index << 1;
  // Compute start and end.
  var start = lastCaptureInfo[CAPTURE(scaled)];
  // If start isn't valid, return undefined.
  if (start < 0) return;
  var end = lastCaptureInfo[CAPTURE(scaled + 1)];
  return %_SubString(string, start, end);
}


function StringReplaceNonGlobalRegExpWithFunction(subject, regexp, replace) {
  var matchInfo = DoRegExpExec(regexp, subject, 0);
  if (IS_NULL(matchInfo)) {
    regexp.lastIndex = 0;
    return subject;
  }
  var index = matchInfo[CAPTURE0];
  var result = %_SubString(subject, 0, index);
  var endOfMatch = matchInfo[CAPTURE1];
  // Compute the parameter list consisting of the match, captures, index,
  // and subject for the replace function invocation.
  // The number of captures plus one for the match.
  var m = NUMBER_OF_CAPTURES(matchInfo) >> 1;
  var replacement;
  if (m == 1) {
    // No captures, only the match, which is always valid.
    var s = %_SubString(subject, index, endOfMatch);
    // Don't call directly to avoid exposing the built-in global object.
    replacement = replace(s, index, subject);
  } else {
    var parameters = new InternalArray(m + 2);
    for (var j = 0; j < m; j++) {
      parameters[j] = CaptureString(subject, matchInfo, j);
    }
    parameters[j] = index;
    parameters[j + 1] = subject;

    replacement = %Apply(replace, UNDEFINED, parameters, 0, j + 2);
  }

  result += replacement;  // The add method converts to string if necessary.
  // Can't use matchInfo any more from here, since the function could
  // overwrite it.
  return result + %_SubString(subject, endOfMatch, subject.length);
}


function RegExpReplace(string, replace) {
  // TODO(littledan): allow non-regexp receivers.
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@replace", this);
  }
  var subject = TO_STRING(string);
  var search = this;

  if (!IS_CALLABLE(replace)) {
    replace = TO_STRING(replace);

    if (!REGEXP_GLOBAL(search)) {
      // Non-global regexp search, string replace.
      var match = DoRegExpExec(search, subject, 0);
      if (match == null) {
        search.lastIndex = 0
        return subject;
      }
      if (replace.length == 0) {
        return %_SubString(subject, 0, match[CAPTURE0]) +
               %_SubString(subject, match[CAPTURE1], subject.length)
      }
      return ExpandReplacement(replace, subject, RegExpLastMatchInfo,
                                 %_SubString(subject, 0, match[CAPTURE0])) +
             %_SubString(subject, match[CAPTURE1], subject.length);
    }

    // Global regexp search, string replace.
    search.lastIndex = 0;
    return %StringReplaceGlobalRegExpWithString(
        subject, search, replace, RegExpLastMatchInfo);
  }

  if (REGEXP_GLOBAL(search)) {
    // Global regexp search, function replace.
    return StringReplaceGlobalRegExpWithFunction(subject, search, replace);
  }
  // Non-global regexp search, function replace.
  return StringReplaceNonGlobalRegExpWithFunction(subject, search, replace);
}


// ES6 21.2.5.9.
function RegExpSearch(string) {
  // TODO(yangguo): allow non-regexp receivers.
  if (!IS_REGEXP(this)) {
    throw MakeTypeError(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@search", this);
  }
  var match = DoRegExpExec(this, TO_STRING(string), 0);
  if (match) return match[CAPTURE0];
  return -1;
}


// Getters for the static properties lastMatch, lastParen, leftContext, and
// rightContext of the RegExp constructor.  The properties are computed based
// on the captures array of the last successful match and the subject string
// of the last successful match.
function RegExpGetLastMatch() {
  var regExpSubject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(regExpSubject,
                     RegExpLastMatchInfo[CAPTURE0],
                     RegExpLastMatchInfo[CAPTURE1]);
}


function RegExpGetLastParen() {
  var length = NUMBER_OF_CAPTURES(RegExpLastMatchInfo);
  if (length <= 2) return '';  // There were no captures.
  // We match the SpiderMonkey behavior: return the substring defined by the
  // last pair (after the first pair) of elements of the capture array even if
  // it is empty.
  var regExpSubject = LAST_SUBJECT(RegExpLastMatchInfo);
  var start = RegExpLastMatchInfo[CAPTURE(length - 2)];
  var end = RegExpLastMatchInfo[CAPTURE(length - 1)];
  if (start != -1 && end != -1) {
    return %_SubString(regExpSubject, start, end);
  }
  return "";
}


function RegExpGetLeftContext() {
  var start_index;
  var subject;
  start_index = RegExpLastMatchInfo[CAPTURE0];
  subject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(subject, 0, start_index);
}


function RegExpGetRightContext() {
  var start_index;
  var subject;
  start_index = RegExpLastMatchInfo[CAPTURE1];
  subject = LAST_SUBJECT(RegExpLastMatchInfo);
  return %_SubString(subject, start_index, subject.length);
}


// The properties $1..$9 are the first nine capturing substrings of the last
// successful match, or ''.  The function RegExpMakeCaptureGetter will be
// called with indices from 1 to 9.
function RegExpMakeCaptureGetter(n) {
  return function foo() {
    var index = n * 2;
    if (index >= NUMBER_OF_CAPTURES(RegExpLastMatchInfo)) return '';
    var matchStart = RegExpLastMatchInfo[CAPTURE(index)];
    var matchEnd = RegExpLastMatchInfo[CAPTURE(index + 1)];
    if (matchStart == -1 || matchEnd == -1) return '';
    return %_SubString(LAST_SUBJECT(RegExpLastMatchInfo), matchStart, matchEnd);
  };
}


// ES6 21.2.5.4.
function RegExpGetGlobal() {
  if (!IS_REGEXP(this)) {
    // TODO(littledan): Remove this RegExp compat workaround
    if (this === GlobalRegExpPrototype) {
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.global");
  }
  return !!REGEXP_GLOBAL(this);
}
%FunctionSetName(RegExpGetGlobal, "RegExp.prototype.global");
%SetNativeFlag(RegExpGetGlobal);


// ES6 21.2.5.5.
function RegExpGetIgnoreCase() {
  if (!IS_REGEXP(this)) {
    // TODO(littledan): Remove this RegExp compat workaround
    if (this === GlobalRegExpPrototype) {
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.ignoreCase");
  }
  return !!REGEXP_IGNORE_CASE(this);
}
%FunctionSetName(RegExpGetIgnoreCase, "RegExp.prototype.ignoreCase");
%SetNativeFlag(RegExpGetIgnoreCase);


// ES6 21.2.5.7.
function RegExpGetMultiline() {
  if (!IS_REGEXP(this)) {
    // TODO(littledan): Remove this RegExp compat workaround
    if (this === GlobalRegExpPrototype) {
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.multiline");
  }
  return !!REGEXP_MULTILINE(this);
}
%FunctionSetName(RegExpGetMultiline, "RegExp.prototype.multiline");
%SetNativeFlag(RegExpGetMultiline);


// ES6 21.2.5.10.
function RegExpGetSource() {
  if (!IS_REGEXP(this)) {
    // TODO(littledan): Remove this RegExp compat workaround
    if (this === GlobalRegExpPrototype) {
      return UNDEFINED;
    }
    throw MakeTypeError(kRegExpNonRegExp, "RegExp.prototype.source");
  }
  return REGEXP_SOURCE(this);
}
%FunctionSetName(RegExpGetSource, "RegExp.prototype.source");
%SetNativeFlag(RegExpGetSource);

// -------------------------------------------------------------------

%FunctionSetInstanceClassName(GlobalRegExp, 'RegExp');
GlobalRegExpPrototype = new GlobalObject();
%FunctionSetPrototype(GlobalRegExp, GlobalRegExpPrototype);
%AddNamedProperty(
    GlobalRegExp.prototype, 'constructor', GlobalRegExp, DONT_ENUM);
%SetCode(GlobalRegExp, RegExpConstructor);

utils.InstallFunctions(GlobalRegExp.prototype, DONT_ENUM, [
  "exec", RegExpExecJS,
  "test", RegExpTest,
  "toString", RegExpToString,
  "compile", RegExpCompileJS,
  matchSymbol, RegExpMatch,
  replaceSymbol, RegExpReplace,
  searchSymbol, RegExpSearch,
  splitSymbol, RegExpSplit,
]);

utils.InstallGetter(GlobalRegExp.prototype, 'global', RegExpGetGlobal);
utils.InstallGetter(GlobalRegExp.prototype, 'ignoreCase', RegExpGetIgnoreCase);
utils.InstallGetter(GlobalRegExp.prototype, 'multiline', RegExpGetMultiline);
utils.InstallGetter(GlobalRegExp.prototype, 'source', RegExpGetSource);

// The properties `input` and `$_` are aliases for each other.  When this
// value is set the value it is set to is coerced to a string.
// Getter and setter for the input.
var RegExpGetInput = function() {
  var regExpInput = LAST_INPUT(RegExpLastMatchInfo);
  return IS_UNDEFINED(regExpInput) ? "" : regExpInput;
};
var RegExpSetInput = function(string) {
  LAST_INPUT(RegExpLastMatchInfo) = TO_STRING(string);
};

%OptimizeObjectForAddingMultipleProperties(GlobalRegExp, 22);
utils.InstallGetterSetter(GlobalRegExp, 'input', RegExpGetInput, RegExpSetInput,
                          DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, '$_', RegExpGetInput, RegExpSetInput,
                          DONT_ENUM | DONT_DELETE);


var NoOpSetter = function(ignored) {};


// Static properties set by a successful match.
utils.InstallGetterSetter(GlobalRegExp, 'lastMatch', RegExpGetLastMatch,
                          NoOpSetter, DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, '$&', RegExpGetLastMatch, NoOpSetter,
                          DONT_ENUM | DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, 'lastParen', RegExpGetLastParen,
                          NoOpSetter, DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, '$+', RegExpGetLastParen, NoOpSetter,
                          DONT_ENUM | DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, 'leftContext', RegExpGetLeftContext,
                          NoOpSetter, DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, '$`', RegExpGetLeftContext, NoOpSetter,
                          DONT_ENUM | DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, 'rightContext', RegExpGetRightContext,
                          NoOpSetter, DONT_DELETE);
utils.InstallGetterSetter(GlobalRegExp, "$'", RegExpGetRightContext, NoOpSetter,
                          DONT_ENUM | DONT_DELETE);

for (var i = 1; i < 10; ++i) {
  utils.InstallGetterSetter(GlobalRegExp, '$' + i, RegExpMakeCaptureGetter(i),
                            NoOpSetter, DONT_DELETE);
}
%ToFastProperties(GlobalRegExp);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.RegExpExec = DoRegExpExec;
  to.RegExpExecNoTests = RegExpExecNoTests;
  to.RegExpLastMatchInfo = RegExpLastMatchInfo;
  to.RegExpTest = RegExpTest;
});

})
