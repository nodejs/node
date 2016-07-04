// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, utils) {

%CheckIsBootstrapping();

// -------------------------------------------------------------------
// Imports

var ArrayIndexOf;
var ArrayJoin;
var GlobalRegExp = global.RegExp;
var GlobalString = global.String;
var InternalArray = utils.InternalArray;
var InternalPackedArray = utils.InternalPackedArray;
var MakeRangeError;
var MakeTypeError;
var MaxSimple;
var MinSimple;
var matchSymbol = utils.ImportNow("match_symbol");
var RegExpExecNoTests;
var replaceSymbol = utils.ImportNow("replace_symbol");
var searchSymbol = utils.ImportNow("search_symbol");
var splitSymbol = utils.ImportNow("split_symbol");

utils.Import(function(from) {
  ArrayIndexOf = from.ArrayIndexOf;
  ArrayJoin = from.ArrayJoin;
  MakeRangeError = from.MakeRangeError;
  MakeTypeError = from.MakeTypeError;
  MaxSimple = from.MaxSimple;
  MinSimple = from.MinSimple;
  RegExpExecNoTests = from.RegExpExecNoTests;
});

//-------------------------------------------------------------------

// ECMA-262 section 15.5.4.2
function StringToString() {
  if (!IS_STRING(this) && !IS_STRING_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'String.prototype.toString');
  }
  return %_ValueOf(this);
}


// ECMA-262 section 15.5.4.3
function StringValueOf() {
  if (!IS_STRING(this) && !IS_STRING_WRAPPER(this)) {
    throw MakeTypeError(kNotGeneric, 'String.prototype.valueOf');
  }
  return %_ValueOf(this);
}


// ECMA-262, section 15.5.4.4
function StringCharAtJS(pos) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.charAt");

  var result = %_StringCharAt(this, pos);
  if (%_IsSmi(result)) {
    result = %_StringCharAt(TO_STRING(this), TO_INTEGER(pos));
  }
  return result;
}


// ECMA-262 section 15.5.4.5
function StringCharCodeAtJS(pos) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.charCodeAt");

  var result = %_StringCharCodeAt(this, pos);
  if (!%_IsSmi(result)) {
    result = %_StringCharCodeAt(TO_STRING(this), TO_INTEGER(pos));
  }
  return result;
}


// ECMA-262, section 15.5.4.6
function StringConcat(other /* and more */) {  // length == 1
  "use strict";
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.concat");
  var s = TO_STRING(this);
  var len = arguments.length;
  for (var i = 0; i < len; ++i) {
    s = s + TO_STRING(arguments[i]);
  }
  return s;
}


// ECMA-262 section 15.5.4.7
function StringIndexOf(pattern, position) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.indexOf");

  var subject = TO_STRING(this);
  pattern = TO_STRING(pattern);
  var index = TO_INTEGER(position);
  if (index < 0) index = 0;
  if (index > subject.length) index = subject.length;
  return %StringIndexOf(subject, pattern, index);
}

%FunctionSetLength(StringIndexOf, 1);


// ECMA-262 section 15.5.4.8
function StringLastIndexOf(pat, pos) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.lastIndexOf");

  var sub = TO_STRING(this);
  var subLength = sub.length;
  var pat = TO_STRING(pat);
  var patLength = pat.length;
  var index = subLength - patLength;
  var position = TO_NUMBER(pos);
  if (!NUMBER_IS_NAN(position)) {
    position = TO_INTEGER(position);
    if (position < 0) {
      position = 0;
    }
    if (position + patLength < subLength) {
      index = position;
    }
  }
  if (index < 0) {
    return -1;
  }
  return %StringLastIndexOf(sub, pat, index);
}

%FunctionSetLength(StringLastIndexOf, 1);


// ECMA-262 section 15.5.4.9
//
// This function is implementation specific.  For now, we do not
// do anything locale specific.
function StringLocaleCompareJS(other) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.localeCompare");

  return %StringLocaleCompare(TO_STRING(this), TO_STRING(other));
}


// ES6 21.1.3.11.
function StringMatchJS(pattern) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.match");

  if (!IS_NULL_OR_UNDEFINED(pattern)) {
    var matcher = pattern[matchSymbol];
    if (!IS_UNDEFINED(matcher)) {
      return %_Call(matcher, pattern, this);
    }
  }

  var subject = TO_STRING(this);

  // Non-regexp argument.
  var regexp = new GlobalRegExp(pattern);
  return RegExpExecNoTests(regexp, subject, 0);
}


