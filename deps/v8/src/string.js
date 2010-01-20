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


// This file relies on the fact that the following declaration has been made
// in runtime.js:
// const $String = global.String;
// const $NaN = 0/0;


// Set the String function and constructor.
%SetCode($String, function(x) {
  var value = %_ArgumentsLength() == 0 ? '' : ToString(x);
  if (%_IsConstructCall()) {
    %_SetValueOf(this, value);
  } else {
    return value;
  }
});

%FunctionSetPrototype($String, new $String());

// ECMA-262 section 15.5.4.2
function StringToString() {
  if (!IS_STRING(this) && !IS_STRING_WRAPPER(this))
    throw new $TypeError('String.prototype.toString is not generic');
  return %_ValueOf(this);
}


// ECMA-262 section 15.5.4.3
function StringValueOf() {
  if (!IS_STRING(this) && !IS_STRING_WRAPPER(this))
    throw new $TypeError('String.prototype.valueOf is not generic');
  return %_ValueOf(this);
}


// ECMA-262, section 15.5.4.4
function StringCharAt(pos) {
  var char_code = %_FastCharCodeAt(this, pos);
  if (!%_IsSmi(char_code)) {
    var subject = ToString(this);
    var index = TO_INTEGER(pos);
    if (index >= subject.length || index < 0) return "";
    char_code = %StringCharCodeAt(subject, index);
  }
  return %CharFromCode(char_code);
}


// ECMA-262 section 15.5.4.5
function StringCharCodeAt(pos) {
  var fast_answer = %_FastCharCodeAt(this, pos);
  if (%_IsSmi(fast_answer)) {
    return fast_answer;
  }
  var subject = ToString(this);
  var index = TO_INTEGER(pos);
  return %StringCharCodeAt(subject, index);
}


// ECMA-262, section 15.5.4.6
function StringConcat() {
  var len = %_ArgumentsLength() + 1;
  var parts = new $Array(len);
  parts[0] = IS_STRING(this) ? this : ToString(this);
  for (var i = 1; i < len; i++) {
    var part = %_Arguments(i - 1);
    parts[i] = IS_STRING(part) ? part : ToString(part);
  }
  return %StringBuilderConcat(parts, len, "");
}

// Match ES3 and Safari
%FunctionSetLength(StringConcat, 1);


// ECMA-262 section 15.5.4.7
function StringIndexOf(searchString /* position */) {  // length == 1
  var subject_str = ToString(this);
  var pattern_str = ToString(searchString);
  var subject_str_len = subject_str.length;
  var pattern_str_len = pattern_str.length;
  var index = 0;
  if (%_ArgumentsLength() > 1) {
    var arg1 = %_Arguments(1);  // position
    index = TO_INTEGER(arg1);
  }
  if (index < 0) index = 0;
  if (index > subject_str_len) index = subject_str_len;
  if (pattern_str_len + index > subject_str_len) return -1;
  return %StringIndexOf(subject_str, pattern_str, index);
}


// ECMA-262 section 15.5.4.8
function StringLastIndexOf(searchString /* position */) {  // length == 1
  var sub = ToString(this);
  var subLength = sub.length;
  var pat = ToString(searchString);
  var patLength = pat.length;
  var index = subLength - patLength;
  if (%_ArgumentsLength() > 1) {
    var position = ToNumber(%_Arguments(1));
    if (!$isNaN(position)) {
      position = TO_INTEGER(position);
      if (position < 0) {
        position = 0;
      }
      if (position + patLength < subLength) {
        index = position
      }
    }
  }
  if (index < 0) {
    return -1;
  }
  return %StringLastIndexOf(sub, pat, index);
}


// ECMA-262 section 15.5.4.9
//
// This function is implementation specific.  For now, we do not
// do anything locale specific.
function StringLocaleCompare(other) {
  if (%_ArgumentsLength() === 0) return 0;

  var this_str = ToString(this);
  var other_str = ToString(other);
  return %StringLocaleCompare(this_str, other_str);
}


// ECMA-262 section 15.5.4.10
function StringMatch(regexp) {
  if (!IS_REGEXP(regexp)) regexp = new ORIGINAL_REGEXP(regexp);
  var subject = ToString(this);

  if (!regexp.global) return regexp.exec(subject);
  %_Log('regexp', 'regexp-match,%0S,%1r', [subject, regexp]);
  // lastMatchInfo is defined in regexp-delay.js.
  return %StringMatch(subject, regexp, lastMatchInfo);
}


