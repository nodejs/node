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
  if (n < 0 || !NUMBER_IS_FINITE(n)) {
    throw MakeRangeError("invalid_count_value", []);
  }

  var elements = new InternalArray(n);
  for (var i = 0; i < n; i++) {
    elements[i] = s;
  }

  return %StringBuilderConcat(elements, n, "");
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


// -------------------------------------------------------------------

function ExtendStringPrototype() {
  %CheckIsBootstrapping();

  // Set up the non-enumerable functions on the String prototype object.
  InstallFunctions($String.prototype, DONT_ENUM, $Array(
    "repeat", StringRepeat,
    "startsWith", StringStartsWith,
    "endsWith", StringEndsWith,
    "contains", StringContains
  ));
}

ExtendStringPrototype();
