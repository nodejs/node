// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

'use strict';

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var GlobalArray = global.Array;
var GlobalObject = global.Object;
var GlobalRegExp = global.RegExp;
var GlobalRegExpPrototype = GlobalRegExp.prototype;
var InternalArray = utils.InternalArray;
var InternalPackedArray = utils.InternalPackedArray;
var MaxSimple;
var MinSimple;
var RegExpExecJS = GlobalRegExp.prototype.exec;
var matchSymbol = utils.ImportNow("match_symbol");
var replaceSymbol = utils.ImportNow("replace_symbol");
var searchSymbol = utils.ImportNow("search_symbol");
var speciesSymbol = utils.ImportNow("species_symbol");
var splitSymbol = utils.ImportNow("split_symbol");
var SpeciesConstructor;

utils.Import(function(from) {
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  SpeciesConstructor = from.SpeciesConstructor;
});

// -------------------------------------------------------------------

// Property of the builtins object for recording the result of the last
// regexp match.  The property RegExpLastMatchInfo includes the matchIndices
// array of the last successful regexp match (an array of start/end index
// pairs for the match and all the captured substrings), the invariant is
// that there are at least two capture indices.  The array also contains
// the subject string for the last successful match.
// We use a JSObject rather than a JSArray so we don't have to manually update
// its length.
var RegExpLastMatchInfo = {
  REGEXP_NUMBER_OF_CAPTURES: 2,
  REGEXP_LAST_SUBJECT:       "",
  REGEXP_LAST_INPUT:         UNDEFINED,  // Settable with RegExpSetInput.
  CAPTURE0:                  0,
  CAPTURE1:                  0
};

// -------------------------------------------------------------------

// ES#sec-isregexp IsRegExp ( argument )
function IsRegExp(o) {
  if (!IS_RECEIVER(o)) return false;
  var is_regexp = o[matchSymbol];
  if (!IS_UNDEFINED(is_regexp)) return TO_BOOLEAN(is_regexp);
  return IS_REGEXP(o);
}


// ES#sec-regexpinitialize
// Runtime Semantics: RegExpInitialize ( obj, pattern, flags )
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


// ES#sec-regexp.prototype.compile RegExp.prototype.compile (pattern, flags)
function RegExpCompileJS(pattern, flags) {
  if (!IS_REGEXP(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        "RegExp.prototype.compile", this);
  }

  if (IS_REGEXP(pattern)) {
    if (!IS_UNDEFINED(flags)) throw %make_type_error(kRegExpFlags);

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



// ES#sec-regexpexec Runtime Semantics: RegExpExec ( R, S )
// Also takes an optional exec method in case our caller
// has already fetched exec.
function RegExpSubclassExec(regexp, string, exec) {
  if (IS_UNDEFINED(exec)) {
    exec = regexp.exec;
  }
  if (IS_CALLABLE(exec)) {
    var result = %_Call(exec, regexp, string);
    if (!IS_RECEIVER(result) && !IS_NULL(result)) {
      throw %make_type_error(kInvalidRegExpExecResult);
    }
    return result;
  }
  return %_Call(RegExpExecJS, regexp, string);
}
%SetForceInlineFlag(RegExpSubclassExec);


// ES#sec-regexp.prototype.test RegExp.prototype.test ( S )
function RegExpSubclassTest(string) {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        'RegExp.prototype.test', this);
  }
  string = TO_STRING(string);
  var match = RegExpSubclassExec(this, string);
  return !IS_NULL(match);
}
%FunctionRemovePrototype(RegExpSubclassTest);


function RegExpToString() {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(
        kIncompatibleMethodReceiver, 'RegExp.prototype.toString', this);
  }
  if (this === GlobalRegExpPrototype) {
    %IncrementUseCounter(kRegExpPrototypeToString);
  }
  return '/' + TO_STRING(this.source) + '/' + TO_STRING(this.flags);
}