// SubString is an internal function that returns the sub string of 'string'.
// If resulting string is of length 1, we use the one character cache
// otherwise we call the runtime system.
function SubString(string, start, end) {
  // Use the one character string cache.
  if (start + 1 == end) {
    var char_code = %_FastCharCodeAt(string, start);
    if (!%_IsSmi(char_code)) {
      char_code = %StringCharCodeAt(string, start);
    }
    return %CharFromCode(char_code);
  }
  return %_SubString(string, start, end);
}


// This has the same size as the lastMatchInfo array, and can be used for
// functions that expect that structure to be returned.  It is used when the
// needle is a string rather than a regexp.  In this case we can't update
// lastMatchArray without erroneously affecting the properties on the global
// RegExp object.
var reusableMatchInfo = [2, "", "", -1, -1];


// ECMA-262, section 15.5.4.11
function StringReplace(search, replace) {
  var subject = IS_STRING(this) ? this : ToString(this);

  // Delegate to one of the regular expression variants if necessary.
  if (IS_REGEXP(search)) {
    %_Log('regexp', 'regexp-replace,%0r,%1S', [search, subject]);
    if (IS_FUNCTION(replace)) {
      return StringReplaceRegExpWithFunction(subject, search, replace);
    } else {
      return StringReplaceRegExp(subject, search, replace);
    }
  }

  // Convert the search argument to a string and search for it.
  search = IS_STRING(search) ? search : ToString(search);
  var start = %StringIndexOf(subject, search, 0);
  if (start < 0) return subject;
  var end = start + search.length;

  var builder = new ReplaceResultBuilder(subject);
  // prefix
  builder.addSpecialSlice(0, start);

  // Compute the string to replace with.
  if (IS_FUNCTION(replace)) {
    builder.add(replace.call(null, search, start, subject));
  } else {
    reusableMatchInfo[CAPTURE0] = start;
    reusableMatchInfo[CAPTURE1] = end;
    if (!IS_STRING(replace)) replace = ToString(replace);
    ExpandReplacement(replace, subject, reusableMatchInfo, builder);
  }

  // suffix
  builder.addSpecialSlice(end, subject.length);

  return builder.generate();
}


// Helper function for regular expressions in String.prototype.replace.
function StringReplaceRegExp(subject, regexp, replace) {
  replace = ToString(replace);
  return %StringReplaceRegExpWithString(subject,
                                        regexp,
                                        replace,
                                        lastMatchInfo);
};


// Expand the $-expressions in the string and return a new string with
// the result.
function ExpandReplacement(string, subject, matchInfo, builder) {
  var next = %StringIndexOf(string, '$', 0);
  if (next < 0) {
    builder.add(string);
    return;
  }

  // Compute the number of captures; see ECMA-262, 15.5.4.11, p. 102.
  var m = NUMBER_OF_CAPTURES(matchInfo) >> 1;  // Includes the match.

  if (next > 0) builder.add(SubString(string, 0, next));
  var length = string.length;

  while (true) {
    var expansion = '$';
    var position = next + 1;
    if (position < length) {
      var peek = %_FastCharCodeAt(string, position);
      if (!%_IsSmi(peek)) {
        peek = %StringCharCodeAt(string, position);
      }
      if (peek == 36) {         // $$
        ++position;
        builder.add('$');
      } else if (peek == 38) {  // $& - match
        ++position;
        builder.addSpecialSlice(matchInfo[CAPTURE0],
                                matchInfo[CAPTURE1]);
      } else if (peek == 96) {  // $` - prefix
        ++position;
        builder.addSpecialSlice(0, matchInfo[CAPTURE0]);
      } else if (peek == 39) {  // $' - suffix
        ++position;
        builder.addSpecialSlice(matchInfo[CAPTURE1], subject.length);
      } else if (peek >= 48 && peek <= 57) {  // $n, 0 <= n <= 9
        ++position;
        var n = peek - 48;
        if (position < length) {
          peek = %_FastCharCodeAt(string, position);
          if (!%_IsSmi(peek)) {
            peek = %StringCharCodeAt(string, position);
          }
          // $nn, 01 <= nn <= 99
          if (n != 0 && peek == 48 || peek >= 49 && peek <= 57) {
            var nn = n * 10 + (peek - 48);
            if (nn < m) {
              // If the two digit capture reference is within range of
              // the captures, we use it instead of the single digit
              // one. Otherwise, we fall back to using the single
              // digit reference. This matches the behavior of
              // SpiderMonkey.
              ++position;
              n = nn;
            }
          }
        }
        if (0 < n && n < m) {
          addCaptureString(builder, matchInfo, n);
        } else {
          // Because of the captures range check in the parsing of two
          // digit capture references, we can only enter here when a
          // single digit capture reference is outside the range of
          // captures.
          builder.add('$');
          --position;
        }
      } else {
        builder.add('$');
      }
    } else {
      builder.add('$');
    }

    // Go the the next $ in the string.
    next = %StringIndexOf(string, '$', position);

    // Return if there are no more $ characters in the string. If we
    // haven't reached the end, we need to append the suffix.
    if (next < 0) {
      if (position < length) {
        builder.add(SubString(string, position, length));
      }
      return;
    }

    // Append substring between the previous and the next $ character.
    builder.add(SubString(string, position, next));
  }
};


