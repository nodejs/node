// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test for conflicting variable bindings.

function AssertEqualsStrictAndSloppy(value, code) {
  assertEquals(value, eval("(function() {" + code + "})()"));
  assertEquals(value, eval("(function() { 'use strict'; " + code + "})()"));
  assertEquals(value, eval("(function() { var x = 0; {" + code + "} })()"));
  assertEquals(value, eval("(function() { 'use strict'; var x = 0; {"
                           + code + "} })()"));
}

function AssertThrowsStrictAndSloppy(code, error) {
  assertThrows("(function() {" + code + "})()", error);
  assertThrows("(function() { 'use strict'; " + code + "})()", error);
  assertThrows("(function() { var x = 0; { " + code + "} })()", error);
  assertThrows("(function() { 'use strict'; var x = 0; {" + code + "} })()",
               error);
}

(function TestClassTDZ() {
  AssertEqualsStrictAndSloppy(
      "x", "function f() { return x; }; class x { }; return f().name;");
  AssertEqualsStrictAndSloppy
      ("x", "class x { }; function f() { return x; }; return f().name;");
  AssertEqualsStrictAndSloppy(
      "x", "class x { }; var result = f().name; " +
      "function f() { return x; }; return result;");
  AssertThrowsStrictAndSloppy(
      "function f() { return x; }; f(); class x { };", ReferenceError);
  AssertThrowsStrictAndSloppy(
      "f(); function f() { return x; }; class x { };", ReferenceError);
  AssertThrowsStrictAndSloppy(
      "f(); class x { }; function f() { return x; };", ReferenceError);
  AssertThrowsStrictAndSloppy(
      "var x = 1; { f(); class x { }; function f() { return x; }; }",
      ReferenceError);
  AssertThrowsStrictAndSloppy("x = 3; class x { };", ReferenceError)
})();

(function TestClassNameConflict() {
  AssertThrowsStrictAndSloppy("class x { }; var x;", SyntaxError);
  AssertThrowsStrictAndSloppy("var x; class x { };", SyntaxError);
  AssertThrowsStrictAndSloppy("class x { }; function x() { };", SyntaxError);
  AssertThrowsStrictAndSloppy("function x() { }; class x { };", SyntaxError);
  AssertThrowsStrictAndSloppy("class x { }; for (var x = 0; false;) { };",
                              SyntaxError);
  AssertThrowsStrictAndSloppy("for (var x = 0; false;) { }; class x { };",
                              SyntaxError);
})();

(function TestClassMutableBinding() {
  AssertEqualsStrictAndSloppy(
      "x3", "class x { }; var y = x.name; x = 3; return y + x;")
})();