function AtSurrogatePair(subject, index) {
  if (index + 1 >= subject.length) return false;
  var first = %_StringCharCodeAt(subject, index);
  if (first < 0xD800 || first > 0xDBFF) return false;
  var second = %_StringCharCodeAt(subject, index + 1);
  return second >= 0xDC00 && second <= 0xDFFF;
}


// Fast path implementation of RegExp.prototype[Symbol.split] which
// doesn't properly call the underlying exec, @@species methods
function RegExpSplit(string, limit) {
  if (!IS_REGEXP(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
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


// ES#sec-regexp.prototype-@@split
// RegExp.prototype [ @@split ] ( string, limit )
function RegExpSubclassSplit(string, limit) {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@split", this);
  }
  string = TO_STRING(string);
  var constructor = SpeciesConstructor(this, GlobalRegExp);
  var flags = TO_STRING(this.flags);

  // TODO(adamk): this fast path is wrong as we doesn't ensure that 'exec'
  // is actually a data property on RegExp.prototype.
  if (IS_REGEXP(this) && constructor === GlobalRegExp) {
    var exec = this.exec;
    if (exec === RegExpExecJS) {
      return %_Call(RegExpSplit, this, string, limit);
    }
  }

  var unicode = %StringIndexOf(flags, 'u', 0) >= 0;
  var sticky = %StringIndexOf(flags, 'y', 0) >= 0;
  var newFlags = sticky ? flags : flags + "y";
  var splitter = new constructor(this, newFlags);
  var array = new GlobalArray();
  var arrayIndex = 0;
  var lim = (IS_UNDEFINED(limit)) ? kMaxUint32 : TO_UINT32(limit);
  var size = string.length;
  var prevStringIndex = 0;
  if (lim === 0) return array;
  var result;
  if (size === 0) {
    result = RegExpSubclassExec(splitter, string);
    if (IS_NULL(result)) %AddElement(array, 0, string);
    return array;
  }
  var stringIndex = prevStringIndex;
  while (stringIndex < size) {
    splitter.lastIndex = stringIndex;
    result = RegExpSubclassExec(splitter, string);
    if (IS_NULL(result)) {
      stringIndex += AdvanceStringIndex(string, stringIndex, unicode);
    } else {
      var end = MinSimple(TO_LENGTH(splitter.lastIndex), size);
      if (end === prevStringIndex) {
        stringIndex += AdvanceStringIndex(string, stringIndex, unicode);
      } else {
        %AddElement(
            array, arrayIndex,
            %_SubString(string, prevStringIndex, stringIndex));
        arrayIndex++;
        if (arrayIndex === lim) return array;
        prevStringIndex = end;
        var numberOfCaptures = MaxSimple(TO_LENGTH(result.length), 0);
        for (var i = 1; i < numberOfCaptures; i++) {
          %AddElement(array, arrayIndex, result[i]);
          arrayIndex++;
          if (arrayIndex === lim) return array;
        }
        stringIndex = prevStringIndex;
      }
    }
  }
  %AddElement(array, arrayIndex,
                     %_SubString(string, prevStringIndex, size));
  return array;
}
%FunctionRemovePrototype(RegExpSubclassSplit);


// ES#sec-regexp.prototype-@@match
// RegExp.prototype [ @@match ] ( string )
function RegExpSubclassMatch(string) {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@match", this);
  }
  string = TO_STRING(string);
  var global = this.global;
  if (!global) return RegExpSubclassExec(this, string);
  var unicode = this.unicode;
  this.lastIndex = 0;
  var array = new InternalArray();
  var n = 0;
  var result;
  while (true) {
    result = RegExpSubclassExec(this, string);
    if (IS_NULL(result)) {
      if (n === 0) return null;
      break;
    }
    var matchStr = TO_STRING(result[0]);
    array[n] = matchStr;
    if (matchStr === "") SetAdvancedStringIndex(this, string, unicode);
    n++;
  }
  var resultArray = [];
  %MoveArrayContents(array, resultArray);
  return resultArray;
}
%FunctionRemovePrototype(RegExpSubclassMatch);


// Legacy implementation of RegExp.prototype[Symbol.replace] which
// doesn't properly call the underlying exec method.

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
        var func_result = %reflect_apply(replace, UNDEFINED, elem);
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

    replacement = %reflect_apply(replace, UNDEFINED, parameters);
  }

  result += replacement;  // The add method converts to string if necessary.
  // Can't use matchInfo any more from here, since the function could
  // overwrite it.
  return result + %_SubString(subject, endOfMatch, subject.length);
}