// Compute the string of a given regular expression capture.
function CaptureString(string, lastCaptureInfo, index) {
  // Scale the index.
  var scaled = index << 1;
  // Compute start and end.
  var start = lastCaptureInfo[CAPTURE(scaled)];
  var end = lastCaptureInfo[CAPTURE(scaled + 1)];
  // If either start or end is missing return undefined.
  if (start < 0 || end < 0) return;
  return SubString(string, start, end);
};


// Add the string of a given regular expression capture to the
// ReplaceResultBuilder
function addCaptureString(builder, matchInfo, index) {
  // Scale the index.
  var scaled = index << 1;
  // Compute start and end.
  var start = matchInfo[CAPTURE(scaled)];
  var end = matchInfo[CAPTURE(scaled + 1)];
  // If either start or end is missing return.
  if (start < 0 || end <= start) return;
  builder.addSpecialSlice(start, end);
};


// Helper function for replacing regular expressions with the result of a
// function application in String.prototype.replace.  The function application
// must be interleaved with the regexp matching (contrary to ECMA-262
// 15.5.4.11) to mimic SpiderMonkey and KJS behavior when the function uses
// the static properties of the RegExp constructor.  Example:
//     'abcd'.replace(/(.)/g, function() { return RegExp.$1; }
// should be 'abcd' and not 'dddd' (or anything else).
function StringReplaceRegExpWithFunction(subject, regexp, replace) {
  var matchInfo = DoRegExpExec(regexp, subject, 0);
  if (IS_NULL(matchInfo)) return subject;

  var result = new ReplaceResultBuilder(subject);
  // There's at least one match.  If the regexp is global, we have to loop
  // over all matches.  The loop is not in C++ code here like the one in
  // RegExp.prototype.exec, because of the interleaved function application.
  // Unfortunately, that means this code is nearly duplicated, here and in
  // jsregexp.cc.
  if (regexp.global) {
    var numberOfCaptures = NUMBER_OF_CAPTURES(matchInfo) >> 1;
    var previous = 0;
    do {
      var startOfMatch = matchInfo[CAPTURE0];
      result.addSpecialSlice(previous, startOfMatch);
      previous = matchInfo[CAPTURE1];
      if (numberOfCaptures == 1) {
        var match = SubString(subject, startOfMatch, previous);
        // Don't call directly to avoid exposing the built-in global object.
        result.add(replace.call(null, match, startOfMatch, subject));
      } else {
        result.add(ApplyReplacementFunction(replace, matchInfo, subject));
      }
      // Can't use matchInfo any more from here, since the function could
      // overwrite it.
      // Continue with the next match.
      // Increment previous if we matched an empty string, as per ECMA-262
      // 15.5.4.10.
      if (previous == startOfMatch) {
        // Add the skipped character to the output, if any.
        if (previous < subject.length) {
          result.addSpecialSlice(previous, previous + 1);
        }
        previous++;
      }

      // Per ECMA-262 15.10.6.2, if the previous index is greater than the
      // string length, there is no match
      matchInfo = (previous > subject.length)
          ? null
          : DoRegExpExec(regexp, subject, previous);
    } while (!IS_NULL(matchInfo));

    // Tack on the final right substring after the last match, if necessary.
    if (previous < subject.length) {
      result.addSpecialSlice(previous, subject.length);
    }
  } else { // Not a global regexp, no need to loop.
    result.addSpecialSlice(0, matchInfo[CAPTURE0]);
    var endOfMatch = matchInfo[CAPTURE1];
    result.add(ApplyReplacementFunction(replace, matchInfo, subject));
    // Can't use matchInfo any more from here, since the function could
    // overwrite it.
    result.addSpecialSlice(endOfMatch, subject.length);
  }

  return result.generate();
}


