// Copyright 2014 the V8 project authors. All rights reserved.
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


// ES6 draft 01-20-14, section 21.1.3.18
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


// ES6 draft 01-20-14, section 21.1.3.7
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


// ES6 draft 01-20-14, section 21.1.3.6
function StringContains(searchString /* position */) {  // length == 1
  CHECK_OBJECT_COERCIBLE(this, "String.prototype.contains");

  var s = TO_STRING_INLINE(this);
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