// Wraps access to matchInfo's captures into a format understood by
// GetSubstitution.
function MatchInfoCaptureWrapper(matches, subject) {
  this.length = NUMBER_OF_CAPTURES(matches) >> 1;
  this.match = matches;
  this.subject = subject;
}

MatchInfoCaptureWrapper.prototype.at = function(ix) {
  const match = this.match;
  const start = match[CAPTURE(ix << 1)];
  if (start < 0) return UNDEFINED;
  return %_SubString(this.subject, start, match[CAPTURE((ix << 1) + 1)]);
};
%SetForceInlineFlag(MatchInfoCaptureWrapper.prototype.at);

function ArrayCaptureWrapper(array) {
  this.length = array.length;
  this.array = array;
}

ArrayCaptureWrapper.prototype.at = function(ix) {
  return this.array[ix];
};
%SetForceInlineFlag(ArrayCaptureWrapper.prototype.at);

function RegExpReplace(string, replace) {
  if (!IS_REGEXP(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
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
      const captures = new MatchInfoCaptureWrapper(match, subject);
      const start = match[CAPTURE0];
      const end = match[CAPTURE1];

      const prefix = %_SubString(subject, 0, start);
      const matched = %_SubString(subject, start, end);
      const suffix = %_SubString(subject, end, subject.length);

      return prefix +
             GetSubstitution(matched, subject, start, captures, replace) +
             suffix;
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


// ES#sec-getsubstitution
// GetSubstitution(matched, str, position, captures, replacement)
// Expand the $-expressions in the string and return a new string with
// the result.
function GetSubstitution(matched, string, position, captures, replacement) {
  var matchLength = matched.length;
  var stringLength = string.length;
  var capturesLength = captures.length;
  var tailPos = position + matchLength;
  var result = "";
  var pos, expansion, peek, next, scaledIndex, advance, newScaledIndex;

  var next = %StringIndexOf(replacement, '$', 0);
  if (next < 0) {
    result += replacement;
    return result;
  }

  if (next > 0) result += %_SubString(replacement, 0, next);

  while (true) {
    expansion = '$';
    pos = next + 1;
    if (pos < replacement.length) {
      peek = %_StringCharCodeAt(replacement, pos);
      if (peek == 36) {         // $$
        ++pos;
        result += '$';
      } else if (peek == 38) {  // $& - match
        ++pos;
        result += matched;
      } else if (peek == 96) {  // $` - prefix
        ++pos;
        result += %_SubString(string, 0, position);
      } else if (peek == 39) {  // $' - suffix
        ++pos;
        result += %_SubString(string, tailPos, stringLength);
      } else if (peek >= 48 && peek <= 57) {
        // Valid indices are $1 .. $9, $01 .. $09 and $10 .. $99
        scaledIndex = (peek - 48);
        advance = 1;
        if (pos + 1 < replacement.length) {
          next = %_StringCharCodeAt(replacement, pos + 1);
          if (next >= 48 && next <= 57) {
            newScaledIndex = scaledIndex * 10 + ((next - 48));
            if (newScaledIndex < capturesLength) {
              scaledIndex = newScaledIndex;
              advance = 2;
            }
          }
        }
        if (scaledIndex != 0 && scaledIndex < capturesLength) {
          var capture = captures.at(scaledIndex);
          if (!IS_UNDEFINED(capture)) result += capture;
          pos += advance;
        } else {
          result += '$';
        }
      } else {
        result += '$';
      }
    } else {
      result += '$';
    }

    // Go the the next $ in the replacement.
    next = %StringIndexOf(replacement, '$', pos);

    // Return if there are no more $ characters in the replacement. If we
    // haven't reached the end, we need to append the suffix.
    if (next < 0) {
      if (pos < replacement.length) {
        result += %_SubString(replacement, pos, replacement.length);
      }
      return result;
    }

    // Append substring between the previous and the next $ character.
    if (next > pos) {
      result += %_SubString(replacement, pos, next);
    }
  }
  return result;
}


// ES#sec-advancestringindex
// AdvanceStringIndex ( S, index, unicode )
function AdvanceStringIndex(string, index, unicode) {
  var increment = 1;
  if (unicode) {
    var first = %_StringCharCodeAt(string, index);
    if (first >= 0xD800 && first <= 0xDBFF && string.length > index + 1) {
      var second = %_StringCharCodeAt(string, index + 1);
      if (second >= 0xDC00 && second <= 0xDFFF) {
        increment = 2;
      }
    }
  }
  return increment;
}


function SetAdvancedStringIndex(regexp, string, unicode) {
  var lastIndex = regexp.lastIndex;
  regexp.lastIndex = lastIndex +
                     AdvanceStringIndex(string, lastIndex, unicode);
}


// ES#sec-regexp.prototype-@@replace
// RegExp.prototype [ @@replace ] ( string, replaceValue )
function RegExpSubclassReplace(string, replace) {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@replace", this);
  }
  string = TO_STRING(string);
  var length = string.length;
  var functionalReplace = IS_CALLABLE(replace);
  if (!functionalReplace) replace = TO_STRING(replace);
  var global = TO_BOOLEAN(this.global);
  if (global) {
    var unicode = TO_BOOLEAN(this.unicode);
    this.lastIndex = 0;
  }

  // TODO(adamk): this fast path is wrong as we doesn't ensure that 'exec'
  // is actually a data property on RegExp.prototype.
  var exec;
  if (IS_REGEXP(this)) {
    exec = this.exec;
    if (exec === RegExpExecJS) {
      return %_Call(RegExpReplace, this, string, replace);
    }
  }

  var results = new InternalArray();
  var result, replacement;
  while (true) {
    result = RegExpSubclassExec(this, string, exec);
    // Ensure exec will be read again on the next loop through.
    exec = UNDEFINED;
    if (IS_NULL(result)) {
      break;
    } else {
      results.push(result);
      if (!global) break;
      var matchStr = TO_STRING(result[0]);
      if (matchStr === "") SetAdvancedStringIndex(this, string, unicode);
    }
  }
  var accumulatedResult = "";
  var nextSourcePosition = 0;
  for (var i = 0; i < results.length; i++) {
    result = results[i];
    var capturesLength = MaxSimple(TO_LENGTH(result.length), 0);
    var matched = TO_STRING(result[0]);
    var matchedLength = matched.length;
    var position = MaxSimple(MinSimple(TO_INTEGER(result.index), length), 0);
    var captures = new InternalArray();
    for (var n = 0; n < capturesLength; n++) {
      var capture = result[n];
      if (!IS_UNDEFINED(capture)) capture = TO_STRING(capture);
      captures[n] = capture;
    }
    if (functionalReplace) {
      var parameters = new InternalArray(capturesLength + 2);
      for (var j = 0; j < capturesLength; j++) {
        parameters[j] = captures[j];
      }
      parameters[j] = position;
      parameters[j + 1] = string;
      replacement = %reflect_apply(replace, UNDEFINED, parameters, 0,
                                   parameters.length);
    } else {
      const capturesWrapper = new ArrayCaptureWrapper(captures);
      replacement = GetSubstitution(matched, string, position, capturesWrapper,
                                    replace);
    }
    if (position >= nextSourcePosition) {
      accumulatedResult +=
        %_SubString(string, nextSourcePosition, position) + replacement;
      nextSourcePosition = position + matchedLength;
    }
  }
  if (nextSourcePosition >= length) return accumulatedResult;
  return accumulatedResult + %_SubString(string, nextSourcePosition, length);
}
%FunctionRemovePrototype(RegExpSubclassReplace);


// ES#sec-regexp.prototype-@@search
// RegExp.prototype [ @@search ] ( string )
function RegExpSubclassSearch(string) {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(kIncompatibleMethodReceiver,
                        "RegExp.prototype.@@search", this);
  }
  string = TO_STRING(string);
  var previousLastIndex = this.lastIndex;
  if (previousLastIndex != 0) this.lastIndex = 0;
  var result = RegExpSubclassExec(this, string);
  var currentLastIndex = this.lastIndex;
  if (currentLastIndex != previousLastIndex) this.lastIndex = previousLastIndex;
  if (IS_NULL(result)) return -1;
  return result.index;
}
%FunctionRemovePrototype(RegExpSubclassSearch);


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


// ES6 21.2.5.3.
function RegExpGetFlags() {
  if (!IS_RECEIVER(this)) {
    throw %make_type_error(
        kRegExpNonObject, "RegExp.prototype.flags", TO_STRING(this));
  }
  var result = '';
  if (this.global) result += 'g';
  if (this.ignoreCase) result += 'i';
  if (this.multiline) result += 'm';
  if (this.unicode) result += 'u';
  if (this.sticky) result += 'y';
  return result;
}


// ES6 21.2.5.4.
function RegExpGetGlobal() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeOldFlagGetter);
      return UNDEFINED;
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.global");
  }
  return TO_BOOLEAN(REGEXP_GLOBAL(this));
}
%SetForceInlineFlag(RegExpGetGlobal);