// Helper function to apply a string replacement function once.
function ApplyReplacementFunction(replace, matchInfo, subject) {
  // Compute the parameter list consisting of the match, captures, index,
  // and subject for the replace function invocation.
  var index = matchInfo[CAPTURE0];
  // The number of captures plus one for the match.
  var m = NUMBER_OF_CAPTURES(matchInfo) >> 1;
  if (m == 1) {
    var s = CaptureString(subject, matchInfo, 0);
    // Don't call directly to avoid exposing the built-in global object.
    return replace.call(null, s, index, subject);
  }
  var parameters = $Array(m + 2);
  for (var j = 0; j < m; j++) {
    parameters[j] = CaptureString(subject, matchInfo, j);
  }
  parameters[j] = index;
  parameters[j + 1] = subject;
  return replace.apply(null, parameters);
}


// ECMA-262 section 15.5.4.12
function StringSearch(re) {
  var regexp = new ORIGINAL_REGEXP(re);
  var s = ToString(this);
  var last_idx = regexp.lastIndex; // keep old lastIndex
  regexp.lastIndex = 0;            // ignore re.global property
  var result = regexp.exec(s);
  regexp.lastIndex = last_idx;     // restore lastIndex
  if (result == null)
    return -1;
  else
    return result.index;
}


// ECMA-262 section 15.5.4.13
function StringSlice(start, end) {
  var s = ToString(this);
  var s_len = s.length;
  var start_i = TO_INTEGER(start);
  var end_i = s_len;
  if (end !== void 0)
    end_i = TO_INTEGER(end);

  if (start_i < 0) {
    start_i += s_len;
    if (start_i < 0)
      start_i = 0;
  } else {
    if (start_i > s_len)
      start_i = s_len;
  }

  if (end_i < 0) {
    end_i += s_len;
    if (end_i < 0)
      end_i = 0;
  } else {
    if (end_i > s_len)
      end_i = s_len;
  }

  var num_c = end_i - start_i;
  if (num_c < 0)
    num_c = 0;

  return SubString(s, start_i, start_i + num_c);
}


// ECMA-262 section 15.5.4.14
function StringSplit(separator, limit) {
  var subject = ToString(this);
  limit = (IS_UNDEFINED(limit)) ? 0xffffffff : TO_UINT32(limit);
  if (limit === 0) return [];

  // ECMA-262 says that if separator is undefined, the result should
  // be an array of size 1 containing the entire string.  SpiderMonkey
  // and KJS have this behaviour only when no separator is given.  If
  // undefined is explicitly given, they convert it to a string and
  // use that.  We do as SpiderMonkey and KJS.
  if (%_ArgumentsLength() === 0) {
    return [subject];
  }

  var length = subject.length;
  if (IS_REGEXP(separator)) {
    %_Log('regexp', 'regexp-split,%0S,%1r', [subject, separator]);
  } else {
    separator = ToString(separator);
    // If the separator string is empty then return the elements in the subject.
    if (separator.length == 0) {
      var result = $Array(length);
      for (var i = 0; i < length; i++) result[i] = subject[i];
      return result;
    }
  }

  if (length === 0) {
    if (splitMatch(separator, subject, 0, 0) != null) return [];
    return [subject];
  }

  var currentIndex = 0;
  var startIndex = 0;
  var result = [];

  while (true) {

    if (startIndex === length) {
      result[result.length] = subject.slice(currentIndex, length);
      return result;
    }

    var matchInfo = splitMatch(separator, subject, currentIndex, startIndex);

    if (IS_NULL(matchInfo)) {
      result[result.length] = subject.slice(currentIndex, length);
      return result;
    }

    var endIndex = matchInfo[CAPTURE1];

    // We ignore a zero-length match at the currentIndex.
    if (startIndex === endIndex && endIndex === currentIndex) {
      startIndex++;
      continue;
    }

    result[result.length] = SubString(subject, currentIndex, matchInfo[CAPTURE0]);
    if (result.length === limit) return result;

    for (var i = 2; i < NUMBER_OF_CAPTURES(matchInfo); i += 2) {
      var start = matchInfo[CAPTURE(i)];
      var end = matchInfo[CAPTURE(i + 1)];
      if (start != -1 && end != -1) {
        result[result.length] = SubString(subject, start, end);
      } else {
        result[result.length] = void 0;
      }
      if (result.length === limit) return result;
    }

    startIndex = currentIndex = endIndex;
  }
}


