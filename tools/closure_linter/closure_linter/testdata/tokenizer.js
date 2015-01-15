// Copyright 2008 The Closure Linter Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Errors relating to tokenizing.
 *
 * @author robbyw@google.com (Robby Walker)
 */

// Regression test: if regular expressions parse incorrectly this will emit an
// error such as: Missing space after '/'
x = /[^\']/;  // and all the other chars

// Regression test: if regular expressions parse incorrectly this will emit an
// error such as: Missing space before +
var regExp = fromStart ? / ^[\t\r\n]+/ : /[ \t\r\n]+$/;

// Regression test for bug 1032312: test for correct parsing of multiline
// strings
// +2: MULTI_LINE_STRING
var RG_MONTH_EVENT_TEMPLATE_SINGLE_QUOTE = new Template(
    '\
<div id="${divID}" class=month_event \
    style="top:${top}px;left:${left}px;width:${width}px;height:${height}px;\
        z-index:' + Z_INDEX_MONTH_EVENT);

// +2: MULTI_LINE_STRING
var RG_MONTH_EVENT_TEMPLATE_DOUBLE_QUOTE = new Template(
    "\
<div id='${divID}' class=month_event \
    style='top:${top}px;left:${left}px;width:${width}px;height:${height}px;\
           z-index:" + Z_INDEX_MONTH_EVENT);

// Regression test for bug 1032312: test for correct parsing of single line
// comment at end of line.  If it's parsed incorrectly, it reads the entire next
// line as a comment.
//
if (x) {
  // If the above line is treated as a comment, the closing brace below will
  // cause the linter to crash.
}

// Regression test for bitwise operators '^=', '>>>' and '>>>=' that weren't
// recognized as operators.
a -= b; a -= c; a ^= c >>> 13; a >>>= 1;

// Regression test as xor was not allowed on the end of a line.
x = 1000 ^
    45;

// Regression test for proper number parsing.  If parsed incorrectly, some of
// these notations can lead to missing spaces errors.
var x = 1e-6 + 1e+6 + 0. + .5 + 0.5 + 0.e-6 + .5e-6 + 0.5e-6 + 0x123abc +
    0X1Ab3 + 1E7;

// Regression test for keyword parsing - making sure the fact that the "do"
// keyword is a part of the identifier below doesn't break anything.
this.undoRedoManager_.undo();

// Regression test for regex as object value not matching.
x = {x: /./};

// Regression test for regex as last array element not matching.
x = [/./];

// Syntax tests for ES6:
x = x => x;