// ES6 21.2.5.5.
function RegExpGetIgnoreCase() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeOldFlagGetter);
      return UNDEFINED;
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.ignoreCase");
  }
  return TO_BOOLEAN(REGEXP_IGNORE_CASE(this));
}


// ES6 21.2.5.7.
function RegExpGetMultiline() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeOldFlagGetter);
      return UNDEFINED;
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.multiline");
  }
  return TO_BOOLEAN(REGEXP_MULTILINE(this));
}


// ES6 21.2.5.10.
function RegExpGetSource() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeSourceGetter);
      return "(?:)";
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.source");
  }
  return REGEXP_SOURCE(this);
}


// ES6 21.2.5.12.
function RegExpGetSticky() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeStickyGetter);
      return UNDEFINED;
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.sticky");
  }
  return TO_BOOLEAN(REGEXP_STICKY(this));
}
%SetForceInlineFlag(RegExpGetSticky);


// ES6 21.2.5.15.
function RegExpGetUnicode() {
  if (!IS_REGEXP(this)) {
    if (this === GlobalRegExpPrototype) {
      %IncrementUseCounter(kRegExpPrototypeUnicodeGetter);
      return UNDEFINED;
    }
    throw %make_type_error(kRegExpNonRegExp, "RegExp.prototype.unicode");
  }
  return TO_BOOLEAN(REGEXP_UNICODE(this));
}
%SetForceInlineFlag(RegExpGetUnicode);