// ECMA-262 section 15.5.4.14
// Helper function used by split.  This version returns the matchInfo
// instead of allocating a new array with basically the same information.
function splitMatch(separator, subject, current_index, start_index) {
  if (IS_REGEXP(separator)) {
    var matchInfo = DoRegExpExec(separator, subject, start_index);
    if (matchInfo == null) return null;
    // Section 15.5.4.14 paragraph two says that we do not allow zero length
    // matches at the end of the string.
    if (matchInfo[CAPTURE0] === subject.length) return null;
    return matchInfo;
  }

  var separatorIndex = subject.indexOf(separator, start_index);
  if (separatorIndex === -1) return null;

  reusableMatchInfo[CAPTURE0] = separatorIndex;
  reusableMatchInfo[CAPTURE1] = separatorIndex + separator.length;
  return reusableMatchInfo;
};


// ECMA-262 section 15.5.4.15
function StringSubstring(start, end) {
  var s = this;
  if (!IS_STRING(s)) s = ToString(s);
  var s_len = s.length;

  var start_i = TO_INTEGER(start);
  if (start_i < 0) {
    start_i = 0;
  } else if (start_i > s_len) {
    start_i = s_len;
  }

  var end_i = s_len;
  if (!IS_UNDEFINED(end)) {
    end_i = TO_INTEGER(end);
    if (end_i > s_len) {
      end_i = s_len;
    } else {
      if (end_i < 0) end_i = 0;
      if (start_i > end_i) {
        var tmp = end_i;
        end_i = start_i;
        start_i = tmp;
      }
    }
  }

  return SubString(s, start_i, end_i);
}


// This is not a part of ECMA-262.
function StringSubstr(start, n) {
  var s = ToString(this);
  var len;

  // Correct n: If not given, set to string length; if explicitly
  // set to undefined, zero, or negative, returns empty string.
  if (n === void 0) {
    len = s.length;
  } else {
    len = TO_INTEGER(n);
    if (len <= 0) return '';
  }

  // Correct start: If not given (or undefined), set to zero; otherwise
  // convert to integer and handle negative case.
  if (start === void 0) {
    start = 0;
  } else {
    start = TO_INTEGER(start);
    // If positive, and greater than or equal to the string length,
    // return empty string.
    if (start >= s.length) return '';
    // If negative and absolute value is larger than the string length,
    // use zero.
    if (start < 0) {
      start += s.length;
      if (start < 0) start = 0;
    }
  }

  var end = start + len;
  if (end > s.length) end = s.length;

  return SubString(s, start, end);
}


// ECMA-262, 15.5.4.16
function StringToLowerCase() {
  return %StringToLowerCase(ToString(this));
}


// ECMA-262, 15.5.4.17
function StringToLocaleLowerCase() {
  return %StringToLowerCase(ToString(this));
}


// ECMA-262, 15.5.4.18
function StringToUpperCase() {
  return %StringToUpperCase(ToString(this));
}


// ECMA-262, 15.5.4.19
function StringToLocaleUpperCase() {
  return %StringToUpperCase(ToString(this));
}

// ES5, 15.5.4.20
function StringTrim() {
  return %StringTrim(ToString(this), true, true);
}

function StringTrimLeft() {
  return %StringTrim(ToString(this), true, false);
}

function StringTrimRight() {
  return %StringTrim(ToString(this), false, true);
}

// ECMA-262, section 15.5.3.2
function StringFromCharCode(code) {
  var n = %_ArgumentsLength();
  if (n == 1) return %CharFromCode(ToNumber(code) & 0xffff)

  // NOTE: This is not super-efficient, but it is necessary because we
  // want to avoid converting to numbers from within the virtual
  // machine. Maybe we can find another way of doing this?
  var codes = new $Array(n);
  for (var i = 0; i < n; i++) codes[i] = ToNumber(%_Arguments(i));
  return %StringFromCharCodeArray(codes);
}