// ECMA-262 v6, section 21.1.3.12
//
// For now we do nothing, as proper normalization requires big tables.
// If Intl is enabled, then i18n.js will override it and provide the the
// proper functionality.
function StringNormalize(formArg) {  // length == 0
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.normalize");
  var s = TO_STRING(this);

  var form = IS_UNDEFINED(formArg) ? 'NFC' : TO_STRING(formArg);

  var NORMALIZATION_FORMS = ['NFC', 'NFD', 'NFKC', 'NFKD'];
  var normalizationForm = %_Call(ArrayIndexOf, NORMALIZATION_FORMS, form);
  if (normalizationForm === -1) {
    throw MakeRangeError(kNormalizationForm,
                         %_Call(ArrayJoin, NORMALIZATION_FORMS, ', '));
  }

  return s;
}

%FunctionSetLength(StringNormalize, 0);


// This has the same size as the RegExpLastMatchInfo array, and can be used
// for functions that expect that structure to be returned.  It is used when
// the needle is a string rather than a regexp.  In this case we can't update
// lastMatchArray without erroneously affecting the properties on the global
// RegExp object.
var reusableMatchInfo = [2, "", "", -1, -1];


// ES6, section 21.1.3.14
function StringReplace(search, replace) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.replace");

  // Decision tree for dispatch
  // .. regexp search (in src/js/regexp.js, RegExpReplace)
  // .... string replace
  // ...... non-global search
  // ........ empty string replace
  // ........ non-empty string replace (with $-expansion)
  // ...... global search
  // ........ no need to circumvent last match info override
  // ........ need to circument last match info override
  // .... function replace
  // ...... global search
  // ...... non-global search
  // .. string search
  // .... special case that replaces with one single character
  // ...... function replace
  // ...... string replace (with $-expansion)

  if (!IS_NULL_OR_UNDEFINED(search)) {
    var replacer = search[replaceSymbol];
    if (!IS_UNDEFINED(replacer)) {
      return %_Call(replacer, search, this, replace);
    }
  }

  var subject = TO_STRING(this);

  search = TO_STRING(search);

  if (search.length == 1 &&
      subject.length > 0xFF &&
      IS_STRING(replace) &&
      %StringIndexOf(replace, '$', 0) < 0) {
    // Searching by traversing a cons string tree and replace with cons of
    // slices works only when the replaced string is a single character, being
    // replaced by a simple string and only pays off for long strings.
    return %StringReplaceOneCharWithString(subject, search, replace);
  }
  var start = %StringIndexOf(subject, search, 0);
  if (start < 0) return subject;
  var end = start + search.length;

  var result = %_SubString(subject, 0, start);

  // Compute the string to replace with.
  if (IS_CALLABLE(replace)) {
    result += replace(search, start, subject);
  } else {
    reusableMatchInfo[CAPTURE0] = start;
    reusableMatchInfo[CAPTURE1] = end;
    result = ExpandReplacement(TO_STRING(replace),
                               subject,
                               reusableMatchInfo,
                               result);
  }

  return result + %_SubString(subject, end, subject.length);
}


// Expand the $-expressions in the string and return a new string with
// the result.
function ExpandReplacement(string, subject, matchInfo, result) {
  var length = string.length;
  var next = %StringIndexOf(string, '$', 0);
  if (next < 0) {
    if (length > 0) result += string;
    return result;
  }

  if (next > 0) result += %_SubString(string, 0, next);

  while (true) {
    var expansion = '$';
    var position = next + 1;
    if (position < length) {
      var peek = %_StringCharCodeAt(string, position);
      if (peek == 36) {         // $$
        ++position;
        result += '$';
      } else if (peek == 38) {  // $& - match
        ++position;
        result +=
          %_SubString(subject, matchInfo[CAPTURE0], matchInfo[CAPTURE1]);
      } else if (peek == 96) {  // $` - prefix
        ++position;
        result += %_SubString(subject, 0, matchInfo[CAPTURE0]);
      } else if (peek == 39) {  // $' - suffix
        ++position;
        result += %_SubString(subject, matchInfo[CAPTURE1], subject.length);
      } else if (peek >= 48 && peek <= 57) {
        // Valid indices are $1 .. $9, $01 .. $09 and $10 .. $99
        var scaled_index = (peek - 48) << 1;
        var advance = 1;
        var number_of_captures = NUMBER_OF_CAPTURES(matchInfo);
        if (position + 1 < string.length) {
          var next = %_StringCharCodeAt(string, position + 1);
          if (next >= 48 && next <= 57) {
            var new_scaled_index = scaled_index * 10 + ((next - 48) << 1);
            if (new_scaled_index < number_of_captures) {
              scaled_index = new_scaled_index;
              advance = 2;
            }
          }
        }
        if (scaled_index != 0 && scaled_index < number_of_captures) {
          var start = matchInfo[CAPTURE(scaled_index)];
          if (start >= 0) {
            result +=
              %_SubString(subject, start, matchInfo[CAPTURE(scaled_index + 1)]);
          }
          position += advance;
        } else {
          result += '$';
        }
      } else {
        result += '$';
      }
    } else {
      result += '$';
    }

    // Go the the next $ in the string.
    next = %StringIndexOf(string, '$', position);

    // Return if there are no more $ characters in the string. If we
    // haven't reached the end, we need to append the suffix.
    if (next < 0) {
      if (position < length) {
        result += %_SubString(string, position, length);
      }
      return result;
    }

    // Append substring between the previous and the next $ character.
    if (next > position) {
      result += %_SubString(string, position, next);
    }
  }
  return result;
}