function RegExpSpecies() {
  return this;
}


// -------------------------------------------------------------------

utils.InstallGetter(GlobalRegExp, speciesSymbol, RegExpSpecies);

utils.InstallFunctions(GlobalRegExp.prototype, DONT_ENUM, [
  "test", RegExpSubclassTest,
  "toString", RegExpToString,
  "compile", RegExpCompileJS,
  matchSymbol, RegExpSubclassMatch,
  replaceSymbol, RegExpSubclassReplace,
  searchSymbol, RegExpSubclassSearch,
  splitSymbol, RegExpSubclassSplit,
]);

utils.InstallGetter(GlobalRegExp.prototype, 'flags', RegExpGetFlags);
utils.InstallGetter(GlobalRegExp.prototype, 'global', RegExpGetGlobal);
utils.InstallGetter(GlobalRegExp.prototype, 'ignoreCase', RegExpGetIgnoreCase);
utils.InstallGetter(GlobalRegExp.prototype, 'multiline', RegExpGetMultiline);
utils.InstallGetter(GlobalRegExp.prototype, 'source', RegExpGetSource);
utils.InstallGetter(GlobalRegExp.prototype, 'sticky', RegExpGetSticky);
utils.InstallGetter(GlobalRegExp.prototype, 'unicode', RegExpGetUnicode);

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

