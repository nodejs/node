// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that hoisting a function out of a lexical scope does not
// lead to a parsing error

// This used to cause a crash in the parser
function f(one) { class x { } { class x { } function g() { one; x; } g() } } f()

// This used to lead to a ReferenceError
function g() { var x = 1; { let x = 2; function g() { x; } g(); } }
assertEquals(undefined, g());

// This used to cause a crash in the parser
function __f_4(one) {
  var __v_10 = one + 1;
  {
    let __v_10 = one + 3;
    function __f_6() {
 one;
 __v_10;
    }
    __f_6();
  }
}
__f_4();
