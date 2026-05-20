// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


function test(expectation, f) {
  var stack;
  try {
    f();
  } catch (e) {
    stack = e.stack;
  }
  assertTrue(stack.indexOf("at eval (evaltest:" + expectation + ")") > 0);
}

/*
(function(
) {
1 + reference_error //@ sourceURL=evaltest
})
*/
test("3:5", new Function(
    '1 + reference_error //@ sourceURL=evaltest'));
/*
(function(x
) {

 1 + reference_error //@ sourceURL=evaltest
})
*/
test("4:6", new Function(
    'x', '\n 1 + reference_error //@ sourceURL=evaltest'));
/*
(function(x

,z//
,y
) {

 1 + reference_error //@ sourceURL=evaltest
})
*/
test("7:6", new Function(
    'x\n\n', "z//\n", "y", '\n 1 + reference_error //@ sourceURL=evaltest'));
/*
(function(x/\*,z//
,y*\/
) {
1 + reference_error //@ sourceURL=evaltest
})
*/
test("4:5", new Function(
    'x/*', "z//\n", "y*/", '1 + reference_error //@ sourceURL=evaltest'));
/*
(function () {
 1 + reference_error //@ sourceURL=evaltest5
})
*/
test("2:6", eval(
    '(function () {\n 1 + reference_error //@ sourceURL=evaltest\n})'));