// ES6 21.1.3.15.
function StringSearch(pattern) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.search");

  if (!IS_NULL_OR_UNDEFINED(pattern)) {
    var searcher = pattern[searchSymbol];
    if (!IS_UNDEFINED(searcher)) {
      return %_Call(searcher, pattern, this);
    }
  }

  var subject = TO_STRING(this);
  var regexp = new GlobalRegExp(pattern);
  return %_Call(regexp[searchSymbol], regexp, subject);
}


// ECMA-262 section 15.5.4.13
function StringSlice(start, end) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.slice");

  var s = TO_STRING(this);
  var s_len = s.length;
  var start_i = TO_INTEGER(start);
  var end_i = s_len;
  if (!IS_UNDEFINED(end)) {
    end_i = TO_INTEGER(end);
  }

  if (start_i < 0) {
    start_i += s_len;
    if (start_i < 0) {
      start_i = 0;
    }
  } else {
    if (start_i > s_len) {
      return '';
    }
  }

  if (end_i < 0) {
    end_i += s_len;
    if (end_i < 0) {
      return '';
    }
  } else {
    if (end_i > s_len) {
      end_i = s_len;
    }
  }

  if (end_i <= start_i) {
    return '';
  }

  return %_SubString(s, start_i, end_i);
}


// ES6 21.1.3.17.
function StringSplitJS(separator, limit) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.split");

  if (!IS_NULL_OR_UNDEFINED(separator)) {
    var splitter = separator[splitSymbol];
    if (!IS_UNDEFINED(splitter)) {
      return %_Call(splitter, separator, this, limit);
    }
  }

  var subject = TO_STRING(this);
  limit = (IS_UNDEFINED(limit)) ? kMaxUint32 : TO_UINT32(limit);

  var length = subject.length;
  var separator_string = TO_STRING(separator);

  if (limit === 0) return [];

  // ECMA-262 says that if separator is undefined, the result should
  // be an array of size 1 containing the entire string.
  if (IS_UNDEFINED(separator)) return [subject];

  var separator_length = separator_string.length;

  // If the separator string is empty then return the elements in the subject.
  if (separator_length === 0) return %StringToArray(subject, limit);

  return %StringSplit(subject, separator_string, limit);
}


// ECMA-262 section 15.5.4.15
function StringSubstring(start, end) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.subString");

  var s = TO_STRING(this);
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

  return %_SubString(s, start_i, end_i);
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.1
function StringSubstr(start, n) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.substr");

  var s = TO_STRING(this);
  var len;

  // Correct n: If not given, set to string length; if explicitly
  // set to undefined, zero, or negative, returns empty string.
  if (IS_UNDEFINED(n)) {
    len = s.length;
  } else {
    len = TO_INTEGER(n);
    if (len <= 0) return '';
  }

  // Correct start: If not given (or undefined), set to zero; otherwise
  // convert to integer and handle negative case.
  if (IS_UNDEFINED(start)) {
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

  return %_SubString(s, start, end);
}


// ECMA-262, 15.5.4.16
function StringToLowerCaseJS() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.toLowerCase");

  return %StringToLowerCase(TO_STRING(this));
}


// ECMA-262, 15.5.4.17
function StringToLocaleLowerCase() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.toLocaleLowerCase");

  return %StringToLowerCase(TO_STRING(this));
}


// ECMA-262, 15.5.4.18
function StringToUpperCaseJS() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.toUpperCase");

  return %StringToUpperCase(TO_STRING(this));
}


// ECMA-262, 15.5.4.19
function StringToLocaleUpperCase() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.toLocaleUpperCase");

  return %StringToUpperCase(TO_STRING(this));
}

// ES5, 15.5.4.20
function StringTrimJS() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.trim");

  return %StringTrim(TO_STRING(this), true, true);
}

