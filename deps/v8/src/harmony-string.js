// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file relies on the fact that the following declaration has been made
// in runtime.js:
// var $String = global.String;
// var $Array = global.Array;

// -------------------------------------------------------------------

// ES6 draft 01-20-14, section 21.1.3.13
function StringRepeat(count) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.repeat");

  var s = TO_STRING_INLINE(this);
  var n = ToInteger(count);
  // The maximum string length is stored in a smi, so a longer repeat
  // must result in a range error.
  if (n < 0 || n > %_MaxSmi()) {
    throw MakeRangeError("invalid_count_value", []);
  }

  var r = "";
  while (true) {
    if (n & 1) r += s;
    n >>= 1;
    if (n === 0) return r;
    s += s;
  }
}


// ES6 draft 04-05-14, section 21.1.3.18
function StringStartsWith(searchString /* position */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.startsWith");

  var s = TO_STRING_INLINE(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError("first_argument_not_regexp",
                        ["String.prototype.startsWith"]);
  }

  var ss = TO_STRING_INLINE(searchString);
  var pos = 0;
  if (%_ArgumentsLength() > 1) {
    pos = %_Arguments(1);  // position
    pos = ToInteger(pos);
  }

  var s_len = s.length;
  var start = MathMin(MathMax(pos, 0), s_len);
  var ss_len = ss.length;
  if (ss_len + start > s_len) {
    return false;
  }

  return %StringIndexOf(s, ss, start) === start;
}


// ES6 draft 04-05-14, section 21.1.3.7
function StringEndsWith(searchString /* position */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.endsWith");

  var s = TO_STRING_INLINE(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError("first_argument_not_regexp",
                        ["String.prototype.endsWith"]);
  }

  var ss = TO_STRING_INLINE(searchString);
  var s_len = s.length;
  var pos = s_len;
  if (%_ArgumentsLength() > 1) {
    var arg = %_Arguments(1);  // position
    if (!IS_UNDEFINED(arg)) {
      pos = ToInteger(arg);
    }
  }

  var end = MathMin(MathMax(pos, 0), s_len);
  var ss_len = ss.length;
  var start = end - ss_len;
  if (start < 0) {
    return false;
  }

  return %StringLastIndexOf(s, ss, start) === start;
}


// ES6 draft 04-05-14, section 21.1.3.6
function StringContains(searchString /* position */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.contains");

  var s = TO_STRING_INLINE(this);

  if (IS_REGEXP(searchString)) {
    throw MakeTypeError("first_argument_not_regexp",
                        ["String.prototype.contains"]);
  }

  var ss = TO_STRING_INLINE(searchString);
  var pos = 0;
  if (%_ArgumentsLength() > 1) {
    pos = %_Arguments(1);  // position
    pos = ToInteger(pos);
  }

  var s_len = s.length;
  var start = MathMin(MathMax(pos, 0), s_len);
  var ss_len = ss.length;
  if (ss_len + start > s_len) {
    return false;
  }

  return %StringIndexOf(s, ss, start) !== -1;
}


// ES6 Draft 05-22-2014, section 21.1.3.3
function StringCodePointAt(pos) {
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.codePointAt");

  var string = TO_STRING_INLINE(this);
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
  var code;
  var length = %_ArgumentsLength();
  var index;
  var result = "";
  for (index = 0; index < length; index++) {
    code = %_Arguments(index);
    if (!%_IsSmi(code)) {
      code = ToNumber(code);
    }
    if (code < 0 || code > 0x10FFFF || code !== TO_INTEGER(code)) {
      throw MakeRangeError("invalid_code_point", [code]);
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

function ExtendStringPrototype() {
  %CheckIsBootstrapping();

  // Set up the non-enumerable functions on the String object.
  InstallFunctions($String, DONT_ENUM, $Array(
    "fromCodePoint", StringFromCodePoint
  ));

  // Set up the non-enumerable functions on the String prototype object.
  InstallFunctions($String.prototype, DONT_ENUM, $Array(
    "codePointAt", StringCodePointAt,
    "contains", StringContains,
    "endsWith", StringEndsWith,
    "repeat", StringRepeat,
    "startsWith", StringStartsWith
  ));
}

ExtendStringPrototype();