// Helper function for very basic XSS protection.
function HtmlEscape(str) {
  return ToString(str).replace(/</g, "&lt;")
                      .replace(/>/g, "&gt;")
                      .replace(/"/g, "&quot;")
                      .replace(/'/g, "&#039;");
};


// Compatibility support for KJS.
// Tested by mozilla/js/tests/js1_5/Regress/regress-276103.js.
function StringLink(s) {
  return "<a href=\"" + HtmlEscape(s) + "\">" + this + "</a>";
}


function StringAnchor(name) {
  return "<a name=\"" + HtmlEscape(name) + "\">" + this + "</a>";
}


function StringFontcolor(color) {
  return "<font color=\"" + HtmlEscape(color) + "\">" + this + "</font>";
}


function StringFontsize(size) {
  return "<font size=\"" + HtmlEscape(size) + "\">" + this + "</font>";
}


function StringBig() {
  return "<big>" + this + "</big>";
}


function StringBlink() {
  return "<blink>" + this + "</blink>";
}


function StringBold() {
  return "<b>" + this + "</b>";
}


function StringFixed() {
  return "<tt>" + this + "</tt>";
}


function StringItalics() {
  return "<i>" + this + "</i>";
}


function StringSmall() {
  return "<small>" + this + "</small>";
}


function StringStrike() {
  return "<strike>" + this + "</strike>";
}


function StringSub() {
  return "<sub>" + this + "</sub>";
}


function StringSup() {
  return "<sup>" + this + "</sup>";
}


// ReplaceResultBuilder support.
function ReplaceResultBuilder(str) {
  this.elements = new $Array();
  this.special_string = str;
}


ReplaceResultBuilder.prototype.add = function(str) {
  if (!IS_STRING(str)) str = ToString(str);
  if (str.length > 0) {
    var elements = this.elements;
    elements[elements.length] = str;
  }
}


ReplaceResultBuilder.prototype.addSpecialSlice = function(start, end) {
  var len = end - start;
  if (len == 0) return;
  var elements = this.elements;
  if (start < 0x80000 && len < 0x800) {
    elements[elements.length] = (start << 11) + len;
  } else {
    // 0 < len <= String::kMaxLength and Smi::kMaxValue >= String::kMaxLength,
    // so -len is a smi.
    elements[elements.length] = -len;
    elements[elements.length] = start;
  }
}


ReplaceResultBuilder.prototype.generate = function() {
  var elements = this.elements;
  return %StringBuilderConcat(elements, elements.length, this.special_string);
}


function StringToJSON(key) {
  return CheckJSONPrimitive(this.valueOf());
}


// -------------------------------------------------------------------

function SetupString() {
  // Setup the constructor property on the String prototype object.
  %SetProperty($String.prototype, "constructor", $String, DONT_ENUM);


  // Setup the non-enumerable functions on the String object.
  InstallFunctions($String, DONT_ENUM, $Array(
    "fromCharCode", StringFromCharCode
  ));


  // Setup the non-enumerable functions on the String prototype object.
  InstallFunctionsOnHiddenPrototype($String.prototype, DONT_ENUM, $Array(
    "valueOf", StringValueOf,
    "toString", StringToString,
    "charAt", StringCharAt,
    "charCodeAt", StringCharCodeAt,
    "concat", StringConcat,
    "indexOf", StringIndexOf,
    "lastIndexOf", StringLastIndexOf,
    "localeCompare", StringLocaleCompare,
    "match", StringMatch,
    "replace", StringReplace,
    "search", StringSearch,
    "slice", StringSlice,
    "split", StringSplit,
    "substring", StringSubstring,
    "substr", StringSubstr,
    "toLowerCase", StringToLowerCase,
    "toLocaleLowerCase", StringToLocaleLowerCase,
    "toUpperCase", StringToUpperCase,
    "toLocaleUpperCase", StringToLocaleUpperCase,
    "trim", StringTrim,
    "trimLeft", StringTrimLeft,
    "trimRight", StringTrimRight,
    "link", StringLink,
    "anchor", StringAnchor,
    "fontcolor", StringFontcolor,
    "fontsize", StringFontsize,
    "big", StringBig,
    "blink", StringBlink,
    "bold", StringBold,
    "fixed", StringFixed,
    "italics", StringItalics,
    "small", StringSmall,
    "strike", StringStrike,
    "sub", StringSub,
    "sup", StringSup,
    "toJSON", StringToJSON
  ));
}


SetupString();