function StringTrimLeft() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.trimLeft");

  return %StringTrim(TO_STRING(this), true, false);
}

function StringTrimRight() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.trimRight");

  return %StringTrim(TO_STRING(this), false, true);
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.2.1
function HtmlEscape(str) {
  return %_Call(StringReplace, TO_STRING(str), /"/g, "&quot;");
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.2
function StringAnchor(name) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.anchor");
  return "<a name=\"" + HtmlEscape(name) + "\">" + TO_STRING(this) +
         "</a>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.3
function StringBig() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.big");
  return "<big>" + TO_STRING(this) + "</big>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.4
function StringBlink() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.blink");
  return "<blink>" + TO_STRING(this) + "</blink>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.5
function StringBold() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.bold");
  return "<b>" + TO_STRING(this) + "</b>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.6
function StringFixed() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fixed");
  return "<tt>" + TO_STRING(this) + "</tt>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.7
function StringFontcolor(color) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontcolor");
  return "<font color=\"" + HtmlEscape(color) + "\">" + TO_STRING(this) +
         "</font>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.8
function StringFontsize(size) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.fontsize");
  return "<font size=\"" + HtmlEscape(size) + "\">" + TO_STRING(this) +
         "</font>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.9
function StringItalics() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.italics");
  return "<i>" + TO_STRING(this) + "</i>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.10
function StringLink(s) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.link");
  return "<a href=\"" + HtmlEscape(s) + "\">" + TO_STRING(this) + "</a>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.11
function StringSmall() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.small");
  return "<small>" + TO_STRING(this) + "</small>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.12
function StringStrike() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.strike");
  return "<strike>" + TO_STRING(this) + "</strike>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.13
function StringSub() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.sub");
  return "<sub>" + TO_STRING(this) + "</sub>";
}


// ES6 draft, revision 26 (2014-07-18), section B.2.3.14
function StringSup() {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.sup");
  return "<sup>" + TO_STRING(this) + "</sup>";
}

// ES6, section 21.1.3.13
function StringRepeat(count) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.repeat");

  var s = TO_STRING(this);
  var n = TO_INTEGER(count);

  if (n < 0 || n === INFINITY) throw MakeRangeError(kInvalidCountValue);

  // Early return to allow an arbitrarily-large repeat of the empty string.
  if (s.length === 0) return "";

  // The maximum string length is stored in a smi, so a longer repeat
  // must result in a range error.
  if (n > %_MaxSmi()) throw MakeRangeError(kInvalidCountValue);

  var r = "";
  while (true) {
    if (n & 1) r += s;
    n >>= 1;
    if (n === 0) return r;
    s += s;
  }
}


// ES6 draft 04-05-14, section 21.1.3.18
function StringStartsWith(searchString, position) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.startsWith");

  var s = TO_STRING(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError(kFirstArgumentNotRegExp, "String.prototype.startsWith");
  }

  var ss = TO_STRING(searchString);
  var pos = TO_INTEGER(position);

  var s_len = s.length;
  var start = MinSimple(MaxSimple(pos, 0), s_len);
  var ss_len = ss.length;
  if (ss_len + start > s_len) {
    return false;
  }

  return %_SubString(s, start, start + ss_len) === ss;
}

%FunctionSetLength(StringStartsWith, 1);


// ES6 draft 04-05-14, section 21.1.3.7
function StringEndsWith(searchString, position) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.endsWith");

  var s = TO_STRING(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError(kFirstArgumentNotRegExp, "String.prototype.endsWith");
  }

  var ss = TO_STRING(searchString);
  var s_len = s.length;
  var pos = !IS_UNDEFINED(position) ? TO_INTEGER(position) : s_len

  var end = MinSimple(MaxSimple(pos, 0), s_len);
  var ss_len = ss.length;
  var start = end - ss_len;
  if (start < 0) {
    return false;
  }

  return %_SubString(s, start, start + ss_len) === ss;
}

%FunctionSetLength(StringEndsWith, 1);


// ES6 draft 04-05-14, section 21.1.3.6
function StringIncludes(searchString, position) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.includes");

  var string = TO_STRING(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError(kFirstArgumentNotRegExp, "String.prototype.includes");
  }

  searchString = TO_STRING(searchString);
  var pos = TO_INTEGER(position);

  var stringLength = string.length;
  if (pos < 0) pos = 0;
  if (pos > stringLength) pos = stringLength;
  var searchStringLength = searchString.length;

  if (searchStringLength + pos > stringLength) {
    return false;
  }

  return %StringIndexOf(string, searchString, pos) !== -1;
}