// TODO(jgruber): All of these getters and setters were intended to be installed
// with various attributes (e.g. DONT_ENUM | DONT_DELETE), but
// InstallGetterSetter had a bug which ignored the passed attributes and
// simply installed as DONT_ENUM instead. We might want to change back
// to the intended attributes at some point.
// On the other hand, installing attributes as DONT_ENUM matches the draft
// specification at
// https://github.com/claudepache/es-regexp-legacy-static-properties

%OptimizeObjectForAddingMultipleProperties(GlobalRegExp, 22);
utils.InstallGetterSetter(GlobalRegExp, 'input', RegExpGetInput, RegExpSetInput,
                          DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, '$_', RegExpGetInput, RegExpSetInput,
                          DONT_ENUM);


var NoOpSetter = function(ignored) {};


// Static properties set by a successful match.
utils.InstallGetterSetter(GlobalRegExp, 'lastMatch', RegExpGetLastMatch,
                          NoOpSetter, DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, '$&', RegExpGetLastMatch, NoOpSetter,
                          DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, 'lastParen', RegExpGetLastParen,
                          NoOpSetter, DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, '$+', RegExpGetLastParen, NoOpSetter,
                          DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, 'leftContext', RegExpGetLeftContext,
                          NoOpSetter, DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, '$`', RegExpGetLeftContext, NoOpSetter,
                          DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, 'rightContext', RegExpGetRightContext,
                          NoOpSetter, DONT_ENUM);
utils.InstallGetterSetter(GlobalRegExp, "$'", RegExpGetRightContext, NoOpSetter,
                          DONT_ENUM);

for (var i = 1; i < 10; ++i) {
  utils.InstallGetterSetter(GlobalRegExp, '$' + i, RegExpMakeCaptureGetter(i),
                            NoOpSetter, DONT_ENUM);
}
%ToFastProperties(GlobalRegExp);

%InstallToContext(["regexp_last_match_info", RegExpLastMatchInfo]);

// -------------------------------------------------------------------
// Internal

var InternalRegExpMatchInfo = {
  REGEXP_NUMBER_OF_CAPTURES: 2,
  REGEXP_LAST_SUBJECT:       "",
  REGEXP_LAST_INPUT:         UNDEFINED,
  CAPTURE0:                  0,
  CAPTURE1:                  0
};

function InternalRegExpMatch(regexp, subject) {
  var matchInfo = %_RegExpExec(regexp, subject, 0, InternalRegExpMatchInfo);
  if (!IS_NULL(matchInfo)) {
    RETURN_NEW_RESULT_FROM_MATCH_INFO(matchInfo, subject);
  }
  return null;
}

function InternalRegExpReplace(regexp, subject, replacement) {
  return %StringReplaceGlobalRegExpWithString(
      subject, regexp, replacement, InternalRegExpMatchInfo);
}

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.GetSubstitution = GetSubstitution;
  to.InternalRegExpMatch = InternalRegExpMatch;
  to.InternalRegExpReplace = InternalRegExpReplace;
  to.IsRegExp = IsRegExp;
  to.RegExpExec = DoRegExpExec;
  to.RegExpInitialize = RegExpInitialize;
  to.RegExpLastMatchInfo = RegExpLastMatchInfo;
});

})