%FunctionSetLength(StringIncludes, 1);


// ES6 Draft 05-22-2014, section 21.1.3.3
function StringCodePointAt(pos) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.codePointAt");

  var string = TO_STRING(this);
  var size = string.length;
  pos = TO_INTEGER(pos);
  if (pos < 0 || pos >= size) {
    return UNDEFINED;
  }
  var first = %_StringCharCodeAt(string, pos);
  if (first < 0xD800 || first > 0xDBFF || pos + 1 == size) {
    return first;
  }
  var second = %_StringCharCodeAt(string, pos + 1);
  if (second < 0xDC00 || second > 0xDFFF) {
    return first;
  }
  return (first - 0xD800) * 0x400 + second + 0x2400;
}


// ES6 Draft 05-22-2014, section 21.1.2.2
function StringFromCodePoint(_) {  // length = 1
  "use strict";
  var code;
  var length = arguments.length;
  var index;
  var result = "";
  for (index = 0; index < length; index++) {
    code = arguments[index];
    if (!%_IsSmi(code)) {
      code = TO_NUMBER(code);
    }
    if (code < 0 || code > 0x10FFFF || code !== TO_INTEGER(code)) {
      throw MakeRangeError(kInvalidCodePoint, code);
    }
    if (code <= 0xFFFF) {
      result += %_StringCharFromCode(code);
    } else {
      code -= 0x10000;
      result += %_StringCharFromCode((code >>> 10) & 0x3FF | 0xD800);
      result += %_StringCharFromCode(code & 0x3FF | 0xDC00);
    }
  }
  return result;
}


// -------------------------------------------------------------------
// String methods related to templates

// ES6 Draft 03-17-2015, section 21.1.2.4
function StringRaw(callSite) {
  "use strict";
  var numberOfSubstitutions = arguments.length;
  var cooked = TO_OBJECT(callSite);
  var raw = TO_OBJECT(cooked.raw);
  var literalSegments = TO_LENGTH(raw.length);
  if (literalSegments <= 0) return "";

  var result = TO_STRING(raw[0]);

  for (var i = 1; i < literalSegments; ++i) {
    if (i < numberOfSubstitutions) {
      result += TO_STRING(arguments[i]);
    }
    result += TO_STRING(raw[i]);
  }

  return result;
}

// -------------------------------------------------------------------

// Set the String function and constructor.
%FunctionSetPrototype(GlobalString, new GlobalString());

// Set up the constructor property on the String prototype object.
%AddNamedProperty(
    GlobalString.prototype, "constructor", GlobalString, DONT_ENUM);

// Set up the non-enumerable functions on the String object.
utils.InstallFunctions(GlobalString, DONT_ENUM, [
  "fromCodePoint", StringFromCodePoint,
  "raw", StringRaw
]);

// Set up the non-enumerable functions on the String prototype object.
utils.InstallFunctions(GlobalString.prototype, DONT_ENUM, [
  "valueOf", StringValueOf,
  "toString", StringToString,
  "charAt", StringCharAtJS,
  "charCodeAt", StringCharCodeAtJS,
  "codePointAt", StringCodePointAt,
  "concat", StringConcat,
  "endsWith", StringEndsWith,
  "includes", StringIncludes,
  "indexOf", StringIndexOf,
  "lastIndexOf", StringLastIndexOf,
  "localeCompare", StringLocaleCompareJS,
  "match", StringMatchJS,
  "normalize", StringNormalize,
  "repeat", StringRepeat,
  "replace", StringReplace,
  "search", StringSearch,
  "slice", StringSlice,
  "split", StringSplitJS,
  "substring", StringSubstring,
  "substr", StringSubstr,
  "startsWith", StringStartsWith,
  "toLowerCase", StringToLowerCaseJS,
  "toLocaleLowerCase", StringToLocaleLowerCase,
  "toUpperCase", StringToUpperCaseJS,
  "toLocaleUpperCase", StringToLocaleUpperCase,
  "trim", StringTrimJS,
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
  "sup", StringSup
]);

// -------------------------------------------------------------------
// Exports

utils.Export(function(to) {
  to.ExpandReplacement = ExpandReplacement;
  to.StringCharAt = StringCharAtJS;
  to.StringIndexOf = StringIndexOf;
  to.StringLastIndexOf = StringLastIndexOf;
  to.StringMatch = StringMatchJS;
  to.StringReplace = StringReplace;
  to.StringSlice = StringSlice;
  to.StringSplit = StringSplitJS;
  to.StringSubstr = StringSubstr;
  to.StringSubstring = StringSubstring;
});

})
